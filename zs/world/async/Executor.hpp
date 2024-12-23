#pragma once
#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <map>
#include <thread>
#include <variant>
#include <vector>

#include "../WorldExport.hpp"
#include "Coro.hpp"
#include "zensim/Platform.hpp"
#include "zensim/ZpcFunction.hpp"
#include "zensim/ZpcResource.hpp"
#include "zensim/execution/ConcurrencyPrimitive.hpp"
#include "zensim/zpc_tpls/moodycamel/concurrent_queue/concurrentqueue.h"

namespace zs {

#ifdef ZS_PLATFORM_OSX
  using jthread = std::jthread;
  using stop_source = std::stop_source;
  using stop_token = std::stop_token;
#else
  using jthread = std::jthread;
  using stop_source = std::stop_source;
  using stop_token = std::stop_token;
#endif

  ///
  /// scheduler
  ///
  struct CoroTaskNode {
    CoroTaskNode& to(CoroTaskNode& dst) {
      _succs.emplace_back(&dst);
      dst._preds.emplace_back(this);
      dst._numDeps.fetch_add(1, std::memory_order_relaxed);
      return *this;
    }

    enum state_e { idle = 0, planned, scheduled, done };
    std::vector<CoroTaskNode*> _preds{}, _succs{};
    std::atomic<int> _numDeps{0};
    Future<void> _task;
    std::string _tag{};
    state_e _state{idle};
  };
  inline UniquePtr<CoroTaskNode> to_task_node(Future<>&& promise) {
    UniquePtr<CoroTaskNode> ret{new CoroTaskNode()};
    ret->_task = zs::move(promise);
    ret->_numDeps = 0;
    ret->_state = CoroTaskNode::idle;
    return ret;
  }
  inline UniquePtr<CoroTaskNode> to_task_node(Future<>&& promise, std::string_view tag) {
    auto ret = to_task_node(zs::move(promise));
    ret->_tag = tag;
    return ret;
  }

  /// @ref Taro (Dianlun Li)
  struct ZS_WORLD_EXPORT Scheduler {
    /// task
    struct PersistentCoroHandle {
      PersistentCoroHandle(std::coroutine_handle<> h) noexcept : h{h} {}
      std::coroutine_handle<> h;
      operator bool() const { return static_cast<bool>(h); }
    };
    using NormalFunction = zs::function<void()>;
    struct OnceCoroHandle {
      OnceCoroHandle(std::coroutine_handle<> h) noexcept : h{h} {}
      std::coroutine_handle<> h;
      operator bool() const { return static_cast<bool>(h); }
    };
    struct StopTask {
      int tag{0};
      operator bool() const { return true; }
    };

    using TaskHandle = std::variant<NormalFunction, PersistentCoroHandle, OnceCoroHandle,
                                    CoroTaskNode*, StopTask>;

    ///
    struct Worker;

    /// prepare task batch
    template <typename Future> void emplace(std::coroutine_handle<Future> task) {
      _tasks.enqueue(PersistentCoroHandle(task));
    }
    void emplace(NormalFunction task) { _tasks.enqueue(zs::move(task)); }

    ///
    void start() {  // scheduled to global work queue
      // ref: moodycamel::ConcurrentQueue samples
      TaskHandle task;
      while (_tasks.try_dequeue(task)) enqueue_(zs::move(task));
    }

    /// @note global scope
    size_t numJobInQueue() const { return _remainingJobs.load(); }
    size_t numTaskInQueue() const { return _remainingTasks.load(); }
    size_t incNumJob() { return _remainingJobs.fetch_add(1); }
    size_t incNumTask() { return _remainingTasks.fetch_add(1); }
    size_t decNumJob() { return _remainingJobs.fetch_sub(1); }
    size_t decNumTask() { return _remainingTasks.fetch_sub(1); }
    void incCountByTask(const TaskHandle& task) {
      switch (task.index()) {
          // NormalFunction, PersistentCoroHandle, OnceCoroHandle, CoroTaskNode*
        case 0:
        case 2:
        case 3:
          incNumJob();
          return;
        case 1:
          incNumTask();
          return;
        case 4:
        default:;
      }
    }
    bool idle() { return numJobInQueue() == 0; }

    // notify (only take effect upon thread sleep)
    inline void notifyAll();
    inline void notifyOne(i32 workerId = -1);
    // signal
    inline void signalAll();
    inline void signalOne(i32 workerId = -1);

    void tick(i32 workerId = -1) { notifyOne(workerId); }
    void wait(i32 workerId = -1) {
      int cnt = 0;
      while (!idle()) {
#if 0
         printf("about to tick at iteration %d, num works: %d, %d\n", cnt++, (int)numJobInQueue(),
                (int)numTaskInQueue());
                if (cnt >= 5) getchar();
#endif
        tick(workerId);
      }
    }

    inline void request_stop(i32 workerId = -1);
    void terminate() { request_stop(); }

    Scheduler(size_t numThreads = std::thread::hardware_concurrency());
    ~Scheduler();

    /// enqueue

    void enqueue(NormalFunction task, i32 workerId = -1) {
      enqueue_(TaskHandle{zs::move(task)}, workerId);
    }

#if 0
    void enqueue(std::coroutine_handle<> coro, true_type once = {}, i32 workerId = -1) {
      // printf("  ->  schedling a to-be [OnceCoroHandle] to worker %d\n", workerId);
      enqueue(OnceCoroHandle{coro}, workerId);
    }
    void enqueue(std::coroutine_handle<> coro, false_type persistent, i32 workerId = -1) {
      // printf("  ->  schedling a to-be [PersistentCoroHandle] to worker %d\n", workerId);
      enqueue(PersistentCoroHandle{coro}, workerId);
    }
#endif

    void enqueue(OnceCoroHandle coro, i32 workerId = -1) {
      // printf("  ->  schedling a [OnceCoroHandle] to worker %d\n", workerId);
      enqueue_(TaskHandle{coro}, workerId);
    }
#if 0
    void enqueue(PersistentCoroHandle coro, i32 workerId = -1) {
      // printf("  ->  schedling a [PersistentCoroHandle] to worker %d\n", workerId);
      enqueue_(TaskHandle{coro}, workerId);
    }
#endif

    void enqueue(CoroTaskNode* node, i32 workerId = -1) {
      // printf("  ->  schedling a [CoroTaskNode *] to worker %d\n", workerId);
      enqueue_(TaskHandle{node}, workerId);
    }

    auto schedule(i32 workerId = -1) {
      struct awaiter : std::suspend_always {
        Scheduler& scheduler;
        i32 idx;
        awaiter(Scheduler& scheduler, int workerId) : scheduler{scheduler}, idx{workerId} {}
        void await_suspend(std::coroutine_handle<> h) {
          scheduler.enqueue_(OnceCoroHandle{h}, idx);
        }
      };
      return awaiter{*this, workerId};
    }
    auto persistent_schedule(i32 workerId = -1) {
      struct awaiter : std::suspend_always {
        Scheduler& scheduler;
        i32 idx;
        awaiter(Scheduler& scheduler, int workerId) : scheduler{scheduler}, idx{workerId} {}
        void await_suspend(std::coroutine_handle<> h) {
          scheduler.enqueue_(PersistentCoroHandle{h}, idx);
        }
      };
      return awaiter{*this, workerId};
    }
    bool schedule(CoroTaskNode* node) {
      /// @note means visited
      if (node->_state == CoroTaskNode::planned) return false;
      node->_state = CoroTaskNode::planned;
      if (node->_numDeps.load() != 0) {
        for (auto pred : node->_preds) {
          if (!schedule(pred)) return false;
        }
      } else {
        enqueue(node);
      }
      return true;
    }

    inline size_t numWorkers() const noexcept;
    /// @note sometimes we want to directly push work to a specific worker thread
    inline Worker& worker(i32 workerId);

    const auto& getWorkerIdMapping() const noexcept { return _workerIds; }

  private:
    /// process
    /// @note task is guaranteed to be valid
    inline void process(Worker& worker, NormalFunction& task);
    inline void process(Worker& worker, PersistentCoroHandle& task);
    inline void process(Worker& worker, OnceCoroHandle& task);
    inline void process(Worker& worker, CoroTaskNode* task);
    inline void process(Worker& worker, StopTask task);

    inline void enqueue_(TaskHandle&& task, i32 workerId = -1);

    inline i32 nextWorkerId() noexcept;

    // store all emplaced tasks
    moodycamel::ConcurrentQueue<TaskHandle> _tasks;
    // store tasks ready to resume()
    moodycamel::ConcurrentQueue<TaskHandle> _pendingTasks;
    std::map<std::thread::id, i32> _workerIds;
    std::vector<Worker> _workers;

    Mutex _mutex;
    std::atomic<size_t> _remainingJobs{0};  // remaining jobs that could be finished in finite times
    std::atomic<size_t> _remainingTasks{0};  // persistent tasks
    std::vector<stop_source> _stopSources;
  };

  struct Scheduler::Worker {
    Worker(Scheduler& scheduler, const std::string& tag = "__unnamed") noexcept
        : _scheduler{scheduler}, _tag{tag}, _idx{scheduler.nextWorkerId()} {}

    Worker(Scheduler& scheduler, jthread&& th, const std::string& tag = "__unnamed") noexcept
        : Worker(scheduler, tag) {
      _jthread = FWD(th);
    }

    Worker& operator=(jthread&& th) {
      _jthread = FWD(th);
      return *this;
    }

    Worker(Worker&& o) noexcept
        : _scheduler{o._scheduler},
          _jthread{zs::move(o._jthread)},
          _pendingTasks{zs::move(o._pendingTasks)},
          _tag{zs::exchange(o._tag, "__unnamed")},
          _idx{zs::exchange(o._idx, -1)} {}

    std::string_view getTag() const noexcept { return _tag; }

    jthread _jthread;
    moodycamel::ConcurrentQueue<TaskHandle> _pendingTasks;
    std::string _tag;
    std::atomic<u32> _status{2};  // 0 sleep, 1 busy, 2 signaled
    Scheduler& _scheduler;
    int _idx;
  };

  // notify (only take effect upon thread sleep)
  void Scheduler::notifyAll() {
    for (auto& worker : _workers) {
      u32 tmp = 0;
      if (worker._status.compare_exchange_weak(tmp, 2)) worker._status.notify_one();
    }
  }
  void Scheduler::notifyOne(i32 workerId) {
    if (workerId != -1) {
      assert(workerId >= 0 && workerId < _workers.size());
      auto& worker = this->worker(workerId);
      u32 tmp = 0;
      if (worker._status.compare_exchange_weak(tmp, 2)) worker._status.notify_one();
    } else
      notifyAll();
  }
  // signal
  void Scheduler::signalAll() {
    for (auto& worker : _workers) {
      u32 tmp = 0;
      worker._status.store(2);
      worker._status.notify_one();
    }
  }
  void Scheduler::signalOne(i32 workerId) {
    if (workerId != -1) {
      assert(workerId >= 0 && workerId < _workers.size());
      this->worker(workerId)._status.store(2);
      this->worker(workerId)._status.notify_one();
    } else
      signalAll();
  }

  void Scheduler::request_stop(i32 workerId) {
    if (workerId != -1) {
      assert(workerId >= 0 && workerId < _workers.size());
      _stopSources[workerId].request_stop();
      // enqueue_(StopTask{}, workerId);
    } else {
      for (int i = 0; i < _workers.size(); ++i) {
        _stopSources[i].request_stop();
        // enqueue_(StopTask{}, i);
      }
    }
#if 0
      puts("before request stop.");
      for (int i = 0; i < _workers.size(); ++i)
        printf("thread [%d] remaining work: %d, %d, status: %d\n", i, (int)numJobInQueue(),
               (int)numTaskInQueue(), _workers[i]._status.load());
#endif
    signalOne(workerId);
#if 0
      puts("done request stop.");
      for (int i = 0; i < _workers.size(); ++i)
        printf("thread [%d] remaining work: %d, %d, status: %d\n", i, (int)numJobInQueue(),
               (int)numTaskInQueue(), _workers[i]._status.load());
#endif
  }

  size_t Scheduler::numWorkers() const noexcept { return _workers.size(); }
  /// @note sometimes we want to directly push work to a specific worker thread
  Scheduler::Worker& Scheduler::worker(i32 workerId) {
    assert(workerId >= 0 && workerId < _workers.size());
    return _workers[workerId];
  }

  /// @note task is guaranteed to be valid
  void Scheduler::process(Worker& worker, NormalFunction& task) {
    task();
    decNumJob();
    // request_stop(); if required
  }
  void Scheduler::process(Worker& worker, PersistentCoroHandle& task) {
    task.h.resume();
    decNumTask();
    if (!task.h.done()) {
      // puts("work rethrown!\n ");
      enqueue_(task, worker._idx);
    }
    // request_stop(); if required
  }
  void Scheduler::process(Worker& worker, OnceCoroHandle& task) {
    task.h.resume();
    decNumJob();
  }
  void Scheduler::process(Worker& worker, CoroTaskNode* task) {
    auto h = task->_task.getHandle();
    h.resume();
    decNumJob();
    if (!h.done()) {
      // puts("work rethrown!\n ");
      enqueue_(task, worker._idx);  // scheduled to the local queue for coherence
    } else {
      for (auto succp : task->_succs) {
        if (succp->_numDeps.fetch_sub(1, std::memory_order_acq_rel) == 1) {
          // printf("enqueueing node of tag: %s\n", succp->_tag.c_str());
          enqueue_(succp);  // scheduled to global queue
        }
      }
    }
  }
  void Scheduler::process(Worker& worker, StopTask task) {}

  void Scheduler::enqueue_(TaskHandle&& task, i32 workerId) {
    incCountByTask(task);
    // printf("--->  schedling a task of type %d to worker %d\n", (int)task.index(), workerId);
    if (workerId == -1) {
      _pendingTasks.enqueue(zs::move(task));
    } else {
      assert(workerId >= 0 && workerId < _workers.size() && "worker index out of bound");
      worker(workerId)._pendingTasks.enqueue(zs::move(task));
    }
    signalOne(workerId);
  }
  i32 Scheduler::nextWorkerId() noexcept { return _workers.size(); }

}  // namespace zs
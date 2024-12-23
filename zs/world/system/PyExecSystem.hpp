#pragma once
#include <atomic>
#include "world/WorldExport.hpp"
#include "ZsExecSystem.hpp"
#include "zensim/execution/Concurrency.h"

namespace zs {

  struct ZS_WORLD_EXPORT PyExecSystem {
  private:
    PyExecSystem() = default;

  public:
    static PyExecSystem &instance();
    static void initialize();

    ~PyExecSystem() = default;
    static void request_termination() { instance()._terminationRequested.store(true); }
    static bool termination_requested() { return instance()._terminationRequested.load(); }

    void reset();
    bool inProgress() const noexcept { return static_cast<bool>(_task.getHandle()); }
    ///  @note _launched may turn true from false
    template <typename F, enable_if_t<is_rvalue_reference_v<F &&>> = 0> bool assignTask(F &&task) {
      if (inProgress()) return false;
      /// @ref
      /// https://stackoverflow.com/questions/78216015/issue-with-gil-on-python-3-12-2
      /// @ref blender/source/blender/python/generic/bpy_thread.cc
      backupPyThreadState();

      _task = [](auto task) -> zs::Future<void> {
        /// @note execute on the dedicated thread, not blocking of the main loop
        task();
        // co_await ZS_EVENT_SCHEDULER().schedule();
        co_return;
      }(zs::move(task));

#if 1
      /// @note make sure only launches pyscript execution within valid window
      ZS_DEDICATED_SCHEDULER().enqueue(Scheduler::OnceCoroHandle{_task.getHandle()});
      // ZS_EVENT_SCHEDULER().emplace(_task.getHandle());
#else
      _cmdExecution = reallyAsync(FWD(task));
      _launched = true;
#endif
      return true;
    }

    bool tryFinish() {
      using namespace std::chrono_literals;
#if 1
      if (_task.isReady()) {
        restorePyThreadState();
        _task = {};
        return true;
      }
#else
      if (!inProgress()) return false;
      auto status = _cmdExecution.wait_for(0ms);
      if (status == std::future_status::ready) {
        _cmdExecution.wait();
        _cmdExecution.get();
        _launched = false;

        restorePyThreadState();
        return true;
      }
#endif
      return false;
    }

    void backupPyThreadState();
    void restorePyThreadState();

    std::atomic<bool> _terminationRequested{false};
    // std::future<void> _cmdExecution{};
    // bool _launched{false};
    void *_pyThreadState{nullptr};

    zs::Future<void> _task;
  };

}  // namespace zs
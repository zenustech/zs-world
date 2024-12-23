#include "Executor.hpp"

#include "zensim/types/Polymorphism.h"

namespace zs {

  Scheduler::Scheduler(size_t numThreads) {
    {
      std::unique_lock lk(_mutex);  // block thread executions until all workers ready
      _workers.reserve(numThreads);
      for (size_t i = 0; i < numThreads; ++i) {
        auto& stopSource = _stopSources.emplace_back();
        _workers.emplace_back(
            *this, jthread(
                       [this, idx = i](stop_token token) {
                         {
                           std::unique_lock lk(_mutex);  // global lock held at the beginning
                           _workerIds[std::this_thread::get_id()] = idx;
                         }
                         auto& worker = _workers[idx];

                         // worker._status.wait(0);
                         do {
                           if (u32 prev = worker._status.exchange(1); prev == 2) {
                             if (token.stop_requested()) break;
                           }
                           TaskHandle task;
                           bool stopRequested = false;
                           // printf("thread %d begin processing local queue (%u).\n", idx,
                           // worker._status.load());
                           while (worker._pendingTasks.try_dequeue(task)) {
                             match(
                                 [this, idx, &worker](auto& task) {
                                   if (task) {
#if 0
                                     printf(
                                         "thread %d processing local queue (status %u), (%d, %d) "
                                         "work left\n",
                                         (int)idx, worker._status.load(), (int)numJobInQueue(),
                                         (int)numTaskInQueue());
#endif
                                     process(worker, task);
                                   }
                                 },
                                 [&stopRequested](StopTask) { stopRequested = true; })(task);
                           }
                           if (stopRequested) break;
                           // printf("thread %d begin processing global queue (%u).\n", idx,
                           // worker._status.load());
                           while (_pendingTasks.try_dequeue(task)) {
                             match([this, idx, &worker](auto& task) {
                               if (task) {
#if 0
                                 printf(
                                     "==\tthread %d processing global queue (status %u), (%d, %d) "
                                     "work left\n",
                                     (int)idx, worker._status.load(), (int)numJobInQueue(),
                                     (int)numTaskInQueue());
#endif
                                 process(worker, task);
                               }
                             })(task);
                           }
#if 0
                           printf(
                               "thread %d done processing works (%u). (stop token: %d) num work "
                               "left (%d, %d)\n",
                               (int)idx, worker._status.load(), (int)token.stop_requested(),
                               (int)numJobInQueue(), (int)numTaskInQueue());
#endif

                           u32 prev = worker._status.exchange(0);
                           if (prev == 2) {
                             if (token.stop_requested()) break;
                           } else if (prev == 1) {
                             worker._status.wait(0);
                           }
#if 0
                           else
                             printf("damn current status : %d, prev: %d\n", worker._status.load(), prev);
#endif
                         } while (!token.stop_requested());

                         worker._status.store(-1);
#if 0
                         printf("(stop token: %d) thread %d exiting (%d).\n",
                                (int)token.stop_requested(), idx, worker._status.load());
#endif
                       },
                       stopSource.get_token()));
      }
    }
  }
  Scheduler::~Scheduler() {
    wait();
    // puts("DONE WAIT");
    terminate();
    // puts("DONE TERMINATE");
    _workers.clear();
    // puts("DONE CLEAR JTHREADS");
  }

}  // namespace zs
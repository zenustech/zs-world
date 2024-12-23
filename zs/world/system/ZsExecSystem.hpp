#pragma once
#include "vulkan/vulkan.hpp"
//
#include "world/WorldExport.hpp"
#include "world/async/Awaitables.hpp"
#include "world/async/Executor.hpp"
#include "zensim/ZpcResource.hpp"
#include "zensim/execution/Concurrency.h"

#ifdef linux
#  undef linux
/// @ref
/// https://stackoverflow.com/questions/19210935/why-does-the-c-preprocessor-interpret-the-word-linux-as-the-constant-1
#endif
// #include "cppcoro/io_service.hpp"

namespace zs {

  struct ZS_WORLD_EXPORT ZsExecSystem {
    static ZsExecSystem &instance();
    static void initialize();

    ZsExecSystem();
    ~ZsExecSystem() = default;
    void reset();

    static Scheduler &ref_task_scheduler() { return *instance()._scheduler; }
    static Scheduler &ref_event_scheduler() { return *instance()._eventLoop; }
    static Scheduler &ref_dedicated_scheduler() { return *instance()._dedicatedWorker; }

    /// compute
    static auto schedule_as_task() { return ref_task_scheduler().schedule(); }
    static void sync_process_tasks() { ref_task_scheduler().wait(); }
    static void issue_tasks() { ref_task_scheduler().start(); }

    /// io
    // static auto io_schedule() { return instance()._ioService.schedule(); }

    /// event
    static auto schedule_as_event() { return ref_event_scheduler().schedule(); }
    static void sync_process_events() { ref_event_scheduler().wait(); }
    static void issue_events() { ref_event_scheduler().start(); }

    /// dedicated worker
    static auto schedule_as_dedicated() { return ref_dedicated_scheduler().schedule(); }
    static void sync_process_dedicated() { ref_dedicated_scheduler().wait(); }
    static void issue_dedicated() { ref_dedicated_scheduler().start(); }

    static void tick() {
      ref_task_scheduler().tick();
      ref_dedicated_scheduler().tick();
      ref_event_scheduler().tick();
    }
    /// @note tasks may only be rescheduled to events or itself
    /// @note dedicated may only be rescheduled to events or itself
    /// @note flush events in the end
    static void flush() {
      sync_process_tasks();
      sync_process_dedicated();
      sync_process_events();
    }

    auto &refVkCmdTaskQueue() noexcept { return _visBufferTaskQueue; }

  private:
    moodycamel::ConcurrentQueue<zs::function<void(vk::CommandBuffer)>> _visBufferTaskQueue;

    // AsyncFlag _save, _cache;
    UniquePtr<Scheduler> _scheduler;        // for general massively-parallel tasks
    UniquePtr<Scheduler> _eventLoop;        // waited at every event loop
    UniquePtr<Scheduler> _dedicatedWorker;  // for tasks where data is only accessible by at most
                                            // one thread simultaneously.
    // cppcoro::io_service _ioService;
    stop_source _ioStopSource;
  };

#define ZS_TASK_SCHEDULER() ::zs::ZsExecSystem::ref_task_scheduler()
#define ZS_EVENT_SCHEDULER() ::zs::ZsExecSystem::ref_event_scheduler()
#define ZS_DEDICATED_SCHEDULER() ::zs::ZsExecSystem::ref_dedicated_scheduler()

#define ZS_FLUSH_TASK_SCHEDULER() zs::ZsExecSystem::sync_process_tasks()
#define ZS_FLUSH_EVENT_SCHEDULER() zs::ZsExecSystem::sync_process_events()
#define ZS_FLUSH_DEDICATED_SCHEDULER() zs::ZsExecSystem::sync_process_dedicated()

  inline auto &zs_execution() { return ZsExecSystem::instance(); }

}  // namespace zs
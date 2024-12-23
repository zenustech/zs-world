#include "ZsExecSystem.hpp"

#include <thread>

namespace zs {

  ZsExecSystem &ZsExecSystem::instance() {
    static ZsExecSystem s_instance{};
    return s_instance;
  }
  void ZsExecSystem::initialize() { (void)instance(); }
  void ZsExecSystem::reset() {
    _scheduler = {};

    // _ioService.stop();
  }

  ZsExecSystem::ZsExecSystem() {
#if 0
    jthread thread = jthread([](const stop_source &stopSource) {
      while (!stopSource.stop_requested()) {
        ;
      }
    });
    _ioStopSource = thread.get_stop_source();
    thread.detach();
#endif
    _scheduler = UniquePtr<Scheduler>(new Scheduler(4));
    _eventLoop = UniquePtr<Scheduler>(new Scheduler(1));
    _dedicatedWorker = UniquePtr<Scheduler>(new Scheduler(1));
  }

}  // namespace zs
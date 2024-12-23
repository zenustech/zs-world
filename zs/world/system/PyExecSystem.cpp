#include "PyExecSystem.hpp"

#include "interface/details/Py.hpp"

namespace zs {

  PyExecSystem &PyExecSystem::instance() {
    static PyExecSystem s_instance{};
    return s_instance;
  }
  void PyExecSystem::initialize() { (void)instance(); }
  void PyExecSystem::reset() {
    // while (!_task.isReady());
    ZS_DEDICATED_SCHEDULER().wait();
    assert(_task.isReady());
    _task = {};
  }
  void PyExecSystem::backupPyThreadState() {
    if (_PyThreadState_UncheckedGet() && PyGILState_Check()) {
      _pyThreadState = PyEval_SaveThread();
    } else
      _pyThreadState = nullptr;
  }
  void PyExecSystem::restorePyThreadState() {
    if (_pyThreadState) {
      PyEval_RestoreThread(static_cast<PyThreadState *>(_pyThreadState));
      _pyThreadState = nullptr;
    }
  }

}  // namespace zs
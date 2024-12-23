#include "Command.hpp"

namespace zs {

  CommandManager::CommandManager() noexcept : _undoSize{0}, _redoSize{0}, _cmds{} {
    _it = _cmds.end();
  }
  void CommandManager::executeCommand(UniquePtr<CommandConcept> c) {
    c->execute();

    auto it = ++_cmds.insert(_it, zs::move(c));
    _undoSize++;
    for (; it != _cmds.end();) {
      it = _cmds.erase(it);
      _undoSize--;
    }
    _it = _cmds.end();
    _redoSize = 0;
  }
  void CommandManager::undo() {
    if (_undoSize) {
      _it = std::prev(_it);
      (*_it)->undo();
      _undoSize--;
    }
  }
  void CommandManager::redo() {
    if (_redoSize) {
      (*_it)->execute();
      _it = std::next(_it);
      _redoSize--;
    }
  }

}  // namespace zs
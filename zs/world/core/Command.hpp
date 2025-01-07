#pragma once
#include <list>

#include "interface/world/CommandInterface.hpp"
#include "world/WorldExport.hpp"
#include "zensim/ZpcResource.hpp"

namespace zs {

  struct ZS_WORLD_EXPORT CommandManager {
    using Container = std::list<CommandConcept*>;

    CommandManager() noexcept;
    ~CommandManager();

    size_t getUndoSize() const noexcept { return _undoSize; }
    size_t getRedoSize() const noexcept { return _redoSize; }
    /// @ref steals this reference
    void executeCommand(CommandConcept* c);
    void undo();
    void redo();

  private:
    size_t _undoSize{0}, _redoSize{0};
    Container _cmds;
    typename Container::iterator _it;
  };

}  // namespace zs
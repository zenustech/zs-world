#pragma once
#include <list>
#include "interface/InterfaceExport.hpp"
#include "zensim/ZpcResource.hpp"

namespace zs {

  struct ZS_INTERFACE_EXPORT CommandConcept {
    virtual ~CommandConcept() = default;

    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual CommandConcept *clone() const = 0;
    virtual void deinit() { ::delete this; }
  };

  struct ZS_INTERFACE_EXPORT CommandManager {
    using Container = std::list<UniquePtr<CommandConcept>>;

    CommandManager() noexcept;
    ~CommandManager() = default;

    size_t getUndoSize() const noexcept { return _undoSize; }
    size_t getRedoSize() const noexcept { return _redoSize; }
    void executeCommand(UniquePtr<CommandConcept> c);
    void undo();
    void redo();

  private:
    size_t _undoSize{0}, _redoSize{0};
    Container _cmds;
    typename Container::iterator _it;
  };

}  // namespace zs
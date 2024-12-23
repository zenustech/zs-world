#pragma once
#include <map>
#include <string>

#include "../World.hpp"
#include "interface/details/PyHelper.hpp"
#include "interface/world/NodeInterface.hpp"

namespace zs {

  template <typename CustomNode> struct ImplNode : NodeConcept {
    ResultType setInput(const char *tag, ZsValue obj) override {
      _inputs[std::string(tag)] = obj;
      return 0;
    }
    ZsValue getOutput(const char *tag) override {
      if (auto it = _outputs.find(std::string(tag)); it != _outputs.end()) return (*it).second;
      return {};
    }

    ResultType apply() override { return static_cast<CustomNode *>(this)->apply(); }

    ~ImplNode() {
      _inputs.clear();
      {
        GILGuard guard;
        _outputs.clear();
      }
    }

    std::map<std::string, ZsValue> _inputs;
    std::map<std::string, ZsVar> _outputs;
  };

#define ZS_DEFNODE(CLASS, ...)                                                                 \
  static struct _zs_setup_node_##CLASS {                                                       \
    static constexpr Descriptor s_descriptor __VA_ARGS__;                                      \
    _zs_setup_node_##CLASS() {                                                                 \
      zs_register_node_factory(                                                                \
          zs_get_world(), #CLASS, [](ContextConcept *) -> NodeConcept * { return new CLASS; }, \
          s_descriptor.getView());                                                             \
    }                                                                                          \
  } g_zs_setup_node_##CLASS

#define ZENO_DEFNODE(Class)                                                                     \
  static struct _Def##Class {                                                                   \
    _Def##Class(::zeno::Descriptor const &desc) {                                               \
      ::zeno::getSession().defNodeClass(                                                        \
          []() -> std::unique_ptr<::zeno::INode> { return std::make_unique<Class>(); }, #Class, \
          desc);                                                                                \
    }                                                                                           \
  } _def##Class

}  // namespace zs
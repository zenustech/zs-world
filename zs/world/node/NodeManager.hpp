#pragma once
#include <map>
#include <set>
#include <string>

#include "interface/world/NodeInterface.hpp"
#include "zensim/ZpcFunction.hpp"
#include "zensim/ZpcResource.hpp"
#include "zensim/execution/ConcurrencyPrimitive.hpp"

namespace zs {

  struct NodeManager : NodeManagerConcept {
    bool registerNodeFactory(const char *label, funcsig_create_node *nodeFactory) override;
    bool registerNodeFactory(const char *label, funcsig_create_node *nodeFactory,
                             NodeDescriptorPort descr) override;
    bool registerUiDescriptor(const char *label, funcsig_get_node_ui_desc *uiDesc) override;
    void displayNodeFactories() override;

    funcsig_create_node *retrieveNodeFactory(const char *label) override;

    bool loadPluginsAt(const char *path) override;

    ~NodeManager() {
      {
        GILGuard guard;
        _nodeUiDescs.clear();
      }
    }

  private:
    struct NodeDeleter {
      void operator()(NodeConcept *node) { node->deinit(); }
    };
    using NodeHolder = UniquePtr<NodeConcept, NodeDeleter>;

    zs::Mutex _mutex;
    std::map<std::string, function<funcsig_register_plugins>> _plugins;
    std::string _currentPluginFile = "<inherent module>";

    std::map<std::string, std::string> _nodePlugins;
    std::map<std::string, funcsig_create_node *> _nodeFactories;
    // std::map<std::string, NodeDescriptorPort> _nodeDescriptors;
    // std::map<std::string, funcsig_get_node_ui_desc *> _nodeUiDescriptors;
    std::map<std::string, ZsVar> _nodeUiDescs;
  };

}  // namespace zs
#include "NodeManager.hpp"

#include "boost/dll.hpp"
#include "boost/dll/alias.hpp"
#include "boost/dll/import.hpp"
#include "boost/dll/shared_library.hpp"
#include "boost/filesystem/directory.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "interface/details/PyHelper.hpp"
#include "zensim/ZpcImplPattern.hpp"
#include "zensim/io/Filesystem.hpp"
#include "zensim/zpc_tpls/fmt/color.h"

namespace zs {

  NodeManager::~NodeManager() {
    {
      GILGuard guard;
      _nodeUiDescs.clear();
    }
  }

  template <typename T>
  auto dll_import_symbol(const boost::dll::shared_library &lib, const char *name) {
    typedef typename boost::dll::detail::import_type<T>::base_type type;

    boost::shared_ptr<boost::dll::shared_library> p
        = boost::make_shared<boost::dll::shared_library>(lib);
    return type(p, boost::addressof(p->get<T>(name)));
  }
  template <typename T> auto dll_import_symbol(boost::dll::shared_library &&lib, const char *name) {
    typedef typename boost::dll::detail::import_type<T>::base_type type;

    boost::shared_ptr<boost::dll::shared_library> p
        = boost::make_shared<boost::dll::shared_library>(boost::move(lib));
    return type(p, boost::addressof(p->get<T>(name)));
  }
  template <class T>
  auto dll_import_symbol(const boost::dll::fs::path &lib, const char *name,
                         boost::dll::load_mode::type mode = boost::dll::load_mode::default_mode) {
    typedef typename boost::dll::detail::import_type<T>::base_type type;

    boost::shared_ptr<boost::dll::shared_library> p
        = boost::make_shared<boost::dll::shared_library>(lib, mode);
    return type(p, boost::addressof(p->get<T>(name)));
  }

  /// load plugins
  bool NodeManager::loadPluginsAt(const char *path) {
    namespace dll = boost::dll;

    if (path == nullptr) return false;

    dll::fs::path p(path == nullptr ? abs_module_directory() : std::string(path));

    auto loadPlugin = [this](const dll::fs::path &path) {
      _currentPluginFile = path.string();
      try {
        dll::shared_library lib(path);
        // func holds a shared ptr of lib
        zs::function<funcsig_register_plugins> func
            = dll_import_symbol<funcsig_register_plugins>(lib, "register_node_factories");
        int n, nTotal;
        func(this, &n, &nTotal);
        assert(n == nTotal && "not all node factories have been successfully registered!");
        _plugins[_currentPluginFile] = func;
        // const char *name = func(this);
        // if (name != nullptr)
        //   _plugins[std::string(name)] = func;
      } catch (const boost::system::system_error &err) {
        fmt::print("Cannot load plugin at {}\n", path.filename().string());
        fmt::print("what: {}, error msg: {}\n", err.what(), err.code().message());
      }
      _currentPluginFile = "<inherent module>";
    };

    if (dll::fs::is_directory(p)) {
      for (auto &&f : boost::filesystem::directory_iterator(p)) {
        // fmt::print("checking file [{}] in dir [{}]\n",
        //            f.path().filename().c_str(), p.filename().c_str());
#ifdef ZS_PLATFORM_WINDOWS
        if (f.path().extension() == ".dll")
#elif defined(ZS_PLATFORM_LINUX)
        if (f.path().extension() == ".so")
#elif defined(ZS_PLATFORM_OSX)
        if (f.path().extension() == ".dylib")
#else
        static_assert(false, "un-supported platform");
#endif
          loadPlugin(f.path());
      }
    } else if (dll::fs::is_regular_file(p)) {
      // fmt::print("checking file [{}]\n", p.filename().c_str());
      loadPlugin(p);
    }
    return true;
  }

  /// interfaces
  bool NodeManager::registerNodeFactory(const char *name, funcsig_create_node *nodeFactory) {
    _mutex.lock();
    const std::string tag(name);
    _nodeFactories[tag] = nodeFactory;
    _nodePlugins[tag] = _currentPluginFile;
    _mutex.unlock();
    return true;
  }

  bool NodeManager::registerUiDescriptor(const char *label, funcsig_get_node_ui_desc *uiDesc) {
    _mutex.lock();
    const std::string tag(label);
    // _nodeUiDescriptors[tag] = uiDesc;
    _nodePlugins[tag] = _currentPluginFile;
    _nodeUiDescs[tag] = uiDesc();
    _mutex.unlock();
    return true;
  }

  bool NodeManager::registerNodeFactory(const char *label, funcsig_create_node *nodeFactory,
                                        NodeDescriptorPort descr) {
    _mutex.lock();
    const std::string tag(label);
    _nodeFactories[tag] = nodeFactory;
    _nodePlugins[tag] = _currentPluginFile;
    // _nodeDescriptors[tag] = descr;
    _nodeUiDescs[tag] = zs_build_node_ui_desc(descr);
    _mutex.unlock();
    return true;
  }

  void NodeManager::displayNodeFactories() {
    fmt::print("there exist {} plugins with {} node factories right now.\n", _plugins.size(),
               _nodeFactories.size());
    fmt::print("\n\tplugin registry funcs:\n");
    fmt::print("========================================\n");
    for (auto &[name, ptr] : _plugins) fmt::print("\t\t[{}]: {}\n", name, (void *)&ptr);
    fmt::print("\n\tnode factories funcs:\n");
    fmt::print("========================================\n");
    for (auto &[name, ptr] : _nodeFactories)
      fmt::print("\t\t[{}]: {} (from plugin: {})\n", name, (void *)&ptr, _nodePlugins.at(name));

    /*
      fmt::print("\n\tnode factories descriptors (static method style):\n");
      fmt::print("========================================\n");
      for (auto &[name, func] : _nodeUiDescriptors) {
        if (ZsVar desc = func()) {
          fmt::print("\t\t[{}] (from {}): \n", name, _nodePlugins.at(name));
          desc.reflect();
          fmt::print("\t\t----------------------------------------\n");
        }
      }
      fmt::print("\n\tnode factories descriptors (conventional macro-style):\n");
      fmt::print("========================================\n");
      for (auto &[name, descrPort] : _nodeDescriptors) {
        fmt::print(
            "\t\t[{}] category [{}]: \t{} inputs, {} outputs, {} attribs, \n", name,
            descrPort._categoryDescriptor.category, descrPort._numInputs,
            descrPort._numOutputs, descrPort._numAttribs);
        for (u32 i = 0; i < descrPort._numInputs; ++i) {
          const SocketDescriptor &input = descrPort._inputDescriptors[i];
          fmt::print("[{}] input: \t[{}]\t[{}]\t[{}]\t[{}]\n", i, input.type,
                     input.name, input.defl, input.doc);
        }
        for (u32 i = 0; i < descrPort._numOutputs; ++i) {
          const SocketDescriptor &output = descrPort._outputDescriptors[i];
          fmt::print("[{}] output: \t[{}]\t[{}]\t[{}]\t[{}]\n", i, output.type,
                     output.name, output.defl, output.doc);
        }
        for (u32 i = 0; i < descrPort._numAttribs; ++i) {
          const AttribDescriptor &attrib = descrPort._attribDescriptors[i];
          fmt::print("[{}] attrib: \t[{}]\t[{}]\t[{}]\t[{}]\n", i, attrib.type,
                     attrib.name, attrib.defl, attrib.doc);
        }
      }
    */
    fmt::print("\n\tnode descriptors:\n");
    fmt::print("========================================\n");
    for (auto &[name, uiDesc] : _nodeUiDescs) {
      if (!uiDesc) continue;
      fmt::print("\t\t[{}]: \n", name);
      uiDesc.reflect();
      fmt::print("\t\t----------------------------------------\n");
    }
  }

  funcsig_create_node *NodeManager::retrieveNodeFactory(const char *name) {
    if (auto tag = std::string(name); _nodeFactories.find(tag) != _nodeFactories.end())
      return _nodeFactories.at(tag);
    return nullptr;
  }

}  // namespace zs
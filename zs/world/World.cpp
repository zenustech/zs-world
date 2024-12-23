#include "World.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include "interface/details/PyHelper.hpp"
#include "node/Context.hpp"
#include "node/NodeManager.hpp"
#include "zensim/io/Filesystem.hpp"
#include "zensim/zpc_tpls/fmt/format.h"
// #include "zensim/zpc_tpls/fmt/format.h"
#include <Python.h>

#if ZS_ENABLE_JIT
#  include "py_zpc_jit/PyZpc.hpp"
#endif

#include <iostream>

namespace zs {

  struct World {
    UserContext _context;

#ifdef ZS_ENABLE_USD
    std::unique_ptr<SceneManagerConcept> _sceneManager = std::make_unique<USDSceneManager>();
#else
    std::unique_ptr<SceneManagerConcept> _sceneManager = std::make_unique<SceneManagerConcept>();
#endif

    // _contents;
    NodeManager _pluginManager;
  };

}  // namespace zs

extern "C" {

zs::World *zs_get_world() {
  static zs::World s_instance{};
  return &s_instance;
}

zs::NodeManagerConcept *zs_get_plugin_manager(zs::World *world) { return &world->_pluginManager; }

zs::SceneManagerConcept *zs_get_scene_manager(zs::World *world) {
  return world->_sceneManager.get();
}

bool zs_register_node_factory_lite(zs::World *world, const char *label,
                                   zs::funcsig_create_node *nodeFactory) {
  world->_pluginManager.registerNodeFactory(label, nodeFactory);
  return true;
}

bool zs_register_node_factory(zs::World *world, const char *label,
                              zs::funcsig_create_node *nodeFactory, zs::NodeDescriptorPort descr) {
  world->_pluginManager.registerNodeFactory(label, nodeFactory, descr);
  return true;
}

bool zs_load_plugins(const char *path) {
  auto &plugin = zs_get_world()->_pluginManager;
  return plugin.loadPluginsAt(path);
}
void zs_display_nodes() {
  auto &plugin = zs_get_world()->_pluginManager;
  plugin.displayNodeFactories();
}
bool zs_execute_node(const char *label) {
  auto &plugin = zs_get_world()->_pluginManager;
  auto nodeFactory = plugin.retrieveNodeFactory(label);
  if (nodeFactory) {
    auto node = nodeFactory(/*ctx*/ nullptr);
    node->apply();
    node->deinit();
    return true;
  }
  return false;
}

static PyObject *_hello_world_var(PyObject * /*self*/, PyObject *value) {
  printf("Hello World var.\n");
  PyErr_SetString(PyExc_RuntimeError, "testing METH_VARARGS");
  return nullptr;
}
static PyObject *_hello_world(PyObject * /*self*/) {
  printf("Hello World.\n");
  // PyErr_SetString(PyExc_RuntimeError, "testing METH_NOARGS");
  // return nullptr;
  Py_RETURN_NONE;
}
static PyObject *_hello_world_0(PyObject * /*self*/, PyObject *value_) {
  ZsValue value{value_};
  printf("checking value type: %d\n", value._idx);
  if (PyVar pybytes = zs_bytes_obj(value)) {
    g_zs_variable_apis.reflect(pybytes.getValue());
    if (auto cstr = pybytes.getValue().asBytes().c_str()) {
      printf("Hello World with bytes [%s].\n", cstr);
      Py_RETURN_NONE;
    }
  }
  printf("Hello World 0.\n");
  // PyErr_SetString(PyExc_RuntimeError, "testing METH_O");
  Py_RETURN_NONE;
}
static PyObject *_exec_node(PyObject *, PyObject *label_) {
  // PyObject *ctx = nullptr
  ZsValue label{label_};
  if (ZsBytes cstr = zs_bytes_obj(label)) {
    auto &plugin = zs_get_world()->_pluginManager;
    auto nodeFactory = plugin.retrieveNodeFactory(static_cast<const char *>(cstr));
    if (nodeFactory) {
      auto node = nodeFactory(nullptr);
      node->apply();
      node->deinit();
    }
    Py_DECREF(static_cast<void *>(cstr));
  } else
    PyErr_SetString(PyExc_RuntimeError, "testing METH_VARARGS");
  Py_RETURN_NONE;
}
static PyMethodDef zpy_ops_methods[] = {
    {"hello_var", (PyCFunction)_hello_world_var, METH_VARARGS, nullptr},
    {"hello", (PyCFunction)_hello_world, METH_NOARGS, nullptr},
    {"hello_0", (PyCFunction)_hello_world_0, METH_O, nullptr},
    {"execute_node", (PyCFunction)_exec_node, METH_O, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

static PyModuleDef zpy_ops_module = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "zpy.ops",
    /*m_doc*/ nullptr,
    /*m_size*/ -1, /* multiple "initialization" just copies the module dict. */
    /*m_methods*/ zpy_ops_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

///
/// zpy
///
static PyObject *_load_plugins(PyObject *, PyObject *path_) {
  ZsValue path{path_};
  if (PyVar pathStr = PyUnicode_AsUTF8String(as_ptr_<PyObject>(path))) {
    auto pathCstr = PyBytes_AsString(pathStr);
    if (pathCstr) {
      std::string msg = std::string("## \tloading plugins at [") + pathCstr + "]\n";
      zs_print_py_cstr(msg.c_str());
      zs_load_plugins(pathCstr);
    }
  }
  Py_RETURN_NONE;
}
static PyObject *_display_nodes(PyObject *) {
  zs_display_nodes();
  Py_RETURN_NONE;
}

namespace fs = std::filesystem;
/*
zpy.run_file("setup-env.py")
*/
static PyObject *_run_file(PyObject *, PyObject *path_) {
  ZsValue path{path_};
  if (PyVar pathStr = zs_bytes_obj(path)) {
    if (auto pathCstr = pathStr.asBytes().c_str()) {
      fs::path p = pathCstr;
      if (fs::exists(p)) {
        std::ifstream is(p);
        if (!is || p.empty()) Py_RETURN_NONE;

        std::string temp, script;
        while (!is.eof()) {
          std::getline(is, temp);
          script += temp + '\n';
        }
        is.close();

        int result = -1;
        PyVar bs = zs_execute_script(script.c_str(), &result);

        printf("file execution result: %s\n", result == 0 ? "success" : "error");
        if (bs) {
          if (result == 1 || result == 2)
            zs_print_err_py_cstr(bs.asBytes().c_str());
          else
            zs_print_py_cstr(bs.asBytes().c_str());
        }
      } else {
        p = fmt::format("{}/resource/{}", zs::abs_exe_directory(), pathCstr);

        std::ifstream is(p);
        if (!is || p.empty()) Py_RETURN_NONE;

        std::stringstream buffer;
        buffer << is.rdbuf();
        // std::string script((std::istreambuf_iterator<char>(is)),
        //                    std::istreambuf_iterator<char>());
        std::string script(buffer.str());
        is.close();

        int result = -1;
        PyVar bs = zs_execute_script(script.c_str(), &result);

        printf("file execution result: %s\n", result == 0 ? "success" : "error");
        if (bs) printf("%s\n", bs.asBytes().c_str());
      }
    }
  }
  Py_RETURN_NONE;
}
static PyMethodDef zpy_methods[] = {
    {"load_plugins", (PyCFunction)_load_plugins, METH_O, nullptr},
    {"display_nodes", (PyCFunction)_display_nodes, METH_NOARGS, nullptr},
    {"run_file", (PyCFunction)_run_file, METH_O, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

///
/// zpy.ui
///
static PyMethodDef zpy_ui_methods[] = {
    {nullptr, nullptr, 0, nullptr},
};
static PyModuleDef zpy_ui_module = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "zpy.ui",
    /*m_doc*/ nullptr,
    /*m_size*/ -1, /* multiple "initialization" just copies the module dict. */
    /*m_methods*/ zpy_ui_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

/*****************
 * zpy.scene
 ******************/
static PyObject *_create_scene(PyObject * /* self */, PyObject *sceneId) {
  PyVar sidVar = zs_bytes_obj(ZsBytes(sceneId));
  if (!sidVar) {
    PyErr_SetString(PyExc_RuntimeError, "[create_scene] failed to parse sceneId");
    Py_RETURN_NONE;
  }

  ZpySceneDesc *sceneObj = (ZpySceneDesc *)ZpySceneDesc_Type.tp_alloc(&ZpySceneDesc_Type, 0);
  if (!sceneObj) {
    PyErr_SetString(PyExc_RuntimeError,
                    "[create_scene] failed to allocate memory for a scene description object");
    Py_RETURN_NONE;
  }

  auto plugin = zs_get_world()->_sceneManager.get();
  auto scene = plugin->createScene(sidVar.asBytes().c_str());
  if (!scene) {
    Py_RETURN_NONE;
  }

  sceneObj->mOwnCounter = 1;
  sceneObj->mScene = scene;
  return (PyObject *)sceneObj;
}
static PyObject *_remove_scene(PyObject * /* self */, PyObject *sceneId) {
  PyVar sidVar = zs_bytes_obj(ZsBytes(sceneId));
  if (!sidVar) {
    PyErr_SetString(PyExc_RuntimeError, "[remove_scene] failed to parse sceneId");
    Py_RETURN_FALSE;
  }

  auto plugin = zs_get_world()->_sceneManager.get();
  if (plugin->removeScene(sidVar.asBytes().c_str())) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}
static PyObject *_clear_all_scenes(PyObject *) {
  auto plugin = zs_get_world()->_sceneManager.get();
  if (plugin->clearAllScenes()) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}
static PyObject *_get_scene(PyObject *, PyObject *sceneId) {
  PyVar sidVar = zs_bytes_obj(ZsBytes(sceneId));
  if (!sidVar) {
    PyErr_SetString(PyExc_RuntimeError, "[get_scene] failed to parse sceneId");
    Py_RETURN_NONE;
  }

  auto plugin = zs_get_world()->_sceneManager.get();
  if (auto *scene = plugin->getScene(sidVar.asBytes().c_str())) {
    ZpySceneDesc *sceneObj = (ZpySceneDesc *)ZpySceneDesc_Type.tp_alloc(&ZpySceneDesc_Type, 0);
    if (!sceneObj) {
      Py_RETURN_NONE;
    }

    sceneObj->mScene = scene;
    sceneObj->mOwnCounter = 1;
    return (PyObject *)sceneObj;
  }
  Py_RETURN_NONE;
}
static PyMethodDef zpy_scene_methods[] = {
    {"create", (PyCFunction)_create_scene, METH_O, nullptr},
    {"get", (PyCFunction)_get_scene, METH_O, nullptr},
    {"remove", (PyCFunction)_remove_scene, METH_O, nullptr},
    {"clear_all", (PyCFunction)_clear_all_scenes, METH_O, nullptr},
    {nullptr, nullptr, 0, nullptr},
};
static PyModuleDef zpy_scene_module = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "zpy.scene",
    /*m_doc*/ nullptr,
    /*m_size*/ -1, /* multiple "initialization" just copies the module dict. */
    /*m_methods*/ zpy_scene_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

void zs_setup_world() {
#if ZS_ENABLE_JIT
  /// @brief import zpc jit module (if exist)
  init_zpc_jit_environment();
#endif
  /// @brief setup zpy module
  if (PyVar mod = PyModule_New("zpy")) {
    // sys.modules acts as a cache of all imported modules. enable 'import zpy'
    PyDict_SetItemString(PyImport_GetModuleDict(), "zpy", mod);
    // directly accesssible in the global context (default execution context)
    PyDict_SetItemString(as_ptr_<PyObject>(zs_world_handle()), "zpy", mod);

    if (PyVar submodule = PyModule_Create(&zpy_ops_module)) {
      // Py_DECREF(PyImport_ImportModule("zpy"));
      if (!PyModule_AddObjectRef(mod, "ops", submodule)) PyErr_Print();
    }
    if (PyVar submodule = PyModule_Create(&zpy_ui_module)) {
      if (!PyModule_AddObjectRef(mod, "ui", submodule)) PyErr_Print();
    }
    if (PyVar submodule = PyModule_Create(&zpy_scene_module)) {
      if (!PyModule_AddObjectRef(mod, "scene", submodule)) PyErr_Print();
      SceneInterfaceInit(submodule);
    }

    //
    for (int i = 0; zpy_methods[i].ml_name; ++i) {
      PyMethodDef *method = &zpy_methods[i];
      PyModule_AddObject(mod, method->ml_name, (PyObject *)PyCFunction_New(method, nullptr));
    }
  } else
    PyErr_Print();
}

/// seperator '.'
void *zs_world_get_module(const char *path) {
  if (PyVar zpyModule = PyImport_ImportModule("zpy")) {
    if (!path || path[0] == '\0') return static_cast<void *>(zpyModule);
    if (PyVar targetModule = PyObject_GetAttrString(zpyModule, path)) {
      return static_cast<void *>(targetModule);
    }
  }
  return nullptr;
}
}
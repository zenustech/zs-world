#include "SceneInterface.hpp"

#include "World.hpp"
#include "interface/details/PyHelper.hpp"

namespace zs {

  SceneConfig::~SceneConfig() {
    {
      GILGuard guard;
      mParameters.clear();
    }
  }
  SceneLightHolder SceneLightConcept::fromPrim(ScenePrimHolder& prim) {
    ScenePrimConcept* ptr = prim.get();
    if (!ptr) {
      return SceneLightHolder();
    }

    prim.release();
    return SceneLightHolder(dynamic_cast<SceneLightConcept*>(ptr));
  }

  SceneCameraHolder SceneCameraConcept::fromPrim(ScenePrimHolder& prim) {
    ScenePrimConcept* ptr = prim.get();
    if (!ptr) {
      return SceneCameraHolder();
    }

    prim.release();
    return SceneCameraHolder(dynamic_cast<SceneCameraConcept*>(ptr));
  }
}  // namespace zs

#ifdef __cplusplus
extern "C" {
#endif
/**********************************
 * Zone For ZpySceneConfig
 ***********************************/
static PyObject* ZpySceneConfig_new(PyTypeObject* type, PyObject* args_ = nullptr,
                                    PyObject* kwds = nullptr) {
  ZpySceneConfig* self = (ZpySceneConfig*)type->tp_alloc(type, 0);
  if (!self) {
    Py_RETURN_NONE;
  }

  self->mConfig = new zs::SceneConfig();
  self->mOwnCounter = 1;
  return (PyObject*)self;
}

static int ZpySceneConfig_init(ZpySceneConfig* self, PyObject* args, PyObject* kwds) { return 0; }

static void ZpySceneConfig_dealloc(ZpySceneConfig* self) {
  if (self->mOwnCounter && self->mConfig) delete self->mConfig;
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* _get(ZpySceneConfig* self, PyObject* _key) {
  PyVar keyVar = zs_bytes_obj(ZsBytes(_key));
  if (!keyVar) {
    Py_RETURN_NONE;
  }
  const char* keyStr = keyVar.asBytes().c_str();
  if (!keyStr) {
    Py_RETURN_NONE;
  }

  auto val = self->mConfig->get(keyStr);
  if (!val) {
    Py_RETURN_NONE;
  }

  return as_ptr_<PyObject>(val);
}

static PyObject* _set(ZpySceneConfig* self, PyObject* _pair) {
  ZsObject args = _pair;
  assert(args.pytype() == &PyTuple_Type);
  PyObject *pa, *pb;
  if (!PyArg_ParseTuple(as_ptr_<PyObject>(args), "OO", &pa, &pb)) {
    Py_RETURN_FALSE;
  }

  PyVar keyVar = zs_bytes_obj(ZsValue(pa));
  if (!keyVar) {
    Py_RETURN_FALSE;
  }
  const char* keyStr = keyVar.asBytes().c_str();
  if (!keyStr) {
    Py_RETURN_FALSE;
  }

  ZsValue val = pb;
  if (val.isCstr() || val.isString() || val.isBytesOrByteArray()) {
    PyVar valVar = zs_string_obj(val);
    if (!valVar) {
      Py_RETURN_FALSE;
    }
    self->mConfig->set(keyStr, ZsBytes(valVar.release()));
  } else {
    self->mConfig->set(keyStr, ZsObject(pb));
  }
  Py_RETURN_TRUE;
}

static PyMethodDef ZpySceneConfig_methods[] = {
    {"get", (PyCFunction)_get, METH_O, nullptr},
    {"set", (PyCFunction)_set, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyTypeObject ZpySceneConfig_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SceneConfig",
    /*tp_basicsize*/ sizeof(ZpySceneConfig),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)ZpySceneConfig_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,  // (reprfunc)ZpySceneConfig_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ ZpySceneConfig_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,  //
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ZpySceneConfig_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ ZpySceneConfig_new,  // PyType_GenericNew
};

/**********************************
 * Zone For ZpyPrim
 ***********************************/
static int ZpyPrim_init(ZpyPrim* self, PyObject* args, PyObject* kwds) { return 0; }

static void ZpyPrim_dealloc(ZpyPrim* self) {
  if (self->mOwnCounter && self->mPrimHolder) self->mPrimHolder.reset();
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* _get_parent(ZpyPrim* self) {
  ZpyPrim* p = (ZpyPrim*)ZpyPrim_Type.tp_alloc(&ZpyPrim_Type, 0);
  if (!p) {
    Py_RETURN_NONE;
  }

  p->mPrimHolder = self->mPrimHolder->getParent();
  if (!p->mPrimHolder) {
    Py_DECREF(p);
    Py_RETURN_NONE;
  }

  p->mOwnCounter = 1;
  return (PyObject*)p;
}

static PyObject* _get_child(ZpyPrim* self, PyObject* _childName) {
  PyVar childNameVar = zs_bytes_obj(ZsBytes(_childName));
  const char* childName = childNameVar ? childNameVar.asBytes().c_str() : nullptr;
  if (!childName) {
    Py_RETURN_NONE;
  }

  ZpyPrim* child = (ZpyPrim*)ZpyPrim_Type.tp_alloc(&ZpyPrim_Type, 0);
  if (!child) {
    Py_RETURN_NONE;
  }

  child->mPrimHolder = self->mPrimHolder->getChild(childName);
  if (!child->mPrimHolder) {
    Py_DECREF(child);
    Py_RETURN_NONE;
  }

  child->mOwnCounter = 1;
  return (PyObject*)child;
}

static PyObject* _get_all_childs(ZpyPrim* self) {
  if (!self->mPrimHolder) {
    Py_RETURN_NONE;
  }

  std::vector<zs::ScenePrimHolder> childs;
  size_t num;
  if (!self->mPrimHolder->getAllChilds(&num, nullptr)) {
    Py_RETURN_NONE;
  }
  childs.resize(num);
  if (!self->mPrimHolder->getAllChilds(&num, childs.data())) {
    Py_RETURN_NONE;
  }

  ZsList childlist = zs_list_obj_default();
  if (childlist.isNone()) {
    Py_RETURN_NONE;
  }

  for (int i = 0; i < childs.size(); ++i) {
    if (!childs[i]->isValid()) continue;

    ZpyPrim* child = (ZpyPrim*)ZpyPrim_Type.tp_alloc(&ZpyPrim_Type, 0);
    if (!child) continue;

    child->mPrimHolder.swap(childs[i]);
    child->mOwnCounter = 1;
    childlist.appendSteal(child);
  }

  return as_ptr_<PyObject>(childlist);
}

static PyObject* _get_name(ZpyPrim* self) {
  if (!self->mPrimHolder) {
    Py_RETURN_NONE;
  }

  const char* name = self->mPrimHolder->getName();
  if (!name) {
    Py_RETURN_NONE;
  }

  return as_ptr_<PyObject>(zs_string_obj_cstr(name));
}

static PyObject* _get_scene(ZpyPrim* self) {
  if (!self->mPrimHolder) Py_RETURN_NONE;

  auto scene = self->mPrimHolder->getScene();
  if (!scene) Py_RETURN_NONE;

  ZpySceneDesc* desc = (ZpySceneDesc*)ZpySceneDesc_Type.tp_alloc(&ZpySceneDesc_Type, 0);
  if (!desc) Py_RETURN_NONE;

  desc->mScene = scene;
  desc->mOwnCounter = 1;

  return (PyObject*)desc;
}

static PyObject* _is_valid(ZpyPrim* self) {
  if (self->mPrimHolder && self->mPrimHolder->isValid()) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

static PyMethodDef ZpyPrim_methods[] = {
    {"get_parent", (PyCFunction)_get_parent, METH_NOARGS, nullptr},
    {"get_name", (PyCFunction)_get_name, METH_NOARGS, nullptr},
    {"get_child", (PyCFunction)_get_child, METH_O, nullptr},
    {"get_all_childs", (PyCFunction)_get_all_childs, METH_NOARGS, nullptr},
    {"get_scene", (PyCFunction)_get_scene, METH_NOARGS, nullptr},
    {"is_valid", (PyCFunction)_is_valid, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyTypeObject ZpyPrim_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Prim",
    /*tp_basicsize*/ sizeof(ZpyPrim),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)ZpyPrim_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,  // (reprfunc)ZpyPrim_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ ZpyPrim_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,  //
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ZpyPrim_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,  // PyType_GenericNew
};

/**********************************
 * Zone For ZpySceneObserver
 ***********************************/
static int ZpySceneObserver_init(ZpySceneObserver* self, PyObject* args, PyObject* kwds) {
  return 0;
}

static void ZpySceneObserver_dealloc(ZpySceneObserver* self) {
  if (self->mOwnCounter && self->mObserver) delete self->mObserver;
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMethodDef ZpySceneObserver_methods[] = {
    // TODO
    {nullptr, nullptr, 0, nullptr},
};

PyTypeObject ZpySceneObserver_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SceneObserver",
    /*tp_basicsize*/ sizeof(ZpySceneObserver),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)ZpySceneObserver_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,  // (reprfunc)ZpySceneObserver_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ ZpySceneObserver_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,  //
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ZpySceneObserver_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr  // PyType_GenericNew
};

/**********************************
 * Zone For ZpySceneDesc
 ***********************************/
static int ZpySceneDesc_init(ZpySceneDesc* self, PyObject* args, PyObject* kwds) { return 0; }

static void ZpySceneDesc_dealloc(ZpySceneDesc* self) {
  // mScene is maintained by scene manager, it shouldn't be deleted by dealloc
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* _open_scene(ZpySceneDesc* self, ZpySceneConfig* _config) {
  if (!self->mScene || !_config->mConfig) {
    Py_RETURN_FALSE;
  }

  if (self->mScene->openScene(*_config->mConfig)) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

static PyObject* _compose_scene(ZpySceneDesc* self, ZpySceneConfig* _config) {
  if (!self->mScene || !_config->mConfig) {
    Py_RETURN_FALSE;
  }

  if (self->mScene->composeScene(*_config->mConfig)) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

static PyObject* _export_scene(ZpySceneDesc* self, ZpySceneConfig* _config) {
  if (!self->mScene || !_config->mConfig) {
    Py_RETURN_FALSE;
  }

  if (self->mScene->exportScene(*_config->mConfig)) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

static PyObject* _create_prim(ZpySceneDesc* self, PyObject* _path) {
  if (!self->mScene) {
    Py_RETURN_NONE;
  }
  PyVar pathVar = zs_bytes_obj(ZsBytes(_path));
  if (!pathVar) {
    Py_RETURN_NONE;
  }
  const char* pathCStr = pathVar.asBytes().c_str();
  if (!pathCStr) {
    Py_RETURN_NONE;
  }

  ZpyPrim* zp = (ZpyPrim*)ZpyPrim_Type.tp_alloc(&ZpyPrim_Type, 0);
  if (!zp) {
    Py_RETURN_NONE;
  }

  zp->mPrimHolder = self->mScene->createPrim(pathCStr);
  if (!zp->mPrimHolder) {
    Py_DECREF(zp);
    Py_RETURN_NONE;
  }

  zp->mOwnCounter = 1;
  return (PyObject*)zp;
}

static PyObject* _get_prim(ZpySceneDesc* self, PyObject* _path) {
  if (!self->mScene) Py_RETURN_NONE;

  PyVar pathVar = zs_bytes_obj(ZsBytes(_path));
  if (!pathVar) Py_RETURN_NONE;

  const char* pathCStr = pathVar.asBytes().c_str();
  if (!pathCStr) Py_RETURN_NONE;

  ZpyPrim* zp = (ZpyPrim*)ZpyPrim_Type.tp_alloc(&ZpyPrim_Type, 0);
  if (!zp) Py_RETURN_NONE;

  zp->mPrimHolder = self->mScene->getPrim(pathCStr);
  if (!zp->mPrimHolder) {
    Py_DECREF(zp);
    Py_RETURN_NONE;
  }

  zp->mOwnCounter = 1;
  return (PyObject*)zp;
}

static PyObject* _get_root_prim(ZpySceneDesc* self) {
  if (!self->mScene) Py_RETURN_NONE;

  ZpyPrim* zp = (ZpyPrim*)ZpyPrim_Type.tp_alloc(&ZpyPrim_Type, 0);
  if (!zp) Py_RETURN_NONE;

  zp->mPrimHolder = self->mScene->getRootPrim();
  if (!zp->mPrimHolder) {
    Py_DECREF(zp);
    Py_RETURN_NONE;
  }

  zp->mOwnCounter = 1;
  return (PyObject*)zp;
}

static PyObject* _remove_prim(ZpySceneDesc* self, PyObject* _path) {
  if (!self->mScene) {
    Py_RETURN_FALSE;
  }
  PyVar pathVar = zs_bytes_obj(ZsBytes(_path));
  if (!pathVar) {
    Py_RETURN_FALSE;
  }
  const char* pathCStr = pathVar.asBytes().c_str();
  if (!pathCStr) {
    Py_RETURN_FALSE;
  }

  if (self->mScene->removePrim(pathCStr)) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

static PyMethodDef USDSceneDesc_methods[] = {
    {"open", (PyCFunction)_open_scene, METH_O, nullptr},
    {"compose", (PyCFunction)_compose_scene, METH_O, nullptr},
    {"save", (PyCFunction)_export_scene, METH_O, nullptr},
    {"create_prim", (PyCFunction)_create_prim, METH_O, nullptr},
    {"get_prim", (PyCFunction)_get_prim, METH_O, nullptr},
    {"get_root_prim", (PyCFunction)_get_root_prim, METH_NOARGS, nullptr},
    {"remove_prim", (PyCFunction)_remove_prim, METH_O, nullptr},
    // {"create_observer", (PyCFunction)_create_observer, METH_O, nullptr},
    // {"remove_observer", (PyCFunction)_remove_observer, METH_O, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyTypeObject ZpySceneDesc_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SceneDesc",
    /*tp_basicsize*/ sizeof(ZpySceneDesc),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)ZpySceneDesc_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,  // (reprfunc)ZpySceneDesc_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ USDSceneDesc_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,  //
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ZpySceneDesc_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr  // PyType_GenericNew
};

// init all scene manage interfaces in zpy
int SceneInterfaceInit(void* _mod) {
  PyObject* mod = static_cast<PyObject*>(_mod);
  if (!mod) return -1;

  if (PyType_Ready(&ZpySceneConfig_Type) < 0) return -1;
  if (PyType_Ready(&ZpyPrim_Type) < 0) return -1;
  if (PyType_Ready(&ZpySceneObserver_Type) < 0) return -1;
  if (PyType_Ready(&ZpySceneDesc_Type) < 0) return -1;

  Py_INCREF(&ZpySceneConfig_Type);
  Py_INCREF(&ZpyPrim_Type);
  Py_INCREF(&ZpySceneObserver_Type);
  Py_INCREF(&ZpySceneDesc_Type);

  PyModule_AddObject(mod, "SceneConfig", (PyObject*)&ZpySceneConfig_Type);
  PyModule_AddObject(mod, "Prim", (PyObject*)&ZpyPrim_Type);
  PyModule_AddObject(mod, "SceneObserver", (PyObject*)&ZpySceneObserver_Type);
  PyModule_AddObject(mod, "SceneDesc", (PyObject*)&ZpySceneDesc_Type);

  return 0;
}
#ifdef __cplusplus
}
#endif

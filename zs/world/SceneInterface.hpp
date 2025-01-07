#pragma once

#include <Python.h>

#include <any>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "world/WorldExport.hpp"
#include "glm/glm.hpp"
#include "interface/details/PyHelper.hpp"
#include "interface/world/ObjectInterface.hpp"
#include "zensim/ZpcFunction.hpp"
#include "zensim/ZpcResource.hpp"
#include "zensim/execution/ConcurrencyPrimitive.hpp"
#include "zensim/geometry/Mesh.hpp"
#include "zensim/vulkan/VkModel.hpp"

namespace zs {
  using SceneName = const char*;

  enum class asset_origin_e { native = 0, usd, houdini, maya, blender, ue, unity };

  struct ZS_WORLD_EXPORT SceneConfig {
  public:
    SceneConfig() = default;
    ~SceneConfig();
    SceneConfig(std::map<std::string, ZsVar>&& other) : mParameters(std::move(other)) {}
    SceneConfig& operator=(SceneConfig&& other) noexcept {
      mParameters = std::move(other.mParameters);
      return *this;
    }

    ZsObject get(const char* key) const {
      if (mParameters.find(key) == mParameters.end()) {
        std::cout << "key not found: " << key << std::endl;
        return {};
      }
      return mParameters.at(key).getValue();
    }

    const char* getCStr(const char* key) const {
      ZsValue val = get(key);
      const char* ret = val.asString().c_str();
      return ret ? ret : "";
    }
    const char* getString(const char* key) const {
      const auto it = mStrParams.find(key);
      if (it == mStrParams.end()) {
        return nullptr;
      }
      return it->second.c_str();
    }

    void set(const char* key, ZsValue value) { mParameters[key] = value; }

    void set(const char* key, const char* value) { mParameters[key] = zs_string_obj_cstr(value); }
    void setString(const char* key, const char* str) { mStrParams[key] = str; }

  private:
    std::map<std::string, ZsVar> mParameters;
    std::map<std::string, std::string> mStrParams;
  };

  struct ScenePrimDeleter {
    void operator()(Deletable* prim) { prim->deinit(); }
  };
  struct ScenePrimConcept;
  struct SceneLightConcept;
  struct SceneCameraConcept;
  using ScenePrimHolder = UniquePtr<ScenePrimConcept, ScenePrimDeleter>;
  using SceneLightHolder = UniquePtr<SceneLightConcept, ScenePrimDeleter>;
  using SceneCameraHolder = UniquePtr<SceneCameraConcept, ScenePrimDeleter>;

  struct SceneDescConcept;
  struct ScenePrimConcept : public Deletable {
    virtual ~ScenePrimConcept() = default;
    virtual void deinit() { ::delete this; }

    // shall we?
    virtual bool isLight() const { return false; }

    virtual const char* getName() const { return ""; }
    virtual const char* getPath() const { return ""; }
    virtual const char* getTypeName() const { return ""; }
    virtual ScenePrimHolder getParent() const { return ScenePrimHolder(); }
    virtual ScenePrimHolder getChild(const char* childName) const { return ScenePrimHolder(); }
    virtual ScenePrimHolder addChild(const char* childName) { return ScenePrimHolder(); }
    virtual bool getAllChilds(size_t* num, ScenePrimHolder* childs) const { return false; }
    virtual SceneDescConcept* getScene() const { return nullptr; }

    virtual bool isValid() const { return false; }
    virtual bool isEditable(const char* attr = nullptr) const { return false; }
    virtual bool setEditable(const char* attr = nullptr, bool editable = false) { return false; }

#define EASY_GET_TIME_SAMPLES(name) \
  virtual size_t get##name##TimeSamples(double*) const { return 0; }
    EASY_GET_TIME_SAMPLES(Visible)
    EASY_GET_TIME_SAMPLES(FaceIndex)
    EASY_GET_TIME_SAMPLES(FaceSize)
    EASY_GET_TIME_SAMPLES(Point)
    EASY_GET_TIME_SAMPLES(UV)
    EASY_GET_TIME_SAMPLES(Normal)
    EASY_GET_TIME_SAMPLES(Color)
    EASY_GET_TIME_SAMPLES(Translation)
    EASY_GET_TIME_SAMPLES(Scale)
    EASY_GET_TIME_SAMPLES(RotationX)
    EASY_GET_TIME_SAMPLES(RotationY)
    EASY_GET_TIME_SAMPLES(RotationZ)
    EASY_GET_TIME_SAMPLES(RotationXYZ)
    EASY_GET_TIME_SAMPLES(RotationXZY)
    EASY_GET_TIME_SAMPLES(RotationYXZ)
    EASY_GET_TIME_SAMPLES(RotationYZX)
    EASY_GET_TIME_SAMPLES(RotationZXY)
    EASY_GET_TIME_SAMPLES(RotationZYX)
    EASY_GET_TIME_SAMPLES(MatrixOp)
#undef EASY_GET_TIME_SAMPLES
    // To be finished, this interface is made for extended attributes in the future
    virtual size_t getAttributeTimeSamples(std::string_view attrLabel, double*) const { return 0; }
    virtual bool isTimeVarying() const { return false; }

    virtual bool getSkelAnimTimeCodeInterval(double& start, double& end) const { return false; }

    virtual bool getVisible(double time = -1.0) const { return false; }
    virtual bool setVisible(bool visible, double time = -1.0) { return false; }
    virtual glm::mat4 getWorldTransform(double time = -1.0) const { return {}; }
    virtual bool setLocalMatrixOpValue(const glm::mat4& transform, double time = -1.0,
                                       const std::string& suffix = "") {
      return false;
    }
    virtual bool setLocalTranslate(const glm::vec3& translation, double time = -1.0) {
      return false;
    }
    virtual bool setLocalScale(const glm::vec3& scale, double time = -1.0) { return false; }
    virtual bool setLocalRotateXYZ(const glm::vec3& euler, double time = -1.0) { return false; }
    virtual glm::mat4 getLocalMatrixOpValue(double time = -1.0,
                                            const std::string& suffix = "") const {
      return {};
    }
    virtual glm::mat4 getLocalTransform(double time = -1.0) const { return {}; }
    virtual glm::vec3 getLocalTranslate(double time = -1.0) const { return {}; }
    virtual glm::vec3 getLocalScale(double time = -1.0) const { return {}; }
    virtual glm::vec3 getLocalRotateXYZ(double time = -1.0) const { return {}; }

    virtual bool copy(const ScenePrimConcept* other) { return false; }
    virtual bool copy(const ScenePrimHolder& other) { return false; }
    virtual Mesh<float, 3, u32, 3> toVisMesh(double time = -1.) const { return {}; }
    virtual bool fromVisMesh(const Mesh<float, 3, u32, 3>& mesh) { return false; }

    virtual std::any getRawPrim() const { return std::any(); }
  };

  struct SceneObserver {
    std::function<void(const zs::ScenePrimHolder&)> onPrimUpdate = nullptr;
    std::function<void(const SceneDescConcept*)> onSceneUpdate = nullptr;
    std::set<std::string> mFocusSceneNames;
    std::set<std::string> mFocusPrimPaths;
  };

  struct SceneLightConcept : public ScenePrimConcept {
    static SceneLightHolder fromPrim(ScenePrimHolder&);

    virtual bool isValid() const { return false; }

    virtual bool setIntensity(float intensity, double time = -1.0) { return false; }
    virtual float getIntensity(double time = -1.0) const { return 0.0f; }
    virtual bool setLightColor(const vec<float, 3>& color, double time = -1.0) { return false; }
    virtual vec<float, 3> getLightColor(double time = -1.0) const { return false; }

    virtual bool setEnableTemperature(bool enable, double time = -1.0) { return false; }
    virtual bool getEnableTemperature(double time = -1.0) const { return false; }
    virtual bool setTemperature(float temperature, double time = -1.0) { return false; }
    virtual float getTemperature(double time = -1.0) const { return 0.0f; }
  };

  struct SceneCameraConcept : public ScenePrimConcept {
    static SceneCameraHolder fromPrim(ScenePrimHolder&);

    // TODO
    virtual bool isValid() const { return false; }
  };

  // the concept of scene description data structure
  struct SceneDescConcept {
    virtual SceneName getName() const { return ""; }
    virtual bool openScene(const SceneConfig&) { return false; }
    virtual bool composeScene(const SceneConfig&) { return false; }
    virtual bool exportScene(const SceneConfig&) { return false; }

    virtual bool getIsYUpAxis() const { return true; }
    virtual bool setIsYUpAxis(bool) { return false; }

    virtual void markZpcTransformDirty() { }
    virtual std::string getZpcTransformSuffix() const { return ""; }

    virtual double getStartTimeCode() const { return 0.0; }
    virtual double getEndTimeCode() const { return 0.0; }
    virtual bool setStartTimeCode(double timecode) { return false; }
    virtual bool setEndTimeCode(double timecode) { return false; }
    virtual double getTimeCodesPerSecond() const { return 0.0; }
    virtual bool setTimeCodesPerSecond(double timecodesPerSecond) { return false; }
    virtual double getFPS() const { return 0.0; }
    virtual bool setFPS(double fps) { return false; }

    virtual ScenePrimHolder createPrim(const char* path) { return ScenePrimHolder(); }
    virtual ScenePrimHolder getPrim(const char* path) const { return ScenePrimHolder(); }
    virtual ScenePrimHolder getRootPrim() const { return ScenePrimHolder(); }
    virtual bool removePrim(const char* path) { return false; }

    virtual void onRender() const {}  // TODO
    virtual std::any getRawScene() const { return std::any(); }
  };

  struct SceneManagerConcept {
  public:
    virtual SceneDescConcept* createScene(SceneName sceneName) { return nullptr; }
    virtual const SceneDescConcept* getScene(SceneName sceneName) const { return nullptr; }
    virtual SceneDescConcept* getScene(SceneName sceneName) { return nullptr; }
    virtual bool getAllScenes(size_t* num, SceneDescConcept** scenes) const { return false; }
    virtual bool getAllSceneNames(size_t* num, SceneName* names) const { return false; }
    virtual bool removeScene(SceneName sceneName) { return false; }
    virtual bool clearAllScenes() { return false; }
    virtual bool addObserver(SceneObserver* sceneObserver) { return false; }
    virtual bool removeObserver(SceneObserver* sceneObserver) { return false; }
  };
}  // namespace zs

/*
 * Define the zpy wrapper of all these base classes
 */
#ifdef __cplusplus
extern "C" {
#endif
int SceneInterfaceInit(void* mod);

// Zpy object definition for SceneConfig
extern PyTypeObject ZpySceneConfig_Type;
#define ZpySceneConfig_Check(v) \
  (PyObject_IsInstance(static_cast<PyObject*>(v), static_cast<PyObject*>(&ZpySceneConfig_Type)))
struct ZpySceneConfig {
  PyObject_HEAD zs::SceneConfig* mConfig = nullptr;
  int mOwnCounter = 0;
};

// Zpy object definition for ScenePrimConcept
extern PyTypeObject ZpyPrim_Type;
#define ZpyPrim_Check(v) \
  (PyObject_IsInstance(static_cast<PyObject*>(v), static_cast<PyObject*>(&ZpyPrim_Type)))
struct ZpyPrim {
  PyObject_HEAD zs::ScenePrimHolder mPrimHolder;
  int mOwnCounter = 0;
};

// Zpy object definition for SceneObserver
extern PyTypeObject ZpySceneObserver_Type;
#define ZpySceneObserver_Check(v) \
  (PyObject_IsInstance(static_cast<PyObject*>(v), static_cast<PyObject*>(&ZpySceneObserver_Type)))
struct ZpySceneObserver {
  PyObject_HEAD zs::SceneObserver* mObserver = nullptr;
  int mOwnCounter = 0;
};

// Zpy object definition for SceneDescConcept
extern PyTypeObject ZpySceneDesc_Type;
#define ZpySceneDesc_Check(v) \
  (PyObject_IsInstance(static_cast<PyObject*>(v), static_cast<PyObject*>(&ZpySceneDesc_Type)))
struct ZpySceneDesc {
  PyObject_HEAD zs::SceneDescConcept* mScene = nullptr;
  int mOwnCounter = 0;
};
#ifdef __cplusplus
}
#endif

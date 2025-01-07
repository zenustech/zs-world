#pragma once

#include <any>
#include <iostream>
#include <set>
#include <string>

#include "world/SceneInterface.hpp"

namespace zs {
  struct USDScenePrim : public ScenePrimConcept {
  public:
    USDScenePrim();
    USDScenePrim(SceneDescConcept* scene, std::any usdPrim);

    virtual ScenePrimHolder getParent() const override;
    virtual ScenePrimHolder getChild(const char* childName) const override;
    virtual ScenePrimHolder addChild(const char* childName) override;
    virtual bool getAllChilds(size_t* num, ScenePrimHolder* childs) const override;

    virtual SceneDescConcept* getScene() const override;
    virtual bool isValid() const override;

#define EASY_OVERRIDE_TIME_SAMPLES(name) \
  virtual size_t get##name##TimeSamples(double*) const override;
    EASY_OVERRIDE_TIME_SAMPLES(Visible);
    EASY_OVERRIDE_TIME_SAMPLES(FaceIndex);
    EASY_OVERRIDE_TIME_SAMPLES(FaceSize);
    EASY_OVERRIDE_TIME_SAMPLES(Point);
    EASY_OVERRIDE_TIME_SAMPLES(UV);
    EASY_OVERRIDE_TIME_SAMPLES(Normal);
    EASY_OVERRIDE_TIME_SAMPLES(Color);
    EASY_OVERRIDE_TIME_SAMPLES(Translation);
    EASY_OVERRIDE_TIME_SAMPLES(Scale);
    EASY_OVERRIDE_TIME_SAMPLES(RotationX);
    EASY_OVERRIDE_TIME_SAMPLES(RotationY);
    EASY_OVERRIDE_TIME_SAMPLES(RotationZ);
    EASY_OVERRIDE_TIME_SAMPLES(RotationXYZ);
    EASY_OVERRIDE_TIME_SAMPLES(RotationXZY);
    EASY_OVERRIDE_TIME_SAMPLES(RotationYXZ);
    EASY_OVERRIDE_TIME_SAMPLES(RotationYZX);
    EASY_OVERRIDE_TIME_SAMPLES(RotationZXY);
    EASY_OVERRIDE_TIME_SAMPLES(RotationZYX);
    EASY_OVERRIDE_TIME_SAMPLES(MatrixOp);
#undef EASY_OVERRIDE_TIME_SAMPLES

    virtual bool isTimeVarying() const override;
    virtual bool getSkelAnimTimeCodeInterval(double& start, double& end) const override;

    virtual const char* getName() const override;
    virtual const char* getPath() const override;
    virtual const char* getTypeName() const override;
    virtual bool getVisible(double time = -1.0) const override;
    virtual bool setVisible(bool visible, double time = -1.0) override;
    virtual glm::mat4 getWorldTransform(double time = -1.0) const override;
    virtual bool setLocalMatrixOpValue(const glm::mat4& transform, double time = -1.0, const std::string& suffix = "") override;
    virtual bool setLocalTranslate(const glm::vec3& translation, double time = -1.0) override;
    virtual bool setLocalScale(const glm::vec3& scale, double time = -1.0) override;
    virtual bool setLocalRotateXYZ(const glm::vec3& euler, double time = -1.0) override;
    virtual glm::mat4 getLocalMatrixOpValue(double time = -1.0, const std::string& suffix = "") const override;
    virtual glm::mat4 getLocalTransform(double = -1.0) const override;
    virtual glm::vec3 getLocalTranslate(double time = -1.0) const override;
    virtual glm::vec3 getLocalScale(double time = -1.0) const override;
    virtual glm::vec3 getLocalRotateXYZ(double time = -1.0) const override;

    virtual bool copy(const ScenePrimConcept* other) override;
    virtual bool copy(const ScenePrimHolder& other) override;
    virtual Mesh<float, 3, u32, 3> toVisMesh(double) const override;
    virtual bool fromVisMesh(const Mesh<float, 3, u32, 3>& mesh) override;

    virtual std::any getRawPrim() const override;

  protected:
    std::any mPrim;
    SceneDescConcept* mScene;
  };

  struct USDSceneLight : public USDScenePrim, public SceneLightConcept {
    virtual bool isValid() const override;

    virtual bool setIntensity(float intensity, double time = -1.0) override;
    virtual float getIntensity(double time = -1.0) const override;
    virtual bool setLightColor(const vec<float, 3>& color, double time = -1.0) override;
    virtual vec<float, 3> getLightColor(double time = -1.0) const override;

    virtual bool setEnableTemperature(bool enable, double time = -1.0) override;
    virtual bool getEnableTemperature(double time = -1.0) const override;
    virtual bool setTemperature(float temperature, double time = -1.0) override;
    virtual float getTemperature(double time = -1.0) const override;
  };

  struct USDSceneDesc : public SceneDescConcept {
  public:
    USDSceneDesc(SceneName sceneName = "");
    USDSceneDesc(const SceneConfig&);

    virtual SceneName getName() const override;
    virtual bool openScene(const SceneConfig& config) override;
    virtual bool composeScene(const SceneConfig&) override;
    virtual bool exportScene(const SceneConfig&) override;

    virtual bool getIsYUpAxis() const override;
    virtual bool setIsYUpAxis(bool) override;
    
    virtual void markZpcTransformDirty() override;
    virtual std::string getZpcTransformSuffix() const override;

    virtual double getStartTimeCode() const override;
    virtual double getEndTimeCode() const override;
    virtual bool setStartTimeCode(double timecode) override;
    virtual bool setEndTimeCode(double timecode) override;
    virtual double getTimeCodesPerSecond() const override;
    virtual bool setTimeCodesPerSecond(double timecodesPerSecond) override;
    virtual double getFPS() const override;
    virtual bool setFPS(double fps) override;

    virtual ScenePrimHolder createPrim(const char* path) override;
    virtual ScenePrimHolder getPrim(const char* path) const override;
    virtual ScenePrimHolder getRootPrim() const override;
    virtual bool removePrim(const char* path) override;

    virtual void onRender() const override {}  // TODO
    virtual std::any getRawScene() const override;

    // TODO: set/get custom data?
    unsigned long long getTransformCount() const;
    void setTransformCount(unsigned long long);
  private:
    std::any _getOrCreateRootStage();
    std::any mRootStage;

    std::string mName;

    bool mZpcTransformEdit = false;

    void viewTree() const;
  };

  struct USDSceneManager : public SceneManagerConcept {
  public:
    USDSceneManager();
    virtual ~USDSceneManager();

    virtual SceneDescConcept* createScene(SceneName name) override;
    virtual bool clearAllScenes() override;
    virtual bool removeScene(SceneName name) override;

    virtual const SceneDescConcept* getScene(SceneName name) const override;
    virtual SceneDescConcept* getScene(SceneName name) override;
    virtual bool getAllScenes(size_t* num, SceneDescConcept** scenes) const override;
    virtual bool getAllSceneNames(size_t* num, SceneName* names) const override;

    virtual bool addObserver(SceneObserver* sceneObserver) override;
    virtual bool removeObserver(SceneObserver* sceneObserver) override;

    SceneDescConcept* findSceneByStage(std::any stage);

  private:
    std::map<std::string, USDSceneDesc> mSceneMap;
    std::map<SceneObserver*, void* /* USDSceneListener */> mObservers;
  };
}  // namespace zs

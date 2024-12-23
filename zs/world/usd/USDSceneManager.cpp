#include <pxr/pxr.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/notice.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/plane.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/geometryLight.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/udimUtils.h>

// #include "USDNodeAligner.hpp"
#include "USDScenePrim.hpp"

namespace zs {
  // USD notice transferer
  struct USDNoticeTransporter : pxr::TfWeakBase {
    USDNoticeTransporter(USDSceneManager* manager, SceneObserver* observer)
        : mManager(manager), mObserver(observer) {
      pxr::TfWeakPtr<USDNoticeTransporter> me(this);
      std::cout << "listening for usd scene change" << std::endl;
      if (observer->onPrimUpdate) {
        mPrimKey = pxr::TfNotice::Register(me, &USDNoticeTransporter::onPrimChanged);
      }
      if (observer->onSceneUpdate) {
        mSceneKey = pxr::TfNotice::Register(me, &USDNoticeTransporter::onSceneChanged);
      }
    }

    virtual ~USDNoticeTransporter() {
      if (mPrimKey.IsValid()) {
        pxr::TfNotice::Revoke(mPrimKey);
      }
      if (mSceneKey.IsValid()) {
        pxr::TfNotice::Revoke(mSceneKey);
      }
    }

    void onPrimChanged(const pxr::UsdNotice::ObjectsChanged& notice) {
      auto stage = notice.GetStage();
      if (!stage) return;
      auto scene = mManager->findSceneByStage(stage);
      if (!scene) return;

      for (auto path : notice.GetResyncedPaths()) {
        auto prim = scene->getPrim(path.GetText());
        if (!prim) continue;

        if (mObserver->mFocusPrimPaths.empty()
            || mObserver->mFocusPrimPaths.contains(prim->getPath())) {
          mObserver->onPrimUpdate(prim);
        }
      }
    }

    void onSceneChanged(const pxr::UsdNotice::StageContentsChanged& notice) {
      if (!mManager) return;

      auto noticeStage = notice.GetStage();
      if (!noticeStage) return;

      auto scene = mManager->findSceneByStage(noticeStage);
      if (!scene) return;

      const auto& focusScenes = mObserver->mFocusSceneNames;
      if (focusScenes.empty() || focusScenes.contains(scene->getName())) {
        mObserver->onSceneUpdate(scene);
      }
    }

    USDSceneManager* mManager = nullptr;
    SceneObserver* mObserver = nullptr;
    pxr::TfNotice::Key mPrimKey;
    pxr::TfNotice::Key mSceneKey;
  };

  /******************
   * USD Scene Manager
   *******************/
  USDSceneManager::USDSceneManager() { std::cout << "Initialize USD Scene Manager" << std::endl; }

  USDSceneManager::~USDSceneManager() { mSceneMap.clear(); }

  bool USDSceneManager::clearAllScenes() {
    if (!mSceneMap.empty()) {
      mSceneMap.clear();
    }

    return true;
  }

  bool USDSceneManager::removeScene(SceneName key) {
    if (!key) return false;
    if (mSceneMap.find(key) != mSceneMap.end()) {
      mSceneMap.erase(key);
    }
    return true;
  }

  SceneDescConcept* USDSceneManager::createScene(SceneName key) {
    if (!key) {
      std::cout << "failed to create scene: sceneId is None" << std::endl;
      return nullptr;
    }
    if (mSceneMap.find(key) != mSceneMap.end()) {  // existing scene id
      return nullptr;
    }

    mSceneMap.emplace(key, USDSceneDesc(key));
    return &mSceneMap[key];
  }

  const SceneDescConcept* USDSceneManager::getScene(SceneName key) const {
    if (!key) return nullptr;
    if (mSceneMap.find(key) == mSceneMap.end()) {
      return nullptr;
    }
    return &(mSceneMap.at(key));
  }

  SceneDescConcept* USDSceneManager::getScene(SceneName key) {
    if (!key) return nullptr;
    if (mSceneMap.find(key) == mSceneMap.end()) {
      return nullptr;
    }
    return &mSceneMap[key];
  }

  bool USDSceneManager::getAllScenes(size_t* num, SceneDescConcept** scenes) const {
    if (!num) return false;

    *num = mSceneMap.size();
    if (!scenes) return true;

    int i = 0;
    for (auto it : mSceneMap) {
      scenes[i] = &it.second;
      ++i;
    }
    return true;
  }

  bool USDSceneManager::getAllSceneNames(size_t* num, SceneName* keys) const {
    if (!num) return false;

    *num = mSceneMap.size();
    if (!keys) return true;

    int i = 0;
    for (auto it : mSceneMap) {
      keys[i] = it.first.c_str();
      ++i;
    }

    return true;
  }

  bool USDSceneManager::addObserver(SceneObserver* sceneObserver) {
    if (mObservers.find(sceneObserver) != mObservers.end()) {
      return false;  // duplicated observers
    }
    mObservers.emplace(sceneObserver, new USDNoticeTransporter(this, sceneObserver));
    return true;
  }

  bool USDSceneManager::removeObserver(SceneObserver* sceneObserver) {
    if (mObservers.find(sceneObserver) == mObservers.end()) {
      return false;
    }
    USDNoticeTransporter* listener = static_cast<USDNoticeTransporter*>(mObservers[sceneObserver]);
    delete listener;
    mObservers.erase(sceneObserver);
    return true;
  }

  SceneDescConcept* USDSceneManager::findSceneByStage(std::any s) {
    auto stage = std::any_cast<pxr::UsdStageWeakPtr>(s);
    if (!stage) return nullptr;

    for (auto it = mSceneMap.begin(); it != mSceneMap.end(); ++it) {
      auto rawScene = it->second.getRawScene();
      if (!rawScene.has_value()) continue;

      auto _stage = std::any_cast<pxr::UsdStageRefPtr>(rawScene);
      if (!_stage || _stage != stage) continue;

      return &it->second;
    }

    return nullptr;
  }
}  // namespace zs

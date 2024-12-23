#include <pxr/pxr.h>
#include <pxr/usd/usd/attribute.h>
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
#include <pxr/usd/usdGeom/tokens.h>
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

#include "USDScenePrim.hpp"

using namespace pxr;

#define GET_STAGE(x) UsdStageRefPtr x = std::any_cast<UsdStageRefPtr>(_getOrCreateRootStage())
#define PARSE_STAGE(x) UsdStageRefPtr x = std::any_cast<UsdStageRefPtr>(mRootStage)

namespace zs {
  /******************
   * USD Scene Description
   *******************/
  USDSceneDesc::USDSceneDesc(SceneName name) : mName(name) {}

  USDSceneDesc::USDSceneDesc(const SceneConfig& config) { composeScene(config); }

  std::any USDSceneDesc::_getOrCreateRootStage() {
    if (!mRootStage.has_value()) {
      mRootStage = UsdStage::CreateInMemory();
    }
    return mRootStage;
  }

  SceneName USDSceneDesc::getName() const { return mName.c_str(); }

  bool USDSceneDesc::openScene(const SceneConfig& config) {
    // std::string usdPath = config.getCStr("srcPath");
    const char* usdPath = nullptr;
    if (auto p = config.getString("srcPath"))
      usdPath = p;
    else
      usdPath = config.getCStr("srcPath");

    auto openStage = UsdStage::Open(usdPath);
    if (!openStage) {
      std::cout << "failed to open stage " << usdPath << std::endl;
      return false;
    }

    mRootStage = openStage;
    return true;
  }

  bool USDSceneDesc::composeScene(const SceneConfig& config) {
    // std::string usdPath = config.getCStr("srcPath");
    const char* usdPath = nullptr;
    if (auto p = config.getString("srcPath"))
      usdPath = p;
    else
      usdPath = config.getCStr("srcPath");

    auto stageToCompose = UsdStage::Open(usdPath);
    if (!stageToCompose) {
      std::cout << "failed to compose usd stage " << usdPath << std::endl;
      return false;
    }

    GET_STAGE(stage);

    // const char* composeMode = config.getCStr("mode");
    const char* composeMode = nullptr;
    if (auto p = config.getString("mode"))
      composeMode = p;
    else
      composeMode = config.getCStr("mode");

    if (strcmp(composeMode, "Reference") == 0) {
      const char* targetPath = config.getCStr("dstPrimPath");
      // check if target path exists
      if (stage->GetPrimAtPath(SdfPath(targetPath)).IsValid()) {
        std::cout << "failed to compose prim as the target path exists: " << targetPath
                  << std::endl;
        return false;
      }
      auto composePrim = stageToCompose->GetDefaultPrim();
      std::cout << "composing prim from given usd: " << composePrim.GetTypeName() << ' '
                << composePrim.GetPath() << std::endl;

      auto refPrim = stage->DefinePrim(SdfPath(targetPath));
      refPrim.GetReferences().AddReference(usdPath);
    } else if (strcmp(composeMode, "Sublayer") == 0) {
      std::cout << "sublayering from given usd: " << usdPath << std::endl;
      int sublayerIndex = (int)config.get("sublayerIndex");
      auto sublayerProxy = stage->GetRootLayer()->GetSubLayerPaths();
      sublayerProxy.Insert(sublayerIndex, usdPath);
    } else {
      std::cout << "unknown compose mode " << composeMode << std::endl;
      return false;
    }

    // viewTree();
    std::cout << "composing finished" << std::endl;

    return true;
  }

  bool USDSceneDesc::exportScene(const SceneConfig& config) {
    const char* exportPath = nullptr;
    if (auto p = config.getString("dstPath"))
      exportPath = p;
    else
      exportPath = config.getCStr("dstPath");

    if (!exportPath) {
      return false;
    }
    if (!mRootStage.has_value()) {
      auto emptyStage = UsdStage::CreateInMemory();
      return emptyStage && emptyStage->GetRootLayer()->Export(exportPath);
    }

    auto stage = std::any_cast<UsdStageRefPtr>(mRootStage);
    
    if (mZpcTransformEdit) {
      // TODO: set/get custom data?
      auto count = getTransformCount();
      setTransformCount(count + 1);
      mZpcTransformEdit = false;
    }

    return stage && stage->GetRootLayer()->Export(exportPath);
  }

  bool USDSceneDesc::getIsYUpAxis() const {
    if (!mRootStage.has_value()) return true;  // we use Y as up axis

    PARSE_STAGE(stage);
    pxr::TfToken upAxis;
    if (stage && stage->GetMetadata(pxr::UsdGeomTokens->upAxis, &upAxis)) {
      return upAxis.GetString() == pxr::UsdGeomTokens->y;
    }
    return true;
  }

  bool USDSceneDesc::setIsYUpAxis(bool isYUpAxis) {
    GET_STAGE(stage);
    return stage
           && stage->SetMetadata(pxr::UsdGeomTokens->upAxis, pxr::TfToken(isYUpAxis ? "Y" : "Z"));
  }

  void USDSceneDesc::markZpcTransformDirty() {
    mZpcTransformEdit = true;
  }

  unsigned long long USDSceneDesc::getTransformCount() const {
    PARSE_STAGE(stage);
    const auto& token = pxr::TfToken("ZpcTransformCount");
    unsigned long long transformCount = 0;
    if (stage->GetRootLayer()->HasCustomLayerData()) {
      auto d = stage->GetRootLayer()->GetCustomLayerData();
      if (d.find(token) != d.end()) {
        transformCount = d[token].Get<unsigned long long>();
      }
    }
    return transformCount;
  }
  void USDSceneDesc::setTransformCount(unsigned long long count) {
    GET_STAGE(stage);
    VtDictionary d;
    const auto& token = pxr::TfToken("ZpcTransformCount");
    if (stage->GetRootLayer()->HasCustomLayerData()) {
      d = stage->GetRootLayer()->GetCustomLayerData();
    }
    d[token] = count;
    stage->GetRootLayer()->SetCustomLayerData(d);
  }

  std::string USDSceneDesc::getZpcTransformSuffix() const {
    return "zpc_" + std::to_string(getTransformCount());
  }

  double USDSceneDesc::getStartTimeCode() const {
    if (!mRootStage.has_value()) {
      return 0.0f;
    }

    PARSE_STAGE(stage);
    return stage->GetStartTimeCode();
  }

  double USDSceneDesc::getEndTimeCode() const {
    if (!mRootStage.has_value()) {
      return 0.0f;
    }

    PARSE_STAGE(stage);
    return stage->GetEndTimeCode();
  }

  bool USDSceneDesc::setStartTimeCode(double time) {
    GET_STAGE(stage);
    stage->SetStartTimeCode(time);
    return true;
  }

  bool USDSceneDesc::setEndTimeCode(double time) {
    GET_STAGE(stage);
    stage->SetEndTimeCode(time);
    return true;
  }

  double USDSceneDesc::getTimeCodesPerSecond() const {
    if (!mRootStage.has_value()) {
      return 0.0f;
    }

    PARSE_STAGE(stage);
    return stage->GetTimeCodesPerSecond();
  }

  bool USDSceneDesc::setTimeCodesPerSecond(double timecodesPerSecond) {
    GET_STAGE(stage);
    stage->SetTimeCodesPerSecond(timecodesPerSecond);
    return true;
  }

  double USDSceneDesc::getFPS() const {
    if (!mRootStage.has_value()) {
      return 0.0f;
    }

    PARSE_STAGE(stage);
    return stage->GetFramesPerSecond();
  }

  bool USDSceneDesc::setFPS(double fps) {
    if (!mRootStage.has_value()) {
      return 0.0f;
    }

    GET_STAGE(stage);
    stage->SetFramesPerSecond(fps);
    return true;
  }

  void USDSceneDesc::viewTree() const {
    if (!mRootStage.has_value()) {
      std::cout << "no root stage, skip viewing" << std::endl;
      return;
    }

    auto stage = std::any_cast<UsdStageRefPtr>(mRootStage);
    std::cout << "viewing tree" << std::endl;
    auto range = stage->Traverse();
    for (const auto& prim : range) {
      const char* path = prim.GetPath().GetString().c_str();

      std::cout << prim.GetTypeName() << ' ' << prim.GetPath() << std::endl;
    }
    std::cout << "end viewing tree" << std::endl;
  }

  ScenePrimHolder USDSceneDesc::createPrim(const char* path) {
    GET_STAGE(stage);
    if (stage->GetPrimAtPath(SdfPath(path)).IsValid()) {
      std::cout << "failed to create prim cause given path is not empty: " << path << std::endl;
      return ScenePrimHolder();
    }

    UsdPrim p = stage->DefinePrim(SdfPath(path));
    if (!p.IsValid()) {
      std::cout << "failed to define prim" << std::endl;
      return ScenePrimHolder();
    }

    return ScenePrimHolder(new USDScenePrim(this, p));
  }

  ScenePrimHolder USDSceneDesc::getPrim(const char* path) const {
    if (!mRootStage.has_value()) {
      return ScenePrimHolder();
    }

    auto stage = std::any_cast<UsdStageRefPtr>(mRootStage);
    auto _prim = stage->GetPrimAtPath(SdfPath(path));
    if (!_prim.IsValid()) {
      return ScenePrimHolder();
    }
    return ScenePrimHolder(new USDScenePrim((USDSceneDesc*)this, _prim));
  }

  ScenePrimHolder USDSceneDesc::getRootPrim() const {
    if (!mRootStage.has_value()) {
      return {};
    }

    auto stage = std::any_cast<UsdStageRefPtr>(mRootStage);
    return ScenePrimHolder(new USDScenePrim((USDSceneDesc*)this, stage->GetPseudoRoot()));
  }

  bool USDSceneDesc::removePrim(const char* path) {
    GET_STAGE(stage);
    return stage->RemovePrim(SdfPath(path));
  }

  std::any USDSceneDesc::getRawScene() const { return mRootStage; }
}  // namespace zs

#undef GET_STAGE

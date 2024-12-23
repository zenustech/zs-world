#include "USDScenePrim.hpp"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usdLux/cylinderLight.h"
#include "pxr/usd/usdLux/diskLight.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/geometryLight.h"
#include "pxr/usd/usdLux/rectLight.h"
#include "pxr/usd/usdLux/sphereLight.h"

namespace zs {
#define COMMON_GET(attrName, val, t, DEFAULT_VALUE)                     \
  if (!isValid()) return false;                                         \
  auto light = pxr::UsdLuxLightAPI(std::any_cast<pxr::UsdPrim>(mPrim)); \
  auto attr = light.Get##attrName##Attr();                              \
  if (!attr.IsValid() || !attr.Get(&val, t)) val = DEFAULT_VALUE;

#define COMMON_SET(attrName, val, t)                                    \
  if (!isValid()) return false;                                         \
  auto light = pxr::UsdLuxLightAPI(std::any_cast<pxr::UsdPrim>(mPrim)); \
  auto attr = light.Get##attrName##Attr();                              \
  if (!attr.IsValid()) {                                                \
    attr = light.Create##attrName##Attr();                              \
  }                                                                     \
  return attr.Set(val, t);

  bool USDSceneLight::isValid() const {
    if (!mScene || !mPrim.has_value()) return false;

    auto light = pxr::UsdLuxLightAPI(std::any_cast<pxr::UsdPrim>(mPrim));
    return light.GetIntensityAttr().IsValid();
  }

  bool USDSceneLight::setIntensity(float intensity, double time) {
    COMMON_SET(Intensity, intensity, time);
  }

  float USDSceneLight::getIntensity(double time) const {
    float intensity = 0.0f;
    COMMON_GET(Intensity, intensity, time, 0.0f);
    return intensity;
  }

  bool USDSceneLight::setLightColor(const vec<float, 3>& color, double time) {
    pxr::GfVec3f clr;
    clr.Set(color.data());
    COMMON_SET(Color, clr, time);
  }

  vec<float, 3> USDSceneLight::getLightColor(double time) const {
    pxr::GfVec3f clr;
    COMMON_GET(Color, clr, time, pxr::GfVec3f(0.0f));
    const float* d = clr.data();
    return {d[0], d[1], d[2]};
  }

  bool USDSceneLight::setEnableTemperature(bool enable, double time) {
    COMMON_SET(EnableColorTemperature, enable, time);
  }

  bool USDSceneLight::getEnableTemperature(double time) const {
    bool enable;
    COMMON_GET(EnableColorTemperature, enable, time, false);
    return enable;
  }

  bool USDSceneLight::setTemperature(float temperature, double time) {
    COMMON_SET(ColorTemperature, temperature, time);
  }

  float USDSceneLight::getTemperature(double time) const {
    float temperature;
    COMMON_GET(ColorTemperature, temperature, time, 0.0f);
    return temperature;
  }

#undef COMMON_GET
#undef COMMON_SET
}  // namespace zs

#include "SceneContext.hpp"

#include "../World.hpp"

namespace zs {

  Weak<ZsPrimitive> SceneContext::getPrimitive(std::string_view label) {
    if (auto it = _primitives.find(label); it != _primitives.end()) return (*it).second;
    return {};
  }
  Weak<ZsPrimitive> SceneContext::getPrimitiveByIdRecurse(PrimIndex id_) {
#if 1
    if (auto it = _idToPrim.find(id_); it != _idToPrim.end()) return (*it).second;
    for (auto &entry : _orderedPrims)
      if (auto r = entry.prim.lock()->getChildByIdRecurse(id_); r.lock()) return r;
    return {};
#else
    for (const auto &[label, prim] : _primitives)
      if (prim->id() == id_) return prim;
    for (const auto &[label, prim] : _primitives)
      if (auto ret = prim->getChildByIdRecurse(id_); ret.lock()) return ret;
    return {};
#endif
  }
  Weak<ZsPrimitive> SceneContext::getPrimitiveByPath(const std::vector<std::string> &path) {
    if (!path.size()) return {};
    Shared<ZsPrimitive> prim = getPrimitive(path[0]).lock();
    for (size_t i = 1; i < path.size(); ++i) {
      if (!prim) break;
      prim = prim->getChild(path[i]).lock();
    }
    return prim;
  }
  std::vector<std::string> SceneContext::getPrimitiveLabels() const {
    std::vector<std::string> ret;
    ret.reserve(_primitives.size());
    // for (const auto &[label, prim] : _primitives) ret.push_back(label);
    for (const auto &entry : _orderedPrims) ret.push_back(entry.label);
    return ret;
  }
  std::vector<Weak<ZsPrimitive>> SceneContext::getPrimitives() {
    std::vector<Weak<ZsPrimitive>> ret;
    ret.reserve(_primitives.size());
    // for (const auto &[label, prim] : _primitives) ret.push_back(prim);
    for (const auto &entry : _orderedPrims) ret.push_back(entry.prim);
    return ret;
  }
  bool SceneContext::registerPrimitive(std::string_view label, Shared<ZsPrimitive> prim) {
    // first is iterator
    auto ret = _primitives.emplace(label, prim).second;
    if (ret) {
      TimeCode st, ed;
      if (prim->queryStartEndTimeCodes(st, ed)) {
        auto originalSt = _timeline.getStartTimeCode();
        auto originalEd = _timeline.getEndTimeCode();
        // fmt::print("queried tc interval: [{}, {}], original [{}, {}]\n", st, ed, originalSt,
        //            originalEd);
        if (std::isnan(originalSt) || st < originalSt) setStartTimeCode(st);
        if (std::isnan(originalEd) || ed > originalEd) setEndTimeCode(ed);
        auto originalTc = _timeline.getCurrentTimeCode();
        if (std::isnan(originalTc)) {
          setCurrentTimeCode(st);
        } else if (originalTc < _timeline.getStartTimeCode()) {
          setCurrentTimeCode(_timeline.getStartTimeCode());
        } else if (originalTc > _timeline.getEndTimeCode()) {
          setCurrentTimeCode(_timeline.getEndTimeCode());
        }
      }
      {
        auto sceneMgr = zs_get_scene_manager(zs_get_world());
        // fmt::print("querying usd scene name: {}\n", prim->details().getUsdSceneName());
        auto scene = sceneMgr->getScene(prim->details().getUsdSceneName().data());
        if (scene) {
          fmt::print("found scene.\n");
          auto curFps = scene->getFPS();
          auto curTcps = scene->getTimeCodesPerSecond();
          auto originalFps = _timeline.getFps();
          auto originalTcps = _timeline.getTcps();
          // fmt::print("original fps: {}, new fps: {}\n", originalFps, curFps);
          if (std::isnan(originalFps)) {
            // fmt::print("setting a new fps: {}\n", curFps);
            setFps(curFps);
          }
          // fmt::print("original tcps: {}, new one: {}\n", originalTcps, curTcps);
          if (std::isnan(originalTcps)) {
            fmt::print("setting a new tcps: {}\n", curTcps);
            setTcps(curTcps);
          } else {
            if (curTcps != originalTcps) {
              throw std::runtime_error(fmt::format(
                  "this usd asset is using a different tcps {} from the scene's current tcps {}.",
                  curTcps, originalTcps));
            }
          }
        }
      }
      _orderedPrims.emplace_back(label, prim);
      _idToPrim.emplace(prim->id(), prim);
    }
    return ret;
  }

  ZsPrimitive *SceneContext::getPrimByIndex(int i) noexcept {
    if (i >= 0 && i < _orderedPrims.size()) return _orderedPrims[i].prim.lock().get();
    return nullptr;
  }
  const ZsPrimitive *SceneContext::getPrimByIndex(int i) const noexcept {
    if (i >= 0 && i < _orderedPrims.size()) return _orderedPrims[i].prim.lock().get();
    return nullptr;
  }
  std::string_view SceneContext::getPrimLabelByIndex(int i) const noexcept {
    if (i >= 0 && i < _orderedPrims.size()) return _orderedPrims[i].label;
    return "";
  }

}  // namespace zs
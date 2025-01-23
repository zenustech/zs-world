#include "Primitive.hpp"

// #include <latch>

#include "PrimitiveConversion.hpp"
#include "PrimitiveTransform.hpp"
#include "interface/details/PyHelper.hpp"
#include "world/World.hpp"
#include "world/system/ResourceSystem.hpp"
#include "world/system/ZsExecSystem.hpp"

#if ZS_ENABLE_OPENMP
#  include "zensim/omp/execution/ExecutionPolicy.hpp"
#else
#  include "zensim/execution/ExecutionPolicy.hpp"
#endif

namespace zs {

  PrimitiveId::PrimitiveId() noexcept { _id = zs_resources().next_prim_id(); }
  PrimitiveId::~PrimitiveId() noexcept {
    if (_id != -1) zs_resources().recycle_prim_id(_id);
    _id = -1;
  }
  PrimitiveId::PrimitiveId(PrimitiveId &&o) noexcept { _id = zs::exchange(o._id, -1); }
  PrimitiveId &PrimitiveId::operator=(PrimitiveId &&o) noexcept {
    if (_id != -1) zs_resources().recycle_prim_id(_id);
    _id = zs::exchange(o._id, -1);
    return *this;
  }

  PrimitiveMeta::~PrimitiveMeta() {
    {
      GILGuard guard;
      _metas.clear();
    }
  }

  void PrimitiveBoundingSphere::init(const glm::vec3 &a, const glm::vec3 &b) noexcept {
    _center = (a + b) * 0.5f;
    _radius = (b - a).length() * 0.5f;
  }
  void PrimitiveBoundingSphere::init(const glm::vec3 &a) noexcept {
    _center = a;
    _radius = 0.01f;
  }
  void PrimitiveBoundingSphere::merge(const glm::vec3 &p) noexcept {
    // Ritter Algorithm
    glm::vec3 o2p = p - _center;
    float dist = glm::length(o2p);
    if (dist <= _radius) return;

    _center = _center + o2p * ((dist - _radius) * 0.5f / dist);
    _radius = 0.5f * (_radius + dist);
  }

  void PrimitiveBoundingBox::init(const glm::vec3 &pos) noexcept { minPos = maxPos = pos; }

  void PrimitiveBoundingBox::merge(const glm::vec3 &pos) noexcept {
    for (int i = 0; i < 3; ++i) {
      minPos[i] = min(minPos[i], pos[i]);
      maxPos[i] = max(maxPos[i], pos[i]);
    }
  }

  ///
  /// PrimitiveDetail
  ///
  bool PrimitiveDetail::meshRequireUpdate(TimeCode newTc) const {
    auto &keyframes = this->keyframes();
    auto originalTc = getCurrentTimeCode();
    if (originalTc == newTc || (std::isnan(originalTc) && std::isnan(newTc))) return false;
    bool ret = false;
    bool updatePos = false;

    /// skeleton animation
    if (keyframes.hasSkelAnim()) {
      auto [skinSt, skinEd] = keyframes.getSkelAnimTimeCodeInterval();
      if (std::isnan(originalTc) != std::isnan(newTc)
          || !((originalTc < skinSt && newTc < skinSt)
               || (originalTc > skinEd && newTc > skinEd))) {
        // fmt::print("[nan to tc] comparing origin tc: {} to new tc: {}\n",
        // originalTc, newTc); fmt::print("[skin] comparing origin tc: {} to new
        // tc: {}\n", originalTc, newTc);
        const_cast<PrimitiveDetail *>(this)->setPosDirty();  // setDirty(dirty_Pos);
        updatePos = true;
        ret = true;
      }
    }

    /// other attribs
    auto checkAttribStatus = [&](const auto &keyframe, auto flag) {
      if (keyframe.isTimeDependent()) {
        auto originalSegmentNo = keyframe.getTimeCodeSegmentIndex(originalTc);
        auto newSegmentNo = keyframe.getTimeCodeSegmentIndex(newTc);
        // fmt::print("comparing attrib [{}] origin segment: {} to new segment:
        // {}\n", label,
        //            originalSegmentNo, newSegmentNo);
        if (originalSegmentNo != newSegmentNo) {
          const_cast<PrimitiveDetail *>(this)->setDirty(flag);
          // fmt::print("comparing origin tc: {} to new tc: {}\n", originalTc,
          // newTc);
        }
      }
    };
    const auto &attribKeyFrames = keyframes.refAttribsKeyFrames();
    if (!updatePos)
      if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_POS_LABEL); it != attribKeyFrames.end())
        checkAttribStatus((*it).second, dirty_Pos);
    if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_COLOR_LABEL); it != attribKeyFrames.end())
      checkAttribStatus((*it).second, dirty_Color);
    if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_UV_LABEL); it != attribKeyFrames.end())
      checkAttribStatus((*it).second, dirty_UV);
    if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_NORMAL_LABEL); it != attribKeyFrames.end())
      checkAttribStatus((*it).second, dirty_Normal);
    if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_TANGENT_LABEL); it != attribKeyFrames.end())
      checkAttribStatus((*it).second, dirty_Tangent);

    if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_FACE_INDEX_LABEL);
        it != attribKeyFrames.end())
      checkAttribStatus((*it).second, dirty_Topo);
    if (auto it = attribKeyFrames.find(KEYFRAME_ATTRIB_FACE_LABEL); it != attribKeyFrames.end())
      checkAttribStatus((*it).second, dirty_Topo);

    return ret;
  }
  bool PrimitiveDetail::transformRequireUpdate(TimeCode newTc) const {
    const auto &kfs = this->keyframes().refTransformKeyFrames();
    auto originalTc = getCurrentTransformTimeCode();  // * might differ from mesh tc ftm
    if (originalTc == newTc || (std::isnan(originalTc) && std::isnan(newTc))) return false;
#if 0
    if (_usdPrim && _usdPrim->isTimeVarying()) return true;
#elif 1
    if (refTransformVarying()) return true;
#else
    // fmt::print("checking transform update: timedependent: {}, otc: {}, newtc:
    // {}\n", kfs.isTimeDependent(), originalTc, newTc);
    if ((_usdPrim && _usdPrim->isTimeVarying()) || kfs.isTimeDependent()) {
      //
      auto transformSt = kfs.getSegmentTimeCode(0);
      auto transformEd = kfs.getSegmentTimeCode(kfs.getNumFrames() - 1);
      fmt::print("checking transform update: num frames: {}, tc[{}, {}]\n", kfs.getNumFrames(),
                 transformSt, transformEd);
      if (std::isnan(originalTc) != std::isnan(newTc)) return true;
      if (!((originalTc < transformSt && newTc < transformSt)
            || (originalTc > transformEd && newTc > transformEd)))
        return true;
    }
#endif
    return false;
  }

  glm::mat4 PrimitiveDetail::getTransform(TimeCode tc) const {
#if 1
    if (transformRequireUpdate(tc)) {
      auto m = _usdPrim->getLocalTransform(tc);
      const_cast<PrimitiveDetail *>(this)->_transform = m;
      // const_cast<PrimitiveDetail *>(this)->setTransformDirty();
      const_cast<PrimitiveDetail *>(this)->setCurrentTransformTimeCode(tc);
      // isTransformDirty()
      // const_cast<PrimitiveDetail *>(this)->unsetDirty(mask_Transform);
    }
    return _transform;
#else
    // auto usdPrim = reinterpret_cast<ScenePrimConcept *>(_usdPrim);
    auto sceneMgr = zs_get_scene_manager(zs_get_world());
    auto scene = sceneMgr->getScene(getUsdSceneName().data());
    if (!scene) return _transform;
    auto usdPrim = scene->getPrim(getUsdPrimPath().data());
    if (!usdPrim || std::string(usdPrim->getName()) == "/") return _transform;
    // fmt::print("\ttc: {}, usdPrim name: {}, path: {}\n", tc,
    // usdPrim->getName(), usdPrim->getPath());
    return usdPrim->getLocalTransform(tc);
#endif
  }
  void PrimitiveDetail::setTransform(TimeCode tc, const glm::mat4 &m) {
#if 1
    if (_usdPrim) {
      auto scene = _usdPrim->getScene();
      auto sfx = scene->getZpcTransformSuffix();
      auto originalZpcTransform = _usdPrim->getLocalMatrixOpValue(tc, sfx);
      auto originalTransform = glm::inverse(originalZpcTransform) * getTransform(tc);
    }
    _transform = m;
    return;
#else
    // auto usdPrim = reinterpret_cast<ScenePrimConcept *>(_usdPrim);
    auto sceneMgr = zs_get_scene_manager(zs_get_world());
    auto scene = sceneMgr->getScene(getUsdSceneName().data());
    if (!scene) {
      // should emplace in the transform keyframes!
      _transform = m;
      return;
    }
    auto usdPrim = scene->getPrim(getUsdPrimPath().data());
    if (!usdPrim || std::string(usdPrim->getName()) == "/") {
      // should emplace in the transform keyframes!
      _transform = m;
      return;
    }
    auto sfx = scene->getZpcTransformSuffix();
    auto originalZpcTransform = usdPrim->getLocalMatrixOpValue(tc, sfx);
    auto originalTransform = glm::inverse(originalZpcTransform) * getTransform(tc);
    usdPrim->setLocalMatrixOpValue(m * glm::inverse(originalTransform), tc, sfx);
#endif
  }
  glm::mat4 PrimitiveDetail::toNativeCoordTransform() const {
    static glm::mat4 eye(1.f);
    switch (_assetOrigin) {
      case asset_origin_e::usd: {
        if (_isYUpAxis) {
          return eye;
        } else {  // Z up axis
          auto C = glm::mat4();
          C[0][0] = 1.f;
          C[2][1] = 1.f;
          C[1][2] = -1.f;
          C[3][3] = 1.f;
          return C;
        }
      }
      case asset_origin_e::maya:
      case asset_origin_e::blender:
      case asset_origin_e::ue:
      case asset_origin_e::unity:

      case asset_origin_e::native:
      case asset_origin_e::houdini:
      default:
        return eye;
    }
  }

  void PrimitiveDetail::setUsdPrim(const ScenePrimConcept *prim) {
    _usdSceneName = prim->getScene()->getName();
    _usdPrimPath = prim->getPath();

    auto sceneMgr = zs_get_scene_manager(zs_get_world());
    auto scene = sceneMgr->getScene(getUsdSceneName().data());
    assert(scene);
    _usdPrim = zs::move(scene->getPrim(getUsdPrimPath().data()));
  }
  void PrimitiveDetail::initLocalBoundingBox() noexcept {
    auto &triMesh = this->triMesh();
    auto &aabb = localBoundingBox();

    aabb.init(glm::vec3(triMesh.nodes[0][0], triMesh.nodes[0][1], triMesh.nodes[0][2]));
    for (const auto &pos : triMesh.nodes) {
      aabb.merge(glm::vec3(pos[0], pos[1], pos[2]));
    }
  }
  void PrimitiveDetail::updateWorldBoundingBox(const glm::mat4 &worldMat) noexcept {
    const auto &aabb = localBoundingBox();
    auto &out = worldBoundingBox();
    glm::vec3 dirx = worldMat * glm::vec4(aabb.minPos.x - aabb.maxPos.x, 0.0f, 0.0f, 0.0f);
    glm::vec3 diry = worldMat * glm::vec4(0.0f, aabb.minPos.y - aabb.maxPos.y, 0.0f, 0.0f);
    glm::vec3 dirz = worldMat * glm::vec4(0.0f, 0.0f, aabb.minPos.z - aabb.maxPos.z, 0.0f);
    glm::vec3 newMax = worldMat * glm::vec4(aabb.maxPos, 1.0f);
    out.init(newMax);
    for (int i = 0; i <= 1; ++i) {
      for (int j = 0; j <= 1; ++j) {
        for (int k = 0; k <= 1; ++k) {
          out.merge(newMax + dirx * float(i) + diry * float(j) + dirz * float(k));
        }
      }
    }
  }
  std::string_view PrimitiveDetail::getUsdSceneName() const noexcept { return _usdSceneName; }
  std::string_view PrimitiveDetail::getUsdPrimPath() const noexcept { return _usdPrimPath; }

  bool PrimitiveDetail::isDirty(DirtyFlag f) const noexcept { return _dirtyFlag & f; }
  void PrimitiveDetail::clearDirtyFlag() noexcept {
    // zs::atomic_exch(exec_omp, &_dirtyFlag, (DirtyFlag)0);
    _dirtyFlag = 0;
  }
  void PrimitiveDetail::setDirty(DirtyFlag f) noexcept {
    // zs::atomic_or(exec_omp, &_dirtyFlag, f);
    _dirtyFlag |= f;
  }
  void PrimitiveDetail::unsetDirty(DirtyFlag f) noexcept {
    // zs::atomic_and(exec_omp, &_dirtyFlag, ~f);
    _dirtyFlag &= ~f;
  }

  bool PrimitiveDetail::isTopoDirty() const noexcept { return _dirtyFlag & mask_Topo; }
  bool PrimitiveDetail::isShapeDirty() const noexcept { return _dirtyFlag & mask_Shape; }
  bool PrimitiveDetail::isAttribDirty() const noexcept { return _dirtyFlag & mask_Attrib; }
  bool PrimitiveDetail::isTransformDirty() const noexcept { return _dirtyFlag & mask_Transform; }
  bool PrimitiveDetail::isTimeCodeDirty() const noexcept { return _dirtyFlag & dirty_TimeCode; }

  void PrimitiveDetail::clearTopoDirtyFlag() noexcept { unsetDirty(mask_Topo); }
  void PrimitiveDetail::clearShapeDirtyFlag() noexcept { unsetDirty(mask_Shape); }
  void PrimitiveDetail::clearAttribDirtyFlag() noexcept { unsetDirty(mask_Attrib); }
  void PrimitiveDetail::clearTransformDirtyFlag() noexcept { unsetDirty(mask_Transform); }
  void PrimitiveDetail::clearTimeCodeDirtyFlag() noexcept { unsetDirty(dirty_TimeCode); }

  ///
  /// PrimitiveStorage
  ///
  bool PrimitiveStorage::applySkinning(TimeCode tc) {
#if ZS_ENABLE_USD
    auto sceneMgr = zs_get_scene_manager(zs_get_world());
    auto scene = sceneMgr->getScene(details().getUsdSceneName().data());
    if (!scene) return false;
    auto prim = scene->getPrim(details().getUsdPrimPath().data());
    return apply_usd_skinning(points(), prim.get(), tc);
#else
    return false;
#endif
  }

  void PrimitiveStorage::updatePrimFromKeyFrames(TimeCode tc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    std::vector<PropertyTag> ptProps{{ATTRIB_POS_TAG, 3}}, vtProps{{POINT_ID_TAG, 1}},
        polyProps{{POLY_SIZE_TAG, 1}, {POLY_OFFSET_TAG, 1}};
    AttrVector &points = _points;
    AttrVector &verts = _verts;
    AttrVector &polys = localPolyPrims()->prims();
    // necessary properties
    points.appendProperties32(pol, ptProps);
    verts.appendProperties32(pol, vtProps);
    polys.appendProperties32(pol, polyProps);

    /// position
    auto &keyframes = details().keyframes();
    const auto &srcPos
        = keyframes.getAttribKeyFrame(KEYFRAME_ATTRIB_POS_LABEL, tc).lock()->attr32();
    points.resize(srcPos.size());
    auto label
        = srcPos.hasProperty(ATTRIB_SKINNING_POS_TAG) ? ATTRIB_SKINNING_POS_TAG : ATTRIB_POS_TAG;
    pol(zip(range(points.attr32(), ATTRIB_POS_TAG, dim_c<3>), range(srcPos, label, dim_c<3>)),
        [](auto &dst, auto src) { dst = src; });
    /// verts
    const auto &srcVerts
        = keyframes.getAttribKeyFrame(KEYFRAME_ATTRIB_FACE_INDEX_LABEL, tc).lock()->attr32();
    verts.resize(srcVerts.size());
    pol(zip(range(verts.attr32(), POINT_ID_TAG, dim_c<1>, prim_id_c),
            range(srcVerts, POINT_ID_TAG, dim_c<1>, prim_id_c)),
        [](auto &dst, auto src) { dst = src; });
    /// poly
    auto &polyKeyframe
        = keyframes.getAttribKeyFrame(KEYFRAME_ATTRIB_FACE_LABEL, tc).lock()->attr32();
    polys.resize(polyKeyframe.size());
    pol(range(polys.size()), [dstPolys = view<space>({}, polys.attr32()),
                              srcPolys = view<space>({}, polyKeyframe)](PrimIndex polyId) mutable {
      dstPolys(POLY_SIZE_TAG, polyId, prim_id_c) = srcPolys(POLY_SIZE_TAG, polyId, prim_id_c);
      dstPolys(POLY_OFFSET_TAG, polyId, prim_id_c) = srcPolys(POLY_OFFSET_TAG, polyId, prim_id_c);
    });
    /// attribs
    auto processAttrib
        = [&](const SmallString &attribTag, auto attribDimC, const AttrVector &attribKeyFrame) {
            auto copyAttrib = [&](AttrVector &primAttrib) {
              primAttrib.appendProperties32(pol, {{attribTag, attribDimC}});
              pol(zip(range(primAttrib.attr32(), attribTag, dim_c<attribDimC>),
                      range(attribKeyFrame.attr32(), attribTag, dim_c<attribDimC>)),
                  [](auto dst, auto src) { dst = src; });
            };
            switch (attribKeyFrame._owner) {
              case prim_attrib_owner_e::point:
                copyAttrib(points);
                break;
              case prim_attrib_owner_e::vert: {
                copyAttrib(verts);
                break;
              }
              case prim_attrib_owner_e::face: {
                copyAttrib(polys);
                break;
              }
              default:
                assert(0 && "attribute processed here should not owned by ones other "
                  "than point/vert/face!");
                break;
            };
          };

    if (keyframes.hasAttrib(KEYFRAME_ATTRIB_UV_LABEL))
      processAttrib(ATTRIB_UV_TAG, wrapv<2>{},
                    *keyframes.getAttribKeyFrame(KEYFRAME_ATTRIB_UV_LABEL, tc).lock());
    if (keyframes.hasAttrib(KEYFRAME_ATTRIB_NORMAL_LABEL))
      processAttrib(ATTRIB_NORMAL_TAG, wrapv<3>{},
                    *keyframes.getAttribKeyFrame(KEYFRAME_ATTRIB_NORMAL_LABEL, tc).lock());
    if (keyframes.hasAttrib(KEYFRAME_ATTRIB_COLOR_LABEL))
      processAttrib(ATTRIB_COLOR_TAG, wrapv<3>{},
                    *keyframes.getAttribKeyFrame(KEYFRAME_ATTRIB_COLOR_LABEL, tc).lock());
  }

  /// @note general mesh -> simple mesh -> visual mesh -> zs (vk) mesh

  Shared<ZsPrimitive> &PrimitiveStorage::visualMesh() {
    auto &ret = _details.visualMesh();
    if (!ret) {
      ret = std::make_shared<ZsPrimitive>();
      if (isSimpleMeshEstablished())
        update_simple_mesh_from_poly_mesh(*this);
      else
        setup_simple_mesh_for_poly_mesh(*this);
      assign_simple_mesh_to_visual_mesh(*this, *ret);
    }
    return ret;
  }

  ZsTriMesh &PrimitiveStorage::triMesh() {
    auto &visMesh = visualMesh();
    auto &triMesh = _details.triMesh();
    assign_visual_mesh_to_trimesh(*visMesh, triMesh);
    return triMesh;
  }

  ///
  /// ZsPrimitive
  ///

  bool ZsPrimitive::queryStartEndTimeCodes(TimeCode &start, TimeCode &end) const noexcept {
    const auto &keyframes = this->keyframes();
    TimeCode st, ed, chSt, chEd;
    auto valid = keyframes.queryStartEndTimeCodes(st, ed);
    for (auto &ch : _childs) {
      TimeCode chSt, chEd;
      if (ch->queryStartEndTimeCodes(chSt, chEd)) {
        if (std::isnan(st) || chSt < st) {
          st = chSt;
          valid = true;
        }
        if (std::isnan(ed) || chEd > ed) {
          ed = chEd;
          valid = true;
        }
      }
    }
    if (valid) {
      start = st;
      end = ed;
    }
    return valid;
  }
  void ZsPrimitive::appendChildPrimitve(ZsPrimitive *prim) {
    prim->_parent = this;
    _childs.emplace_back(prim);
  }

  VkModel &ZsPrimitive::vkTriMesh(VulkanContext &ctx) {
    auto &vkTriMesh = _details.vkTriMesh();
    if (vkTriMesh.pCtx() != &ctx) {
      ZsTriMesh &triMesh = this->triMesh();
      vkTriMesh = VkModel(ctx, triMesh, zs_transform());
    }
    return vkTriMesh;
  }

  // asynchrounous
  zs::Future<Shared<ZsPrimitive>> ZsPrimitive::visualMeshAsync(TimeCode tc) {
    Shared<ZsPrimitive> ret = std::make_shared<ZsPrimitive>();
    updatePrimFromKeyFrames(tc);
    if (keyframes().hasSkelAnim()) applySkinning(tc);

    setup_simple_mesh_for_poly_mesh(*this);
    assign_simple_mesh_to_visual_mesh(*this, *ret);
    co_return ret;
  }
  zs::Future<void> ZsPrimitive::zsMeshAsync(TimeCode tc) {
    updatePrimFromKeyFrames(tc);
    if (keyframes().hasSkelAnim()) applySkinning(tc);

    setup_simple_mesh_for_poly_mesh(*this);
    assign_simple_mesh_to_zsmesh(*this, &details().triMesh(), &details().lineMesh(),
                                 &details().pointMesh());
    co_return;
  }

  zs::Future<void> ZsPrimitive::vkTriMeshAsync(VulkanContext &ctx, TimeCode tc) {
    zs_resources().inc_inflight_prim_cnt();

    co_await schedule_on(ZS_TASK_SCHEDULER(), zsMeshAsync(tc));
    auto &triMesh = details().triMesh();

    /// @brief acceleration structure maintenance (including total box)
    if (details().isShapeDirty()) {
#if ZS_ENABLE_OPENMP
      auto pol = omp_exec();
#else
      auto pol = seq_exec();
#endif
      Vector<AABBBox<3, f32>> bvs{triMesh.elems.size()};
      pol(enumerate(bvs), [&triMesh](int ei, auto &bv) {
        auto tri = triMesh.elems[ei];
        const auto &poses = triMesh.nodes;
        auto mi = zs::vec<f32, 3>{
            zs::min(zs::min(poses[tri[0]][0], poses[tri[1]][0]), poses[tri[2]][0]),
            zs::min(zs::min(poses[tri[0]][1], poses[tri[1]][1]), poses[tri[2]][1]),
            zs::min(zs::min(poses[tri[0]][2], poses[tri[1]][2]), poses[tri[2]][2])};
        auto ma = zs::vec<f32, 3>{
            zs::max(zs::max(poses[tri[0]][0], poses[tri[1]][0]), poses[tri[2]][0]),
            zs::max(zs::max(poses[tri[0]][1], poses[tri[1]][1]), poses[tri[2]][1]),
            zs::max(zs::max(poses[tri[0]][2], poses[tri[1]][2]), poses[tri[2]][2])};
        bv = AABBBox<3, f32>{mi, ma};
      });
      auto &bvh = details().triBvh();
      if (details().isTopoDirty()) {
        bvh.buildRefit(pol, bvs);
      } else {  // isDirty(dirty_Pos) == true
        bvh.refit(pol, bvs);
      }
      auto rootBv = bvh.getTotalBox(pol);
      details().localBoundingBox()
          = PrimitiveBoundingBox{glm::vec3{rootBv._min[0], rootBv._min[1], rootBv._min[2]},
                                 glm::vec3{rootBv._max[0], rootBv._max[1], rootBv._max[2]}};
    }

    VkModel &ret = details().vkTriMesh();
    // for batch processing later
    zs_execution().refVkCmdTaskQueue().enqueue([&ret, &ctx, &triMesh, tc,
                                                this](vk::CommandBuffer cmd) {
      if (details().isTopoDirty()) {
        ret.parseFromMesh(ctx, cmd, triMesh);

        /// processed at the end of each iteration
        details().clearTopoDirtyFlag();
        details().clearAttribDirtyFlag();
      } else if (details().isAttribDirty()) {
        ret.updateAttribsFromMesh(
            cmd, triMesh, details().isDirty(details().dirty_Pos),
            details().isDirty(details().dirty_Color), details().isDirty(details().dirty_UV),
            details().isDirty(details().dirty_Normal), details().isDirty(details().dirty_Tangent));
        if (details().isDirty(details().dirty_TextureId)) {
          ret.updatePointTextureId(cmd, reinterpret_cast<const int *>(triMesh.texids.data()),
                                   triMesh.texids.size() * 2 * sizeof(int));
        }

        /// processed at the end of each iteration
        details().clearAttribDirtyFlag();
      }

      // only update timecode after all other prim states are done updated
      if (details().isTimeCodeDirty()) {
        details().setCurrentTimeCode(tc);
        details().unsetTimeCodeDirty();
      }

      details().refProcessingFlag() = 0;  // done processing
    });
    /// @brief eventually triggers the above resources' batch-processing
    zs_resources().dec_inflight_prim_cnt();

    co_await ZS_EVENT_SCHEDULER().schedule();
    co_return;
  }

  ZsPrimitive *ZsPrimitive::queryVisualMesh(TimeCode tc) {
    if (_visualMeshAsync.isDone()) {
      try {
        return _visualMeshAsync.ref().get();
      } catch (std::runtime_error er) {
        // fmt::print("query visual mesh failed.\n");
        return nullptr;
      }
    } else if (!_visualMeshAsync.getHandle()) {  // isReady()
      _visualMeshAsync = visualMeshAsync(tc);
      _visualMeshAsync.resume();
    }
    return nullptr;
  }
  ZsTriMesh *ZsPrimitive::queryTriMesh(TimeCode tc) {
    if (_zsMeshAsync.isDone()) {
      try {
        return &details().triMesh();
      } catch (std::runtime_error er) {
        // fmt::print("query visual mesh failed.\n");
        return nullptr;
      }
    } else if (!_zsMeshAsync.getHandle()) {  // isReady()
      _zsMeshAsync = zsMeshAsync(tc);
      _zsMeshAsync.resume();
    }
    return nullptr;
  }
  VkModel *ZsPrimitive::queryVkTriMesh(VulkanContext &ctx, TimeCode tc) {
#if 0
    if (_vkTriMeshAsync.isDone()) {
      try {
        if (details().meshRequireUpdate(tc)) {
          _vkTriMeshAsync = vkTriMeshAsync(ctx, tc);
          _vkTriMeshAsync.resume();
          return nullptr;
        } else
          return &details().vkTriMesh();
      } catch (const std::exception &e) {
        fmt::print("query vk tri mesh failed. [{}]\n", e.what());
        return nullptr;
      }
    } else if (!_vkTriMeshAsync.getHandle()) {  // isReady()
      _vkTriMeshAsync = vkTriMeshAsync(ctx, tc);
      _vkTriMeshAsync.resume();
    }
    return nullptr;
#else
    // if (_vkTriMeshAsync.isReady()) {
    if (details().refProcessingFlag() == 0) {
      try {
        bool tcNeedUpd = details().meshRequireUpdate(tc);
        if (!_vkTriMeshAsync.getHandle() || tcNeedUpd) {
          details().refProcessingFlag() = 1;
          if (tcNeedUpd) details().setTimeCodeDirty();
          _vkTriMeshAsync = vkTriMeshAsync(ctx, tc);
          _vkTriMeshAsync.resume();
        }
      } catch (const std::exception &e) {
        fmt::print("query vk tri mesh failed. [{}]\n", e.what());
        // return nullptr;
      }
    }
    return &details().vkTriMesh();
#endif
  }

}  // namespace zs
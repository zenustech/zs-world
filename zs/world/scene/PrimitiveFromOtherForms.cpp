#include "Primitive.hpp"
#include "PrimitiveConversion.hpp"
#include "Timeline.hpp"
#include "world/system/ResourceSystem.hpp"
#if ZS_ENABLE_USD
#  include "pxr/pxr.h"
#  include "pxr/usd/sdf/copyUtils.h"
#  include "pxr/usd/usd/attribute.h"
#  include "pxr/usd/usd/prim.h"
#  include "pxr/usd/usd/primRange.h"
#  include "pxr/usd/usd/relationship.h"
#  include "pxr/usd/usd/stage.h"
#  include "pxr/usd/usdGeom/camera.h"
#  include "pxr/usd/usdGeom/capsule.h"
#  include "pxr/usd/usdGeom/cone.h"
#  include "pxr/usd/usdGeom/cylinder.h"
#  include "pxr/usd/usdGeom/mesh.h"
#  include "pxr/usd/usdGeom/plane.h"
#  include "pxr/usd/usdGeom/primvarsAPI.h"
#  include "pxr/usd/usdGeom/sphere.h"
#  include "pxr/usd/usdGeom/visibilityAPI.h"
#  include "pxr/usd/usdGeom/xform.h"
#  include "pxr/usd/usdGeom/xformCommonAPI.h"
#  include "pxr/usd/usdGeom/xformOp.h"
#  include "pxr/usd/usdGeom/xformable.h"
#  include "pxr/usd/usdShade/material.h"
#  include "pxr/usd/usdShade/materialBindingAPI.h"
#  include "pxr/usd/usdShade/shader.h"
#  include "pxr/usd/usdShade/udimUtils.h"
#  include "pxr/usd/usdSkel/animQuery.h"
#  include "pxr/usd/usdSkel/animation.h"
#  include "pxr/usd/usdSkel/bindingAPI.h"
#  include "pxr/usd/usdSkel/blendShapeQuery.h"
#  include "pxr/usd/usdSkel/cache.h"
#  include "pxr/usd/usdSkel/root.h"
#  include "pxr/usd/usdSkel/skeleton.h"
#  include "pxr/usd/usdSkel/skeletonQuery.h"
#  include "pxr/usd/usdSkel/skinningQuery.h"
#  include "pxr/usd/usdSkel/utils.h"
// lighting
#  include <pxr/usd/usdLux/cylinderLight.h>
#  include <pxr/usd/usdLux/diskLight.h>
#  include <pxr/usd/usdLux/distantLight.h>
#  include <pxr/usd/usdLux/domeLight.h>
#  include <pxr/usd/usdLux/geometryLight.h>
#  include <pxr/usd/usdLux/rectLight.h>
#  include <pxr/usd/usdLux/sphereLight.h>
#endif
#include "zensim/geometry/Mesh.hpp"
#include "zensim/types/Property.h"

#if ZS_ENABLE_OPENMP
#  include "zensim/omp/execution/ExecutionPolicy.hpp"
#else
#  include "zensim/execution/ExecutionPolicy.hpp"
#endif

namespace zs {

  void assign_trimesh_to_primitive(const Mesh<f32, 3, u32, 3>& triMesh, ZsPrimitive& geom,
                                   const source_location& loc) {
    geom.reset();
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    // copy points, verts
    const auto numPts = triMesh.nodes.size();
    std::vector<PropertyTag> ptProps{{ATTRIB_POS_TAG, 3}}, vtProps{{POINT_ID_TAG, 1}};
    // color is mandated
    if (triMesh.colors.size()) {
      assert(triMesh.colors.size() == numPts);
    }
    ptProps.push_back(PropertyTag{ATTRIB_COLOR_TAG, 3});

    std::vector<std::array<float, 3>> nrmVals;
    if (triMesh.norms.size()) {
      assert(triMesh.norms.size() == numPts);
    } else {
      nrmVals.resize(numPts);
      compute_mesh_normal(triMesh, 1.f, nrmVals);
    }
    ptProps.push_back(PropertyTag{ATTRIB_NORMAL_TAG, 3});

    if (triMesh.uvs.size()) {
      assert(triMesh.uvs.size() == numPts);
      ptProps.push_back(PropertyTag{ATTRIB_UV_TAG, 2});
    }
    geom.points().appendProperties32(pol, ptProps, loc);
    geom.verts().appendProperties32(pol, vtProps, loc);
    geom.points().resize(numPts);
    geom.verts().resize(numPts);

    pol(enumerate(range(geom.verts().attr32(), POINT_ID_TAG, dim_c<1>, prim_id_c)),
        [&triMesh, &nrmVals, pts = view<space>({}, geom.points().attr32())](
            u32 id, PrimIndex& pid) mutable {
          pid = id;

          for (int d = 0; d < 3; ++d) pts(ATTRIB_POS_TAG, d, id) = triMesh.nodes[id][d];
          if (triMesh.colors.size()) {
            for (int d = 0; d < 3; ++d) pts(ATTRIB_COLOR_TAG, d, id) = triMesh.colors[id][d];
          } else
            pts.tuple(dim_c<3>, ATTRIB_COLOR_TAG, id) = zs::vec<float, 3>{0.7f, 0.7f, 0.7f};

          if (triMesh.norms.size()) {
            for (int d = 0; d < 3; ++d) pts(ATTRIB_NORMAL_TAG, d, id) = triMesh.norms[id][d];
          } else {
            const auto& val = nrmVals[id];
            pts.tuple(dim_c<3>, ATTRIB_NORMAL_TAG, id) = zs::vec<float, 3>{val[0], val[1], val[2]};
          }

          if (triMesh.uvs.size())
            for (int d = 0; d < 2; ++d) pts(ATTRIB_UV_TAG, d, id) = triMesh.uvs[id][d];
        });

    // copy tris (local prim), global prim
    const auto numTris = triMesh.elems.size();
    TriPrimContainer& geomTris = *geom.localTriPrims();
    geomTris.prims().appendProperties32(pol, {{ELEM_VERT_ID_TAG, 3}}, loc);
    geomTris.resize(numTris);
    auto& geomPrims = geom.globalPrims();
    geomPrims.resize(numTris);

    pol(enumerate(range(geomTris.prims().attr32(), ELEM_VERT_ID_TAG, dim_c<3>, prim_id_c),
                  geomPrims),
        [&triMesh, &geomPrims](PrimIndex eid, auto& ids,
                               zs::tuple<PrimTypeIndex, PrimIndex>& entryNo) mutable {
          const auto& tri = triMesh.elems[eid];
          ids[0] = tri[0];
          ids[1] = tri[1];
          ids[2] = tri[2];
          zs::get<0>(entryNo) = PrimitiveStorage::Tri_;
          zs::get<1>(entryNo) = eid;
        });
  }
  ZsPrimitive trimesh_to_primitive(const Mesh<f32, 3, u32, 3>& triMesh,
                                   const source_location& loc) {
    ZsPrimitive geom{};
    assign_trimesh_to_primitive(triMesh, geom, loc);
    return geom;
  }

  ZsPrimitive* build_primitive_from_trimesh(const Mesh<f32, 3, u32, 3>& triMesh,
                                            const source_location& loc) {
    ZsPrimitive* ret = new ZsPrimitive();

#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    /// load all keyframes
    auto& keyframes = (*ret).keyframes();
    const auto defaultTimeCode = g_default_timecode();

    const auto numPts = triMesh.nodes.size();

    auto initKeyframsAttrib
        = [&](PropertyTag prop, std::string_view keyframeLabel, const auto& triMeshAttrib) {
            AttrVector attrib;
            attrib.appendProperties32(pol, {prop}, loc);
            attrib.resize(triMeshAttrib.size());
            attrib._owner = prim_attrib_owner_e::point;
            // assign poses to posAttrib
            pol(enumerate(triMeshAttrib),
                [attribView = view<space>(attrib.attr32()), nchns = prop.numChannels,
                 chnOffset = attrib.getPropertyOffset(prop.name)](PrimIndex i,
                                                                  const auto& triAttrib) mutable {
                  for (int d = 0; d < nchns; ++d) attribView(chnOffset + d, i) = triAttrib[d];
                });
            keyframes.emplaceAttribDefault(std::string{keyframeLabel}, zs::move(attrib));
          };

    initKeyframsAttrib({ATTRIB_POS_TAG, 3}, KEYFRAME_ATTRIB_POS_LABEL, triMesh.nodes);

    if (triMesh.colors.size() == numPts)
      initKeyframsAttrib({ATTRIB_COLOR_TAG, 3}, KEYFRAME_ATTRIB_COLOR_LABEL, triMesh.colors);
    else {
      std::vector<std::array<float, 3>> clrVals(numPts, {0.7f, 0.7f, 0.7f});
      initKeyframsAttrib({ATTRIB_COLOR_TAG, 3}, KEYFRAME_ATTRIB_COLOR_LABEL, clrVals);
    }

    if (triMesh.norms.size() == numPts)
      initKeyframsAttrib({ATTRIB_NORMAL_TAG, 3}, KEYFRAME_ATTRIB_NORMAL_LABEL, triMesh.norms);

    if (triMesh.uvs.size() == numPts)
      initKeyframsAttrib({ATTRIB_UV_TAG, 2}, KEYFRAME_ATTRIB_UV_LABEL, triMesh.uvs);

    /// topo
    {
      const auto numTris = triMesh.elems.size();
      const auto numVerts = numTris * 3;
      AttrVector vertAttrib, faceAttrib;
      vertAttrib.appendProperties32(pol, {{POINT_ID_TAG, 1}}, loc);
      vertAttrib.resize(numVerts);
      vertAttrib._owner = prim_attrib_owner_e::vert;
      pol(enumerate(triMesh.elems), [vertAttrib = view<space>(vertAttrib.attr32()),
                                     pidChnOffset = vertAttrib.getPropertyOffset(POINT_ID_TAG)](
                                        PrimIndex i, const auto& tri) mutable {
        vertAttrib(pidChnOffset, i * 3 + 0, prim_id_c) = tri[0];
        vertAttrib(pidChnOffset, i * 3 + 1, prim_id_c) = tri[1];
        vertAttrib(pidChnOffset, i * 3 + 2, prim_id_c) = tri[2];
      });
      faceAttrib.appendProperties32(pol, {{POLY_SIZE_TAG, 1}, {POLY_OFFSET_TAG, 1}}, loc);
      faceAttrib.resize(numTris);
      faceAttrib._owner = prim_attrib_owner_e::face;
      pol(enumerate(range(faceAttrib.attr32(), POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                    range(faceAttrib.attr32(), POLY_OFFSET_TAG, dim_c<1>, prim_id_c)),
          [&](PrimIndex triI, PrimIndex& polySize, PrimIndex& polyOffset) {
            polySize = 3;
            polyOffset = 3 * triI;
          });
      keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_FACE_LABEL, zs::move(faceAttrib));
      keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_FACE_INDEX_LABEL, zs::move(vertAttrib));

      auto& geomPrims = ret->globalPrims();
      geomPrims.resize(numTris);
      pol(enumerate(geomPrims),
          [&triMesh, &geomPrims](PrimIndex eid,
                                 zs::tuple<PrimTypeIndex, PrimIndex>& entryNo) mutable {
            zs::get<0>(entryNo) = PrimitiveStorage::Tri_;
            zs::get<1>(entryNo) = eid;
          });
    }
    ret->updatePrimFromKeyFrames(g_default_timecode());

    return ret;
  }

#if ZS_ENABLE_USD
  /// usd prim
  static void _applySkinning(pxr::VtArray<pxr::GfVec3f>& points,
                             pxr::VtArray<pxr::GfVec3f>& normals /* TODO */,
                             const pxr::UsdGeomMesh& mesh, double time);

  bool usdprim_face_order_is_left_handed(const ScenePrimConcept* scenePrim,
                                         const source_location& loc) {
    if (!scenePrim) return false;
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      // bool isDoubleSided;
      // usdMesh.GetDoubleSidedAttr().Get(&isDoubleSided);  // TODO: handle double sided meshes

      // decide whether we use left handed order to construct faces
      pxr::TfToken faceOrder;
      usdMesh.GetOrientationAttr().Get(&faceOrder);
      return (faceOrder.GetString() == "leftHanded");
    } catch (const std::exception& e) {
      fmt::print("usdprim_face_order_is_left_handed: {}\n", e.what());
      return false;
    }
  }

  bool retrieve_usdprim_attrib_position(const ScenePrimConcept* scenePrim, double time,
                                        int* numPoints, glm::vec3* pos,
                                        const source_location& loc) {
    if (!scenePrim) return false;

// #  if ZS_ENABLE_OPENMP
#  if 0
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      pxr::VtArray<pxr::GfVec3f> usdPoses, /*empty*/ usdNrms;
      usdMesh.GetPointsAttr().Get(&usdPoses, time);
      const auto numPts = usdPoses.size();

      if (numPoints) *numPoints = numPts;
      if (pos == nullptr) return numPoints != nullptr;

      if (numPts == 0) return false;

      // skinning calculation including blendShape
      // _applySkinning(usdPoses, usdNrms, usdMesh, time);

      pol(enumerate(range(pos, pos + numPts)), [&usdPoses](PrimIndex pid, glm::vec3& p) mutable {
        for (int d = 0; d < 3; ++d) p[d] = usdPoses[pid][d];
      });
    } catch (const std::exception& e) {
      fmt::print("retrieve_usdprim_attrib_position: {}\n", e.what());
      return false;
    }
    return true;
  }

  bool retrieve_usdprim_attrib_face(const ScenePrimConcept* scenePrim, double time, int* numIndices,
                                    int* numFaces, int* indices, int* faceSizes,
                                    const source_location& loc) {
    if (!scenePrim) return false;

// #  if ZS_ENABLE_OPENMP
#  if 0
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      // bool isDoubleSided;
      // usdMesh.GetDoubleSidedAttr().Get(&isDoubleSided);  // TODO: handle double sided meshes

      // decide whether we use left handed order to construct faces
      pxr::TfToken faceOrder;
      usdMesh.GetOrientationAttr().Get(&faceOrder, time);
      bool isReversedFaceOrder = (faceOrder.GetString() == "leftHanded");

      pxr::VtArray<PrimIndex> usdFaceSizes;  // numbers of vertices for each face
      pxr::VtArray<PrimIndex> usdVerts;
      usdMesh.GetFaceVertexCountsAttr().Get(&usdFaceSizes, time);
      usdMesh.GetFaceVertexIndicesAttr().Get(&usdVerts, time);

      {
        if (numIndices) *numIndices = usdVerts.size();
        if (numFaces) *numFaces = usdFaceSizes.size();
      }
      if (!(indices && faceSizes)) return numIndices || numFaces;

      if (usdFaceSizes.size() == 0 || usdVerts.size() == 0) return false;

      pol(enumerate(range(indices, indices + usdVerts.size())),
          [&usdVerts](PrimIndex vid, int& pid) mutable { pid = usdVerts[vid]; });

      pol(enumerate(range(faceSizes, faceSizes + usdFaceSizes.size())),
          [&usdFaceSizes](PrimIndex fid, int& faceSize) mutable { faceSize = usdFaceSizes[fid]; });

    } catch (const std::exception& e) {
      fmt::print("retrieve_usdprim_attrib_face: {}\n", e.what());
      return false;
    }
    return true;
  }

  bool usdprim_has_attrib_uv(const ScenePrimConcept* scenePrim, const source_location& loc) {
    if (!scenePrim) return false;
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);
      pxr::UsdGeomPrimvarsAPI primvarAPI(usdMesh);
      auto stPrimvar = primvarAPI.GetPrimvar(pxr::TfToken("st"));
      return stPrimvar.HasValue();
    } catch (const std::exception& e) {
      fmt::print("usdprim_has_attrib_uv issue: {}", e.what());
      return false;
    }
  }
  bool usdprim_has_attrib_normal(const ScenePrimConcept* scenePrim, const source_location& loc) {
    if (!scenePrim) return false;
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);
      auto usdNormalAttrib = usdMesh.GetNormalsAttr();
      return usdNormalAttrib.HasValue();
    } catch (const std::exception& e) {
      fmt::print("usdprim_has_attrib_normal issue: {}", e.what());
      return false;
    }
  }
  bool usdprim_has_attrib_color(const ScenePrimConcept* scenePrim, const source_location& loc) {
    if (!scenePrim) return false;
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);
      auto usdColorAttrib = usdMesh.GetDisplayColorAttr();
      return usdColorAttrib.HasValue();
    } catch (const std::exception& e) {
      fmt::print("usdprim_has_attrib_color issue: {}", e.what());
      return false;
    }
  }

  bool retrieve_usdprim_attrib_uv(const ScenePrimConcept* scenePrim, double time, int numPoints,
                                  int numVerts, int numFaces, std::vector<glm::vec2>& uvs,
                                  prim_attrib_owner_e* owner, const source_location& loc) {
    if (!scenePrim) return false;

// #  if ZS_ENABLE_OPENMP
#  if 0
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      pxr::VtArray<pxr::GfVec2f> usdUVs;
      pxr::VtArray<int> usdUVIndices;
      pxr::UsdGeomPrimvarsAPI primvarAPI(usdMesh);
      auto stPrimvar = primvarAPI.GetPrimvar(pxr::TfToken("st"));  // TODO: multi-UVs
      bool uvOnPoint = false, uvOnVert = false, uvOnFace = false;
      if (stPrimvar.HasValue() && stPrimvar.Get(&usdUVs, time)) {
        if (stPrimvar.IsIndexed()) {
          if (stPrimvar.GetIndices(&usdUVIndices, time) && usdUVIndices.size() != 0) {
            uvOnVert = usdUVIndices.size() == numVerts;
            uvOnFace = usdUVIndices.size() == numFaces;
          }
        }
        if (!uvOnVert && !uvOnFace) {
          uvOnPoint = usdUVs.size() == 1 || usdUVs.size() == numPoints;
        }
      }

      if (!(uvOnPoint || uvOnVert || uvOnFace)) return false;

      if (uvOnVert) {
        if (owner) *owner = prim_attrib_owner_e::vert;
        uvs.resize(numVerts);
      } else if (uvOnFace) {
        if (owner) *owner = prim_attrib_owner_e::face;
        uvs.resize(numFaces);
      } else {
        // uvOnPoint
        if (owner) *owner = prim_attrib_owner_e::point;
        uvs.resize(numPoints);
      }
      if (uvOnVert || uvOnFace) {
        // on vert/face
        pol(enumerate(uvs), [&usdUVs, &usdUVIndices](PrimIndex id_, glm::vec2& uv) mutable {
          PrimIndex id = usdUVIndices[id_];
          uv[0] = usdUVs[id][0];
          uv[1] = usdUVs[id][1];
        });
      } else {
        pol(enumerate(uvs), [&usdUVs](PrimIndex id_, glm::vec2& uv) mutable {
          PrimIndex id = usdUVs.size() == 1 ? 0 : id_;
          uv[0] = usdUVs[id][0];
          uv[1] = usdUVs[id][1];
        });
      }
    } catch (const std::exception& e) {
      fmt::print("retrieve_usdprim_attrib_uv: {}\n", e.what());
      return false;
    }
    return true;
  }
  bool retrieve_usdprim_attrib_normal(const ScenePrimConcept* scenePrim, double time, int numPoints,
                                      int numVerts, int numFaces, std::vector<glm::vec3>& nrms,
                                      prim_attrib_owner_e* owner, const source_location& loc) {
    if (!scenePrim) return false;

// #  if ZS_ENABLE_OPENMP
#  if 0
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      pxr::VtArray<pxr::GfVec3f> usdNrms;
      auto usdNormalAttrib = usdMesh.GetNormalsAttr();
      if (!usdNormalAttrib.HasValue()) return false;

      usdNormalAttrib.Get(&usdNrms, time);
      bool nrmOnPoint = false, nrmOnVert, nrmOnFace;
      nrmOnVert = usdNrms.size() == numVerts;
      nrmOnFace = usdNrms.size() == numFaces;
      if (!nrmOnVert && !nrmOnFace) nrmOnPoint = usdNrms.size() == 1 || usdNrms.size() == numPoints;

      if (nrmOnVert) {
        if (owner) *owner = prim_attrib_owner_e::vert;
        nrms.resize(numVerts);
      } else if (nrmOnPoint) {
        if (owner) *owner = prim_attrib_owner_e::point;
        nrms.resize(numPoints);
      } else {
        if (owner) *owner = prim_attrib_owner_e::face;
        nrms.resize(numFaces);
      }
      pol(enumerate(nrms), [&usdNrms](PrimIndex id_, glm::vec3& nrm) mutable {
        PrimIndex id = usdNrms.size() == 1 ? 0 : id_;
        nrm[0] = usdNrms[id][0];
        nrm[1] = usdNrms[id][1];
        nrm[2] = usdNrms[id][2];
      });

    } catch (const std::exception& e) {
      fmt::print("retrieve_usdprim_attrib_normal: {}\n", e.what());
      return false;
    }
    return true;
  }
  bool retrieve_usdprim_attrib_color(const ScenePrimConcept* scenePrim, double time, int numPoints,
                                     int numVerts, int numFaces, std::vector<glm::vec3>& clrs,
                                     prim_attrib_owner_e* owner, const source_location& loc) {
    if (!scenePrim) return false;

// #  if ZS_ENABLE_OPENMP
#  if 0
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif
    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      pxr::VtArray<pxr::GfVec3f> usdClrs;
      auto usdColorAttrib = usdMesh.GetDisplayColorAttr();
      if (!usdColorAttrib.HasValue()) return false;

      usdColorAttrib.Get(&usdClrs, time);
      bool clrOnPoint = false, clrOnVert, clrOnFace;
      clrOnVert = usdClrs.size() == numVerts;
      clrOnFace = usdClrs.size() == numFaces;
      if (!clrOnVert && !clrOnFace) clrOnPoint = usdClrs.size() == 1 || usdClrs.size() == numPoints;

      if (clrOnVert) {
        if (owner) *owner = prim_attrib_owner_e::vert;
        clrs.resize(numVerts);
      } else if (clrOnPoint) {
        if (owner) *owner = prim_attrib_owner_e::point;
        clrs.resize(numPoints);
      } else {
        if (owner) *owner = prim_attrib_owner_e::face;
        clrs.resize(numFaces);
      }
      pol(enumerate(clrs), [&usdClrs](PrimIndex id_, glm::vec3& clr) mutable {
        PrimIndex id = usdClrs.size() == 1 ? 0 : id_;
        clr[0] = usdClrs[id][0];
        clr[1] = usdClrs[id][1];
        clr[2] = usdClrs[id][2];
      });
    } catch (const std::exception& e) {
      fmt::print("retrieve_usdprim_attrib_color: {}\n", e.what());
      return false;
    }
    return true;
  }

  bool parse_usdprim_light(const std::string& lightType, const ScenePrimConcept* scenePrim, ZsPrimitive* zsPrim) {
    // TODO: lightType
    if (scenePrim == nullptr) return false;
    Shared<LightPrimContainer> lightPrim = zsPrim->localLightPrims();
    if (!lightPrim) return false;

    try {
      lightPrim->lightType() = lightType;

      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      auto light = pxr::UsdLuxLightAPI(usdPrim);

      // light position in world space
      glm::mat4 worldMat = zsPrim->visualTransform();
      lightPrim->lightPosition() = worldMat[3]; // ??

      // light intensity
      float intensity;
      if (light.GetIntensityAttr().Get(&intensity)) {
        lightPrim->intensity() = intensity;
      }

      glm::vec3 color;
      pxr::UsdAttribute attr;
      pxr::VtValue attrValue;

      // light color
      attr = light.GetColorAttr();
      pxr::GfVec3f _col;
      attr.Get(&_col);
      lightPrim->lightColor() = { _col[0], _col[1], _col[2] };
      // TODO: temperature color
      /*
      bool enableTemperature = false;
      light.GetEnableColorTemperatureAttr().Get<bool>(&enableTemperature);
      lightPrim->useTemperatureColor = enableTemperature;
      if (enableTemperature) {
        float temp;
        attr = light.GetColorTemperatureAttr();
        attr.Get(&temp);
        // TODO: calculate color from temperature
      }
      */

      // TODO: light texture
      /*
      auto texAttr = usdPrim.GetAttribute(pxr::TfToken("inputs:texture:file"));
      std::string texturePath = "";
      if (texAttr.HasValue()) {
        pxr::SdfAssetPath texPath;
        texAttr.Get(&texPath);
        texturePath = texPath.GetResolvedPath();
      }
      */

    } catch (const std::exception& e) {
      fmt::print("parse_usdprim_light: {}\n", e.what());
      return false;
    }
    return true;
  }

  void assign_usdmesh_to_primitive(const ScenePrimConcept* scenePrim, ZsPrimitive& geom,
                                   double time, const source_location& loc) {
    if (!scenePrim) return;

    geom.reset();
// #  if ZS_ENABLE_OPENMP
#  if 0
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif

    try {
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);

      auto extent = usdMesh.GetExtentAttr();  // bounding box

      bool isDoubleSided;
      usdMesh.GetDoubleSidedAttr().Get(&isDoubleSided);  // TODO: handle double sided meshes

      // decide whether we use left handed order to construct faces
      pxr::TfToken faceOrder;
      usdMesh.GetOrientationAttr().Get(&faceOrder, time);
      bool isReversedFaceOrder = (faceOrder.GetString() == "leftHanded");

      /*** Start setting up mesh ***/
      pxr::VtArray<pxr::GfVec3f> usdPoses;
      pxr::VtArray<pxr::GfVec3f> usdClrs, usdNrms;
      pxr::VtArray<pxr::GfVec2f> usdUVs;
      pxr::VtArray<int> usdUVIndices;

      usdMesh.GetPointsAttr().Get(&usdPoses, time);

      const auto numPts = usdPoses.size();
      if (numPts == 0) return;

      pxr::VtArray<PrimIndex> polySizes;  // numbers of vertices for each face
      pxr::VtArray<PrimIndex> loopIndices;
      bool legalMesh = false;
      legalMesh |= usdMesh.GetFaceVertexCountsAttr().Get(&polySizes, time);
      legalMesh |= usdMesh.GetFaceVertexIndicesAttr().Get(&loopIndices, time);
      // fmt::print("num polies: {}\n", polySizes.size());
      // fmt::print("num indices: {}\n", loopIndices.size());
      if (!legalMesh || polySizes.size() == 0 || loopIndices.size() == 0) return;

      bool clrOnPoint = false, nrmOnPoint = false, uvOnPoint = false, uvOnVert = false,
           uvOnFace = false;
      std::vector<PropertyTag> ptProps{{ATTRIB_POS_TAG, 3}};
      std::vector<PropertyTag> polyProps{{POLY_SIZE_TAG, 1}, {POLY_OFFSET_TAG, 1}};
      // Colors
      auto usdColorAttrib = usdMesh.GetDisplayColorAttr();
      if (usdColorAttrib.HasValue()) {
        usdColorAttrib.Get(&usdClrs, time);
        // fmt::print("num colors: {}\n", usdClrs.size());
        assert(usdClrs.size() == numPts || usdClrs.size() == 1
               || usdClrs.size() == polySizes.size());
        clrOnPoint = (usdClrs.size() != polySizes.size());
        if (clrOnPoint)
          ptProps.push_back(PropertyTag{ATTRIB_COLOR_TAG, 3});
        else
          polyProps.push_back(PropertyTag{ATTRIB_COLOR_TAG, 3});
      }
      // Normals
      auto usdNormalAttrib = usdMesh.GetNormalsAttr();
      if (usdNormalAttrib.HasValue()) {
        usdNormalAttrib.Get(&usdNrms, time);
        assert(usdNrms.size() == numPts || usdNrms.size() == 1
               || usdNrms.size() == polySizes.size());
        nrmOnPoint = (usdNrms.size() != polySizes.size());
        if (nrmOnPoint)
          ptProps.push_back(PropertyTag{ATTRIB_NORMAL_TAG, 3});
        else
          polyProps.push_back(PropertyTag{ATTRIB_NORMAL_TAG, 3});
      }
      // UVs
      pxr::UsdGeomPrimvarsAPI primvarAPI(usdMesh);
      auto stPrimvar = primvarAPI.GetPrimvar(pxr::TfToken("st"));  // TODO: multi-UVs
      if (stPrimvar.HasValue() && stPrimvar.Get(&usdUVs, time)) {
        if (stPrimvar.IsIndexed()) {
          if (stPrimvar.GetIndices(&usdUVIndices, time) && usdUVIndices.size() != 0) {
            uvOnVert = usdUVIndices.size() == loopIndices.size();
            uvOnFace = usdUVIndices.size() == polySizes.size();
          }
        }

        if (!uvOnVert && !uvOnFace) {
          uvOnPoint = usdUVs.size() == 1 || usdUVs.size() == numPts;
        }

        if (uvOnPoint) ptProps.emplace_back(PropertyTag{ATTRIB_UV_TAG, 2});
      }
      // skinning calculation including blendShape
      _applySkinning(usdPoses, usdNrms, usdMesh, time);

      // copy points, verts
      geom.points().appendProperties32(pol, ptProps, loc);
      geom.points().resize(numPts);
      // fmt::print("usd pos size: {} ({}), clr size: {}\n", usdPoses.size(),
      // geom.points().size(), usdClrs.size());
      pol(range(geom.points().size()),
          [&usdPoses, &usdClrs, &usdNrms, &usdUVs, &clrOnPoint, &nrmOnPoint, &uvOnPoint,
           pts = view<space>({}, geom.points().attr32())](PrimIndex pid) mutable {
            for (int d = 0; d < 3; ++d) pts(ATTRIB_POS_TAG, d, pid) = usdPoses[pid][d];
            if (clrOnPoint) {
              auto cid = usdClrs.size() == 1 ? 0 : pid;
              for (int d = 0; d < 3; ++d) pts(ATTRIB_COLOR_TAG, d, pid) = usdClrs[cid][d];
            }

            if (nrmOnPoint) {
              auto nid = usdNrms.size() == 1 ? 0 : pid;
              for (int d = 0; d < 3; ++d) pts(ATTRIB_NORMAL_TAG, d, pid) = usdNrms[nid][d];
            }

            if (uvOnPoint) {
              auto nid = usdUVs.size() == 1 ? 0 : pid;
              pts(ATTRIB_UV_TAG, 0, pid) = usdUVs[nid][0];
              pts(ATTRIB_UV_TAG, 1, pid) = usdUVs[nid][1];
            }
          });

      std::vector<PrimIndex> polyOffsets(polySizes.size());
      inclusive_scan(pol, zs::begin(polySizes), zs::end(polySizes), zs::begin(polyOffsets));

      // copy tris (local prim), global prim
      const auto numIndices = polyOffsets.back();
      std::vector<PropertyTag> loopProps{{POINT_ID_TAG, 1}};
      if (uvOnVert || uvOnFace) loopProps.push_back({ATTRIB_UV_TAG, 2});
      geom.verts().appendProperties32(pol, loopProps, loc);
      geom.verts().resize(numIndices);

      PolyPrimContainer& geomPolys = *geom.localPolyPrims();
      const auto numPolys = polySizes.size();
      geomPolys.prims().appendProperties32(pol, polyProps, loc);
      geomPolys.resize(numPolys);
      auto& geomPrims = geom.globalPrims();
      geomPrims.resize(numPolys);

      pol(enumerate(polySizes, geomPrims),
          [&polyOffsets, &loopIndices, &usdClrs, &usdNrms, &usdUVs, &usdUVIndices,
           polys = view<space>({}, geomPolys.prims().attr32()),
           loops = view<space>({}, geom.verts().attr32()), clrOnPoint, nrmOnPoint, uvOnVert,
           uvOnFace, isReversedFaceOrder](u32 polyId, PrimIndex polySize,
                                          zs::tuple<PrimTypeIndex, PrimIndex>& entryNo) mutable {
            PrimIndex st = 0;
            if (polyId) st = polyOffsets[polyId - 1];
            PrimIndex ed = st + polySize;

            polys(POLY_OFFSET_TAG, polyId, prim_id_c) = st;
            polys(POLY_SIZE_TAG, polyId, prim_id_c) = polySize;
            if (!clrOnPoint && !usdClrs.empty())
              polys.tuple(dim_c<3>, ATTRIB_COLOR_TAG, polyId)
                  = zs::vec<float, 3>{usdClrs[polyId][0], usdClrs[polyId][1], usdClrs[polyId][2]};
            if (!nrmOnPoint && !usdNrms.empty())
              polys.tuple(dim_c<3>, ATTRIB_NORMAL_TAG, polyId)
                  = zs::vec<float, 3>{usdNrms[polyId][0], usdNrms[polyId][1], usdNrms[polyId][2]};
            if (uvOnVert || uvOnFace) {
              for (int i = st; i < ed; ++i) {
                const auto& indexedUV = usdUVs[usdUVIndices[uvOnVert ? i : polyId]];
                loops.tuple(dim_c<2>, ATTRIB_UV_TAG, i)
                    = zs::vec<float, 2>{indexedUV[0], indexedUV[1]};
              }
            }
            loops(POINT_ID_TAG, st, prim_id_c) = loopIndices[st];
            if (isReversedFaceOrder) {
              for (int i = 1; i < polySize; ++i) {
                loops(POINT_ID_TAG, st + i, prim_id_c) = loopIndices[ed - i];
              }
              st = ed;
            } else {
              for (++st; st != ed; ++st) {
                loops(POINT_ID_TAG, st, prim_id_c) = loopIndices[st];
              }
            }

            zs::get<0>(entryNo) = PrimitiveStorage::Poly_;
            zs::get<1>(entryNo) = polyId;
          });
    } catch (const std::runtime_error& e) {
      fmt::print("exception: {}\n", e.what());
    }
  }
  void assign_usdmesh_to_primitive(const ScenePrimHolder& scenePrim, ZsPrimitive& geom, double time,
                                   const source_location& loc) {
    assign_usdmesh_to_primitive(scenePrim.get(), geom, time, loc);
  }

  ZsPrimitive usdmesh_to_primitive(const ScenePrimConcept* scenePrim, double time,
                                   const source_location& loc) {
    ZsPrimitive geom{};
    assign_usdmesh_to_primitive(scenePrim, geom, time, loc);
    return geom;
  }
  ZsPrimitive usdmesh_to_primitive(const ScenePrimHolder& scenePrim, double time,
                                   const source_location& loc) {
    return usdmesh_to_primitive(scenePrim.get(), time, loc);
  }

  ZsPrimitive* build_primitive_from_usdprim(const ScenePrimConcept* prim, double time,
                                            const source_location& loc) {
    if (!prim) return nullptr;

    ZsPrimitive* ret = new ZsPrimitive();

    /// details
    // id
    // (*ret).id() = ResourceSystem::next_prim_id();
    // label
    (*ret).label() = prim->getName();
    // path
    (*ret).path() = prim->getPath();
    // transform
    (*ret).details().setTransform(prim->getLocalTransform());

    auto& detail = (*ret).details();
    detail.setAssetOrigin(asset_origin_e::usd);
    detail.isRightHandedCoord() = true;  // USD doesn't provide any efficient methods to get it,
                                         // let's mark it true as default
    // check stage up axis
    detail.isYUpAxis() = prim->getScene()->getIsYUpAxis();

    /// geometry
    assign_usdmesh_to_primitive(prim, *ret, time, loc);

    // lighting
    const std::string& typeName = prim->getTypeName();
    if (typeName.size() >= 5 && typeName.substr(typeName.size() - 5, 5) == "Light") {
      parse_usdprim_light(typeName, prim, ret);
    }

    /// children
    size_t nChilds;
    prim->getAllChilds(&nChilds, nullptr);
    std::vector<ScenePrimHolder> childs(nChilds);
    if (nChilds) {
      prim->getAllChilds(&nChilds, childs.data());

      for (size_t i = 0; i < nChilds; ++i) {
        auto ch = build_primitive_from_usdprim(childs[i].get(), time, loc);
        ret->appendChildPrimitve(ch);
      }
    }
    return ret;
  }
  ZsPrimitive* build_primitive_from_usdprim(const ScenePrimHolder& prim, double time,
                                            const source_location& loc) {
    return build_primitive_from_usdprim(prim.get(), time, loc);
  }
  ZsPrimitive* build_primitive_from_usdprim(const ScenePrimConcept* prim,
                                            const source_location& loc) {
    if (!prim) return nullptr;

    ZsPrimitive* ret = new ZsPrimitive();

#  if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif

    /// load all keyframes
    auto& keyframes = (*ret).keyframes();
    const auto defaultTimeCode = g_default_timecode();

    // points
    auto ptTcSz = prim->getPointTimeSamples(nullptr);
    std::vector<TimeCode> ptTcs(ptTcSz);
    prim->getPointTimeSamples(ptTcs.data());

    if (ptTcSz) {
      for (auto tc : ptTcs) {
        AttrVector posAttrib;
        if constexpr (false) {
          SerializationBuffer buffer;
          auto sz = bitsery::quickSerialization(BitseryOutputAdapter{buffer}, posAttrib);
          posAttrib.clear();
          auto recovered
              = bitsery::quickDeserialization(BitseryInputAdapter{buffer.data(), sz}, posAttrib);
          assert(recovered.first == bitsery::ReaderError::NoError && recovered.second);
        }
        int numPoints;
        std::vector<glm::vec3> poses;
        if (retrieve_usdprim_attrib_position(prim, tc, &numPoints, nullptr, loc)) {
          poses.resize(numPoints);
          retrieve_usdprim_attrib_position(prim, tc, &numPoints, poses.data(), loc);
          posAttrib.appendProperties32(pol, {{ATTRIB_POS_TAG, 3}}, loc);
          posAttrib.resize(numPoints);
          // assign poses to posAttrib
          pol(enumerate(poses), [pos = view<space>(posAttrib.attr32()),
                                 chnOffset = posAttrib.getPropertyOffset(ATTRIB_POS_TAG)](
                                    int i, const glm::vec3& p) mutable {
            pos.tuple(dim_c<3>, chnOffset, i) = zs::vec<float, 3>{p[0], p[1], p[2]};
          });
          keyframes.emplaceAttribKeyFrame(KEYFRAME_ATTRIB_POS_LABEL, tc, zs::move(posAttrib));
        }
      }
    } else {
      AttrVector posAttrib;
      int numPoints;
      std::vector<glm::vec3> poses;
      if (retrieve_usdprim_attrib_position(prim, defaultTimeCode, &numPoints, nullptr, loc)) {
        poses.resize(numPoints);
        retrieve_usdprim_attrib_position(prim, defaultTimeCode, &numPoints, poses.data(), loc);
        posAttrib.appendProperties32(pol, {{ATTRIB_POS_TAG, 3}}, loc);
        posAttrib.resize(numPoints);
        // assign poses to posAttrib
        pol(enumerate(poses), [pos = view<space>(posAttrib.attr32()),
                               chnOffset = posAttrib.getPropertyOffset(ATTRIB_POS_TAG)](
                                  int i, const glm::vec3& p) mutable {
          pos.tuple(dim_c<3>, chnOffset, i) = zs::vec<float, 3>{p[0], p[1], p[2]};
        });
        keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_POS_LABEL, zs::move(posAttrib));
      }
    }

    // topo
    auto vtTcSz = prim->getFaceIndexTimeSamples(nullptr);
    std::vector<TimeCode> vtTcs(vtTcSz);
    prim->getFaceIndexTimeSamples(vtTcs.data());
    auto polyTcSz = prim->getFaceSizeTimeSamples(nullptr);
    std::vector<TimeCode> polyTcs(polyTcSz);
    prim->getFaceSizeTimeSamples(polyTcs.data());
    auto topoTcs = merge_timecodes(pol, vtTcs, polyTcs);

    /// TODO
    bool faceOrderLeftHanded = usdprim_face_order_is_left_handed(prim);
    if (topoTcs.size()) {
      for (auto tc : topoTcs) {
        AttrVector vertAttrib, faceAttrib;
        int numVerts, numFaces;
        std::vector<int> verts, faceSizes;
        if (retrieve_usdprim_attrib_face(prim, tc, &numVerts, &numFaces, nullptr, nullptr, loc)) {
          verts.resize(numVerts);
          faceSizes.resize(numFaces);
          retrieve_usdprim_attrib_face(prim, tc, &numVerts, &numFaces, verts.data(),
                                       faceSizes.data(), loc);
          vertAttrib._owner = prim_attrib_owner_e::vert;
          vertAttrib.appendProperties32(pol, {{POINT_ID_TAG, 1}}, loc);
          vertAttrib.resize(numVerts);
          faceAttrib._owner = prim_attrib_owner_e::face;
          faceAttrib.appendProperties32(pol, {{POLY_SIZE_TAG, 1}, {POLY_OFFSET_TAG, 1}}, loc);
          faceAttrib.resize(numFaces);

          // assign faces
          pol(enumerate(faceSizes), [faceSizes = view<space>(faceAttrib.attr32()),
                                     szChnOffset = faceAttrib.getPropertyOffset(POLY_SIZE_TAG)](
                                        int i, int faceSize) mutable {
            faceSizes(szChnOffset, i, prim_id_c) = faceSize;
          });
          exclusive_scan(pol, faceAttrib.attr32().begin(POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                         faceAttrib.attr32().end(POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                         faceAttrib.attr32().begin(POLY_OFFSET_TAG, dim_c<1>, prim_id_c));

          // assign verts
          if (faceOrderLeftHanded) {
            pol(zip(range(faceAttrib.attr32(), POLY_OFFSET_TAG, dim_c<1>, prim_id_c),
                    range(faceAttrib.attr32(), POLY_SIZE_TAG, dim_c<1>, prim_id_c)),
                [&verts, vertAttrib = view<space>(vertAttrib.attr32()),
                 pidChnOffset = vertAttrib.getPropertyOffset(POINT_ID_TAG)](
                    PrimIndex polyOffset, PrimIndex polySize) mutable {
                  PrimIndex ed = polyOffset + polySize;
                  vertAttrib(pidChnOffset, polyOffset, prim_id_c) = verts[polyOffset];
                  for (PrimIndex i = 1; i < polySize; ++i)
                    vertAttrib(pidChnOffset, polyOffset + i, prim_id_c) = verts[ed - i];
                });
          } else
            pol(enumerate(verts),
                [vertAttrib = view<space>(vertAttrib.attr32()),
                 pidChnOffset = vertAttrib.getPropertyOffset(POINT_ID_TAG)](
                    int i, int vert) mutable { vertAttrib(pidChnOffset, i, prim_id_c) = vert; });
          keyframes.emplaceAttribKeyFrame(KEYFRAME_ATTRIB_FACE_LABEL, tc, zs::move(faceAttrib));
          keyframes.emplaceAttribKeyFrame(KEYFRAME_ATTRIB_FACE_INDEX_LABEL, tc,
                                          zs::move(vertAttrib));
        }
      }
    } else {
      AttrVector vertAttrib, faceAttrib;
      int numVerts, numFaces;
      std::vector<int> verts, faceSizes;
      if (retrieve_usdprim_attrib_face(prim, defaultTimeCode, &numVerts, &numFaces, nullptr,
                                       nullptr, loc)) {
        verts.resize(numVerts);
        faceSizes.resize(numFaces);
        retrieve_usdprim_attrib_face(prim, defaultTimeCode, &numVerts, &numFaces, verts.data(),
                                     faceSizes.data(), loc);
        vertAttrib._owner = prim_attrib_owner_e::vert;
        vertAttrib.resize(numVerts);
        vertAttrib.appendProperties32(pol, {{POINT_ID_TAG, 1}}, loc);
        faceAttrib._owner = prim_attrib_owner_e::face;
        faceAttrib.resize(numFaces);
        faceAttrib.appendProperties32(pol, {{POLY_SIZE_TAG, 1}, {POLY_OFFSET_TAG, 1}}, loc);

        // assign faces
        pol(enumerate(faceSizes),
            [faceSizes = view<space>(faceAttrib.attr32()),
             szChnOffset = faceAttrib.getPropertyOffset(POLY_SIZE_TAG)](
                int i, int faceSize) mutable { faceSizes(szChnOffset, i, prim_id_c) = faceSize; });
        exclusive_scan(pol, faceAttrib.attr32().begin(POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                       faceAttrib.attr32().end(POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                       faceAttrib.attr32().begin(POLY_OFFSET_TAG, dim_c<1>, prim_id_c));

        // assign verts
        if (faceOrderLeftHanded) {
          pol(zip(range(faceAttrib.attr32(), POLY_OFFSET_TAG, dim_c<1>, prim_id_c),
                  range(faceAttrib.attr32(), POLY_SIZE_TAG, dim_c<1>, prim_id_c)),
              [&verts, vertAttrib = view<space>(vertAttrib.attr32()),
               pidChnOffset = vertAttrib.getPropertyOffset(POINT_ID_TAG)](
                  PrimIndex polyOffset, PrimIndex polySize) mutable {
                PrimIndex ed = polyOffset + polySize;
                vertAttrib(pidChnOffset, polyOffset, prim_id_c) = verts[polyOffset];
                for (PrimIndex i = 1; i < polySize; ++i)
                  vertAttrib(pidChnOffset, polyOffset + i, prim_id_c) = verts[ed - i];
              });
        } else
          pol(enumerate(verts),
              [vertAttrib = view<space>(vertAttrib.attr32()),
               pidChnOffset = vertAttrib.getPropertyOffset(POINT_ID_TAG)](int i, int vert) mutable {
                vertAttrib(pidChnOffset, i, prim_id_c) = vert;
              });
        keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_FACE_LABEL, zs::move(faceAttrib));
        keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_FACE_INDEX_LABEL, zs::move(vertAttrib));
      } else {
        /// @brief default topology init for pure points
        verts.resize(numVerts);
        std::iota(verts.begin(), verts.end(), 0);
        faceSizes.resize(numVerts, 1);
        vertAttrib._owner = prim_attrib_owner_e::vert;
        vertAttrib.appendProperties32(pol, {{POINT_ID_TAG, 1}}, loc);
        vertAttrib.resize(numVerts);
        faceAttrib._owner = prim_attrib_owner_e::face;
        faceAttrib.appendProperties32(pol, {{POLY_SIZE_TAG, 1}, {POLY_OFFSET_TAG, 1}}, loc);
        faceAttrib.resize(numVerts);
        // assign verts/faceSizes to topoAttrib
        pol(enumerate(verts),
            [verts = view<space>(vertAttrib.attr32()),
             pidChnOffset = vertAttrib.getPropertyOffset(POINT_ID_TAG)](int i, int vert) mutable {
              verts(pidChnOffset, i, prim_id_c) = vert;
            });
        keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_FACE_INDEX_LABEL, zs::move(vertAttrib));
        pol(enumerate(faceSizes),
            [faceSizes = view<space>(faceAttrib.attr32()),
             szChnOffset = faceAttrib.getPropertyOffset(POLY_SIZE_TAG)](
                int i, int faceSize) mutable { faceSizes(szChnOffset, i, prim_id_c) = faceSize; });
        exclusive_scan(pol, faceAttrib.attr32().begin(POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                       faceAttrib.attr32().end(POLY_SIZE_TAG, dim_c<1>, prim_id_c),
                       faceAttrib.attr32().begin(POLY_OFFSET_TAG, dim_c<1>, prim_id_c));
        keyframes.emplaceAttribDefault(KEYFRAME_ATTRIB_FACE_LABEL, zs::move(faceAttrib));
      }
    }

    // attribs: uv, normal, color

    auto initAttribKeyFrame
        = [&prim, &keyframes, &loc, &pol](TimeCode tc, auto fn, const char* kfLabel,
                                          const char* attrTag, auto attr_dim_c) {
            constexpr int D_ = RM_CVREF_T(attr_dim_c)::value;
            using T = glm::vec<D_, float, glm::qualifier::defaultp>;
            std::vector<T> attrs;
            int numPoints = keyframes.getNumPoints(tc), numVerts = keyframes.getNumVerts(tc),
                numFaces = keyframes.getNumFaces(tc);
            prim_attrib_owner_e owner;
            if (fn(prim, tc, numPoints, numVerts, numFaces, attrs, &owner, loc)) {
              AttrVector attrib;
              attrib._owner = owner;
              attrib.appendProperties32(pol, {{attrTag, D_}}, loc);
              attrib.resize(attrs.size());
              pol(zip(range(attrib.attr32(), attrTag, dim_c<D_>), attrs),
                  [D_ = D_](auto dst, auto src) mutable {
                    for (int d = 0; d < D_; ++d) dst[d] = src[d];
                  });
              keyframes.emplaceAttribKeyFrame(kfLabel, tc, zs::move(attrib));
            }
          };

    auto uvTcSz = prim->getUVTimeSamples(nullptr);
    std::vector<TimeCode> uvTcs(uvTcSz);
    prim->getUVTimeSamples(uvTcs.data());
    if (uvTcSz) {
      for (auto tc : uvTcs) {
        initAttribKeyFrame(
            tc, [](auto&&... args) { return retrieve_usdprim_attrib_uv(FWD(args)...); },
            KEYFRAME_ATTRIB_UV_LABEL, ATTRIB_UV_TAG, wrapv<2>{});
      }
    } else if (usdprim_has_attrib_uv(prim, loc)) {
      initAttribKeyFrame(
          defaultTimeCode, [](auto&&... args) { return retrieve_usdprim_attrib_uv(FWD(args)...); },
          KEYFRAME_ATTRIB_UV_LABEL, ATTRIB_UV_TAG, wrapv<2>{});
    }

    auto nrmTcSz = prim->getNormalTimeSamples(nullptr);
    std::vector<TimeCode> nrmTcs(nrmTcSz);
    prim->getNormalTimeSamples(nrmTcs.data());
    if (nrmTcSz) {
      for (auto tc : nrmTcs) {
        initAttribKeyFrame(
            tc, [](auto&&... args) { return retrieve_usdprim_attrib_normal(FWD(args)...); },
            KEYFRAME_ATTRIB_NORMAL_LABEL, ATTRIB_NORMAL_TAG, wrapv<3>{});
      }
    } else if (usdprim_has_attrib_normal(prim, loc)) {
      initAttribKeyFrame(
          defaultTimeCode,
          [](auto&&... args) { return retrieve_usdprim_attrib_normal(FWD(args)...); },
          KEYFRAME_ATTRIB_NORMAL_LABEL, ATTRIB_NORMAL_TAG, wrapv<3>{});
    }

    auto clrTcSz = prim->getColorTimeSamples(nullptr);
    std::vector<TimeCode> clrTcs(clrTcSz);
    prim->getColorTimeSamples(clrTcs.data());
    if (clrTcSz) {
      for (auto tc : clrTcs) {
        initAttribKeyFrame(
            tc, [](auto&&... args) { return retrieve_usdprim_attrib_color(FWD(args)...); },
            KEYFRAME_ATTRIB_COLOR_LABEL, ATTRIB_COLOR_TAG, wrapv<3>{});
      }
    } else if (usdprim_has_attrib_color(prim, loc)) {
      initAttribKeyFrame(
          defaultTimeCode,
          [](auto&&... args) { return retrieve_usdprim_attrib_color(FWD(args)...); },
          KEYFRAME_ATTRIB_COLOR_LABEL, ATTRIB_COLOR_TAG, wrapv<3>{});
    }

    // visibility
    auto visTcSz = prim->getVisibleTimeSamples(nullptr);
    std::vector<TimeCode> visTcs(visTcSz);
    prim->getVisibleTimeSamples(visTcs.data());
    if (visTcSz) {
      for (auto tc : visTcs) keyframes.emplaceVisibilityKeyFrame(tc, prim->getVisible(tc));
    } else
      keyframes.emplaceVisibilityDefault(false);

      // transform
#  define GET_TRANSFORM_TIMECODES(VAR_NAME, API_NAME)                \
    auto VAR_NAME##TcSz = prim->get##API_NAME##TimeSamples(nullptr); \
    std::vector<TimeCode> VAR_NAME##Tcs(VAR_NAME##TcSz);             \
    prim->get##API_NAME##TimeSamples(VAR_NAME##Tcs.data());

    GET_TRANSFORM_TIMECODES(translation, Translation);
    GET_TRANSFORM_TIMECODES(scale, Scale);
    GET_TRANSFORM_TIMECODES(rotX, RotationX);
    GET_TRANSFORM_TIMECODES(rotY, RotationY);
    GET_TRANSFORM_TIMECODES(rotZ, RotationZ);
    GET_TRANSFORM_TIMECODES(rotXYZ, RotationXYZ);
    GET_TRANSFORM_TIMECODES(rotXZY, RotationXZY);
    GET_TRANSFORM_TIMECODES(rotYXZ, RotationYXZ);
    GET_TRANSFORM_TIMECODES(rotYZX, RotationYZX);
    GET_TRANSFORM_TIMECODES(rotZXY, RotationZXY);
    GET_TRANSFORM_TIMECODES(rotZYX, RotationZYX);
    GET_TRANSFORM_TIMECODES(matOp, MatrixOp);

    auto transformTcs
        = merge_timecodes(pol, translationTcs, scaleTcs, rotXTcs, rotYTcs, rotZTcs, rotXYZTcs,
                          rotXZYTcs, rotYXZTcs, rotYZXTcs, rotZXYTcs, rotZYXTcs);
    // fmt::print("{} transform tcs in total, first: {}, last: {}\n", transformTcs.size(),
    // transformTcs.size() ? transformTcs[0] : -1., transformTcs.size() ? transformTcs.back() :
    // -1.);
    for (auto tc : transformTcs) {
      keyframes.emplaceTransformKeyFrame(tc);  // prim->getLocalTransform(tc)
    }

    // skeleton animation
    {
      TimeCode st, ed;
      if (prim->getSkelAnimTimeCodeInterval(st, ed)) keyframes.setSkelAnimTimeCodeInterval(st, ed);
    }

    /// @note setup usd prim link
    ret->details().setUsdPrim(prim);

    /// @note this takes skeleton animation into account
    auto totalTcs
        = merge_timecodes(pol, ptTcs, topoTcs, uvTcs, nrmTcs, clrTcs, visTcs, transformTcs);
    keyframes.setGlobalTimeCodes(totalTcs);

#  if 0
    {
      // create writer and serialize
      SerializationBuffer buffer;
      bitsery::ext::PointerLinkingContext ctx{};
      BitserySerializer ser{ctx, buffer};
      Shared<bool> tmp{new bool()};
      *tmp = true;
      ser.template ext<sizeof(bool)>(tmp, BitseryStdSmartPtr{});
      ser.adapter().flush();
      auto sz = ser.adapter().writtenBytesCount();
      // ser.object(keyframes._visibility);
      fmt::print("serialization written size Shared<bool>: {}\n", sz);

      // BitseryDeserializer deser{ctx, buffer.begin(), sz};
      BitseryDeserializer deser{ctx, BitseryReader{buffer.data(), sz}};
      Shared<bool> opt{};
      deser.ext<sizeof(bool)>(opt, BitseryStdSmartPtr{});
      fmt::print("deserialized shared_ptr: {}\n", *opt);
      assert(deser.adapter().error() == bitsery::ReaderError::NoError);
      assert(deser.adapter().isCompletedSuccessfully());

      {
        Shared<AttrVector> tmp{new AttrVector()};
        ser.ext(tmp, BitseryStdSmartPtr{});
        ser.adapter().flush();
        auto sz = ser.adapter().writtenBytesCount();
        // ser.object(keyframes._visibility);
        fmt::print("serialization written size Shared<AttrVector>: {}\n", sz);
      }
#    if 1
      {
        SerializationBuffer buffer;
        bitsery::ext::PointerLinkingContext ctx{};

        BitserySerializer ser{ctx, buffer};
        ser.object(keyframes);
        ser.adapter().flush();
        auto sz = ser.adapter().writtenBytesCount();

        {
          PrimKeyFrames opt;
          bitsery::ext::PointerLinkingContext ctx{};
          BitseryDeserializer deser{ctx, BitseryReader{buffer.data(), sz}};
          deser.object(opt);
          opt.getAttrib(KEYFRAME_ATTRIB_POS_LABEL, 0).lock()->printDbg("\tpos kf at 0.");
          assert(deser.adapter().error() == bitsery::ReaderError::NoError);
          assert(deser.adapter().isCompletedSuccessfully());
        }
      }
#    endif
    }
#  endif

    TimeCode tc{};
    if (totalTcs.size()) {
      tc = totalTcs[0];
      ret->updatePrimFromKeyFrames(tc);
      if (keyframes.hasSkelAnim()) ret->applySkinning(tc);
      ret->details().setCurrentTimeCode(tc);
    } else if (keyframes.hasSkelAnim()) {
      auto [st, ed] = keyframes.getSkelAnimTimeCodeInterval();
      tc = st;
      ret->updatePrimFromKeyFrames(tc);
      if (keyframes.hasSkelAnim()) ret->applySkinning(tc);  // force apply
      ret->details().setCurrentTimeCode(tc);
    } else {
      // auto tc = prim->getScene()->getStartTimeCode();
      tc = g_default_timecode();
      ret->updatePrimFromKeyFrames(tc);
      if (keyframes.hasSkelAnim()) ret->applySkinning(tc);  // force apply
      ret->details().setCurrentTimeCode(tc);
    }

    /// details
    // label
    (*ret).label() = prim->getName();
    // path
    (*ret).path() = prim->getPath();
    // transform
    (*ret).details().setCurrentTransformTimeCode(tc);
    (*ret).details().setTransform(prim->getLocalTransform(tc));
    (*ret).details().refTransformVarying() = prim->isTimeVarying();

    auto& detail = (*ret).details();
    detail.setAssetOrigin(asset_origin_e::usd);
    detail.isRightHandedCoord() = true;  // USD doesn't provide any efficient methods to get it,
                                         // let's mark it true as default
    // check stage up axis
    detail.isYUpAxis() = prim->getScene()->getIsYUpAxis();

    // lighting
    const std::string& typeName = prim->getTypeName();
    if (typeName.size() >= 5 && typeName.substr(typeName.size() - 5, 5) == "Light") {
      parse_usdprim_light(typeName, prim, ret);
    }

    /// children
    size_t nChilds;
    prim->getAllChilds(&nChilds, nullptr);
    std::vector<ScenePrimHolder> childs(nChilds);
    if (nChilds) {
      prim->getAllChilds(&nChilds, childs.data());

      for (size_t i = 0; i < nChilds; ++i) {
        auto ch = build_primitive_from_usdprim(childs[i].get(), loc);
        ret->appendChildPrimitve(ch);
      }
    }
    return ret;
  }
  ZsPrimitive* build_primitive_from_usdprim(const ScenePrimHolder& prim,
                                            const source_location& loc) {
    return build_primitive_from_usdprim(prim.get(), loc);
  }

  static void _applyBlendShape(pxr::VtArray<pxr::GfVec3f>& points,
                               pxr::VtArray<pxr::GfVec3f>& normals,
                               pxr::UsdSkelBindingAPI& skelBinding,
                               pxr::UsdSkelSkeletonQuery& skelQuery,
                               pxr::UsdSkelSkinningQuery& skinQuery, double time) {
    if (!skinQuery.HasBlendShapes()) {
      return;
    }

    const pxr::UsdSkelAnimQuery& animQuery = skelQuery.GetAnimQuery();
    if (!animQuery) {
      return;
    }

    pxr::VtArray<float> weights;
    pxr::VtArray<float> realWeights;
    if (!animQuery.ComputeBlendShapeWeights(&weights, time)) {
      return;
    }

    if (skinQuery.GetBlendShapeMapper()) {
      if (!skinQuery.GetBlendShapeMapper()->Remap(weights, &realWeights)) {
        return;
      }
    } else {
      realWeights = std::move(weights);
    }

    pxr::UsdSkelBlendShapeQuery blendShapeQuery(skelBinding);
    pxr::VtArray<float> subShapeWeights;
    pxr::VtArray<unsigned int> blendShapeIndices;
    pxr::VtArray<unsigned int> subShapeIndices;
    if (!blendShapeQuery.ComputeSubShapeWeights(realWeights, &subShapeWeights, &blendShapeIndices,
                                                &subShapeIndices)) {
      return;
    }

    blendShapeQuery.ComputeDeformedPoints(
        pxr::TfMakeSpan(subShapeWeights), pxr::TfMakeSpan(blendShapeIndices),
        pxr::TfMakeSpan(subShapeIndices), blendShapeQuery.ComputeBlendShapePointIndices(),
        blendShapeQuery.ComputeSubShapePointOffsets(), pxr::TfMakeSpan(points));
    if (normals.size() > 0) {
      blendShapeQuery.ComputeDeformedNormals(
          pxr::TfMakeSpan(subShapeWeights), pxr::TfMakeSpan(blendShapeIndices),
          pxr::TfMakeSpan(subShapeIndices), blendShapeQuery.ComputeBlendShapePointIndices(),
          blendShapeQuery.ComputeSubShapePointOffsets(), pxr::TfMakeSpan(normals));
    }
  }
  void _applySkinning(pxr::VtArray<pxr::GfVec3f>& points,
                      pxr::VtArray<pxr::GfVec3f>& normals /* TODO */, const pxr::UsdGeomMesh& mesh,
                      double time) {
    pxr::UsdSkelCache skelCache;
    pxr::UsdSkelBindingAPI skelBinding(mesh.GetPrim());

    auto skelRoot = pxr::UsdSkelRoot::Find(mesh.GetPrim());
    if (!skelRoot) {
      // no skelRoot found for this mesh
      return;
    }

    skelCache.Populate(skelRoot, pxr::UsdTraverseInstanceProxies());
    pxr::UsdSkelSkeletonQuery skelQuery
        = skelCache.GetSkelQuery(skelBinding.GetInheritedSkeleton());
    pxr::UsdSkelSkinningQuery skinQuery = skelCache.GetSkinningQuery(mesh.GetPrim());

    pxr::VtArray<pxr::GfMatrix4d> skinningTransforms;
    if (!skelQuery.ComputeSkinningTransforms(&skinningTransforms, time)) {
      return;
    }

    _applyBlendShape(points, normals, skelBinding, skelQuery, skinQuery, time);

    if (!skinQuery.ComputeSkinnedPoints(skinningTransforms, &points, time)) {
      return;
    }

    pxr::GfMatrix4d bindTransform = skinQuery.GetGeomBindTransform(time).GetInverse();
    for (auto& point : points) {
      point = bindTransform.Transform(point);
    }

    // TODO: normals
  }

  bool apply_usd_skinning(AttrVector& posAttrib, const ScenePrimConcept* scenePrim, double time,
                          const source_location& loc) {
    if (posAttrib.size() == 0 || !scenePrim) return false;
    bool applyNormals = posAttrib.hasProperty(ATTRIB_NORMAL_TAG);
    pxr::VtArray<pxr::GfVec3f> points;
    pxr::VtArray<pxr::GfVec3f> normals;
    points.reserve(posAttrib.size());
    for (auto p : range(posAttrib.attr32(), ATTRIB_POS_TAG, dim_c<3>)) {
      points.emplace_back(pxr::GfVec3f{p[0], p[1], p[2]});
    }
    if (applyNormals) {
      normals.reserve(posAttrib.size());
      for (auto n : range(posAttrib.attr32(), ATTRIB_NORMAL_TAG, dim_c<3>)) {
        normals.emplace_back(pxr::GfVec3f{n[0], n[1], n[2]});
      }
    }

#  if 0
    if (points.size() > 0)
      fmt::print("{}-th (of {}) original pos: {}, {}, {}\n", 0, points.size(), points[0][0],
                 points[0][1], points[0][2]);
    if (points.size() > 1)
      fmt::print("{}-th (of {}) original pos: {}, {}, {}\n", 1, points.size(), points[1][0],
                 points[1][1], points[1][2]);
    if (points.size() > 2)
      fmt::print("{}-th (of {}) original pos: {}, {}, {}\n", 2, points.size(), points[2][0],
                 points[2][1], points[2][2]);
#  endif

    try {
      // fmt::print("prim : {}, path : {}\n", scenePrim->getName(), scenePrim->getPath());
      auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
      if (!usdPrim.IsValid()) return false;

      const auto& mesh = pxr::UsdGeomMesh(usdPrim);
      pxr::UsdSkelCache skelCache;
      pxr::UsdSkelBindingAPI skelBinding(mesh.GetPrim());

      auto skelRoot = pxr::UsdSkelRoot::Find(mesh.GetPrim());
      if (!skelRoot) {
        // no skelRoot found for this mesh
        return false;
      }

      skelCache.Populate(skelRoot, pxr::UsdTraverseInstanceProxies());
      pxr::UsdSkelSkeletonQuery skelQuery
          = skelCache.GetSkelQuery(skelBinding.GetInheritedSkeleton());
      pxr::UsdSkelSkinningQuery skinQuery = skelCache.GetSkinningQuery(mesh.GetPrim());

      pxr::VtArray<pxr::GfMatrix4d> skinningTransforms;
      if (!skelQuery.ComputeSkinningTransforms(&skinningTransforms, time)) {
        return false;
      }

      _applyBlendShape(points, normals, skelBinding, skelQuery, skinQuery, time);

      if (!skinQuery.ComputeSkinnedPoints(skinningTransforms, &points, time)) {
        return false;
      }
      if (applyNormals && !skinQuery.ComputeSkinnedNormals(skinningTransforms, &normals, time)) {
        return false;
      }

      pxr::GfMatrix4d bindTransform = skinQuery.GetGeomBindTransform(time).GetInverse();
      for (auto& point : points) {
        point = bindTransform.Transform(point);
      }

#  if 0
      if (points.size() > 0)
        fmt::print("{}-th (of {}) transformed pos: {}, {}, {}\n", 0, points.size(), points[0][0],
                   points[0][1], points[0][2]);
      if (points.size() > 1)
        fmt::print("{}-th (of {}) transformed pos: {}, {}, {}\n", 1, points.size(), points[1][0],
                   points[1][1], points[1][2]);
      if (points.size() > 2)
        fmt::print("{}-th (of {}) transformed pos: {}, {}, {}\n", 2, points.size(), points[2][0],
                   points[2][1], points[2][2]);
#  endif

#  if ZS_ENABLE_OPENMP
      auto pol = omp_exec();
      constexpr auto space = execspace_e::openmp;
#  else
      auto pol = seq_exec();
      constexpr auto space = execspace_e::host;
#  endif

      posAttrib.appendProperties32(pol, {{ATTRIB_SKINNING_POS_TAG, 3}}, loc);
      pol(zip(points, range(posAttrib.attr32(), ATTRIB_SKINNING_POS_TAG, dim_c<3>)),
          [](const auto& src, auto dst) {
            for (int d = 0; d < 3; ++d) dst[d] = src[d];
          });

      return true;
    } catch (const std::exception& e) {
      fmt::print("apply_usd_skinning: {}\n", e.what());
    }
    return false;
  }
#endif

}  // namespace zs
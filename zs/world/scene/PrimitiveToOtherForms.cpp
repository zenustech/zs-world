#include "Primitive.hpp"
#include "PrimitiveConversion.hpp"
//
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
#endif
#include "zensim/types/Property.h"

#if ZS_ENABLE_OPENMP
#  include "zensim/omp/execution/ExecutionPolicy.hpp"
#else
#  include "zensim/execution/ExecutionPolicy.hpp"
#endif

namespace zs {

  void assign_primitive_to_trimesh(const ZsPrimitive& geom, Mesh<f32, 3, u32, 3>& triMesh,
                                   const source_location& loc) {
    auto& nodes = triMesh.nodes;
    auto& uvs = triMesh.uvs;
    auto& norms = triMesh.norms;
    auto& colors = triMesh.colors;
    auto& elems = triMesh.elems;

    triMesh.clear();

#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif
    // geometry points
    const auto& points = geom.points();
    const auto numPts = points.size();
    nodes.resize(numPts);
    if (points.hasProperty(ATTRIB_COLOR_TAG)) colors.resize(numPts);
    if (points.hasProperty(ATTRIB_NORMAL_TAG)) norms.resize(numPts);
    if (points.hasProperty(ATTRIB_UV_TAG)) uvs.resize(numPts);

    // geometry vertices
    const auto& verts = geom.verts();
    const auto numVerts = verts.size();

    auto& geomPrims = geom.globalPrims();

    // num tris
    auto numPrims = geomPrims.size();
    std::vector<PrimIndex> numTrisPerPrim(numPrims + 1);
    pol(zip(geomPrims, numTrisPerPrim),
        [&geom](const zs::tuple<PrimTypeIndex, PrimIndex>& entryNo, PrimIndex& numTris) {
          auto localPrimId = zs::get<1>(entryNo);
          switch (zs::get<0>(entryNo)) {
            case PrimitiveStorage::Tri_: {
              // const TriPrimContainer& geomTris = *geom.localTriPrims();
              numTris = 1;
              break;
            }
            case PrimitiveStorage::Poly_: {
              const PolyPrimContainer& geomPolys = *geom.localPolyPrims();
              // auto polysView = view<space>({}, geomPolys.prims().attr32());
              auto polySize = geomPolys.prims().attr32().begin(POLY_SIZE_TAG, dim_c<1>,
                                                               prim_id_c)[localPrimId];
              numTris = polySize >= 3 ? polySize - 2 : 0;
              break;
            }
            default:;
          }
        });

    // prim to tris
    std::vector<PrimIndex> triOffsetsPerPrim(numPrims + 1);
    exclusive_scan(pol, zs::begin(numTrisPerPrim), zs::end(numTrisPerPrim),
                   zs::begin(triOffsetsPerPrim), 0);
    elems.resize(triOffsetsPerPrim.back());
    pol(enumerate(geomPrims), [&geom, &triOffsetsPerPrim, &numTrisPerPrim, &elems](
                                  PrimIndex globalPrimId,
                                  const zs::tuple<PrimTypeIndex, PrimIndex>& entryNo) {
      auto localPrimId = zs::get<1>(entryNo);
      auto numTris = numTrisPerPrim[globalPrimId];
      if (numTris == 0) return;  // skip this invalid prim
      auto triSt = triOffsetsPerPrim[globalPrimId];
      switch (zs::get<0>(entryNo)) {
        case PrimitiveStorage::Tri_: {
          const TriPrimContainer& geomTris = *geom.localTriPrims();
          const auto& tri
              = geomTris.prims().attr32().begin(ELEM_VERT_ID_TAG, dim_c<3>, prim_id_c)[localPrimId];
          for (int d = 0; d != 3; ++d) elems[triSt][d] = tri[d];
          break;
        }
        case PrimitiveStorage::Poly_: {
          const PolyPrimContainer& geomPolys = *geom.localPolyPrims();
          // auto polysView = view<space>({}, geomPolys.prims().attr32());
          auto polySt
              = geomPolys.prims().attr32().begin(POLY_OFFSET_TAG, dim_c<1>, prim_id_c)[localPrimId];
          auto loopPids = geom.verts().attr32().begin(POINT_ID_TAG, dim_c<1>, prim_id_c);
          int primVidSub = 1;  // subscript

          // triangle fan style conversion
          PrimIndex tid0 = loopPids[polySt];
          for (int triOffset = 0; triOffset < numTris; ++triOffset) {
            auto triNo = triSt + triOffset;
            elems[triNo][0] = tid0;
            elems[triNo][1] = loopPids[polySt + primVidSub];
            elems[triNo][2] = loopPids[polySt + primVidSub + 1];
            primVidSub++;
          }
          break;
        }
        default:;
      }
    });
    // points and attribs (nrm, uv, clr)
    pol(range(numPts),
        [pts = view<space>({}, points.attr32()), &nodes, &norms, &colors, &uvs](PrimIndex pid) {
          for (int d = 0; d != 3; ++d) nodes[pid][d] = pts(ATTRIB_POS_TAG, d, pid);
          if (colors.size())
            for (int d = 0; d != 3; ++d) colors[pid][d] = pts(ATTRIB_COLOR_TAG, d, pid);
          if (norms.size())
            for (int d = 0; d != 3; ++d) norms[pid][d] = pts(ATTRIB_NORMAL_TAG, d, pid);
          if (uvs.size())
            for (int d = 0; d != 2; ++d) uvs[pid][d] = pts(ATTRIB_UV_TAG, d, pid);
        });
  }

  Mesh<f32, 3, u32, 3> primitive_to_trimesh(const ZsPrimitive& geom, const source_location& loc) {
    Mesh<f32, 3, u32, 3> mesh{};
    assign_primitive_to_trimesh(geom, mesh, loc);
    return mesh;
  }

#if ZS_ENABLE_USD
  void assign_primitive_to_usdprim(const ZsPrimitive& geom, ScenePrimConcept* scenePrim,
                                   double time, const source_location& loc) {
    if (!scenePrim) return;

    auto usdPrim = std::any_cast<pxr::UsdPrim>(scenePrim->getRawPrim());
    if (!usdPrim.IsValid()) return;

    if (!usdPrim.SetTypeName(pxr::TfToken("Mesh"))) return;

#  if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#  else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#  endif
    auto usdMesh = pxr::UsdGeomMesh(usdPrim);

#  define USD_MESH_ATTR(x, suffix)  \
    auto x = usdMesh.Get##suffix(); \
    if (!x.IsValid()) {             \
      x = usdMesh.Create##suffix(); \
    }                               \
    x.Clear();

    const AttrVector& points = geom.points();

    // Positions
    USD_MESH_ATTR(usdPosAttrib, PointsAttr);
    pxr::VtArray<pxr::GfVec3f> usdPoses(points.size());
    pol(range(points.size()), [&usdPoses, pts = view<space>({}, points.attr32())](PrimIndex pid) {
      for (int d = 0; d < 3; ++d) usdPoses[pid][d] = pts(ATTRIB_POS_TAG, d, pid);
    });
    usdPosAttrib.Set(usdPoses);

    // Normals
    if (points.hasProperty(ATTRIB_NORMAL_TAG)) {
      USD_MESH_ATTR(usdNormAttrib, NormalsAttr);
      pxr::VtArray<pxr::GfVec3f> usdNrms(points.size());
      pol(range(points.size()), [&usdNrms, pts = view<space>({}, points.attr32())](PrimIndex pid) {
        for (int d = 0; d < 3; ++d) usdNrms[pid][d] = pts(ATTRIB_NORMAL_TAG, d, pid);
      });
      usdNormAttrib.Set(usdNrms);
    }
    // Colors
    if (points.hasProperty(ATTRIB_COLOR_TAG)) {
      USD_MESH_ATTR(usdColorAttrib, DisplayColorAttr);
      pxr::VtArray<pxr::GfVec3f> usdClrs(points.size());
      pol(range(points.size()), [&usdClrs, pts = view<space>({}, points.attr32())](PrimIndex pid) {
        for (int d = 0; d < 3; ++d) usdClrs[pid][d] = pts(ATTRIB_COLOR_TAG, d, pid);
      });
      usdColorAttrib.Set(usdClrs);
    }
    // UVs
    if (points.hasProperty(ATTRIB_UV_TAG)) {
      pxr::UsdGeomPrimvarsAPI primvarAPI(usdMesh);
      auto stPrimvar = primvarAPI.GetPrimvar(pxr::TfToken("st"));
      if (stPrimvar) {
        primvarAPI.RemovePrimvar(pxr::TfToken("st"));
      }
      stPrimvar = primvarAPI.CreatePrimvar(
          pxr::TfToken("st"),
          pxr::SdfSchema::GetInstance().FindType(pxr::TfType::Find(pxr::VtArray<pxr::GfVec2f>())));

      pxr::VtArray<pxr::GfVec2f> usdUVs(points.size());
      pol(range(points.size()), [&usdUVs, pts = view<space>({}, points.attr32())](PrimIndex pid) {
        usdUVs[pid][0] = pts(ATTRIB_UV_TAG, 0, pid);
        usdUVs[pid][1] = pts(ATTRIB_UV_TAG, 1, pid);
      });
      stPrimvar.Set(usdUVs, time);
    }

    const AttrVector& verts = geom.verts();
    const auto& geomPrims = geom.globalPrims();

    // Indexing
    USD_MESH_ATTR(usdVertAttrib, FaceVertexIndicesAttr);
    USD_MESH_ATTR(usdPolySizeAttrib, FaceVertexCountsAttr);
    pxr::VtArray<PrimIndex> usdPolySizes(geomPrims.size());
    // poly-wise
    std::vector<PrimIndex> numPolys(geomPrims.size()), polyDsts(geomPrims.size());
    // vert-wise
    std::vector<PrimIndex> polySizes(geomPrims.size() + 1), polyOffsets(geomPrims.size() + 1);
    pol(zip(numPolys, polySizes, geomPrims),
        [&geom](PrimIndex& numPoly, PrimIndex& polySize,
                const zs::tuple<PrimTypeIndex, PrimIndex>& entryNo) {
          switch (zs::get<0>(entryNo)) {
            case PrimitiveStorage::Poly_: {
              const PolyPrimContainer& geomPolys = *geom.localPolyPrims();
              numPoly = 1;
              polySize = geomPolys.prims().attr32().begin(POLY_SIZE_TAG, dim_c<1>,
                                                          prim_id_c)[zs::get<1>(entryNo)];
              break;
            }
            case PrimitiveStorage::Tri_: {
              numPoly = 1;
              polySize = 3;
              break;
            }
            case PrimitiveStorage::Line_: {
              numPoly = 1;
              polySize = 2;
              break;
            }
            case PrimitiveStorage::Point_: {
              numPoly = 1;
              polySize = 1;
              break;
            }
            default:
              polySize = 0;
              numPoly = 0;
          };
        });
    exclusive_scan(pol, zs::begin(numPolys), zs::end(numPolys), zs::begin(polyDsts), 0);
    exclusive_scan(pol, zs::begin(polySizes), zs::end(polySizes), zs::begin(polyOffsets), 0);
    pxr::VtArray<PrimIndex> usdPolyIndices(polyOffsets.back());  // faceSize * indicesSizePerFace

    pol(enumerate(numPolys, polyDsts, polySizes, polyOffsets, geomPrims),
        [&usdPolyIndices, &usdPolySizes, &geom,
         verts = verts.attr32().begin(POINT_ID_TAG, dim_c<1>, prim_id_c)](
            u32 polyId, PrimIndex numPoly, PrimIndex polyDst, PrimIndex usdPolySize,
            PrimIndex usdPolyOffset, const zs::tuple<PrimTypeIndex, PrimIndex>& entryNo) mutable {
          if (numPoly == 0) return;
          PrimIndex localPrimId = zs::get<1>(entryNo);
          // poly size
          usdPolySizes[polyDst] = usdPolySize;
          // poly indices
          switch (zs::get<0>(entryNo)) {
            case PrimitiveStorage::Poly_: {
              const PolyPrimContainer& geomPolys = *geom.localPolyPrims();
              auto localPolyOffset = geomPolys.prims().attr32().begin(POLY_OFFSET_TAG, dim_c<1>,
                                                                      prim_id_c)[localPrimId];
              for (PrimIndex d = 0; d < usdPolySize; ++d)
                usdPolyIndices[usdPolyOffset + d] = verts[localPolyOffset + d];
              break;
            }
            case PrimitiveStorage::Tri_: {
              const TriPrimContainer& geomTris = *geom.localTriPrims();
              auto tri = geomTris.prims().attr32().begin(ELEM_VERT_ID_TAG, dim_c<3>,
                                                         prim_id_c)[localPrimId];
              assert(usdPolySize == 3);
              for (PrimIndex d = 0; d < 3; ++d) usdPolyIndices[usdPolyOffset + d] = tri[d];
              break;
            }
            case PrimitiveStorage::Line_: {
              const LinePrimContainer& geomLines = *geom.localLinePrims();
              auto line = geomLines.prims().attr32().begin(ELEM_VERT_ID_TAG, dim_c<2>,
                                                           prim_id_c)[localPrimId];
              assert(usdPolySize == 2);
              for (PrimIndex d = 0; d < 2; ++d) usdPolyIndices[usdPolyOffset + d] = line[d];
              break;
            }
            case PrimitiveStorage::Point_: {
              const PointPrimContainer& geomPts = *geom.localPointPrims();
              auto pt = geomPts.prims().attr32().begin(ELEM_VERT_ID_TAG, dim_c<1>,
                                                       prim_id_c)[localPrimId];
              assert(usdPolySize == 1);
              usdPolyIndices[usdPolyOffset] = pt;
              break;
            }
            default:;
          }
        });
    usdVertAttrib.Set(usdPolyIndices);
    usdPolySizeAttrib.Set(usdPolySizes);

#  undef USD_MESH_ATTR
  }
  void assign_primitive_to_usdprim(const ZsPrimitive& geom, ScenePrimHolder& scenePrim, double time,
                                   const source_location& loc) {
    assign_primitive_to_usdprim(geom, scenePrim.get(), time, loc);
  }

#endif

}  // namespace zs
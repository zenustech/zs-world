#include "PrimitiveTransform.hpp"

#include <mutex>

#include "Primitive.hpp"
#include "zensim/execution/ConcurrencyPrimitive.hpp"

#if ZS_ENABLE_OPENMP
#  include "zensim/omp/execution/ExecutionPolicy.hpp"
#else
#  include "zensim/execution/ExecutionPolicy.hpp"
#endif

namespace zs {

  void assign_visual_mesh_to_pointmesh(const PrimitiveStorage &src, ZsPointMesh &dst,
                                       const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    const auto &points = src.points();
    auto pointPrims = src.localPointPrims();

    dst.clear();

    auto &nodes = dst.nodes;
    auto &uvs = dst.uvs;
    auto &norms = dst.norms;
    auto &colors = dst.colors;
    auto &elems = dst.elems;

    elems.resize(pointPrims->prims().size());
    const auto numPts = points.size();
    nodes.resize(numPts);
    uvs.resize(0);
    if (points.hasProperty(ATTRIB_NORMAL_TAG) && points.getPropertySize(ATTRIB_NORMAL_TAG) == 3)
      norms.resize(numPts);
    if (points.hasProperty(ATTRIB_COLOR_TAG) && points.getPropertySize(ATTRIB_COLOR_TAG) == 3)
      colors.resize(numPts);

    /// accessors
    auto pointView = view<space>({}, points.attr32());

    /// assign points
    pol(range(points.size()), [&](PrimIndex pid) {
      auto pos = pointView.pack(dim_c<3>, ATTRIB_POS_TAG, pid);
      nodes[pid] = {pos[0], pos[1], pos[2]};

      if (norms.size() == numPts) {
        auto nrm = pointView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, pid);
        norms[pid] = {nrm[0], nrm[1], nrm[2]};
      }
      if (colors.size() == numPts) {
        auto clr = pointView.pack(dim_c<3>, ATTRIB_COLOR_TAG, pid);
        colors[pid] = {clr[0], clr[1], clr[2]};
      }
    });
    /// assign tri prims
    pol(zip(range(pointPrims->prims().attr32(), ELEM_VERT_ID_TAG, dim_c<1>, prim_id_c), elems),
        [](const auto &src, auto &dst) { dst[0] = src; });
  }

  void assign_visual_mesh_to_linemesh(const PrimitiveStorage &src, ZsLineMesh &dst,
                                      const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    const auto &points = src.points();
    auto linePrims = src.localLinePrims();

    dst.clear();

    auto &nodes = dst.nodes;
    auto &uvs = dst.uvs;
    auto &norms = dst.norms;
    auto &colors = dst.colors;
    auto &elems = dst.elems;

    elems.resize(linePrims->prims().size());
    const auto numPts = points.size();
    nodes.resize(numPts);
    uvs.resize(0);
    if (points.hasProperty(ATTRIB_NORMAL_TAG) && points.getPropertySize(ATTRIB_NORMAL_TAG) == 3)
      norms.resize(numPts);
    if (points.hasProperty(ATTRIB_COLOR_TAG) && points.getPropertySize(ATTRIB_COLOR_TAG) == 3)
      colors.resize(numPts);

    /// accessors
    auto pointView = view<space>({}, points.attr32());

    /// assign points
    pol(range(points.size()), [&](PrimIndex pid) {
      auto pos = pointView.pack(dim_c<3>, ATTRIB_POS_TAG, pid);
      nodes[pid] = {pos[0], pos[1], pos[2]};

      if (norms.size() == numPts) {
        auto nrm = pointView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, pid);
        norms[pid] = {nrm[0], nrm[1], nrm[2]};
      }
      if (colors.size() == numPts) {
        auto clr = pointView.pack(dim_c<3>, ATTRIB_COLOR_TAG, pid);
        colors[pid] = {clr[0], clr[1], clr[2]};
      }
    });
    /// assign tri prims
    pol(zip(range(linePrims->prims().attr32(), ELEM_VERT_ID_TAG, dim_c<2>, prim_id_c), elems),
        [](const auto &src, auto &dst) {
          for (int d = 0; d < 2; ++d) dst[d] = src[d];
        });
  }

  void assign_visual_mesh_to_trimesh(const PrimitiveStorage &src, ZsTriMesh &dst,
                                     const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    const auto &points = src.points();
    auto triPrims = src.localTriPrims();

    dst.clear();

    auto &nodes = dst.nodes;
    auto &uvs = dst.uvs;
    auto &norms = dst.norms;
    auto &colors = dst.colors;
    auto &elems = dst.elems;

    dst.texturePath = src.details().texturePath();

    elems.resize(triPrims->prims().size());
    const auto numPts = points.size();
    nodes.resize(numPts);
    if (points.hasProperty(ATTRIB_UV_TAG) && points.getPropertySize(ATTRIB_UV_TAG) == 2)
      uvs.resize(numPts);
    if (points.hasProperty(ATTRIB_NORMAL_TAG) && points.getPropertySize(ATTRIB_NORMAL_TAG) == 3)
      norms.resize(numPts);
    if (points.hasProperty(ATTRIB_COLOR_TAG) && points.getPropertySize(ATTRIB_COLOR_TAG) == 3)
      colors.resize(numPts);

    /// accessors
    auto pointView = view<space>({}, points.attr32());

    /// assign points
    pol(range(points.size()), [&](PrimIndex pid) {
      auto pos = pointView.pack(dim_c<3>, ATTRIB_POS_TAG, pid);
      nodes[pid] = {pos[0], pos[1], pos[2]};

      if (uvs.size() == numPts) {
        auto uv = pointView.pack(dim_c<2>, ATTRIB_UV_TAG, pid);
        uvs[pid] = {uv[0], uv[1]};
      }
      if (norms.size() == numPts) {
        auto nrm = pointView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, pid);
        norms[pid] = {nrm[0], nrm[1], nrm[2]};
      }
      if (colors.size() == numPts) {
        auto clr = pointView.pack(dim_c<3>, ATTRIB_COLOR_TAG, pid);
        colors[pid] = {clr[0], clr[1], clr[2]};
      }
    });
    /// assign tri prims
    pol(zip(range(triPrims->prims().attr32(), ELEM_VERT_ID_TAG, dim_c<3>, prim_id_c), elems),
        [](const auto &src, auto &dst) {
          for (int d = 0; d < 3; ++d) dst[d] = src[d];
        });
  }

  void assign_simple_mesh_to_zsmesh(const PrimitiveStorage &src, ZsTriMesh *pTriMesh,
                                    ZsLineMesh *pLineMesh, ZsPointMesh *pPointMesh,
                                    const source_location &loc) {
    const auto &points = src.points();
    const auto &verts = src.verts();
    const auto &pointPrims = src.localPointPrims();
    const auto &linePrims = src.localLinePrims();
    const auto &triPrims = src.localTriPrims();
    // const auto &polyPrims = src.localPolyPrims();

#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    /// @note for parallel maintenance
    std::vector<std::mutex> ptLocks(points.size());
    /**
     *  @brief split prim-verts [pt, line, tri, (no poly here)] by divergent [nrm, clr, uv,
     * tan] attributes
     *  @note p: prim type, v: prim index, d: dimension
     */
    // using PrimTypeVertDimId = zs::tuple<prim_type_e, PrimIndex, PrimIndex>;
    // std::vector<std::vector<PrimTypeVertDimId>> pvdIds(points.size());
    /** @brief split verts [pt, line, tri, (no poly here)] by divergent [nrm, clr, uv,
     * tan] attributes
     *  @note since ZsPrimitive's upgrade (compared to zeno), attributes are always stored on
     * [verts] rather than on [prims] (e.g. "uv0/1/2" on [tris] prims).
     *  @note prefer storing vert index rather than <prim type, prim index, dimension>
     */
    std::vector<std::vector<PrimIndex>> vertIdsPerPoint(points.size());

    /// split points by attributes [prop] of basic type [typec, e.g. wrapt<f32>] of dim [dimc, e.g.
    /// wrapv<3>{}]
    auto splitPoints = [&](SmallString prop, auto typec, auto dimc) {
      constexpr int N = dimc();
      using T = typename remove_reference_t<decltype(typec)>::type;
      using Prop = zs::vec<T, N>;
      std::vector<std::vector<Prop>> pvdProps(points.size());

      /// initialize pvdProps by current global pvdIds
      pol(range(points.size()), [&, vertView = view<space>({}, verts.attr32())](PrimIndex pid) {
        auto &variantPts = vertIdsPerPoint[pid];
        auto numVariants = variantPts.size();
        auto &props = pvdProps[pid];
        props.reserve(numVariants);
        for (PrimIndex j = 0; j < numVariants; ++j)
          props.push_back(
              vertView.pack(dim_c<RM_CVREF_T(dimc)::value>, prop, variantPts[j], typec));
      });

      /// iterate [pt, line, tri], with corresponding primDimC [1, 2, 3] to search divergent
      /// vertices directing to the same point
      auto iteratePrims
          = [&, vertView = view<space>({}, verts.attr32())](const auto &localPrims, auto primDimC) {
              const auto &prims = localPrims->prims();
              pol(enumerate(range(prims.attr32(), ELEM_VERT_ID_TAG,
                                  dim_c<RM_CVREF_T(primDimC)::value>, prim_id_c)),
                  [&](PrimIndex ei, auto vids) {
                    constexpr int dime = RM_CVREF_T(primDimC)::value;
                    for (int d = 0; d < dime; ++d) {
                      PrimIndex vid, pid;
                      if constexpr (is_integral_v<RM_CVREF_T(vids)>) {
                        vid = vids;
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      } else {
                        vid = vids[d];
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      }
                      // prop on vert
                      Prop vProp = vertView.pack(dim_c<RM_CVREF_T(dimc)::value>, prop, vid, typec);
                      // [deprecated]: prim type [localPrims], prim index: ei, dim index: d
                      // store vert index directly
                      // constexpr PrimTypeIndex primTypeId = get_prim_container_type_index<
                      //     remove_const_t<typename RM_CVREF_T(localPrims)::element_type>>();
                      {
                        ptLocks[pid].lock();
                        auto &variantPtProps = pvdProps[pid];
                        auto numVariants = variantPtProps.size();
                        PrimIndex j = 0;
                        for (; j < numVariants; ++j)
                          if (variantPtProps[j] == vProp) break;
                        if (j == numVariants) {  // no match, should insert as an candidate
                          vertIdsPerPoint[pid].push_back(vid);
                          pvdProps[pid].push_back(vProp);
                        }
                        ptLocks[pid].unlock();
                      }
                    }
                  });
            };
      iteratePrims(pointPrims, wrapv<1>{});
      iteratePrims(linePrims, wrapv<2>{});
      iteratePrims(triPrims, wrapv<3>{});
      /// @note instead of directly iterating verts, iterate prims may avoid "dead" verts (not
      /// referenced by any prims).
      /// @note [poly] should not exist, because we are dealing with [simple_mesh] here
    };

    assign_attribs_from_prim_to_vert(const_cast<PrimitiveStorage &>(src),
                                     {{ATTRIB_UV_TAG, 2},
                                      {ATTRIB_NORMAL_TAG, 3},
                                      {ATTRIB_COLOR_TAG, 3},
                                      {ATTRIB_TEXTURE_ID_TAG, 2},
                                      {ATTRIB_TANGENT_TAG, 3}},
                                     loc);

    bool hasVertUv = false, hasVertNrm = false, hasVertClr = false, hasVertTan = false,
         hasVertTexId = false;
    bool hasPointUv = false, hasPointNrm = false, hasPointClr = false, hasPointTan = false,
         hasPointTexId = false;
    /// split points by [clr, nrm, tan, uv]
    if (verts.hasProperty(ATTRIB_UV_TAG)) {
      splitPoints(ATTRIB_UV_TAG, wrapt<f32>{}, wrapv<2>{});
      hasVertUv = true;
    } else if (points.hasProperty(ATTRIB_UV_TAG)) {
      hasPointUv = true;
    }

    if (verts.hasProperty(ATTRIB_NORMAL_TAG)) {
      splitPoints(ATTRIB_NORMAL_TAG, wrapt<f32>{}, wrapv<3>{});
      hasVertNrm = true;
    } else if (points.hasProperty(ATTRIB_NORMAL_TAG)) {
      hasPointNrm = true;
    }

    if (verts.hasProperty(ATTRIB_COLOR_TAG)) {
      splitPoints(ATTRIB_COLOR_TAG, wrapt<f32>{}, wrapv<3>{});
      hasVertClr = true;
    } else if (points.hasProperty(ATTRIB_COLOR_TAG)) {
      hasPointClr = true;
    }

    if (verts.hasProperty(ATTRIB_TANGENT_TAG)) {
      splitPoints(ATTRIB_TANGENT_TAG, wrapt<f32>{}, wrapv<3>{});
      hasVertTan = true;
    } else if (points.hasProperty(ATTRIB_TANGENT_TAG)) {
      hasPointTan = true;
    }

    if (verts.hasProperty(ATTRIB_TEXTURE_ID_TAG)) {
      splitPoints(ATTRIB_TEXTURE_ID_TAG, wrapt<i32>{}, wrapv<2>{});
      hasVertTexId = true;
    } else if (points.hasProperty(ATTRIB_TEXTURE_ID_TAG)) {
      hasPointTexId = true;
    }
    /// which prim, which dimension

    /// if splitting by the above attributes does nothing, then split points by divergent verts
    if (!hasVertUv && !hasVertNrm && !hasVertClr && !hasVertTan && !hasVertTexId) {
      /// iterate [pt, line, tri], with corresponding primDimC [1, 2, 3] to search divergent
      /// vertices directing to the same point
      auto iteratePrims
          = [&, vertView = view<space>({}, verts.attr32())](const auto &localPrims, auto primDimC) {
              const auto &prims = localPrims->prims();
              pol(enumerate(range(prims.attr32(), ELEM_VERT_ID_TAG,
                                  dim_c<RM_CVREF_T(primDimC)::value>, prim_id_c)),
                  [&](PrimIndex ei, auto vids) {
                    constexpr int dime = RM_CVREF_T(primDimC)::value;
                    for (int d = 0; d < dime; ++d) {
                      PrimIndex vid, pid;
                      if constexpr (is_integral_v<RM_CVREF_T(vids)>) {
                        vid = vids;
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      } else {
                        vid = vids[d];
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      }
                      {
                        ptLocks[pid].lock();
                        auto &variantPts = vertIdsPerPoint[pid];
                        auto numVariants = variantPts.size();
                        PrimIndex j = 0;
                        for (; j < numVariants; ++j)
                          if (variantPts[j] == vid
                              || vertView(POINT_ID_TAG, variantPts[j], prim_id_c) == pid)
                            break;
                        if (j == numVariants)  // no match, should insert as an candidate
                          variantPts.push_back(vid);
                        ptLocks[pid].unlock();
                      }
                    }
                  });
            };
      iteratePrims(pointPrims, wrapv<1>{});
      iteratePrims(linePrims, wrapv<2>{});
      iteratePrims(triPrims, wrapv<3>{});
    }

    std::vector<PrimIndex> numVariantsPerPoint(points.size() + 1),
        prevPointOffsets(points.size() + 1);
    pol(zip(vertIdsPerPoint, numVariantsPerPoint),
        [](const auto &ids, PrimIndex &n) { n = ids.size(); });
    exclusive_scan(pol, std::begin(numVariantsPerPoint), std::end(numVariantsPerPoint),
                   std::begin(prevPointOffsets), 0, zs::plus<PrimIndex>{}, loc);

    /// split [points] and assign [verts] attributes [uv, nrm, clr, tan] to [points]

    /// @brief initialize points of visual mesh
    /// @note skinning pos (if exist) should precede pos tag
    auto posLabel
        = points.hasProperty(ATTRIB_SKINNING_POS_TAG) ? ATTRIB_SKINNING_POS_TAG : ATTRIB_POS_TAG;

    auto setupZsMeshPoints = [&](auto &mesh) {
      mesh.nodes.resize(prevPointOffsets.back());
      mesh.vids.resize(mesh.nodes.size());
      if (hasVertUv || hasPointUv)
        mesh.uvs.resize(mesh.nodes.size());
      else
        mesh.uvs.clear();
      if (hasVertNrm || hasPointNrm)
        mesh.norms.resize(mesh.nodes.size());
      else
        mesh.norms.clear();
      if (hasVertClr || hasPointClr)
        mesh.colors.resize(mesh.nodes.size());
      else
        mesh.colors.clear();
      if (hasVertTan || hasPointTan)
        mesh.tans.resize(mesh.nodes.size());
      else
        mesh.tans.clear();
      if (hasVertTexId || hasPointTexId)
        mesh.texids.resize(mesh.nodes.size());
      else
        mesh.texids.clear();

      pol(zip(range(points.size()), prevPointOffsets),
          [&, pointView = view<space>({}, points.attr32()),
           vertView = view<space>({}, verts.attr32())](PrimIndex pid, PrimIndex offset) mutable {
            const auto &ids = vertIdsPerPoint[pid];
            auto sz = ids.size();
            for (PrimIndex j = 0; j < sz; ++j) {
              auto vid = ids[j];
              // [verts] init
              PrimIndex dstVid = offset + j;
              // [points] attributes init
              {
                auto v = pointView.pack(dim_c<3>, posLabel, pid);
                mesh.nodes[dstVid] = {v[0], v[1], v[2]};
                mesh.vids[dstVid] = vid;
              }

              if (hasVertUv) {
                auto v = vertView.pack(dim_c<2>, ATTRIB_UV_TAG, vid);
                mesh.uvs[dstVid] = {v[0], v[1]};
              } else if (hasPointUv) {
                auto v = pointView.pack(dim_c<2>, ATTRIB_UV_TAG, pid);
                mesh.uvs[dstVid] = {v[0], v[1]};
              }

              if (hasVertNrm) {
                auto v = vertView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, vid);
                mesh.norms[dstVid] = {v[0], v[1], v[2]};
              } else if (hasPointNrm) {
                auto v = pointView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, pid);
                mesh.norms[dstVid] = {v[0], v[1], v[2]};
              }

              if (hasVertClr) {
                auto v = vertView.pack(dim_c<3>, ATTRIB_COLOR_TAG, vid);
                mesh.colors[dstVid] = {v[0], v[1], v[2]};
              } else if (hasPointClr) {
                auto v = pointView.pack(dim_c<3>, ATTRIB_COLOR_TAG, pid);
                mesh.colors[dstVid] = {v[0], v[1], v[2]};
              }

              if (hasVertTan) {
                auto v = vertView.pack(dim_c<3>, ATTRIB_TANGENT_TAG, vid);
                mesh.tans[dstVid] = {v[0], v[1], v[2]};
              } else if (hasPointTan) {
                auto v = pointView.pack(dim_c<3>, ATTRIB_TANGENT_TAG, pid);
                mesh.tans[dstVid] = {v[0], v[1], v[2]};
              }

              if (hasVertTexId) {
                auto v = vertView.pack(dim_c<2>, ATTRIB_TEXTURE_ID_TAG, vid, prim_id_c);
                mesh.texids[dstVid] = {v[0], v[1]};
              } else if (hasPointTexId) {
                auto v = pointView.pack(dim_c<2>, ATTRIB_TEXTURE_ID_TAG, pid, prim_id_c);
                mesh.texids[dstVid] = {v[0], v[1]};
              }
            }
          });
    };
    if (pTriMesh) setupZsMeshPoints(*pTriMesh);
    if (pLineMesh) setupZsMeshPoints(*pLineMesh);
    if (pPointMesh) setupZsMeshPoints(*pPointMesh);

    /// iterate [pt, line, tri], with corresponding primDimC [1, 2, 3],
    /// and squash [prim-vert-point] mapping to [prim-point].
    auto remapPrimIndices = [&](const auto &srcLocalPrims, auto &prims, auto primDimC) {
      static_assert(std::tuple_size_v<typename remove_reference_t<decltype(prims)>::value_type>
                        == RM_REF_T(primDimC)::value,
                    "element dimension mismatch");
      auto &srcPrims = srcLocalPrims->prims();
      prims.resize(srcPrims.size());
      pol(enumerate(range(srcPrims.attr32(), ELEM_VERT_ID_TAG, dim_c<RM_CVREF_T(primDimC)::value>,
                          prim_id_c),
                    prims),
          [&, vertView = view<space>({}, verts.attr32()),
           points = view<space>({}, points.attr32())](PrimIndex ei, const auto &originalVids,
                                                      auto &vids) {
            constexpr int dime = RM_REF_T(primDimC)::value;

            for (int d = 0; d < dime; ++d) {
              PrimIndex vid, pid;
              if constexpr (is_integral_v<RM_CVREF_T(originalVids)>)
                vid = originalVids;
              else
                vid = originalVids[d];
              pid = vertView(POINT_ID_TAG, vid, prim_id_c);
              const auto offset = prevPointOffsets[pid];
              const auto &ids = vertIdsPerPoint[pid];
              PrimIndex j = 0;
              for (; j < ids.size(); ++j)
                /// @note find the candidate that is the exact same vert,
                ///  or another vert pointing to the same point (pid)
                if (ids[j] == vid || vertView(POINT_ID_TAG, ids[j], prim_id_c) == pid) break;
              assert(j != ids.size());

              // [prims] indices update
              vids[d] = offset + j;
            }
          });
    };

    if (pPointMesh) remapPrimIndices(pointPrims, pPointMesh->elems, wrapv<1>{});
    if (pLineMesh) remapPrimIndices(linePrims, pLineMesh->elems, wrapv<2>{});
    if (pTriMesh) remapPrimIndices(triPrims, pTriMesh->elems, wrapv<3>{});
  }

  void write_zs_mesh_points_to_simple_mesh_verts(PrimitiveStorage &geom, ZsTriMesh *pTriMesh,
                                                 ZsLineMesh *pLineMesh, ZsPointMesh *pPointMesh,
                                                 const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    std::map<std::string, int> propsToWrite;
    auto gatherProps = [&](auto &zsmesh) {
      if (zsmesh.nodes.size() && zsmesh.vids.size() == zsmesh.nodes.size()) {
        if (zsmesh.uvs.size() == zsmesh.nodes.size()) propsToWrite[ATTRIB_UV_TAG] = 2;
        if (zsmesh.norms.size() == zsmesh.nodes.size()) propsToWrite[ATTRIB_NORMAL_TAG] = 3;
        if (zsmesh.colors.size() == zsmesh.nodes.size()) propsToWrite[ATTRIB_COLOR_TAG] = 3;
        if (zsmesh.tans.size() == zsmesh.nodes.size()) propsToWrite[ATTRIB_TANGENT_TAG] = 3;
        if (zsmesh.texids.size() == zsmesh.nodes.size()) propsToWrite[ATTRIB_TEXTURE_ID_TAG] = 2;
      }
    };
    if (pTriMesh) gatherProps(*pTriMesh);
    if (pLineMesh) gatherProps(*pLineMesh);
    if (pPointMesh) gatherProps(*pPointMesh);

    auto &verts = geom.verts();

    std::vector<PropertyTag> vtTags;
    for (const auto &[name, dim] : propsToWrite) vtTags.push_back(PropertyTag{name, dim});
    verts.appendProperties32(pol, vtTags);

    /// accumulate
    auto iterateMesh = [&](auto &zsmesh) {
      if (zsmesh.nodes.size() && zsmesh.vids.size() == zsmesh.nodes.size()) {
        pol(
            range(zsmesh.nodes.size()),
            [&, verts = view<space>({}, verts.attr32())](PrimIndex pid) mutable {
              auto vid = zsmesh.vids[pid];

              if (zsmesh.uvs.size() == zsmesh.nodes.size())
                for (int d = 0; d < 2; ++d) verts(ATTRIB_UV_TAG, d, vid) = zsmesh.uvs[pid][d];

              if (zsmesh.norms.size() == zsmesh.nodes.size())
                for (int d = 0; d < 3; ++d) verts(ATTRIB_NORMAL_TAG, d, vid) = zsmesh.norms[pid][d];

              if (zsmesh.colors.size() == zsmesh.nodes.size())
                for (int d = 0; d < 3; ++d) verts(ATTRIB_COLOR_TAG, d, vid) = zsmesh.colors[pid][d];

              if (zsmesh.tans.size() == zsmesh.nodes.size())
                for (int d = 0; d < 3; ++d) verts(ATTRIB_TANGENT_TAG, d, vid) = zsmesh.tans[pid][d];

              /// @note be cautious about the possible divergence
              if (zsmesh.texids.size() == zsmesh.nodes.size())
                for (int d = 0; d < 2; ++d)
                  verts(ATTRIB_TEXTURE_ID_TAG, d, vid, prim_id_c) = zsmesh.texids[pid][d];
            },
            loc);
      }
    };
    if (pTriMesh) iterateMesh(*pTriMesh);
    if (pLineMesh) iterateMesh(*pLineMesh);
    if (pPointMesh) iterateMesh(*pPointMesh);
  }

  void assign_attribs_from_prim_to_vert(PrimitiveStorage &geom,
                                        const std::vector<PropertyTag> &attrTags_,
                                        const source_location &loc) {
    auto &points = geom.points();
    auto &verts = geom.verts();

    auto pointPrims = geom.localPointPrims();
    auto linePrims = geom.localLinePrims();
    auto triPrims = geom.localTriPrims();
    auto polyPrims = geom.localPolyPrims();

#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    /// @note sanity check
    for (const auto &prop : attrTags_) {
      if (verts.hasProperty(prop.name)) {
        assert(verts.getPropertySize(prop.name) == prop.numChannels);
      }
    }

    /// @brief assign per-prim [point, line, tri] attributes to [verts]
    auto iteratePrims = [&](const AttrVector &prims, auto primDimC) {
      using ChnOffsetT = decltype(declval<PropertyTag>().numChannels);
      using ChnSizeT = typename RM_CVREF_T(prims.attr32())::channel_counter_type;
      std::vector<PropertyTag> tbdAttrTags;
      for (const auto &attrTag : attrTags_) {
        if (prims.hasProperty(attrTag.name)) {
          if (attrTag.numChannels == prims.getPropertySize(attrTag.name)) {
            tbdAttrTags.push_back(attrTag);
          }
        }
      }
      std::vector<zs::tuple<ChnOffsetT, ChnOffsetT, ChnSizeT>> attrTags;
      if (tbdAttrTags.size()) verts.appendProperties32(pol, tbdAttrTags);
      for (const auto &attrTag : tbdAttrTags) {
        if (attrTag.numChannels == verts.getPropertySize(attrTag.name))
          attrTags.emplace_back(verts.getPropertyOffset(attrTag.name),
                                prims.getPropertyOffset(attrTag.name), attrTag.numChannels);
      }
      pol(range(prims.size()), [&, primView = view<space>(prims.attr32()),
                                primIdOffset = prims.getPropertyOffset(ELEM_VERT_ID_TAG),
                                vertView = view<space>({}, verts.attr32())](PrimIndex ei) mutable {
        constexpr int dime = RM_CVREF_T(primDimC)::value;
        auto vids = primView.pack(dim_c<dime>, primIdOffset, ei, prim_id_c);
        for (int d = 0; d < dime; ++d) {
          auto vid = vids[d];

          // prop on prim
          for (const auto &[vertPropOffset, primPropOffset, propSz] : attrTags) {
            for (ChnOffsetT chn = 0; chn != propSz; ++chn) {
              // assign prop to vert
              vertView(vertPropOffset + chn, vid) = primView(primPropOffset + chn, ei);
            }
          }
        }
      });
    };
    iteratePrims(pointPrims->prims(), wrapv<1>{});
    iteratePrims(linePrims->prims(), wrapv<2>{});
    iteratePrims(triPrims->prims(), wrapv<3>{});

    /// @brief assign poly prim attributes to [verts]
    {
      const auto &prims = polyPrims->prims();
      using ChnOffsetT = decltype(declval<PropertyTag>().numChannels);
      using ChnSizeT = typename RM_CVREF_T(prims.attr32())::channel_counter_type;
      std::vector<PropertyTag> tbdAttrTags;
      for (const auto &attrTag : attrTags_) {
        if (prims.hasProperty(attrTag.name)) {
          if (attrTag.numChannels == prims.getPropertySize(attrTag.name)) {
            tbdAttrTags.push_back(attrTag);
          }
        }
      }
      std::vector<zs::tuple<ChnOffsetT, ChnOffsetT, ChnSizeT>> attrTags;
      if (tbdAttrTags.size()) verts.appendProperties32(pol, tbdAttrTags);
      for (const auto &attrTag : tbdAttrTags) {
        if (attrTag.numChannels == verts.getPropertySize(attrTag.name))
          attrTags.emplace_back(verts.getPropertyOffset(attrTag.name),
                                prims.getPropertyOffset(attrTag.name), attrTag.numChannels);
      }
      pol(range(prims.size()),
          [&, polyView = view<space>(prims.attr32()),
           polyOffsetChn = prims.getPropertyOffset(POLY_OFFSET_TAG),
           polySizeChn = prims.getPropertyOffset(POLY_SIZE_TAG),
           vertView = view<space>({}, verts.attr32())](PrimIndex polyI) mutable {
            auto vid = polyView(polyOffsetChn, polyI, prim_id_c);
            const auto ed = vid + polyView(polySizeChn, polyI, prim_id_c);
            for (; vid != ed; ++vid) {
              // prop on prim
              for (const auto &[vertPropOffset, primPropOffset, propSz] : attrTags) {
                for (ChnOffsetT chn = 0; chn != propSz; ++chn) {
                  // assign prop to vert
                  vertView(vertPropOffset + chn, vid) = polyView(primPropOffset + chn, polyI);
                }
              }
            }
          });
    }
  }

  void assign_simple_mesh_to_visual_mesh(const PrimitiveStorage &src, PrimitiveStorage &dst,
                                         const source_location &loc) {
    dst.reset();
    const auto &points = src.points();
    const auto &verts = src.verts();
    const auto &pointPrims = src.localPointPrims();
    const auto &linePrims = src.localLinePrims();
    const auto &triPrims = src.localTriPrims();
    // const auto &polyPrims = src.localPolyPrims();

    dst.details().texturePath() = src.details().texturePath();

#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    /// @note for parallel maintenance
    std::vector<std::mutex> ptLocks(points.size());
    /**
     *  @brief split prim-verts [pt, line, tri, (no poly here)] by divergent [nrm, clr, uv,
     * tan] attributes
     *  @note p: prim type, v: prim index, d: dimension
     */
    // using PrimTypeVertDimId = zs::tuple<prim_type_e, PrimIndex, PrimIndex>;
    // std::vector<std::vector<PrimTypeVertDimId>> pvdIds(points.size());
    /** @brief split verts [pt, line, tri, (no poly here)] by divergent [nrm, clr, uv,
     * tan] attributes
     *  @note since ZsPrimitive's upgrade (compared to zeno), attributes are always stored on
     * [verts] rather than on [prims] (e.g. "uv0/1/2" on [tris] prims).
     *  @note prefer storing vert index rather than <prim type, prim index, dimension>
     */
    std::vector<std::vector<PrimIndex>> vertIdsPerPoint(points.size());

    /// split points by attributes [prop] of basic type [typec, e.g. wrapt<f32>] of dim [dimc, e.g.
    /// wrapv<3>{}]
    auto splitPoints = [&](SmallString prop, auto typec, auto dimc) {
      constexpr int N = dimc();
      using T = typename remove_reference_t<decltype(typec)>::type;
      using Prop = zs::vec<T, N>;
      std::vector<std::vector<Prop>> pvdProps(points.size());

/// initialize pvdProps by current global pvdIds
#if 0
      auto getEntryPrimId = [](const PrimTypeVertDimId &entry) { return zs::get<1>(entry); };
      auto getEntryPrimDimId = [](const PrimTypeVertDimId &entry) { return zs::get<2>(entry); };
      // [key, prim index, dim index]
      // using PrimVertId = zs::tuple<Prop, prim_type_e, PrimIndex, PrimIndex>;
      pol(range(points.size()), [&](PrimIndex pid) {
        auto &variantPts = pvdIds[pid];
        auto numVariants = variantPts.size();
        pvdProps[pid].reserve(numVariants);
        for (PrimIndex j = 0; j < numVariants; ++j) {
          const PrimTypeVertDimId &entry = variantPts[j];
          const prim_type_e primType = zs::get<0>(entry);
          const auto ei = getEntryPrimId(entry);
          const auto dimi = getEntryPrimDimId(entry);
          switch (primType) {
            case prim_type_e::Point_: {
              auto ps = view<space>({}, pointPrims->prims().attr32());
              assert(dimi == 0);
              PrimIndex vid = ps(ELEM_VERT_ID_TAG, /*dimi*/ 0, ei);
              pvdProps[j].push_back(vertView.pack(dim_c<dimc()>, prop, vid));
              break;
            }
            case prim_type_e::Line_: {
              auto ps = view<space>({}, linePrims->prims().attr32());
              assert(dimi < 2);
              PrimIndex vid = ps(ELEM_VERT_ID_TAG, dimi, ei);
              pvdProps[j].push_back(vertView.pack(dim_c<dimc()>, prop, vid));
              break;
            }
            case prim_type_e::Tri_: {
              auto ps = view<space>({}, triPrims->prims().attr32());
              assert(dimi < 3);
              PrimIndex vid = ps(ELEM_VERT_ID_TAG, dimi, ei);
              pvdProps[j].push_back(vertView.pack(dim_c<dimc()>, prop, vid));
              break;
            }
            default:
              // non reachable
              break;
          }
        }
      });
#else
      pol(range(points.size()), [&, vertView = view<space>({}, verts.attr32())](PrimIndex pid) {
        auto &variantPts = vertIdsPerPoint[pid];
        auto numVariants = variantPts.size();
        auto &props = pvdProps[pid];
        props.reserve(numVariants);
        for (PrimIndex j = 0; j < numVariants; ++j)
          props.push_back(vertView.pack(dim_c<RM_CVREF_T(dimc)::value>, prop, variantPts[j]));
      });
#endif

      /// iterate [pt, line, tri], with corresponding primDimC [1, 2, 3] to search divergent
      /// vertices directing to the same point
      auto iteratePrims
          = [&, vertView = view<space>({}, verts.attr32())](const auto &localPrims, auto primDimC) {
              const auto &prims = localPrims->prims();
              pol(enumerate(range(prims.attr32(), ELEM_VERT_ID_TAG,
                                  dim_c<RM_CVREF_T(primDimC)::value>, prim_id_c)),
                  [&](PrimIndex ei, auto vids) {
                    constexpr int dime = RM_CVREF_T(primDimC)::value;
                    for (int d = 0; d < dime; ++d) {
                      PrimIndex vid, pid;
                      if constexpr (is_integral_v<RM_CVREF_T(vids)>) {
                        vid = vids;
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      } else {
                        vid = vids[d];
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      }
                      // prop on vert
                      Prop vProp = vertView.pack(dim_c<RM_CVREF_T(dimc)::value>, prop, vid);
                      // [deprecated]: prim type [localPrims], prim index: ei, dim index: d
                      // store vert index directly
                      // constexpr PrimTypeIndex primTypeId = get_prim_container_type_index<
                      //     remove_const_t<typename RM_CVREF_T(localPrims)::element_type>>();
                      {
                        ptLocks[pid].lock();
                        auto &variantPtProps = pvdProps[pid];
                        auto numVariants = variantPtProps.size();
                        PrimIndex j = 0;
                        for (; j < numVariants; ++j)
                          if (variantPtProps[j] == vProp) break;
                        if (j == numVariants) {  // no match, should insert as an candidate
                          vertIdsPerPoint[pid].push_back(vid);
                          pvdProps[pid].push_back(vProp);
                        }
                        ptLocks[pid].unlock();
                      }
                    }
                  });
            };
      iteratePrims(pointPrims, wrapv<1>{});
      iteratePrims(linePrims, wrapv<2>{});
      iteratePrims(triPrims, wrapv<3>{});
      /// @note instead of directly iterating verts, iterate prims may avoid "dead" verts (not
      /// referenced by any prims).
      /// @note [poly] should not exist, because we are dealing with [simple_mesh] here
    };

    assign_attribs_from_prim_to_vert(
        const_cast<PrimitiveStorage &>(src),
        {{ATTRIB_NORMAL_TAG, 3}, {ATTRIB_COLOR_TAG, 3}, {ATTRIB_TANGENT_TAG, 3}}, loc);

    bool hasVertUv = false, hasVertNrm = false, hasVertClr = false, hasVertTan = false;
    bool hasPointUv = false, hasPointNrm = false, hasPointClr = false, hasPointTan = false;
    std::vector<PropertyTag> ptProps{{ATTRIB_POS_TAG, 3}};
    /// split points by [clr, nrm, tan, uv]
    if (verts.hasProperty(ATTRIB_UV_TAG)) {
      splitPoints(ATTRIB_UV_TAG, wrapt<f32>{}, wrapv<2>{});
      ptProps.push_back(PropertyTag{ATTRIB_UV_TAG, 2});
      hasVertUv = true;
    } else if (points.hasProperty(ATTRIB_UV_TAG)) {
      ptProps.push_back(PropertyTag{ATTRIB_UV_TAG, 2});
      hasPointUv = true;
    }

    if (verts.hasProperty(ATTRIB_NORMAL_TAG)) {
      splitPoints(ATTRIB_NORMAL_TAG, wrapt<f32>{}, wrapv<3>{});
      ptProps.push_back(PropertyTag{ATTRIB_NORMAL_TAG, 3});
      hasVertNrm = true;
    } else if (points.hasProperty(ATTRIB_NORMAL_TAG)) {
      ptProps.push_back(PropertyTag{ATTRIB_NORMAL_TAG, 3});
      hasPointNrm = true;
    }

    if (verts.hasProperty(ATTRIB_COLOR_TAG)) {
      splitPoints(ATTRIB_COLOR_TAG, wrapt<f32>{}, wrapv<3>{});
      ptProps.push_back(PropertyTag{ATTRIB_COLOR_TAG, 3});
      hasVertClr = true;
    } else if (points.hasProperty(ATTRIB_COLOR_TAG)) {
      ptProps.push_back(PropertyTag{ATTRIB_COLOR_TAG, 3});
      hasPointClr = true;
    }

    if (verts.hasProperty(ATTRIB_TANGENT_TAG)) {
      splitPoints(ATTRIB_TANGENT_TAG, wrapt<f32>{}, wrapv<3>{});
      ptProps.push_back(PropertyTag{ATTRIB_TANGENT_TAG, 3});
      hasVertTan = true;
    } else if (points.hasProperty(ATTRIB_TANGENT_TAG)) {
      ptProps.push_back(PropertyTag{ATTRIB_TANGENT_TAG, 3});
      hasPointTan = true;
    }
    /// which prim, which dimension

    /// if splitting by the above attributes does nothing, then split points by divergent verts
    if (!hasVertUv && !hasVertNrm && !hasVertClr && !hasVertTan) {
      /// iterate [pt, line, tri], with corresponding primDimC [1, 2, 3] to search divergent
      /// vertices directing to the same point
      auto iteratePrims
          = [&, vertView = view<space>({}, verts.attr32())](const auto &localPrims, auto primDimC) {
              const auto &prims = localPrims->prims();
              pol(enumerate(range(prims.attr32(), ELEM_VERT_ID_TAG,
                                  dim_c<RM_CVREF_T(primDimC)::value>, prim_id_c)),
                  [&](PrimIndex ei, auto vids) {
                    constexpr int dime = RM_CVREF_T(primDimC)::value;
                    for (int d = 0; d < dime; ++d) {
                      PrimIndex vid, pid;
                      if constexpr (is_integral_v<RM_CVREF_T(vids)>) {
                        vid = vids;
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      } else {
                        vid = vids[d];
                        pid = vertView(POINT_ID_TAG, vid, prim_id_c);
                      }
                      {
                        ptLocks[pid].lock();
                        auto &variantPts = vertIdsPerPoint[pid];
                        auto numVariants = variantPts.size();
                        PrimIndex j = 0;
                        for (; j < numVariants; ++j)
                          if (variantPts[j] == vid
                              || vertView(POINT_ID_TAG, variantPts[j], prim_id_c) == pid)
                            break;
                        if (j == numVariants)  // no match, should insert as an candidate
                          variantPts.push_back(vid);
                        ptLocks[pid].unlock();
                      }
                    }
                  });
            };
      iteratePrims(pointPrims, wrapv<1>{});
      iteratePrims(linePrims, wrapv<2>{});
      iteratePrims(triPrims, wrapv<3>{});
    }

    std::vector<PrimIndex> numVariantsPerPoint(points.size() + 1),
        prevPointOffsets(points.size() + 1);
    pol(zip(vertIdsPerPoint, numVariantsPerPoint),
        [](const auto &ids, PrimIndex &n) { n = ids.size(); });
    exclusive_scan(pol, std::begin(numVariantsPerPoint), std::end(numVariantsPerPoint),
                   std::begin(prevPointOffsets), 0, zs::plus<PrimIndex>{}, loc);

    /// split [points] and assign [verts] attributes [uv, nrm, clr, tan] to [points]
    auto &dstPoints = dst.points();
    dstPoints.appendProperties32(pol, ptProps, loc);
    dstPoints.resize(prevPointOffsets.back());

    auto &dstVerts = dst.verts();
    dstVerts.appendProperties32(pol, {{POINT_ID_TAG, 1}}, loc);
    dstVerts.resize(prevPointOffsets.back());

    /// @brief initialize points of visual mesh
    /// @note skinning pos (if exist) should precede pos tag
    auto posLabel
        = points.hasProperty(ATTRIB_SKINNING_POS_TAG) ? ATTRIB_SKINNING_POS_TAG : ATTRIB_POS_TAG;
    pol(zip(range(points.size()), prevPointOffsets),
        [&, dstPtView = view<space>({}, dstPoints.attr32()),
         dstVtView = view<space>(dstVerts.attr32()),
         dstVtPidChn = dstVerts.getPropertyOffset(POINT_ID_TAG),
         pointView = view<space>({}, points.attr32()),
         vertView = view<space>({}, verts.attr32())](PrimIndex pid, PrimIndex offset) mutable {
          const auto &ids = vertIdsPerPoint[pid];
          auto sz = ids.size();
          for (PrimIndex j = 0; j < sz; ++j) {
            auto vid = ids[j];
            // [verts] init
            PrimIndex dstVid = offset + j;
            dstVtView(dstVtPidChn, dstVid, prim_id_c) = dstVid;
            // [points] attributes init
            dstPtView.tuple(dim_c<3>, ATTRIB_POS_TAG, dstVid)
                = pointView.pack(dim_c<3>, posLabel, pid);

            if (hasVertUv)
              dstPtView.tuple(dim_c<2>, ATTRIB_UV_TAG, dstVid)
                  = vertView.pack(dim_c<2>, ATTRIB_UV_TAG, vid);
            else if (hasPointUv)
              dstPtView.tuple(dim_c<2>, ATTRIB_UV_TAG, dstVid)
                  = pointView.pack(dim_c<2>, ATTRIB_UV_TAG, pid);

            if (hasVertNrm)
              dstPtView.tuple(dim_c<3>, ATTRIB_NORMAL_TAG, dstVid)
                  = vertView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, vid);
            else if (hasPointNrm)
              dstPtView.tuple(dim_c<3>, ATTRIB_NORMAL_TAG, dstVid)
                  = pointView.pack(dim_c<3>, ATTRIB_NORMAL_TAG, pid);

            if (hasVertClr)
              dstPtView.tuple(dim_c<3>, ATTRIB_COLOR_TAG, dstVid)
                  = vertView.pack(dim_c<3>, ATTRIB_COLOR_TAG, vid);
            else if (hasPointClr)
              dstPtView.tuple(dim_c<3>, ATTRIB_COLOR_TAG, dstVid)
                  = pointView.pack(dim_c<3>, ATTRIB_COLOR_TAG, pid);

            if (hasVertTan)
              dstPtView.tuple(dim_c<3>, ATTRIB_TANGENT_TAG, dstVid)
                  = vertView.pack(dim_c<3>, ATTRIB_TANGENT_TAG, vid);
            else if (hasPointTan)
              dstPtView.tuple(dim_c<3>, ATTRIB_TANGENT_TAG, dstVid)
                  = pointView.pack(dim_c<3>, ATTRIB_TANGENT_TAG, pid);
          }
        });

    /// iterate [pt, line, tri], with corresponding primDimC [1, 2, 3],
    /// and squash [prim-vert-point] mapping to [prim-point].
    auto remapPrimIndices = [&](const auto &srcLocalPrims, auto &localPrims, auto primDimC) {
      auto &srcPrims = srcLocalPrims->prims();
      auto &prims = localPrims->prims();
      pol(enumerate(range(srcPrims.attr32(), ELEM_VERT_ID_TAG, dim_c<RM_CVREF_T(primDimC)::value>,
                          prim_id_c),
                    range(prims.attr32(), ELEM_VERT_ID_TAG, dim_c<RM_CVREF_T(primDimC)::value>,
                          prim_id_c)),
          [&, vertView = view<space>({}, verts.attr32()), points = view<space>({}, points.attr32()),
           prims = view<space>(prims.attr32())](PrimIndex ei, const auto &originalVids,
                                                auto &vids) {
            constexpr int dime = RM_CVREF_T(primDimC)::value;

            for (int d = 0; d < dime; ++d) {
              PrimIndex vid, pid;
              if constexpr (is_integral_v<RM_CVREF_T(originalVids)>) {
                vid = originalVids;
                pid = vertView(POINT_ID_TAG, vid, prim_id_c);
              } else {
                vid = originalVids[d];
                pid = vertView(POINT_ID_TAG, vid, prim_id_c);
              }
              const auto offset = prevPointOffsets[pid];
              const auto &ids = vertIdsPerPoint[pid];
              PrimIndex j = 0;
              for (; j < ids.size(); ++j)
                /// @note find the candidate that is the same vert,
                ///  or a vert pointing to the same point (pid)
                if (ids[j] == vid || vertView(POINT_ID_TAG, ids[j], prim_id_c) == pid) break;
              assert(j != ids.size());

              // [prims] indices update
              if constexpr (is_integral_v<RM_CVREF_T(vids)>)
                vids = offset + j;
              else
                vids[d] = offset + j;
            }
          });
    };

    auto dstPointPrims = dst.localPointPrims();
    dstPointPrims->prims().appendProperties32(pol, {{ELEM_VERT_ID_TAG, 1}});
    dstPointPrims->prims().resize(pointPrims->prims().size());
    remapPrimIndices(pointPrims, dstPointPrims, wrapv<1>{});

    auto dstLinePrims = dst.localLinePrims();
    dstLinePrims->prims().appendProperties32(pol, {{ELEM_VERT_ID_TAG, 2}});
    dstLinePrims->prims().resize(linePrims->prims().size());
    remapPrimIndices(linePrims, dstLinePrims, wrapv<2>{});

    auto dstTriPrims = dst.localTriPrims();
    dstTriPrims->prims().appendProperties32(pol, {{ELEM_VERT_ID_TAG, 3}});
    dstTriPrims->prims().resize(triPrims->prims().size());
    remapPrimIndices(triPrims, dstTriPrims, wrapv<3>{});
  }

  /// TODO: 64-bits attributes copy
  void setup_simple_mesh_for_poly_mesh(PrimitiveStorage &geom, bool appendPrim,
                                       const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    auto &pointPrims = geom.localPointPrims()->prims();
    auto &linePrims = geom.localLinePrims()->prims();
    auto &triPrims = geom.localTriPrims()->prims();

    auto &polyPrims = geom.localPolyPrims()->prims();
    const auto &polys = polyPrims.attr32();
    const auto numPolys = polys.size();

    if (numPolys == 0) return;

    std::vector<PrimIndex> numPointPrimsPerPoly(numPolys + 1), numLinePrimsPerPoly(numPolys + 1),
        numTriPrimsPerPoly(numPolys + 1);
    std::vector<PrimIndex> pointPrimOffsets(numPolys + 1), linePrimOffsets(numPolys + 1),
        triPrimOffsets(numPolys + 1);

    // size
    pol(enumerate(range(polys, POLY_SIZE_TAG, dim_c<1>, prim_id_c)),
        [&](PrimIndex polyI, PrimIndex sz) {
          if (sz == 1)
            numPointPrimsPerPoly[polyI] = 1;
          else if (sz == 2)
            numLinePrimsPerPoly[polyI] = 1;
          else
            numTriPrimsPerPoly[polyI] = sz - 2;
        });

    // offset
    exclusive_scan(pol, zs::begin(numPointPrimsPerPoly), zs::end(numPointPrimsPerPoly),
                   zs::begin(pointPrimOffsets), 0, zs::plus<PrimIndex>{}, loc);
    exclusive_scan(pol, zs::begin(numLinePrimsPerPoly), zs::end(numLinePrimsPerPoly),
                   zs::begin(linePrimOffsets), 0, zs::plus<PrimIndex>{}, loc);
    exclusive_scan(pol, zs::begin(numTriPrimsPerPoly), zs::end(numTriPrimsPerPoly),
                   zs::begin(triPrimOffsets), 0, zs::plus<PrimIndex>{}, loc);

    // transform
    PrimIndex pointPrimOffset = 0;
    PrimIndex linePrimOffset = 0;
    PrimIndex triPrimOffset = 0;
    if (appendPrim) {
      pointPrimOffset = pointPrims.size();
      linePrimOffset = linePrims.size();
      triPrimOffset = triPrims.size();
    }
    const auto &polyTags = polys.getPropertyTags();

    std::vector<PropertyTag> ptPrimTags{{ELEM_VERT_ID_TAG, 1}, {TO_POLY_ID_TAG, 1}},
        linePrimTags{{ELEM_VERT_ID_TAG, 2}, {TO_POLY_ID_TAG, 1}},
        triPrimTags{{ELEM_VERT_ID_TAG, 3}, {TO_POLY_ID_TAG, 1}};
    for (const auto &polyTag : polyTags)
      if (polyTag.name != POLY_OFFSET_TAG && polyTag.name != POLY_SIZE_TAG) {
        ptPrimTags.push_back(polyTag);
        linePrimTags.push_back(polyTag);
        triPrimTags.push_back(polyTag);
      }

    pointPrims.appendProperties32(pol, ptPrimTags, loc);
    pointPrims.resize(pointPrimOffset + pointPrimOffsets.back());

    linePrims.appendProperties32(pol, linePrimTags, loc);
    linePrims.resize(linePrimOffset + linePrimOffsets.back());

    triPrims.appendProperties32(pol, triPrimTags, loc);
    triPrims.resize(triPrimOffset + triPrimOffsets.back());

    auto pointPrimView = view<space>({}, pointPrims.attr32());
    auto linePrimView = view<space>({}, linePrims.attr32());
    auto triPrimView = view<space>({}, triPrims.attr32());
    auto polyPrimView = view<space>({}, polys);

    pol(
        range(numPolys),
        [&, polyOffsetChn = polys.getPropertyOffset(POLY_OFFSET_TAG),
         polySizeChn = polys.getPropertyOffset(POLY_SIZE_TAG)](PrimIndex polyI) {
          auto vertOffset = polyPrimView(polyOffsetChn, polyI, prim_id_c);
          auto polySize = polyPrimView(polySizeChn, polyI, prim_id_c);

          if (polySize == 1) {
            auto dstPrimOffset = pointPrimOffset + pointPrimOffsets[polyI];
            pointPrimView(ELEM_VERT_ID_TAG, dstPrimOffset, prim_id_c) = vertOffset;
            pointPrimView(TO_POLY_ID_TAG, dstPrimOffset, prim_id_c) = polyI;
            // copy custom attribs
            for (const auto &prop : polyTags) {
              if (prop.name != POLY_OFFSET_TAG && prop.name != POLY_SIZE_TAG) {
                for (int d = 0; d < prop.numChannels; ++d)
                  pointPrimView(prop.name, d, dstPrimOffset) = polyPrimView(prop.name, d, polyI);
              }
            }
          } else if (polySize == 2) {
            auto dstPrimOffset = linePrimOffset + linePrimOffsets[polyI];
            linePrimView(ELEM_VERT_ID_TAG, 0, dstPrimOffset, prim_id_c) = vertOffset;
            linePrimView(ELEM_VERT_ID_TAG, 1, dstPrimOffset, prim_id_c) = vertOffset + 1;
            linePrimView(TO_POLY_ID_TAG, dstPrimOffset, prim_id_c) = polyI;
            // copy custom attribs
            for (const auto &prop : polyTags) {
              if (prop.name != POLY_OFFSET_TAG && prop.name != POLY_SIZE_TAG) {
                for (int d = 0; d < prop.numChannels; ++d)
                  linePrimView(prop.name, d, dstPrimOffset) = polyPrimView(prop.name, d, polyI);
              }
            }
          } else {
            auto dstPrimOffset = triPrimOffset + triPrimOffsets[polyI];
            for (int j = 0; j + 2 < polySize; ++j) {
              triPrimView(ELEM_VERT_ID_TAG, 0, dstPrimOffset + j, prim_id_c) = vertOffset;
              triPrimView(ELEM_VERT_ID_TAG, 1, dstPrimOffset + j, prim_id_c) = vertOffset + j + 1;
              triPrimView(ELEM_VERT_ID_TAG, 2, dstPrimOffset + j, prim_id_c) = vertOffset + j + 2;
              triPrimView(TO_POLY_ID_TAG, dstPrimOffset + j, prim_id_c) = polyI;
              // copy custom attribs
              for (const auto &prop : polyTags) {
                if (prop.name != POLY_OFFSET_TAG && prop.name != POLY_SIZE_TAG) {
                  for (int d = 0; d < prop.numChannels; ++d)
                    triPrimView(prop.name, d, dstPrimOffset + j)
                        = polyPrimView(prop.name, d, polyI);
                }
              }
            }
          }
        },
        loc);
  }

  void update_simple_mesh_from_poly_mesh(PrimitiveStorage &geom, const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    auto &pointPrims = geom.localPointPrims()->prims();
    auto &linePrims = geom.localLinePrims()->prims();
    auto &triPrims = geom.localTriPrims()->prims();

    auto &polyPrims = geom.localPolyPrims()->prims();
    const auto &polys = polyPrims.attr32();
    const auto &polyTags = polys.getPropertyTags();

    assert(geom.isSimpleMeshEstablished() && "geom should setup a simple mesh before updating it");

    std::vector<PropertyTag> ptPrimTags{}, linePrimTags{}, triPrimTags{};
    for (const auto &polyTag : polyTags)
      if (polyTag.name != POLY_OFFSET_TAG && polyTag.name != POLY_SIZE_TAG) {
        ptPrimTags.push_back(polyTag);
        linePrimTags.push_back(polyTag);
        triPrimTags.push_back(polyTag);
      }
    pointPrims.appendProperties32(pol, ptPrimTags, loc);
    linePrims.appendProperties32(pol, linePrimTags, loc);
    triPrims.appendProperties32(pol, triPrimTags, loc);

    auto polyPrimView = view<space>({}, polys);

    auto iteratePrims = [&](auto &prims, const auto &tags) {
      pol(range(prims.size()),
          [&, primView = view<space>({}, prims.attr32())](PrimIndex ei) mutable {
            PrimIndex polyI = primView(TO_POLY_ID_TAG, ei, prim_id_c);
            for (const auto &prop : tags) {
              for (int d = 0; d < prop.numChannels; ++d)
                primView(prop.name, d, ei) = polyPrimView(prop.name, d, polyI);
            }
          });
    };
    iteratePrims(pointPrims, ptPrimTags);
    iteratePrims(linePrims, linePrimTags);
    iteratePrims(triPrims, triPrimTags);
  }

  void write_simple_mesh_to_poly_mesh(PrimitiveStorage &geom, const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif

    const auto &pointPrims = geom.localPointPrims()->prims();
    const auto &linePrims = geom.localLinePrims()->prims();
    const auto &triPrims = geom.localTriPrims()->prims();

    auto &polyPrims = geom.localPolyPrims()->prims();
    auto &polys = polyPrims.attr32();

    assert(geom.isSimpleMeshEstablished() && "geom should setup a simple mesh before updating it");

    std::vector<PropertyTag> polyPrimTags{};
    for (const auto &primTag : pointPrims.getProperties())
      if (primTag.name != ELEM_VERT_ID_TAG && primTag.name != TO_POLY_ID_TAG)
        polyPrimTags.push_back(primTag);
    for (const auto &primTag : linePrims.getProperties())
      if (primTag.name != ELEM_VERT_ID_TAG && primTag.name != TO_POLY_ID_TAG)
        polyPrimTags.push_back(primTag);
    for (const auto &primTag : triPrims.getProperties())
      if (primTag.name != ELEM_VERT_ID_TAG && primTag.name != TO_POLY_ID_TAG)
        polyPrimTags.push_back(primTag);
    polyPrims.appendProperties32(pol, polyPrimTags, loc);

    auto polyPrimView = view<space>({}, polys);

    /// clear poly properties that are going to be written by simple prims
    pol(
        range(polys.size()),
        [&](PrimIndex polyI) {
          for (const auto &prop : polyPrimTags)
            for (int d = 0; d < prop.numChannels; ++d) polyPrimView(prop.name, d, polyI) = 0.f;
        },
        loc);

    /// accumulate
    auto iteratePrims = [&](auto &prims) {
      const auto &primTags = prims.getProperties();
      pol(
          range(prims.size()),
          [&, primView = view<space>({}, prims.attr32())](PrimIndex ei) {
            PrimIndex polyI = primView(TO_POLY_ID_TAG, ei, prim_id_c);
            for (const auto &prop : primTags) {
              if (prop.name == ELEM_VERT_ID_TAG || prop.name == TO_POLY_ID_TAG) continue;
              for (int d = 0; d < prop.numChannels; ++d)
                atomic_add(wrapv<space>{}, &polyPrimView(prop.name, d, polyI),
                           primView(prop.name, d, ei));
            }
          },
          loc);
    };
    iteratePrims(pointPrims);
    iteratePrims(linePrims);
    iteratePrims(triPrims);

    /// average updated poly properties
    pol(
        range(polys.size()),
        [&](PrimIndex polyI) {
          auto polySize = polyPrimView(POLY_SIZE_TAG, polyI, prim_id_c);
          int numSimplePrims = polySize > 2 ? polySize - 2 : 1;
          for (const auto &prop : polyPrimTags)
            for (int d = 0; d < prop.numChannels; ++d)
              polyPrimView(prop.name, d, polyI) /= numSimplePrims;
        },
        loc);
  }

}  // namespace zs
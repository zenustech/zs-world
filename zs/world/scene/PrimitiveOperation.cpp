#include "PrimitiveOperation.hpp"

#include "PrimitiveTransform.hpp"

#if ZS_ENABLE_OPENMP
#  include "zensim/omp/execution/ExecutionPolicy.hpp"
#else
#  include "zensim/execution/ExecutionPolicy.hpp"
#endif

namespace zs {

  void assign_zsmesh_colors(PrimitiveStorage &prim, const std::vector<PrimIndex> &candidates,
                            zs::vec<float, 3> color, const source_location &loc) {
#if ZS_ENABLE_OPENMP
    auto pol = omp_exec();
    constexpr auto space = execspace_e::openmp;
#else
    auto pol = seq_exec();
    constexpr auto space = execspace_e::host;
#endif
    bool processed = false;
    auto process = [&](auto &zsmesh) {
      PrimIndex numPts = zsmesh.nodes.size();
      if (numPts == zsmesh.colors.size() && numPts == zsmesh.vids.size()) {
        pol(
            candidates,
            [&](PrimIndex pid) {
              for (int d = 0; d < 3; ++d) zsmesh.colors[pid][d] = color[d];
              // fmt::print("painting point {} with color ({}, {}, {})\n", pid, color[0], color[1],
              //            color[2]);
            },
            loc);
        processed = true;
      }
    };

    auto &details = prim.details();
    if (details.triMesh().nodes.size()) {
      process(details.triMesh());
      details.setColorDirty();
    }
    if (details.lineMesh().nodes.size()) {
      process(details.lineMesh());
      details.setColorDirty();
    }
    if (details.pointMesh().nodes.size()) {
      process(details.pointMesh());
      details.setColorDirty();
    }
    // sync simple-mesh, poly-mesh, key-frames
    if (processed)
      write_zs_mesh_points_to_simple_mesh_verts(prim, &details.triMesh(), &details.lineMesh(),
                                                &details.pointMesh(), loc);
  }

}  // namespace zs
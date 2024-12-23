#include "PrimitiveQuery.hpp"

#if ZS_ENABLE_OPENMP
#  include "zensim/omp/execution/ExecutionPolicy.hpp"
#else
#  include "zensim/execution/ExecutionPolicy.hpp"
#endif

namespace zs {

  /*
#if ZS_ENABLE_OPENMP
auto pol = omp_exec();
constexpr auto space = execspace_e::openmp;
#else
auto pol = seq_exec();
constexpr auto space = execspace_e::host;
#endif
*/

  bool get_ray_intersection_with_prim(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                                      const ZsPrimitive &prim, glm::vec3 *hitPt) {
    const auto &bvh = prim.details().triBvh();
    /// @note if triBvh is empty, then no intersection is expected
    if (bvh.getNumLeaves() == 0) return false;

    const auto &triMesh = prim.details().triMesh();
    f32 dist = detail::deduce_numeric_max<f32>();
    auto bvhv = proxy<execspace_e::host>(bvh);
    /// @note primitive should not be in the process of update
    const auto &primTransform = prim.currentTimeVisualTransform();
    const auto primTransformInv = glm::inverse(primTransform);
    auto toZsVec = [](const auto &v) { return zs::vec<f32, 3>{v[0], v[1], v[2]}; };
    auto toGlmVec = [](const auto &v) { return glm::vec3{v[0], v[1], v[2]}; };
    auto ro = toZsVec(primTransformInv * glm::vec4(rayOrigin, 1.f));
    auto rd = toZsVec(
        glm::normalize(primTransformInv * glm::vec4(rayDirection, 0.f)));  // ignore translation

    bvhv.ray_intersect(ro, rd, [&triMesh, &ro, &rd, &dist](PrimIndex triNo) {
      auto tri = triMesh.elems[triNo];
      auto t0 = zs::vec<f32, 3>{triMesh.nodes[tri[0]][0], triMesh.nodes[tri[0]][1],
                                triMesh.nodes[tri[0]][2]};
      auto t1 = zs::vec<f32, 3>{triMesh.nodes[tri[1]][0], triMesh.nodes[tri[1]][1],
                                triMesh.nodes[tri[1]][2]};
      auto t2 = zs::vec<f32, 3>{triMesh.nodes[tri[2]][0], triMesh.nodes[tri[2]][1],
                                triMesh.nodes[tri[2]][2]};
      if (auto d = ray_tri_intersect(ro, rd, t0, t1, t2); d < dist) {
        dist = d;
        // fmt::print("printing ray intersected with {}, (tri {})\n", dist, triNo);
      }
    });
    if (dist != detail::deduce_numeric_max<f32>()) {
      // fmt::print("ray intersected with {}, bvh leaf size: {}\n", dist, bvh.getNumLeaves());
      if (hitPt) {
        *hitPt = glm::vec3(primTransform * glm::vec4(toGlmVec(ro + rd * dist), 1.f));
      }
      return true;
    }
    return false;
  }

}  // namespace zs
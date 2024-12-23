#pragma once
#include "Primitive.hpp"

namespace zs {

  ZS_WORLD_EXPORT bool get_ray_intersection_with_prim(const glm::vec3 &rayOrigin,
                                                      const glm::vec3 &rayDirection,
                                                      const ZsPrimitive &prim,
                                                      glm::vec3 *hitPt = nullptr);

}
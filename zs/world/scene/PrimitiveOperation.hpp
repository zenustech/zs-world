#pragma once
#include "../WorldExport.hpp"
#include "Primitive.hpp"

namespace zs {

  /// poly mesh to other forms
  /// @brief assign visual mesh (ZsPrimitive) to trimesh (zs::Mesh)
  ZS_WORLD_EXPORT void assign_zsmesh_colors(PrimitiveStorage& prim,
                                            const std::vector<PrimIndex>& candidates,
                                            zs::vec<float, 3> color,
                                            const source_location& loc
                                            = source_location::current());

}  // namespace zs
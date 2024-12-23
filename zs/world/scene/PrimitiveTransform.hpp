#pragma once
#include "../WorldExport.hpp"
#include "Primitive.hpp"

namespace zs {

  /// poly mesh to other forms
  /// @brief assign visual mesh (ZsPrimitive) to trimesh (zs::Mesh)
  ZS_WORLD_EXPORT void assign_visual_mesh_to_pointmesh(const PrimitiveStorage& src,
                                                       ZsPointMesh& dst,
                                                       const source_location& loc
                                                       = source_location::current());

  /// @brief assign visual mesh (ZsPrimitive) to trimesh (zs::Mesh)
  ZS_WORLD_EXPORT void assign_visual_mesh_to_linemesh(const PrimitiveStorage& src, ZsLineMesh& dst,
                                                      const source_location& loc
                                                      = source_location::current());

  /// @brief assign visual mesh (ZsPrimitive) to trimesh (zs::Mesh)
  ZS_WORLD_EXPORT void assign_visual_mesh_to_trimesh(const PrimitiveStorage& src, ZsTriMesh& dst,
                                                     const source_location& loc
                                                     = source_location::current());

  /// @brief split points upon divergent [verts] attributes
  /// @brief only preserve [nrm, tan, clr, uv] point attributes
  /// @note usually calls assign_attribs_from_prim_to_vert(src, {{"nrm", 3}, {"clr", 3}, {"tan",
  ZS_WORLD_EXPORT void assign_simple_mesh_to_zsmesh(
      const PrimitiveStorage& src, ZsTriMesh* pTriMesh = nullptr, ZsLineMesh* pLineMesh = nullptr,
      ZsPointMesh* pPointMesh = nullptr, const source_location& loc = source_location::current());

  /// @brief assign prim attributes to vert attributes
  ZS_WORLD_EXPORT void assign_attribs_from_prim_to_vert(PrimitiveStorage& geom,
                                                        const std::vector<PropertyTag>& attrTags,
                                                        const source_location& loc
                                                        = source_location::current());

  /// @brief split points upon divergent [verts] attributes
  /// @brief only preserve [nrm, tan, clr, uv] point attributes
  /// @note usually calls assign_attribs_from_prim_to_vert(src, {{"nrm", 3}, {"clr", 3}, {"tan",
  /// 3}}) first
  ZS_WORLD_EXPORT void assign_simple_mesh_to_visual_mesh(const PrimitiveStorage& src,
                                                         PrimitiveStorage& dst,
                                                         const source_location& loc
                                                         = source_location::current());

  /// @note transform poly-prims to tri/line/point prims
  ZS_WORLD_EXPORT void setup_simple_mesh_for_poly_mesh(PrimitiveStorage& geom,
                                                       bool appendPrim = false,
                                                       const source_location& loc
                                                       = source_location::current());

  ZS_WORLD_EXPORT void update_simple_mesh_from_poly_mesh(PrimitiveStorage& geom,
                                                         const source_location& loc
                                                         = source_location::current());

  ZS_WORLD_EXPORT void write_simple_mesh_to_poly_mesh(PrimitiveStorage& geom,
                                                      const source_location& loc
                                                      = source_location::current());

#if 0
  ZS_WORLD_EXPORT void update_primitive_to_visual_mesh(const ZsPrimitive& src, ZsPrimitive& dst,
                                                       const source_location& loc
                                                       = source_location::current());

  /// @brief transform point/line/tri rpims to poly prims
  ZS_WORLD_EXPORT void convert_primitive_to_polymesh(ZsPrimitive& geom,
                                                     const source_location& loc
                                                     = source_location::current());
#endif

}  // namespace zs
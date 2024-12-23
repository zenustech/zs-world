#pragma once
#include "../SceneInterface.hpp"
#include "../WorldExport.hpp"
#include "Primitive.hpp"

namespace zs {

  /// to ZsPrimitive
  ZS_WORLD_EXPORT void assign_trimesh_to_primitive(const Mesh<f32, 3, u32, 3>& triMesh,
                                                   ZsPrimitive& geom,
                                                   const source_location& loc
                                                   = source_location::current());
  ZS_WORLD_EXPORT ZsPrimitive trimesh_to_primitive(const Mesh<f32, 3, u32, 3>& triMesh,
                                                   const source_location& loc
                                                   = source_location::current());

  ZS_WORLD_EXPORT ZsPrimitive* build_primitive_from_trimesh(const Mesh<f32, 3, u32, 3>& triMesh,
                                                            const source_location& loc
                                                            = source_location::current());

#if ZS_ENABLE_USD
  ZS_WORLD_EXPORT void assign_usdmesh_to_primitive(const ScenePrimHolder& scenePrim,
                                                   ZsPrimitive& geom, double time,
                                                   const source_location& loc
                                                   = source_location::current());
  ZS_WORLD_EXPORT void assign_usdmesh_to_primitive(const ScenePrimConcept* scenePrim,
                                                   ZsPrimitive& geom, double time,
                                                   const source_location& loc
                                                   = source_location::current());
  ZS_WORLD_EXPORT ZsPrimitive usdmesh_to_primitive(const ScenePrimHolder& scenePrim, double time,
                                                   const source_location& loc
                                                   = source_location::current());
  ZS_WORLD_EXPORT ZsPrimitive usdmesh_to_primitive(const ScenePrimConcept* scenePrim, double time,
                                                   const source_location& loc
                                                   = source_location::current());
#endif

  /// from ZsPrimitive
  ZS_WORLD_EXPORT void assign_primitive_to_trimesh(const ZsPrimitive& geom,
                                                   Mesh<f32, 3, u32, 3>& triMesh,
                                                   const source_location& loc
                                                   = source_location::current());
  ZS_WORLD_EXPORT Mesh<f32, 3, u32, 3> primitive_to_trimesh(const ZsPrimitive& geom,
                                                            const source_location& loc
                                                            = source_location::current());
#if ZS_ENABLE_USD
  ZS_WORLD_EXPORT void assign_primitive_to_usdprim(const ZsPrimitive& geom,
                                                   ScenePrimConcept* scenePrim, double time,
                                                   const source_location& loc
                                                   = source_location::current());
  ZS_WORLD_EXPORT void assign_primitive_to_usdprim(const ZsPrimitive& geom,
                                                   ScenePrimHolder& scenePrim, double time,
                                                   const source_location& loc
                                                   = source_location::current());
#endif

#if ZS_ENABLE_USD
  ZS_WORLD_EXPORT ZsPrimitive* build_primitive_from_usdprim(const ScenePrimConcept* scenePrim,
                                                            double time,
                                                            const source_location& loc
                                                            = source_location::current());

  ZS_WORLD_EXPORT ZsPrimitive* build_primitive_from_usdprim(const ScenePrimHolder& scenePrim,
                                                            double time,
                                                            const source_location& loc
                                                            = source_location::current());

  ZS_WORLD_EXPORT ZsPrimitive* build_primitive_from_usdprim(const ScenePrimConcept* scenePrim,
                                                            const source_location& loc
                                                            = source_location::current());

  ZS_WORLD_EXPORT ZsPrimitive* build_primitive_from_usdprim(const ScenePrimHolder& scenePrim,
                                                            const source_location& loc
                                                            = source_location::current());

  /// @note mesh
  ZS_WORLD_EXPORT bool retrieve_usdprim_attrib_position(const ScenePrimConcept* scenePrim,
                                                        double time, int* numPoints,
                                                        glm::vec3* pos = nullptr,
                                                        const source_location& loc
                                                        = source_location::current());
  ZS_WORLD_EXPORT bool retrieve_usdprim_attrib_face(const ScenePrimConcept* scenePrim, double time,
                                                    int* numIndices, int* numFaces,
                                                    int* indices = nullptr, int* sizes = nullptr,
                                                    const source_location& loc
                                                    = source_location::current());

  ZS_WORLD_EXPORT bool usdprim_face_order_is_left_handed(const ScenePrimConcept* scenePrim,
                                                         const source_location& loc
                                                         = source_location::current());

  /// @note attribute
  ZS_WORLD_EXPORT bool usdprim_has_attrib_uv(const ScenePrimConcept* scenePrim,
                                             const source_location& loc
                                             = source_location::current());
  ZS_WORLD_EXPORT bool usdprim_has_attrib_normal(const ScenePrimConcept* scenePrim,
                                                 const source_location& loc
                                                 = source_location::current());
  ZS_WORLD_EXPORT bool usdprim_has_attrib_color(const ScenePrimConcept* scenePrim,
                                                const source_location& loc
                                                = source_location::current());
  // usd prim utilities
  ZS_WORLD_EXPORT bool retrieve_usdprim_attrib_uv(const ScenePrimConcept* scenePrim, double time,
                                                  int numPoints, int numVerts, int numFaces,
                                                  std::vector<glm::vec2>& uvs,
                                                  prim_attrib_owner_e* owner = nullptr,
                                                  const source_location& loc
                                                  = source_location::current());
  ZS_WORLD_EXPORT bool retrieve_usdprim_attrib_normal(const ScenePrimConcept* scenePrim,
                                                      double time, int numPoints, int numVerts,
                                                      int numFaces, std::vector<glm::vec3>& nrms,
                                                      prim_attrib_owner_e* owner = nullptr,
                                                      const source_location& loc
                                                      = source_location::current());
  ZS_WORLD_EXPORT bool retrieve_usdprim_attrib_color(const ScenePrimConcept* scenePrim, double time,
                                                     int numPoints, int numVerts, int numFaces,
                                                     std::vector<glm::vec3>& clrs,
                                                     prim_attrib_owner_e* owner = nullptr,
                                                     const source_location& loc
                                                     = source_location::current());

  ZS_WORLD_EXPORT bool apply_usd_skinning(AttrVector& posAttrib, const ScenePrimConcept* scenePrim,
                                          double time,
                                          const source_location& loc = source_location::current());
#endif

}  // namespace zs
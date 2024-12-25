#pragma once

#include "zensim/geometry/Mesh.hpp"
#include "zensim/math/Vec.h"

#include "world/WorldExport.hpp"

namespace zs {
  using TriMesh = Mesh<float, 3, u32, 3>;
  ZS_WORLD_EXPORT TriMesh createSphere(float radius = 1.0f, unsigned rows = 32, unsigned cols = 32,
                                       bool hasNormal = true, bool hasUV = true,
                                       bool isFlipFace = false);
  ZS_WORLD_EXPORT TriMesh createCapsule(float radius = 1.0f, float height = 1.0f, char axis = 'Y',
                                        unsigned halfRows = 16, unsigned cols = 32,
                                        bool hasNormal = true, bool hasUV = true);
  // TODO: hasUV not supported
  ZS_WORLD_EXPORT TriMesh createCube(float size = 1.0f, unsigned divX = 2, unsigned divY = 2,
                                     unsigned divZ = 2, bool hasNormal = true, bool hasUV = true);
  ZS_WORLD_EXPORT TriMesh createPlane(float width = 1.0f, float height = 1.0f, char axis = 'Y',
                                      unsigned rows = 1, unsigned cols = 1, bool hasNormal = true,
                                      bool hasUV = true);
  ZS_WORLD_EXPORT TriMesh createDisk(float radius = 1.0f, unsigned divisions = 32,
                                     bool hasNormal = true, bool hasUV = true);
  // TODO: index uv not supported
  ZS_WORLD_EXPORT TriMesh createTube(float topRadius = 1.0f, float bottomRadius = 1.0f,
                                     float height = 1.0f, unsigned rows = 3, unsigned cols = 12,
                                     bool hasNormal = true, bool hasUV = true);
  // TODO: hasUV not supported
  ZS_WORLD_EXPORT TriMesh createTorus(float outRadius = 1.0f, float inRadius = 0.5f,
                                      unsigned outSegments = 32, unsigned inSegments = 16,
                                      bool hasNormal = true, bool hasUV = true);
  // TODO: hasUV not supported
  ZS_WORLD_EXPORT TriMesh createCone(float radius = 1.0f, float height = 1.0f, char axis = 'Y',
                                     unsigned cols = 32, bool hasNormal = true, bool hasUV = true);
  // TODO: hasUV not supported
  ZS_WORLD_EXPORT TriMesh createCylinder(float radius = 1.0f, float height = 1.0f, char axis = 'Y',
                                         unsigned cols = 32, bool hasNormal = true,
                                         bool hasUV = true);
}  // namespace zs

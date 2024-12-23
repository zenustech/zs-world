#pragma once
#include "../WorldExport.hpp"
#include "Primitive.hpp"

namespace zs {

  /// serializer
  struct ZS_WORLD_EXPORT PrimContainerSerializer : PrimContainerVisitor {
    void visit(PolyPrimContainer& prim) override {}
    void visit(TriPrimContainer& prim) override {}
    void visit(LinePrimContainer& prim) override {}
    void visit(PointPrimContainer& prim) override {}
    void visit(SdfPrimContainer& prim) override {}
    void visit(PackPrimContainer& prim) override {}
    void visit(AnalyticPrimContainer& prim) override {}
    void visit(CameraPrimContainer& prim) override {}
    void visit(LightPrimContainer& prim) override {}
  };

}  // namespace zs
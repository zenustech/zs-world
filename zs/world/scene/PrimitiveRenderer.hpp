#pragma once
#include "../WorldExport.hpp"
#include "Primitive.hpp"

namespace zs {

  /// render
  struct ZS_WORLD_EXPORT PrimContainerRenderer : PrimContainerVisitor {
    void visit(PolyPrimContainer& prim) override {}
    void visit(TriPrimContainer& prim) override {}
    void visit(LinePrimContainer& prim) override {}
    void visit(PointPrimContainer& prim) override {}
    void visit(SdfPrimContainer& prim) override {}
    void visit(PackPrimContainer& prim) override {}
    void visit(AnalyticPrimContainer& prim) override {}
    void visit(CameraPrimContainer& prim) override {}
    void visit(LightPrimContainer& prim) override {}

    PrimContainerRenderer(VulkanContext& ctx, ZsPrimitive& p) noexcept : ctx{ctx}, prim{p} {}

  protected:
    VulkanContext& ctx;
    ZsPrimitive& prim;
    std::set<ZsPrimitive*> drawnPrims;
  };

}  // namespace zs
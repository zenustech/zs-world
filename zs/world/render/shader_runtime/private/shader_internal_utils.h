#pragma once

#include <fmt/format.h>
#include <iostream>
#include "zensim/vulkan/Vulkan.hpp"

namespace zs::shader {

  // Many Slang API functions return detailed diagnostic information
  // (error messages, warnings, etc.) as a "blob" of data, or return
  // a null blob pointer instead if there were no issues.
  //
  // For convenience, we define a subroutine that will dump the information
  // in a diagnostic blob if one is produced, and skip it otherwise.
  //
  inline void diagnose_if_needed(slang::IBlob* diagnosticsBlob) {
      if ( diagnosticsBlob != nullptr ) {
          std::cerr << fmt::format("Diagnostics message:\n{}\n",
                                 static_cast<const char*>(diagnosticsBlob->getBufferPointer()));
      }
  }

  inline auto get_vulkan_device() -> vk::Device {
    return Vulkan::context(0).device;
  }

  inline vk::ShaderStageFlagBits convert_slang_stage_to_vulkan_stage(const SlangStage stage) {
    switch (stage) {
      case SLANG_STAGE_VERTEX:
        return vk::ShaderStageFlagBits::eVertex;
      // case SLANG_STAGE_FRAGMENT: same as pixel
      case SLANG_STAGE_PIXEL:
        return vk::ShaderStageFlagBits::eFragment;
      case SLANG_STAGE_HULL:
        return vk::ShaderStageFlagBits::eTessellationControl;
      case SLANG_STAGE_DOMAIN:
        return vk::ShaderStageFlagBits::eTessellationEvaluation;
      case SLANG_STAGE_GEOMETRY:
        return vk::ShaderStageFlagBits::eGeometry;
      case SLANG_STAGE_MESH:
        return vk::ShaderStageFlagBits::eMeshEXT;
      case SLANG_STAGE_COMPUTE:
        return vk::ShaderStageFlagBits::eCompute;
      case SLANG_STAGE_CALLABLE:
        return vk::ShaderStageFlagBits::eCallableKHR;
      case SLANG_STAGE_MISS:
        return vk::ShaderStageFlagBits::eMissKHR;
      case SLANG_STAGE_ANY_HIT:
        return vk::ShaderStageFlagBits::eAnyHitKHR;
      case SLANG_STAGE_CLOSEST_HIT:
        return vk::ShaderStageFlagBits::eClosestHitKHR;
      case SLANG_STAGE_INTERSECTION:
        return vk::ShaderStageFlagBits::eIntersectionKHR;
      case SLANG_STAGE_RAY_GENERATION:
        return vk::ShaderStageFlagBits::eRaygenKHR;
      case SLANG_STAGE_AMPLIFICATION:
        return vk::ShaderStageFlagBits::eTaskEXT;
      case SLANG_STAGE_NONE:
      default:
        return vk::ShaderStageFlagBits::eAll;
    }
  }

}

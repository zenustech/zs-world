#pragma once

#include <string_view>


namespace zs::shader {
  struct ShaderModuleDesc {
    // TODO(darc): specialization parameters

    /// @brief Path to shader module
    std::string_view module_path;
  };

  struct ShaderBundleDesc {
    size_t num_modules;
    ShaderModuleDesc* modules;

    ShaderBundleDesc& set_num_modules(const size_t value) {
      num_modules = value;
      return *this;
    }
    ShaderBundleDesc& set_modules(ShaderModuleDesc* value) {
      modules = value;
      return *this;
    }
  };
}  // namespace zs::shader
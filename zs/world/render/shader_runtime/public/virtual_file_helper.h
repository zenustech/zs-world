#pragma once

#include <string>
#include <memory>
#include <string_view>
#include "zensim/zpc_tpls/tl/expected.hpp"
#include "render_shader_runtime_export.h"

namespace zs {

  enum class PathResolveError {
    eVirtualPathNotFound,
  };

  class RENDER_SHADER_RUNTIME_API IVirtualFileMapping {
  public:
    virtual ~IVirtualFileMapping() = default;

    static std::unique_ptr<IVirtualFileMapping> create();

    /// @brief Add a virtual directory mapping
    virtual void add_mapping(std::string_view virtual_directory, std::string_view abs_path) = 0;

    /// @brief Resolve a virtual path
    [[nodiscard]] virtual tl::expected<std::string, PathResolveError> resolve(std::string_view virtual_path) const = 0;

    /// @brief Check if file exist with given path
    [[nodiscard]] virtual bool exist(std::string_view path) = 0;
  };
} // namespace zs


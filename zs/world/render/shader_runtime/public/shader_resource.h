#pragma once

#include "shader_mixins.h"
#include "zensim/vulkan/VkShader.hpp"
#include "zensim/zpc_tpls/tl/expected.hpp"
#include "render_shader_runtime_export.h"


namespace zs {

  enum class ShaderResourceError {
    CompileError,
    EmptyTargetCode,
    /// @brief Code blob must be aligned as 4-byte.
    AlignmentError,
  };

  struct RENDER_SHADER_RUNTIME_INTERNAL IShaderRuntimeResource {
    virtual ~IShaderRuntimeResource() = default;
  };

  class RENDER_SHADER_RUNTIME_API IShaderFunctionProxy : public IShaderRuntimeResource {
  };

  class RENDER_SHADER_RUNTIME_API IShaderEntrypointProxy : public IShaderRuntimeResource, public ICanGetSpirVCodeMixin {
  public:
    /// @brief Create a zpc ShaderModule.
    /// @param context_index Which vulkan context used to create.
    [[nodiscard]] virtual tl::expected<ShaderModule, ShaderResourceError> create_zs_shader_module(size_t context_index) = 0;
  };

} // namespace zs

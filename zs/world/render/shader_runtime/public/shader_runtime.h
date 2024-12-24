#pragma once

#include <memory>
#include <string_view>
#include <zensim/container/Vector.hpp>

#include "render_shader_runtime_export.h"
#include "shader_descriptors.h"
#include "shader_mixins.h"
#include "shader_resource.h"

namespace zs {
  class IVirtualFileMapping;

  /// @brief Shader library contains multiple shader entrypoints and reflection info.
  class RENDER_SHADER_RUNTIME_API IShaderLibrary : public IShaderRuntimeResource, public ICanGetSpirVCodeMixin {
  public:
    [[nodiscard]] virtual bool is_valid() const = 0;

    [[nodiscard]] virtual size_t num_entrypoints() const = 0;

    /// @param index entrypoint index, start with 0.
    [[nodiscard]] virtual std::unique_ptr<IShaderEntrypointProxy> get_entrypoint_by_index(size_t index) = 0;

    [[nodiscard]] virtual std::unique_ptr<IShaderEntrypointProxy> get_entrypoint_by_name(std::string_view name) = 0;
  };

  /// @brief A set of interface to manage shaders.
  /// @note This is a singleton class. If not otherwise noted, it is not thread safe.
  class RENDER_SHADER_RUNTIME_API IShaderManager {
  public:
    virtual ~IShaderManager() = default;

    /// @brief A thread-safe function to get the shader manager instance.
    /// @note Thread-safe
    static IShaderManager& get();

    virtual void setup_default_virtual_path();

    virtual IVirtualFileMapping* get_path_mapper() = 0;
    [[nodiscard]] virtual const IVirtualFileMapping* get_path_mapper() const = 0;

    /// @brief Loading a shader by given name. If shader has already loaded, return from manager registry directly.
    virtual IShaderLibrary* load_shader(const shader::ShaderModuleDesc& descriptor) = 0;

  protected:

    friend class SlangVirtualFilesystem;
  };
} // namespace zs


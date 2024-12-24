#include "shader_runtime.h"

#include <forward_list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <filesystem>

#include "slang-com-ptr.h"
#include "fmt/format.h"
#include "impl/virtual_filesystem.h"
#include "shader_internal_utils.h"
#include "virtual_file_helper.h"

#include "zensim/io/Filesystem.hpp"

namespace zs {

  class ShaderFunctionImpl final : public IShaderFunctionProxy {
  public:
    explicit ShaderFunctionImpl(Slang::ComPtr<slang::IComponentType> parent, slang::FunctionReflection* function_reflection)
      : m_parent_(std::move(parent))
      , m_function_reflection_(function_reflection)
    {}

  private:
    Slang::ComPtr<slang::IComponentType> m_parent_;
    slang::FunctionReflection* m_function_reflection_;
  };

  class ShaderEntrypointImpl final : public IShaderEntrypointProxy {
  public:
    explicit ShaderEntrypointImpl(Slang::ComPtr<slang::IComponentType> parent, size_t index)
      : m_parent_(std::move(parent))
      , m_entrypoint_index_(index)
    {}

    Vector<std::byte> get_spirv_code(bool* o_is_success) override {
      Slang::ComPtr<slang::IBlob> code_blob, diagnostics_blob;
      Vector<std::byte> result{};
      if (SLANG_SUCCEEDED(m_parent_->getEntryPointCode(m_entrypoint_index_, 0, code_blob.writeRef(), diagnostics_blob.writeRef()))) {
        if (o_is_success != nullptr) *o_is_success = true;
        result.resize(code_blob->getBufferSize());
        std::memcpy(result.data(), code_blob->getBufferPointer(), code_blob->getBufferSize());
      } else {
        if (o_is_success != nullptr) *o_is_success = false;
      }
      shader::diagnose_if_needed(diagnostics_blob);
      return result;
    }

    tl::expected<ShaderModule, ShaderResourceError> create_zs_shader_module(const size_t context_index) override {
      bool is_compile_success = false;
      Vector<std::byte> code = get_spirv_code(&is_compile_success);
      if (!is_compile_success) {
        return tl::make_unexpected(ShaderResourceError::CompileError);
      }
      if (code.empty()) {
        return tl::make_unexpected(ShaderResourceError::EmptyTargetCode);
      }
      if (code.size() & (sizeof(u32) - 1)) {
        return tl::make_unexpected(ShaderResourceError::AlignmentError);
      }

      slang::EntryPointReflection* entrypoint = m_parent_->getLayout()->getEntryPointByIndex(m_entrypoint_index_);
      const SlangStage shader_stage = entrypoint->getStage();

      VulkanContext& context = Vulkan::context(static_cast<int>(context_index));
      return context.createShaderModule(reinterpret_cast<u32*>(code.data()), code.size() / sizeof(u32), shader::convert_slang_stage_to_vulkan_stage(shader_stage));
    }

  private:
    Slang::ComPtr<slang::IComponentType> m_parent_;
    size_t m_entrypoint_index_;
  };

  class ShaderLibraryImpl final : public IShaderLibrary {
  public:
    explicit ShaderLibraryImpl(
      Slang::ComPtr<slang::IGlobalSession> global_session,
      Slang::ComPtr<slang::ISession> session,
      Slang::ComPtr<slang::IComponentType> program
    )
      : m_global_session_(std::move(global_session))
      , m_session_(std::move(session))
      , m_program_(std::move(program))
    {}

    [[nodiscard]] bool is_valid() const override {
      return m_program_ && m_program_->getLayout() != nullptr;
    }

    [[nodiscard]] size_t num_entrypoints() const override {
      if (!is_valid()) {
        return 0;
      }

      return m_program_->getLayout()->getParameterCount();
    }

    std::unique_ptr<IShaderEntrypointProxy> get_entrypoint_by_index(size_t index) override {
      if (!is_valid()) {
        return nullptr;
      }

      slang::ProgramLayout* layout = m_program_->getLayout();

      if (index >= layout->getEntryPointCount()) {
        return nullptr;
      }

      slang::EntryPointReflection* entry_point = layout->getEntryPointByIndex(index);

      if (entry_point != nullptr) {
        return std::make_unique<ShaderEntrypointImpl>(m_program_, index);
      }

      return nullptr;
    }

    std::unique_ptr<IShaderEntrypointProxy> get_entrypoint_by_name(std::string_view name) override {
      if (!is_valid()) {
        return nullptr;
      }

      slang::ProgramLayout* layout = m_program_->getLayout();

      for (int i = 0; i < layout->getEntryPointCount(); ++i) {
        slang::EntryPointReflection* entrypoint = layout->getEntryPointByIndex(i);
        if (entrypoint != nullptr && std::string_view(entrypoint->getName()) == name) {
          return std::make_unique<ShaderEntrypointImpl>(m_program_, i);
        }
      }

      return nullptr;
    }

    Vector<std::byte> get_spirv_code(bool* o_is_success) override {
      Slang::ComPtr<slang::IBlob> code_blob, diagnostics_blob;
      Vector<std::byte> result{};
      if (SLANG_SUCCEEDED(m_program_->getTargetCode(0, code_blob.writeRef(), diagnostics_blob.writeRef()))) {
        if (o_is_success != nullptr) *o_is_success = true;
        result.resize(code_blob->getBufferSize());
        std::memcpy(result.data(), code_blob->getBufferPointer(), code_blob->getBufferSize());
      } else {
        if (o_is_success != nullptr) *o_is_success = false;
      }
      shader::diagnose_if_needed(diagnostics_blob);
      return result;
    }

  private:
    // It is wired that we need to hold the reference counter of parent context.
    // But if we don't, it might crash while program exit (unordered deconstruction?).
    Slang::ComPtr<slang::IGlobalSession> m_global_session_;
    Slang::ComPtr<slang::ISession> m_session_;
    Slang::ComPtr<slang::IComponentType> m_program_;
  };

  class ShaderManagerImpl final : public IShaderManager {
  public:
    ShaderManagerImpl()
      : m_directory_mapping_(IVirtualFileMapping::create())
    {}

    [[nodiscard]] const IVirtualFileMapping* get_path_mapper() const override {
      return m_directory_mapping_.get();
    }

    IVirtualFileMapping* get_path_mapper() override {
      return m_directory_mapping_.get();
    }

    static Slang::ComPtr<slang::IGlobalSession> get_slang_global_session() {
      if (!m_slang_global_session_) {
        if (SLANG_FAILED(slang::createGlobalSession(m_slang_global_session_.writeRef()))) {
          throw std::runtime_error("Failed to create Slang global session");
        }
      }
      return m_slang_global_session_;
    }

    static Slang::ComPtr<slang::ISession> new_slang_session() {
      const auto global_session = get_slang_global_session();

      slang::TargetDesc target_desc{};
      target_desc.format = SLANG_SPIRV;
      target_desc.profile = global_session->findProfile("spirv_1_6");
      target_desc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY | SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM;
      target_desc.forceGLSLScalarBufferLayout = true;

      slang::SessionDesc session_desc{};
      session_desc.targets = &target_desc;
      session_desc.targetCount = 1;
      session_desc.fileSystem = m_virtual_file_system_;

      const char* search_paths[] {
        "/",
      };
      session_desc.searchPaths = search_paths;
      session_desc.searchPathCount = sizeof(search_paths) / sizeof(const char*);

      session_desc.preprocessorMacroCount = 0;

      Slang::ComPtr<slang::ISession> session;
      if (SLANG_FAILED(global_session->createSession(session_desc, session.writeRef()))) {
        throw std::runtime_error("Failed to create slang compile session");
      }

      return session;
    }

    IShaderLibrary* load_shader(const shader::ShaderModuleDesc& descriptor) override {
      auto session = new_slang_session();

      IShaderLibrary* result = nullptr;
      Slang::ComPtr<slang::IBlob> diagnostics_blob;
      std::vector<slang::IComponentType*> modules;

      slang::IModule* module = session->loadModule(std::string(descriptor.module_path).c_str(), diagnostics_blob.writeRef());
      shader::diagnose_if_needed(diagnostics_blob);
      if (!module) {
        return nullptr;
      }

      modules.push_back(module);
      for (int i = 0; i < module->getDefinedEntryPointCount(); ++i) {
        Slang::ComPtr<slang::IEntryPoint> entry_point;
        if (SLANG_SUCCEEDED(module->getDefinedEntryPoint(i, entry_point.writeRef()))) {
          modules.push_back(entry_point);
        }
      }

      result = create_or_get_managed_shader_library(session, modules);

      return result;
    }

  protected:
    IShaderLibrary* create_or_get_managed_shader_library(
      const Slang::ComPtr<slang::ISession>& session,
      const std::vector<slang::IComponentType*>& modules) {

      std::lock_guard lock(m_mutex_);

      IShaderLibrary* result = nullptr;
      Slang::ComPtr<slang::IBlob> diagnostics_blob;
      Slang::ComPtr<slang::IComponentType> instanced_library;
      if (SLANG_SUCCEEDED(session->createCompositeComponentType(modules.data(), modules.size(), instanced_library.writeRef(), diagnostics_blob.writeRef()))) {
        result = &m_cached_shader_libraries_.emplace_front(get_slang_global_session(), session, instanced_library);
      }
      shader::diagnose_if_needed(diagnostics_blob);

      return result;
    }

  private:
    std::unique_ptr<IVirtualFileMapping> m_directory_mapping_;
    std::forward_list<ShaderLibraryImpl> m_cached_shader_libraries_;

    std::recursive_mutex m_mutex_;

    thread_local static Slang::ComPtr<slang::IGlobalSession> m_slang_global_session_;
    static ISlangMutableFileSystem* m_virtual_file_system_;
  };

  thread_local Slang::ComPtr<slang::IGlobalSession> ShaderManagerImpl::m_slang_global_session_{};
  ISlangMutableFileSystem* ShaderManagerImpl::m_virtual_file_system_ = new SlangVirtualFilesystem();

  IShaderManager& IShaderManager::get() {
    static ShaderManagerImpl static_manager{};
    return static_manager;
  }

  void IShaderManager::setup_default_virtual_path() {
    const auto working_dir = abs_exe_directory(); // std::filesystem::absolute(std::filesystem::current_path()).generic_string();
    IVirtualFileMapping* mapper = get_path_mapper();
    mapper->add_mapping("/cwd", working_dir);

    const auto built_in_path = std::filesystem::absolute(std::filesystem::path(working_dir) / "resource" / "shaders").generic_string();
    mapper->add_mapping("/zpc", built_in_path);
  }
} // namespace zs

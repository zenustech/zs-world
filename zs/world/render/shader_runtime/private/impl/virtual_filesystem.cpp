#include "impl/virtual_filesystem.h"

#include <filesystem>
#include <fstream>

#include "core/slang-blob.h"
#include "core/slang-io.h"
#include "core/slang-string-util.h"
#include "shader_runtime.h"
#include "virtual_file_helper.h"

namespace zs {

  SlangResult SlangVirtualFilesystem::loadFile(char const* path, ISlangBlob** outBlob) {
    *outBlob = nullptr;

    IVirtualFileMapping* mapper = IShaderManager::get().get_path_mapper();

    if (auto resolved_path = mapper->resolve(path)) {
      const std::string_view p(*resolved_path);
      if (std::filesystem::exists(p)) {
        Slang::ScopedAllocation alloc;
        const size_t filesize = std::filesystem::file_size(p);
        auto* buf = static_cast<char*>(alloc.allocate(filesize));
        std::ifstream file(p.data(), std::ios::binary);
        if (!file.read(buf, static_cast<ptrdiff_t>(filesize))) {
          return SLANG_E_CANNOT_OPEN;
        }
        *outBlob = Slang::RawBlob::moveCreate(alloc).detach();
        return SLANG_OK;
      }
    }

    return SLANG_E_NOT_AVAILABLE;
  }

  SlangResult SlangVirtualFilesystem::getPathType(const char* path, SlangPathType* pathTypeOut) {
    if (auto resolved_path = IShaderManager::get().get_path_mapper()->resolve(path)) {
      return Slang::Path::getPathType(resolved_path->c_str(), pathTypeOut);
    }
    return SLANG_E_NOT_FOUND;
  }

  SlangResult SlangVirtualFilesystem::getPath(PathKind kind, const char* path,
                                              ISlangBlob** outPath) {
    auto resolved_path = IShaderManager::get().get_path_mapper()->resolve(path);
    if (resolved_path) {
      const Slang::String result(resolved_path->c_str());
      Slang::String canonical_result(resolved_path->c_str());
      SLANG_RETURN_ON_FAIL(Slang::Path::getCanonical(result, canonical_result));
      *outPath = Slang::StringUtil::createStringBlob(canonical_result).detach();
      return SLANG_OK;
    }
    return SLANG_E_NOT_AVAILABLE;
  }

  SlangResult SlangVirtualFilesystem::getFileUniqueIdentity(const char* path,
                                                            ISlangBlob** outUniqueIdentity) {
    return getPath(PathKind::Canonical, path, outUniqueIdentity);
  }
  SlangResult SlangVirtualFilesystem::calcCombinedPath(SlangPathType fromPathType,
                                                       const char* fromPath, const char* path,
                                                       ISlangBlob** pathOut) {
    Slang::String result{};
    if (fromPath != nullptr) {
      result.append(fromPath);
    }
    if (path != nullptr) {
      result.append(path);
    }

    *pathOut = Slang::StringUtil::createStringBlob(result).detach();
    return SLANG_OK;
  }

  void SlangVirtualFilesystem::clearCache() {}

  SlangResult SlangVirtualFilesystem::enumeratePathContents(const char* path,
                                                            FileSystemContentsCallBack callback,
                                                            void* userData) {
    struct FileVisitor : Slang::Path::Visitor {
      void accept(Slang::Path::Type type, const Slang::UnownedStringSlice& filename) override {
        m_buffer.clear();
        m_buffer.append(filename);

        SlangPathType path_type;
        switch (type) {
          case Slang::Path::Type::File: path_type = SLANG_PATH_TYPE_FILE; break;
          case Slang::Path::Type::Directory: path_type = SLANG_PATH_TYPE_DIRECTORY; break;
          default: return;
        }

        m_callback(path_type, m_buffer.getBuffer(), m_user_data);
      }

      FileVisitor(FileSystemContentsCallBack callback, void* user_data)
        : m_callback(callback)
        , m_user_data(user_data)
      {}

      Slang::StringBuilder m_buffer;
      FileSystemContentsCallBack m_callback;
      void* m_user_data;
    };

    FileVisitor visitor(callback, userData);
    Slang::Path::find(path, nullptr, &visitor);

    return SLANG_OK;
  }

  OSPathKind SlangVirtualFilesystem::getOSPathKind() { return OSPathKind::OperatingSystem; }

  SlangResult SlangVirtualFilesystem::saveFile(const char* path, const void* data, size_t size) {
    auto resolved_path = IShaderManager::get().get_path_mapper()->resolve(path);
    if (!resolved_path) {
      return SLANG_E_NOT_AVAILABLE;
    }
    std::ofstream stream(*resolved_path, std::ios::out | std::ios::binary);
    stream.write(static_cast<const char*>(data), static_cast<ptrdiff_t>(size));
    return SLANG_OK;
  }

  SlangResult SlangVirtualFilesystem::saveFileBlob(const char* path, ISlangBlob* dataBlob) {
    return saveFile(path, dataBlob->getBufferPointer(), dataBlob->getBufferSize());
  }

  SlangResult SlangVirtualFilesystem::remove(const char* path) {
    return SLANG_E_NOT_IMPLEMENTED;
  }

  void* SlangVirtualFilesystem::castAs(const Slang::Guid& guid) { return getInterface(guid); }

  SlangResult SlangVirtualFilesystem::createDirectory(const char* path) {
    return SLANG_E_NOT_IMPLEMENTED;
  }

  ISlangUnknown* SlangVirtualFilesystem::getInterface(const Slang::Guid& guid) {
    SLANG_UNUSED(guid);
    return static_cast<ISlangFileSystem*>(this);
  }

  void* SlangVirtualFilesystem::getObject(const Slang::Guid& guid) {
    SLANG_UNUSED(guid);
    return nullptr;
  }

} // namespace zs

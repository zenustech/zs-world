#pragma once

#include "core/slang-com-object.h"
#include "slang-com-helper.h"
#include "slang.h"

namespace zs {

  class SlangVirtualFilesystem : public ISlangMutableFileSystem, public Slang::ComBaseObject {
  public:
    // Begin ISlangUnknown interface
    SLANG_COM_BASE_IUNKNOWN_ALL
    // End ISlangUnknown interface

    // Begin ISlangCastable interface
    SLANG_NO_THROW void* SLANG_MCALL castAs(const Slang::Guid& guid) SLANG_OVERRIDE;
    // End ISlangCastable interface

    // Begin ISlangFileSystem interface
    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(char const* path, ISlangBlob** outBlob) SLANG_OVERRIDE;
    // End ISlangFileSystem interface

    // Begin ISlangFileSystemExt interface
    SlangResult getPathType(const char* path, SlangPathType* pathTypeOut) override;
    SlangResult getPath(PathKind kind, const char* path, ISlangBlob** outPath) override;
    SlangResult getFileUniqueIdentity(const char* path, ISlangBlob** outUniqueIdentity) override;
    SlangResult calcCombinedPath(SlangPathType fromPathType, const char* fromPath, const char* path, ISlangBlob** pathOut) override;
    void clearCache() override;
    SlangResult enumeratePathContents(const char* path, FileSystemContentsCallBack callback, void* userData) override;
    OSPathKind getOSPathKind() override;
    // End ISlangFileSystemExt interface

    // Begin ISlangModifyableFileSystem interface
    SlangResult saveFile(const char* path, const void* data, size_t size) override;
    SlangResult saveFileBlob(const char* path, ISlangBlob* dataBlob) override;
    SlangResult remove(const char* path) override;
    SlangResult createDirectory(const char* path) override;
    // End ISlangModifyableFileSystem interface

  private:
    ISlangUnknown* getInterface(const Slang::Guid& guid);
    void* getObject(const Slang::Guid& guid);
  };

} // namespace zs

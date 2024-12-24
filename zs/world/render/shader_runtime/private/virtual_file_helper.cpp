#include "virtual_file_helper.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace zs {

  class VirtualFileMappingImpl : public IVirtualFileMapping {
  public:
    void add_mapping(std::string_view virtual_directory, std::string_view abs_path) override {
      std::string virtual_dir(virtual_directory);
      while (virtual_dir.back() == '/') {
        virtual_dir.pop_back();
      }
      std::string abs_dir(abs_path);
      while (abs_dir.back() == '/' || abs_dir.back() == '\\') {
        abs_dir.pop_back();
      }
      m_mappings_[virtual_dir] = abs_dir;
    }

    [[nodiscard]] tl::expected<std::string, PathResolveError> resolve(std::string_view virtual_path) const override {
      const auto it = std::find_if(m_mappings_.begin(), m_mappings_.end(), [&](const auto& pair) {
        return virtual_path.compare(0, pair.first.size(), pair.first) == 0 &&
          (virtual_path.size() == pair.first.size() || virtual_path[pair.first.size()] == '/');
      });
      if (it != m_mappings_.end()) {
        std::string resolved_path = it->second + std::string(virtual_path.substr(it->first.size()));
        std::replace(resolved_path.begin(), resolved_path.end(), '\\', '/');
        return resolved_path;
      }
      return tl::make_unexpected(PathResolveError::eVirtualPathNotFound);
    }

    bool exist(const std::string_view path) override {
      if (auto resolve_result = resolve(path)) {
        return std::filesystem::exists(*resolve_result);
      }
      return false;
    }

  private:
    std::unordered_map<std::string, std::string> m_mappings_;
  };

  std::unique_ptr<IVirtualFileMapping> IVirtualFileMapping::create() {
    return std::make_unique<VirtualFileMappingImpl>();
  }

} // namespace zs

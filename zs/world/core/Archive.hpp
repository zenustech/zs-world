#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "world/WorldExport.hpp"
#include "zensim/ZpcImplPattern.hpp"

namespace zs {

  /**
  @brief  For resource/asset IO and serialization/deserialization
  Building block for cache system
   */
  struct ZS_WORLD_EXPORT Archive {
    Archive() = default;
    Archive(std::string_view filename) : _fileName{filename} {}
    Archive(Archive &&o) noexcept
        : _modified{zs::exchange(o._modified, false)},
          _isAscii{zs::exchange(o._isAscii, true)},
          _fileName{zs::move(o._fileName)},
          _buffer{zs::move(o._buffer)} {}
    Archive &operator=(Archive &&o) noexcept {
      Archive tmp(zs::move(o));
      swap(*this, tmp);
      return *this;
    }
    Archive(const Archive &) = delete;
    Archive &operator=(const Archive &) = delete;
    friend void swap(Archive &l, Archive &r) noexcept {
      zs_swap(l._modified, r._modified);
      zs_swap(l._isAscii, r._isAscii);
      zs_swap(l._fileName, r._fileName);
      zs_swap(l._buffer, r._buffer);
    }

    ~Archive() {
      if (valid() && modified()) {
        if (_isAscii)
          saveToFileAscii();
        else
          saveToFileBinary();
      }
    }

    /// @brief set ** modified state ** to true
    /// @return whether ** modified state ** has been changed
    bool setChanged() { return !zs::exchange(_modified, true); }

    /// @brief set ** modified state ** to false
    /// @return whether ** modified state ** has been changed
    bool setUnchanged() { return zs::exchange(_modified, false); }

    bool setString(std::string_view buffer) {
      _buffer.resize(buffer.size());
      _buffer.assign(buffer.begin(), buffer.end());
      return setChanged();
    }
    std::string getBuffer() const { return std::string(_buffer.begin(), _buffer.end()); }

    std::string_view fileName() const noexcept {
      if (_fileName) return *_fileName;
      return "";
    }

    /// IO
    int loadFromFile(std::string_view fn = "", bool isAscii = true);
    int saveToFile(std::string_view fn = "", bool isAscii = true);
    int loadFromFileAscii(std::string_view fn = "") { return loadFromFile(fn, true); }
    int saveToFileAscii(std::string_view fn = "") { return saveToFile(fn, true); }
    int loadFromFileBinary(std::string_view fn = "") { return loadFromFile(fn, false); }
    int saveToFileBinary(std::string_view fn = "") { return saveToFile(fn, true); }

    bool valid() const noexcept { return _fileName.has_value(); }
    bool modified() const noexcept { return _modified; }
    bool empty() const noexcept { return valid() && _buffer.empty(); }

  protected:
    bool _modified{false}, _isAscii{true};
    std::optional<std::string> _fileName{};
    std::vector<char> _buffer{};  // could be ascii or binary
  };

}  // namespace zs
#pragma once
#include <string>

#include "world/WorldExport.hpp"
#include "zensim/ZpcMeta.hpp"

namespace zs {

  struct ConsoleRecord {
    enum type_e : int { unknown = -1, info = 0, warn = 1, error = 2 };

    std::string str;
    int type = -1;  // -1: default, 0: info, 1: warn, 2: error

    ConsoleRecord() = default;
    ConsoleRecord(std::string msg, int type = -1) : str{zs::move(msg)}, type{type} {}

    operator std::string &() noexcept { return str; }
    operator const std::string &() const noexcept { return str; }
    explicit operator std::string_view() noexcept { return str; }
    char *data() noexcept { return str.data(); }
    const char *data() const noexcept { return str.c_str(); }
    const char *c_str() const noexcept { return str.c_str(); }
    auto size() const noexcept { return str.size(); }
  };

}  // namespace zs
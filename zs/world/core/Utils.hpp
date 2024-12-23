#pragma once
#include "world/WorldExport.hpp"
#include "interface/world/ObjectInterface.hpp"
//
#include <cstdio>
// #include <sstream>
#include <string>
#include <vector>

namespace zs {

/// @ref
/// https://stackoverflow.com/questions/5911147/how-to-redirect-printf-output-back-into-code
#define OLD

  class ZS_WORLD_EXPORT StdStreamRedirector {
  public:
#ifdef OLD
    StdStreamRedirector(int fileno = 1)
        : _fno{fileno}, _originalStreamFd(-1), _pipeOut(-1), _pipeIn(-1), _active(false) {}
#else
    StdStreamRedirector(int fileno = 1) : _fno{fileno}, _active(false) {}
#endif

    void start();

    void stop(std::string &captured_output);

    bool active() const noexcept { return _active; }

  private:
    int _fno;
    bool _active;
#ifdef OLD
    int _originalStreamFd;
    int _pipeOut;
    int _pipeIn;
#else
    void *pipe_;
    std::streambuf *original_buf_;
#endif
  };

  template <typename... Ts> std::string cformat(const char *fmt, Ts &&...args) {
    int sz = std::snprintf(nullptr, 0, fmt, args...);
    std::string buf;
    buf.reserve(sz + 1);
    std::sprintf(buf.data(), fmt, FWD(args)...);
    return buf;
  }

  ZS_WORLD_EXPORT std::vector<std::string> parse_string_segments(std::string_view tags,
                                                                 std::string_view sep);

  ZS_WORLD_EXPORT ZsVar zs_int_descriptor(ZsObject expr);
  ZS_WORLD_EXPORT ZsVar zs_float_descriptor(ZsObject expr);
  // ZS_WORLD_EXPORT ZsVar zs_intn_descriptor(ZsObject expr);
  // ZS_WORLD_EXPORT ZsVar zs_floatn_descriptor(ZsObject expr);
  ZS_WORLD_EXPORT ZsVar zs_string_descriptor(ZsObject expr);
  ZS_WORLD_EXPORT ZsVar zs_list_descriptor(ZsObject expr);
  ZS_WORLD_EXPORT ZsVar zs_dict_descriptor(ZsObject expr);

}  // namespace zs
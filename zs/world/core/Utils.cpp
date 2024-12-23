#include "Utils.hpp"

#include <assert.h>
#include <stdio.h>

#include <stdexcept>

#include "interface/details/PyHelper.hpp"
#ifdef _WIN32
#  include <io.h>
// #include <winsock2.h>

#  include <windows.h>
#  define pipe(X) _pipe(X, 4096, 0)
#  define dup _dup
#  define dup2 _dup2
#  define close _close
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

#ifndef OLD
#  include <boost/process.hpp>
#  include <iostream>
namespace bp = boost::process;
#endif

namespace zs {

#ifndef _WIN32
  static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }
#else
  static void set_nonblocking(int fd) {
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
#  if 0
  DWORD available = 0;
  if (!PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
    fprintf(stderr, "PeekNamedPipe before set_nonblocking: %lu, avail: %d\n", GetLastError(), (int)available);
    SetLastError(0);
  }
#  endif

    DWORD mode = PIPE_NOWAIT;  // | PIPE_TYPE_BYTE
    if (!SetNamedPipeHandleState(handle, &mode, NULL, NULL)) {
      fprintf(stderr, "Failed to set non-blocking mode: %lu\n", GetLastError());
      // perror("Failed to set non-blocking mode");
      // errno = 0;
    }
#  if 0
  if (!PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
    fprintf(stderr, "PeekNamedPipe right after set_nonblocking: %lu, avail: %d\n", GetLastError(), (int)available);
    SetLastError(0);
  }
#  endif
  }
#endif

  static bool is_pipe_full(int fd) {
#ifdef _WIN32
    if (errno != 0) {
      // perror("Before checking pipe status");
      errno = 0;
      return true;
    }
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    DWORD available = 0;
    if (!PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
#  if 0
    fprintf(stderr, "Failed to PeekNamedPipe: %lu, avail: %d\n", GetLastError(), (int)available);
    SetLastError(0);
    fprintf(stderr, "after PeekNamedPipe: %lu, avail: %d\n", GetLastError(), (int)available);
    return true;
#  endif
      if (errno != 0) {
        perror("PeekNamedPipe");
        errno = 0;
        return true;
      }
    }
    return false;
    // return available == 0;
#else
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(fd, &write_fds);
    timeval timeout = {0, 0};  // No timeout
    int result = select(fd + 1, NULL, &write_fds, NULL, &timeout);
    if (result == -1) {
      perror("select");
      return true;
    }
    return result == 0;
#endif
  }

  void StdStreamRedirector::start() {
    if (_active) {
      throw std::runtime_error("Capture already started.");
    }

#ifdef OLD
    // Create a pipe
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
      perror("pipe");
      errno = 0;
      exit(1);
    }
    _pipeOut = pipe_fds[0];
    _pipeIn = pipe_fds[1];
    // fcntl(_pipeIn, F_SETFL, O_NONBLOCK);
    // fcntl(_pipeOut, F_SETFL, O_NONBLOCK);
    set_nonblocking(_pipeIn);
    // set_nonblocking(_pipeOut);

    // Save original stdout file descriptor
    _originalStreamFd = -1;
    if (_fno == 1)
      _originalStreamFd = dup(fileno(stdout));
    else if (_fno == 2)
      _originalStreamFd = dup(fileno(stderr));
    if (_originalStreamFd == -1) {
      perror("dup");
      errno = 0;
      exit(1);
    }

    // Redirect stdout to the pipe
    int rc = -1;
    if (_fno == 1)
      rc = dup2(_pipeIn, fileno(stdout));
    else
      rc = dup2(_pipeIn, fileno(stderr));
    if (rc == -1) {
      perror("dup2");
      exit(1);
    }
#else
    if (pipe_) delete static_cast<bp::ipstream *>(pipe_);
    pipe_ = new bp::ipstream();
    original_buf_ = std::cout.rdbuf();
    std::cout.rdbuf(static_cast<bp::ipstream *>(pipe_)->rdbuf());
#endif

    _active = true;
  }

  void StdStreamRedirector::stop(std::string &captured_output) {
    if (!_active) {
      throw std::runtime_error("Capture not started.");
    }

#ifdef OLD
    // if (errno)
    //   perror("stop_0 already failed");
    // Flush the stream to make sure all output is written to the pipe
    int res = 0;
    if (_fno == 1)
      res = fflush(stdout);
    else
      res = fflush(stderr);
    if (res == EOF) {
      perror("Error fflush");
      errno = 0;
    }

    bool isPipeFull = is_pipe_full(_pipeIn);

    // Restore original stdout
    int rc = -1;
    if (_fno == 1)
      rc = dup2(_originalStreamFd, fileno(stdout));
    else
      rc = dup2(_originalStreamFd, fileno(stderr));
    if (rc == -1) {
      perror("dup2");
      errno = 0;
      exit(1);
    }
    close(_originalStreamFd);
    _originalStreamFd = -1;

#  if 0
  /// @note
  /// https://stackoverflow.com/questions/43194259/strange-issue-redirecting-output-using-dup2-sys-call
  if (_fno == 1)
    freopen(NULL, "w", stdout);
  else
    freopen(NULL, "w", stderr);
  if (errno) {
    perror("freopen");
    errno = 0;
  }
#  endif

    // Close the write end of the pipe
    close(_pipeIn);
    _pipeIn = -1;

    // Read the captured output from the pipe
    char buffer[1024];
    int bytes_read;
    while ((bytes_read = read(_pipeOut, buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';
      captured_output += buffer;
    }
    if (isPipeFull)
      captured_output += "\n\t*** BEWARE! PIPE IS WRITTEN FULL! CAPTURED DATA IS "
                       "INCOMPLETE! ***\n\n";
    close(_pipeOut);
    _pipeOut = -1;
#else
    // Restore original stdout buffer
    std::cout.rdbuf(original_buf_);

    // Read the captured output from the pipe
    std::ostringstream oss;
    oss << static_cast<bp::ipstream *>(pipe_)->rdbuf();
    captured_output = oss.str();
#endif

    _active = false;
  }

  ZsVar zs_int_descriptor(ZsObject contents) {
    ZsVar ret;
    if (contents) {
      if (contents.isIntegral()) {
        ret.share(contents);
      } else if (contents.isString()) {
        if (PyVar bs = zs_bytes_obj(contents)) {
          ret = zs_eval_expr(bs.asBytes().c_str());
        } else {
          ret = zs_long_obj_long_long(0);
        }
      } else if (contents.isBytesOrByteArray()) {
        ret = zs_eval_expr(contents.asBytes().c_str());
      } else {
        ret = zs_long_obj_long_long(0);
      }

      if (!ret.getValue().isIntegral()) ret = zs_long_obj_long_long(0);

    } else
      ret = zs_long_obj_long_long(0);
    return ret;
  }

  ZsVar zs_float_descriptor(ZsObject contents) {
    ZsVar ret;
    if (contents) {
      if (contents) {
        if (contents.isFloatingPoint()) {
          ret.share(contents);
        } else if (contents.isString()) {
          if (PyVar bs = zs_bytes_obj(contents)) {
            ret = zs_eval_expr(bs.asBytes().c_str());
          } else {
            ret = zs_float_obj_double(0);
          }
        } else if (contents.isBytesOrByteArray()) {
          ret = zs_eval_expr(contents.asBytes().c_str());
        } else {
          ret = zs_float_obj_double(0);
        }

        if (!ret.getValue().isFloatingPoint()) ret = zs_float_obj_double(0);

      } else
        ret = zs_float_obj_double(0);
    } else
      ret = zs_float_obj_double(0);
    return ret;
  }
  ZsVar zs_string_descriptor(ZsObject contents) {
    ZsVar ret;
    if (contents) {
      if (contents.isString()) {
        ret.share(contents);
      } else if (contents.isBytesOrByteArray()) {
        ret = zs_string_obj_cstr(contents.asBytes().c_str());
      } else {
        ret.share(ZsValue(zs_string_obj_cstr("")));
      }
    } else {
      ret = zs_string_obj_cstr("");
    }
    return ret;
  }
  ZsVar zs_list_descriptor(ZsObject contents) {
    ZsVar ret;
    if (contents) {
      if (contents.isList()) {
        ret.share(contents);  // deepcopy
      } else if (contents.isString()) {
        if (PyVar bs = zs_bytes_obj(contents)) {
          ret = zs_eval_expr(bs.asBytes().c_str());
        } else {
          ret = zs_list_obj_default();
        }
      } else if (contents.isBytesOrByteArray()) {
        ret = zs_eval_expr(contents.asBytes().c_str());
      } else {
        ret = zs_list_obj_default();
      }

      if (!ret.getValue().isList()) ret = zs_list_obj_default();

    } else {
      ret = zs_list_obj_default();
    }
    return ret;
  }
  ZsVar zs_dict_descriptor(ZsObject contents) {
    ZsVar ret;
    if (contents) {
      if (contents.isDict()) {
        ret.share(contents);  // deepcopy
      } else if (contents.isString()) {
        if (PyVar bs = zs_bytes_obj(contents)) {
          ret = zs_eval_expr(bs.asBytes().c_str());
        } else {
          ret = zs_dict_obj_default();
        }
      } else if (contents.isBytesOrByteArray()) {
        ret = zs_eval_expr(contents.asBytes().c_str());
      } else {
        ret = zs_dict_obj_default();
      }

      if (!ret.getValue().isDict()) ret = zs_dict_obj_default();

    } else {
      ret = zs_dict_obj_default();
    }
    return ret;
  }

  std::vector<std::string> parse_string_segments(std::string_view tags, std::string_view sep) {
    using Ti = typename std::vector<std::string>::size_type;
    std::vector<std::string> res;
    Ti st = tags.find_first_not_of(sep, 0);
    for (auto ed = tags.find_first_of(sep, st + 1); ed != std::string::npos;
         ed = tags.find_first_of(sep, st + 1)) {
      res.emplace_back(tags.substr(st, ed - st));
      st = tags.find_first_not_of(sep, ed);
      if (st == std::string::npos) break;
    }
    if (st != std::string::npos && st < tags.size()) {
      res.emplace_back(tags.substr(st));
    }
    return res;
  }

}  // namespace zs

#ifdef _WIN32
#  undef pipe(X)
#  undef dup
#  undef dup2
#  undef close
#endif
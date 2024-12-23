#include "world/core/Archive.hpp"

#include <fstream>

#include "interface/details/PyHelper.hpp"
// #include "zensim/zpc_tpls/fmt/format.h"

namespace zs {

  int Archive::loadFromFile(std::string_view fn, bool isAscii) {
    std::ios_base::openmode mode = std::ios::in;
    if (!isAscii) mode |= std::ios::binary;
    _isAscii = isAscii;

    if (fn.empty()) {
      if (_fileName.has_value())
        fn = *_fileName;
      else {
        zs_print_err_py_cstr("unable to load from file when no filename is specified.");
        return -1;
      }
    } else
      _fileName = fn;

    std::ifstream inFile(fn.data(), mode);
    if (inFile.is_open()) {
      inFile.seekg(0, std::ios::end);
      size_t size = inFile.tellg();
      inFile.seekg(0, std::ios::beg);
      _buffer.resize(size);
      if (size > 0) inFile.read(&_buffer[0], size);

      inFile.close();
      // setChanged();
      return 0;
    }
    return -1;
  }

  int Archive::saveToFile(std::string_view fn, bool isAscii) {
    std::ios_base::openmode mode = std::ios::out;
    if (!isAscii) mode |= std::ios::binary;
    if (_isAscii != isAscii && valid()) {
      zs_print_err_py_cstr("try to save to file in a different encoding (ascii/binary).");
      return -1;
    }

    if (fn.empty()) {
      if (_fileName.has_value())
        fn = *_fileName;
      else {
        zs_print_err_py_cstr("unable to save to file when no filename is specified.");
        return -1;
      }
    } else
      _fileName = fn;

    std::ofstream outFile(fn.data(), mode);
    if (outFile.is_open()) {
      outFile.write(_buffer.data(), _buffer.size());
      outFile.close();
      return 0;
    }
    return -1;
  }

}  // namespace zs
#include "PyZpc.hpp"

#include <cassert>

#include "interface/world/value_type/ValueInterface.hpp"
#include "zensim/io/Filesystem.hpp"
#include "zensim/zpc_tpls/fmt/format.h"

extern "C" {

void init_zpc_jit_environment() {
  int state = 0;
  auto exeDir = zs::abs_exe_directory();
  std::replace(exeDir.begin(), exeDir.end(), '\\', '/');
  auto pyLibsDir = exeDir + "/resource/py_libs";
  std::replace(pyLibsDir.begin(), pyLibsDir.end(), '\\', '/');
  fmt::print("py libs directory: {}\n", pyLibsDir);
  fmt::print("exe directory: {}\n", exeDir);
  fmt::print("clang lib filename: {}\n", ZPC_OUTPUT_BINARY_CLANG);
  auto script = fmt::format(
      R"(
import sys
sys.path.append('{}')

import zpcjit
zpcjit.init_zpc_lib('{}')
zpcjit.init_zpc_clang_lib('{}')
  )",
      pyLibsDir, exeDir + "/" + ZPC_OUTPUT_BINARY_CAPIS, exeDir + "/" + ZPC_OUTPUT_BINARY_CLANG);
  zs_execute_script(script.c_str(), &state);
  assert(state == 0);
#if ZS_ENABLE_CUDA
  fmt::print("nvrtc lib filename: {}\n", ZPC_OUTPUT_BINARY_NVRTC);
  zs_execute_script(
      fmt::format("zpcjit.init_zpc_nvrtc_lib('{}')", exeDir + "/" + ZPC_OUTPUT_BINARY_NVRTC)
          .c_str(),
      &state);
  assert(state == 0);
#endif
  puts("done init zpc jit env");
}
}
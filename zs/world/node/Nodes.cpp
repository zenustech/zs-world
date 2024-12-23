#include "NodeHelper.hpp"
#include "zensim/zpc_tpls/fmt/core.h"

namespace zs {

struct HelloWorldNode : ImplNode<HelloWorldNode> {
  // ResultType apply() { printf("Impl: Hello world.\n"); }
  ResultType apply() {
    printf("Override Impl: Hello world.\n");
    for (auto &[key, input] : this->_inputs) {
      // fmt::print("input  [{}, {}]\n", key, input->reflect());
    }
    for (auto &[key, output] : this->_outputs) {
      fmt::print(fmt::runtime("output {\nkey: \t{}, value: \t"), key);
      output.reflect();
      fmt::print(fmt::runtime("}\n"));
    }
    return Result::Success;
  }
};

ZS_DEFNODE(HelloWorldNode, {{{"int", "count", "0"}, {"len"}},
                            {{"float", "t", "0.f"}},
                            {},
                            {"hello_world.abc/cba/fin"}});

} // namespace zs
#pragma once

#include <cstddef>
#include "zensim/container/Vector.hpp"


namespace zs {
  class ICanGetSpirVCodeMixin {
  public:
    virtual ~ICanGetSpirVCodeMixin() = default;

    /// @brief Get SPIR-V code of current library
    /// @param o_is_success Pass a valid pointer to feed is success flag. Nothing set if nullptr.
    [[nodiscard]] virtual Vector<std::byte> get_spirv_code(bool* o_is_success) = 0;
  };
}

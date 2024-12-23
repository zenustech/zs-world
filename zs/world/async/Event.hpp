///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include <condition_variable>

#include "../WorldExport.hpp"
#include "zensim/Platform.hpp"
#include "zensim/execution/ConcurrencyPrimitive.hpp"

namespace zs {

  class ZS_WORLD_EXPORT lightweight_manual_reset_event {
  public:
    lightweight_manual_reset_event(bool initiallySet = false);

    ~lightweight_manual_reset_event();

    void set() noexcept;

    void reset() noexcept;

    void wait() noexcept;

  private:
#ifdef ZS_PLATFORM_LINUX
    std::atomic<int> m_value;
#elif defined(ZS_PLATFORM_WINDOWS)
    // Windows 8 or newer we can use WaitOnAddress()
    std::atomic<std::uint8_t> m_value;
#else
    // For other platforms that don't have a native futex
    // or manual reset event we can just use a std::mutex
    // and std::condition_variable to perform the wait.
    // Not so lightweight, but should be portable to all platforms.
    Mutex m_mutex;
    std::condition_variable_any m_cv;
    bool m_isSet;
#endif
  };

}  // namespace zs
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#include "Event.hpp"

namespace zs {

#include <system_error>

#ifdef ZS_PLATFORM_WINDOWS
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

  lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
      : m_value(initiallySet ? 1 : 0) {}

  lightweight_manual_reset_event::~lightweight_manual_reset_event() {}

  void lightweight_manual_reset_event::set() noexcept {
    m_value.store(1, std::memory_order_release);
    WakeByAddressAll(&m_value);
  }

  void lightweight_manual_reset_event::reset() noexcept {
    m_value.store(0, std::memory_order_relaxed);
  }

  void lightweight_manual_reset_event::wait() noexcept {
    // Wait in a loop as WaitOnAddress() can have spurious wake-ups.
    int value = m_value.load(std::memory_order_acquire);
    BOOL ok = TRUE;
    while (value == 0) {
      if (!ok) {
        // Previous call to WaitOnAddress() failed for some reason.
        // Put thread to sleep to avoid sitting in a busy loop if it keeps failing.
        Sleep(1);
      }

      ok = WaitOnAddress(&m_value, &value, sizeof(m_value), INFINITE);
      value = m_value.load(std::memory_order_acquire);
    }
  }

#elif defined(ZS_PLATFORM_LINUX)

#  include <linux/futex.h>
#  include <sys/syscall.h>
#  include <sys/time.h>
#  include <unistd.h>

#  include <cassert>
#  include <cerrno>
#  include <climits>

  namespace {
    namespace local {
      // No futex() function provided by libc.
      // Wrap the syscall ourselves here.
      int futex(int* UserAddress, int FutexOperation, int Value, const struct timespec* timeout,
                int* UserAddress2, int Value3) {
        return syscall(SYS_futex, UserAddress, FutexOperation, Value, timeout, UserAddress2,
                       Value3);
      }
    }  // namespace local
  }  // namespace

  lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
      : m_value(initiallySet ? 1 : 0) {}

  lightweight_manual_reset_event::~lightweight_manual_reset_event() {}

  void lightweight_manual_reset_event::set() noexcept {
    m_value.store(1, std::memory_order_release);

    constexpr int numberOfWaitersToWakeUp = INT_MAX;

    [[maybe_unused]] int numberOfWaitersWokenUp
        = local::futex(reinterpret_cast<int*>(&m_value), FUTEX_WAKE_PRIVATE,
                       numberOfWaitersToWakeUp, nullptr, nullptr, 0);

    // There are no errors expected here unless this class (or the caller)
    // has done something wrong.
    assert(numberOfWaitersWokenUp != -1);
  }

  void lightweight_manual_reset_event::reset() noexcept {
    m_value.store(0, std::memory_order_relaxed);
  }

  void lightweight_manual_reset_event::wait() noexcept {
    // Wait in a loop as futex() can have spurious wake-ups.
    int oldValue = m_value.load(std::memory_order_acquire);
    while (oldValue == 0) {
      int result = local::futex(reinterpret_cast<int*>(&m_value), FUTEX_WAIT_PRIVATE, oldValue,
                                nullptr, nullptr, 0);
      if (result == -1) {
        if (errno == EAGAIN) {
          // The state was changed from zero before we could wait.
          // Must have been changed to 1.
          return;
        }

        // Other errors we'll treat as transient and just read the
        // value and go around the loop again.
      }

      oldValue = m_value.load(std::memory_order_acquire);
    }
  }

#else

  lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
      : m_isSet(initiallySet) {}

  lightweight_manual_reset_event::~lightweight_manual_reset_event() {}

  void lightweight_manual_reset_event::set() noexcept {
    {
      std::lock_guard<Mutex> lock(m_mutex);
      m_isSet = true;
    }
    m_cv.notify_all();
  }

  void lightweight_manual_reset_event::reset() noexcept {
    std::lock_guard<Mutex> lock(m_mutex);
    m_isSet = false;
  }

  void lightweight_manual_reset_event::wait() noexcept {
    std::unique_lock<Mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_isSet; });
  }

#endif

}  // namespace zs
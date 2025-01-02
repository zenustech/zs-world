#pragma once
#include <any>
#include <variant>

#include "Coro.hpp"

namespace zs {

  template <typename... Es> struct Event {};

  /// @ref author: Pavel Novikov
  struct StateMachine {
    struct promise_type;
    using CoroHandle = std::coroutine_handle<promise_type>;

    template <typename E> void onEvent(E&& e);

    ~StateMachine() {
      if (_coroHandle) {
        _coroHandle.destroy();
        _coroHandle = {};
      }
    }
    StateMachine(StateMachine&& o) noexcept : _coroHandle{zs::exchange(o._coroHandle, nullptr)} {}
    StateMachine& operator=(StateMachine&& o) noexcept {
      if (this == zs::addressof(o)) return *this;
      StateMachine tmp(zs::move(o));
      zs_swap(_coroHandle, tmp._coroHandle);
      return *this;
    }
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;

    StateMachine(CoroHandle handle = nullptr) noexcept : _coroHandle{handle} {}

  protected:
    CoroHandle _coroHandle;
  };

  struct StateMachine::promise_type {
    using CoroHandle = StateMachine::CoroHandle;

    StateMachine get_return_object() noexcept {
      return std::coroutine_handle<promise_type>::from_promise(*this);
    }
    std::suspend_never initial_suspend() const noexcept { return {}; }
    std::suspend_always final_suspend() const noexcept { return {}; }

    template <typename... Es> auto await_transform(Event<Es...>) noexcept {
      isWantedEvent
          = [](const std::type_info& type) -> bool { return ((type == typeid(Es)) || ...); };

      struct awaiter {
        awaiter(std::any* e) noexcept : event{e} {}

        bool await_ready() const noexcept { return false; }
        std::variant<Es...> await_resume() const {
          std::variant<Es...> ret;
          (void)((event->type() == typeid(Es) ? (ret = zs::move(*std::any_cast<Es>(event)), true)
                                              : false)
                 || ...);
          return ret;
        }
        void await_suspend(CoroHandle) noexcept {}

        const std::any* event{nullptr};
      };
      return awaiter{&currentEvent};
    }
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}

    std::any currentEvent{};
    bool (*isWantedEvent)(const std::type_info&) = nullptr;
  };

  template <typename E> void StateMachine::onEvent(E&& e) {
    auto& p = _coroHandle.promise();
    if (p.isWantedEvent(typeid(E))) {
      p.currentEvent = zs::forward<E>(e);
      _coroHandle.resume();
    };
  }

}  // namespace zs
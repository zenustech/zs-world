#pragma once
#include <cassert>
#include <coroutine>
#include <exception>
#include <variant>

#include "../WorldExport.hpp"
#include "zensim/ZpcMeta.hpp"
#include "zensim/types/Polymorphism.h"

namespace zs {

  ///
  ///
  ///
  struct promise_base_type {
    friend struct FinalAwaiter;
    struct FinalAwaiter {
      constexpr bool await_ready() const noexcept { return false; }
      template <typename P>
      std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) noexcept {
        if (auto c = h.promise()._awaitingCoroutine)
          return c;
        else
          return std::noop_coroutine();
      }
      constexpr void await_resume() const noexcept {}
    };

    promise_base_type() noexcept = default;
    std::suspend_always initial_suspend() noexcept { return {}; }
    FinalAwaiter final_suspend() noexcept { return {}; }

    void set_continuation(std::coroutine_handle<> h) noexcept { _awaitingCoroutine = h; }

    bool _inSuspension{true};

  protected:
    std::coroutine_handle<> _awaitingCoroutine{};
  };

  /// RAII coroutine type
  template <typename R> struct PromiseType;
  template <typename R = void> struct Future {
    using promise_type = PromiseType<R>;
    using CoroHandle = std::coroutine_handle<promise_type>;

    friend struct awaiter_base_type;
    struct awaiter_base_type {
      bool await_ready() noexcept {
        auto skipSuspension = !_awaitedCoroutine || _awaitedCoroutine.done();
        if (skipSuspension) _awaitedCoroutine.promise()._inSuspension = false;  // !skipSuspension
        return skipSuspension;
      }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
        _awaitedCoroutine.promise().set_continuation(awaitingCoroutine);
        _awaitedCoroutine.promise()._inSuspension = false;  // resume
        return _awaitedCoroutine;
      }

      awaiter_base_type(CoroHandle coroHandle) : _awaitedCoroutine{coroHandle} {}

    protected:
      CoroHandle _awaitedCoroutine;
    };

    Future(CoroHandle handle = nullptr) noexcept : _coroHandle{handle} {}
    Future(Future&& o) noexcept : _coroHandle{zs::exchange(o._coroHandle, {})} {}
    Future& operator=(Future&& o) noexcept {
      if (this == &o) return *this;
      Future tmp(zs::move(o));
      swap(tmp);
      return *this;
    }
    void swap(Future& b) noexcept { zs_swap(_coroHandle, b._coroHandle); }
    ~Future() {
      if (_coroHandle) {
        _coroHandle.destroy();
        _coroHandle = {};
      }
    }

    template <typename Expr> decltype(auto) await_transform(Expr&& expr) const {
      _coroHandle.promise()._inSuspension = true;  // suspend
      return zs::forward<Expr>(expr);
    }
    template <typename Expr> decltype(auto) await_transform(Expr&& expr) {
      _coroHandle.promise()._inSuspension = true;  // suspend
      return zs::forward<Expr>(expr);
    }
    auto operator co_await() & noexcept {
      struct awaiter : awaiter_base_type {
        decltype(auto) await_resume() {
          assert(this->_awaitedCoroutine);
          return this->_awaitedCoroutine.promise().get();
        }
      };
      return awaiter{_coroHandle};
    }
    auto operator co_await() && noexcept {
      struct awaiter : awaiter_base_type {
        decltype(auto) await_resume() {
          assert(this->_awaitedCoroutine);
          return zs::move(this->_awaitedCoroutine.promise()).get();
        }
      };
      return awaiter{_coroHandle};
    }

    auto getHandle() const { return _coroHandle; }
    void resume() const {
      _coroHandle.promise()._inSuspension = false;
      _coroHandle.resume();
    }
    // _coroHandle.done()
    bool isReady() const noexcept {
      return !_coroHandle || (_coroHandle.promise()._inSuspension && _coroHandle.done());
    }
    bool isDone() const noexcept {
      return _coroHandle && _coroHandle.promise()._inSuspension && _coroHandle.done();
    }
    bool isInProgress() const noexcept {
      return _coroHandle && !_coroHandle.promise()._inSuspension;
    }

    decltype(auto) ref() { return _coroHandle.promise().get(); }
    decltype(auto) get() & { return _coroHandle.promise().get(); }
    decltype(auto) get() && { return zs::move(_coroHandle.promise()).get(); }

    explicit operator R() { return get(); }

  protected:
    CoroHandle _coroHandle;
  };

  using Task = Future<>;

  /// Task coroutine's promise_type
  template <typename R> struct PromiseType final : promise_base_type {
    PromiseType() noexcept = default;
    ~PromiseType() noexcept(noexcept(declval<R>().~R())
                            && noexcept(declval<std::exception_ptr>().~exception_ptr())) {
#if 1
      // delegate to std::variant
#else
      switch (_resultType) {
        case result_type::value:
          _result.~R();
          break;
        case result_type::exception:
          _exception.~exception_ptr();
          break;
        default:
          break;
      }
#endif
    }

    Future<R> get_return_object() noexcept {
      return std::coroutine_handle<PromiseType>::from_promise(*this);
    }
    void unhandled_exception() {
#if 1
      _result = std::exception_ptr(std::current_exception());
#else
      ::new (static_cast<void*>(zs::addressof(_exception)))
          std::exception_ptr(std::current_exception());
      _resultType = result_type::exception;
#endif
    }
    template <typename VALUE, enable_if_t<is_convertible_v<VALUE&&, R>> = 0>
    void return_value(VALUE&& value) noexcept(std::is_nothrow_constructible_v<R, VALUE&&>) {
#if 1
      _result = R(zs::forward<VALUE>(value));
#else
      ::new (static_cast<void*>(zs::addressof(_result))) R(zs::forward<VALUE>(value));
      _resultType = result_type::value;
#endif
    }

    bool isValueInitialized() const noexcept {
      return !std::holds_alternative<std::monostate>(_result);
    }

    R& get() & {  // additional
#if 1
      switch (_result.index()) {
        case 1:
          std::rethrow_exception(std::get<1>(_result));
        case 0:
          assert(false);
        case 2:
        default:
          return std::get<2>(_result);
      }
#else
      switch (_resultType) {
        case result_type::exception:
          std::rethrow_exception(_exception);
        case result_type::empty:
          assert(false);
        case result_type::value:
        default:
          return _result;
      }
#endif
    }
    // HACK: Need to have co_await of task<int> return prvalue rather than
    // rvalue-reference to work around an issue with MSVC where returning
    // rvalue reference of a fundamental type from await_resume() will
    // cause the value to be copied to a temporary. This breaks the
    // sync_wait() implementation.
    // See https://github.com/lewissbaker/cppcoro/issues/40#issuecomment-326864107
    using rvalue_type = conditional_t<is_arithmetic_v<R> || is_pointer_v<R>, R, R&&>;
    rvalue_type get() && {
#if 1
      switch (_result.index()) {
        case 1:
          std::rethrow_exception(std::get<1>(zs::move(_result)));
        case 0:
          assert(false);
        case 2:
        default:
          return std::get<2>(zs::move(_result));
      }
#else
      switch (_resultType) {
        case result_type::exception:
          std::rethrow_exception(_exception);
        case result_type::empty:
          assert(false);
        case result_type::value:
        default:
          return zs::move(_result);
      }
#endif
    }

    // enum class result_type { empty, value, exception };

  private:
#if 1
    std::variant<std::monostate, std::exception_ptr, R> _result;
#else
    result_type _resultType{result_type::empty};
    union {
      std::exception_ptr _exception{};
      R _result;
    };
#endif
  };
  template <> struct PromiseType<void> : promise_base_type {
    PromiseType() noexcept = default;

    Future<void> get_return_object() noexcept {
      return std::coroutine_handle<PromiseType>::from_promise(*this);
    }
    void unhandled_exception() noexcept { _exception = std::current_exception(); }
    void return_void() noexcept {}

    void get() {
      if (_exception) std::rethrow_exception(_exception);
    }

  private:
    std::exception_ptr _exception;
  };
  template <typename R> struct PromiseType<R&> final : promise_base_type {
    PromiseType() noexcept = default;

    Future<R&> get_return_object() noexcept {
      return std::coroutine_handle<PromiseType>::from_promise(*this);
    }
    void unhandled_exception() noexcept { _exception = std::current_exception(); }
    void return_value(R& value) { _result = addressof(value); }

    R& get() & {  // additional
      if (_exception) std::rethrow_exception(_exception);
      return *_result;
    }

  private:
    R* _result{nullptr};
    std::exception_ptr _exception{};
  };

}  // namespace zs
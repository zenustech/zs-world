#pragma once
#include <atomic>
#include <cassert>
#include <coroutine>
#include <future>
#include <vector>

#include "world/core/Signal.hpp"
#include "Coro.hpp"
#include "Event.hpp"
#include "cppcoro/cancellation_registration.hpp"
#include "cppcoro/cancellation_source.hpp"
#include "cppcoro/cancellation_token.hpp"
#include "zensim/ZpcMeta.hpp"
#include "zensim/ZpcResource.hpp"
#include "zensim/ZpcTuple.hpp"

// #include "zensim/zpc_tpls/fmt/format.h"

namespace std {

  /// @ref CppCon 2017_ Gor Nishanov “Naked coroutines live (with networking)”
  template <typename R, typename... Args> struct coroutine_traits<future<R>, Args...> {
    struct promise_type {
      suspend_always initial_suspend() noexcept { return {}; }
      suspend_always final_suspend() noexcept { return {}; }
      void return_value(const R& v) { p.set_value(v); }
      void return_value(R&& v) { p.set_value(move(v)); }

      auto get_return_object() { return p.get_future(); }
      void unhandled_exception() { p.set_exception(std::current_exception()); }
      promise<R> p;
    };
  };
}  // namespace std

namespace zs {

  ///////////////////////////////////////////////////////////////////////////////
  // Copyright (c) Lewis Baker
  // Licenced under MIT license. See LICENSE.txt for details.
  ///////////////////////////////////////////////////////////////////////////////
  namespace detail {

    struct void_value {};

    ///
    /// coroutine traits
    template <typename T> struct is_coroutine_handle : false_type {};

    template <typename PROMISE> struct is_coroutine_handle<std::coroutine_handle<PROMISE>>
        : true_type {};

    // NOTE: We're accepting a return value of coroutine_handle<P> here
    // which is an extension supported by Clang which is not yet part of
    // the C++ coroutines TS.
    template <typename T> struct is_valid_await_suspend_return_value
        : std::disjunction<is_void<T>, is_same<T, bool>, is_coroutine_handle<T>> {};

    template <typename T, typename = void_t<>> struct is_awaiter : false_type {};

    // NOTE: We're testing whether await_suspend() will be callable using an
    // arbitrary coroutine_handle here by checking if it supports being passed
    // a coroutine_handle<void>. This may result in a false-result for some
    // types which are only awaitable within a certain context.
    template <typename T> struct is_awaiter<
        T, void_t<decltype(declval<T>().await_ready()),
                  decltype(declval<T>().await_suspend(declval<std::coroutine_handle<>>())),
                  decltype(declval<T>().await_resume())>>
        : std::conjunction<
              is_constructible<bool, decltype(declval<T>().await_ready())>,
              detail::is_valid_await_suspend_return_value<decltype(declval<T>().await_suspend(
                  declval<std::coroutine_handle<>>()))>> {};

    template <typename T>
    auto get_awaiter_impl(T&& value,
                          int) noexcept(noexcept(static_cast<T&&>(value).operator co_await()))
        -> decltype(static_cast<T&&>(value).operator co_await()) {
      return static_cast<T&&>(value).operator co_await();
    }

    template <typename T>
    auto get_awaiter_impl(T&& value,
                          long) noexcept(noexcept(operator co_await(static_cast<T&&>(value))))
        -> decltype(operator co_await(static_cast<T&&>(value))) {
      return operator co_await(static_cast<T&&>(value));
    }

    template <typename T, enable_if_t<is_awaiter<T&&>::value> = 0>
    T&& get_awaiter_impl(T&& value, ...) noexcept {
      return static_cast<T&&>(value);
    }

    template <typename T>
    auto get_awaiter(T&& value) noexcept(noexcept(detail::get_awaiter_impl(static_cast<T&&>(value),
                                                                           123)))
        -> decltype(detail::get_awaiter_impl(static_cast<T&&>(value), 123)) {
      return detail::get_awaiter_impl(static_cast<T&&>(value), 123);
    }

  }  // namespace detail

  template <typename T, typename = void> struct awaitable_traits {};

  template <typename T>
  struct awaitable_traits<T, void_t<decltype(detail::get_awaiter(declval<T>()))>> {
    using awaiter_t = decltype(detail::get_awaiter(declval<T>()));

    using await_result_t = decltype(declval<awaiter_t>().await_resume());
  };
  template <typename T, typename = void> struct is_awaitable : false_type {};

  template <typename T> struct is_awaitable<T, void_t<decltype(detail::get_awaiter(declval<T>()))>>
      : true_type {};

  template <typename T> constexpr bool is_awaitable_v = is_awaitable<T>::value;

  ///
  /// via (persistent)
  template <typename SCHEDULER, typename AWAITABLE>
  Future<void> via(SCHEDULER& scheduler, AWAITABLE task) {
    struct Awaiter {
      SCHEDULER& scheduler;
      AWAITABLE task;
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h) {
        scheduler.schedule([this, h]() mutable {
          while (!task.getHandle().done()) task.resume();
          h.resume();
        });
      }
      void await_resume() {}
    };

    co_await Awaiter{scheduler, zs::move(task)};
    co_return;
  }

  /// schedule_on (finish awaitable then)
  template <typename SCHEDULER, typename AWAITABLE>
  auto schedule_on(SCHEDULER& scheduler, AWAITABLE awaitable)
      -> Future<remove_rvalue_reference_t<typename awaitable_traits<AWAITABLE>::await_result_t>> {
    co_await scheduler.schedule();
    co_return co_await zs::move(awaitable);
  }

  /// resume_on (finish awaitable first)
  template <typename SCHEDULER, typename AWAITABLE,
            typename AWAIT_RESULT
            = remove_rvalue_reference_t<typename awaitable_traits<AWAITABLE>::await_result_t>,
            std::enable_if_t<!std::is_void_v<AWAIT_RESULT>, int> = 0>
  auto resume_on(SCHEDULER& scheduler, AWAITABLE awaitable) -> Future<AWAIT_RESULT> {
    bool rescheduled = false;
    std::exception_ptr ex;
    try {
      // We manually get the awaiter here so that we can keep
      // it alive across the call to `scheduler.schedule()`
      // just in case the result is a reference to a value
      // in the awaiter that would otherwise be a temporary
      // and destructed before the value could be returned.

      auto&& awaiter = detail::get_awaiter(static_cast<AWAITABLE&&>(awaitable));

      auto&& result = co_await static_cast<decltype(awaiter)>(awaiter);

      // Flag as rescheduled before scheduling in case it is the
      // schedule() operation that throws an exception as we don't
      // want to attempt to schedule twice if scheduling fails.
      rescheduled = true;

      co_await scheduler.schedule();

      co_return static_cast<decltype(result)>(result);
    } catch (...) {
      ex = std::current_exception();
    }

    // We still want to resume on the scheduler even in the presence
    // of an exception.
    if (!rescheduled) {
      co_await scheduler.schedule();
    }

    std::rethrow_exception(ex);
  }

  template <typename SCHEDULER, typename AWAITABLE,
            typename AWAIT_RESULT
            = remove_rvalue_reference_t<typename awaitable_traits<AWAITABLE>::await_result_t>,
            std::enable_if_t<std::is_void_v<AWAIT_RESULT>, int> = 0>
  auto resume_on(SCHEDULER& scheduler, AWAITABLE awaitable) -> Future<> {
    std::exception_ptr ex;
    try {
      co_await static_cast<AWAITABLE&&>(awaitable);
    } catch (...) {
      ex = std::current_exception();
    }

    // NOTE: We're assuming that `schedule()` operation is noexcept
    // here. If it were to throw what would we do if 'ex' was non-null?
    // Presumably we'd treat it the same as throwing an exception while
    // unwinding and call std::terminate()?

    co_await scheduler.schedule();

    if (ex) {
      std::rethrow_exception(ex);
    }
  }

  /// sync_wait
  namespace detail {
    template <typename RESULT> class sync_wait_task;

    template <typename RESULT> class sync_wait_task_promise final {
      using coroutine_handle_t = std::coroutine_handle<sync_wait_task_promise<RESULT>>;

    public:
      using reference = RESULT&&;

      sync_wait_task_promise() noexcept {}

      void start(lightweight_manual_reset_event& event) {
        m_event = &event;
        coroutine_handle_t::from_promise(*this).resume();
      }

      auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

      std::suspend_always initial_suspend() noexcept { return {}; }

      auto final_suspend() noexcept {
        class completion_notifier {
        public:
          bool await_ready() const noexcept { return false; }

          void await_suspend(coroutine_handle_t coroutine) const noexcept {
            coroutine.promise().m_event->set();
          }

          void await_resume() noexcept {}
        };

        return completion_notifier{};
      }

      auto yield_value(reference result) noexcept {
        m_result = zs::addressof(result);
        return final_suspend();
      }

      void return_void() noexcept {
        // The coroutine should have either yielded a value or thrown
        // an exception in which case it should have bypassed return_void().
        assert(false);
      }

      void unhandled_exception() { m_exception = std::current_exception(); }

      reference result() {
        if (m_exception) {
          std::rethrow_exception(m_exception);
        }

        return static_cast<reference>(*m_result);
      }

    private:
      lightweight_manual_reset_event* m_event;
      remove_reference_t<RESULT>* m_result;
      std::exception_ptr m_exception;
    };

    template <> class sync_wait_task_promise<void> {
      using coroutine_handle_t = std::coroutine_handle<sync_wait_task_promise<void>>;

    public:
      sync_wait_task_promise() noexcept {}

      void start(lightweight_manual_reset_event& event) {
        m_event = &event;
        coroutine_handle_t::from_promise(*this).resume();
      }

      auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

      std::suspend_always initial_suspend() noexcept { return {}; }

      auto final_suspend() noexcept {
        class completion_notifier {
        public:
          bool await_ready() const noexcept { return false; }

          void await_suspend(coroutine_handle_t coroutine) const noexcept {
            coroutine.promise().m_event->set();
          }

          void await_resume() noexcept {}
        };

        return completion_notifier{};
      }

      void return_void() {}

      void unhandled_exception() { m_exception = std::current_exception(); }

      void result() {
        if (m_exception) {
          std::rethrow_exception(m_exception);
        }
      }

    private:
      lightweight_manual_reset_event* m_event;
      std::exception_ptr m_exception;
    };

    template <typename RESULT> class sync_wait_task final {
    public:
      using promise_type = sync_wait_task_promise<RESULT>;

      using coroutine_handle_t = std::coroutine_handle<promise_type>;

      sync_wait_task(coroutine_handle_t coroutine) noexcept : m_coroutine(coroutine) {}

      sync_wait_task(sync_wait_task&& other) noexcept
          : m_coroutine(zs::exchange(other.m_coroutine, coroutine_handle_t{})) {}

      ~sync_wait_task() {
        if (m_coroutine) m_coroutine.destroy();
      }

      sync_wait_task(const sync_wait_task&) = delete;
      sync_wait_task& operator=(const sync_wait_task&) = delete;

      void start(lightweight_manual_reset_event& event) noexcept {
        m_coroutine.promise().start(event);
      }

      decltype(auto) result() { return m_coroutine.promise().result(); }

    private:
      coroutine_handle_t m_coroutine;
    };

    /// @note RESULT could be reference
    template <typename AWAITABLE,
              typename RESULT = typename awaitable_traits<AWAITABLE&&>::await_result_t,
              enable_if_t<!is_void_v<RESULT>> = 0>
    sync_wait_task<RESULT> make_sync_wait_task(AWAITABLE&& awaitable) {
      co_yield co_await zs::forward<AWAITABLE>(awaitable);
    }

    template <typename AWAITABLE,
              typename RESULT = typename awaitable_traits<AWAITABLE&&>::await_result_t,
              enable_if_t<is_void_v<RESULT>> = 0>
    sync_wait_task<void> make_sync_wait_task(AWAITABLE&& awaitable) {
      co_await zs::forward<AWAITABLE>(awaitable);
    }
  }  // namespace detail

  template <typename AWAITABLE> auto sync_wait(AWAITABLE&& awaitable) ->
      typename awaitable_traits<AWAITABLE&&>::await_result_t {
    auto task = detail::make_sync_wait_task(zs::forward<AWAITABLE>(awaitable));
    lightweight_manual_reset_event event;
    task.start(event);
    event.wait();
    return task.result();
  }

  /// when_all
  namespace detail {
    class when_all_counter {
    public:
      when_all_counter(i32 count) noexcept : _count(count), _awaitingCoroutine(nullptr) {}

      bool is_ready() const noexcept {
        // We consider this complete if we're asking whether it's ready
        // after a coroutine has already been registered.
        return static_cast<bool>(_awaitingCoroutine);
      }

      bool try_await(std::coroutine_handle<> awaitingCoroutine) noexcept {
        _awaitingCoroutine = awaitingCoroutine;
        return _count.fetch_sub(1, std::memory_order_acq_rel) > 0;
      }

      void notify_awaitable_completed() noexcept {
        if (_count.fetch_sub(1, std::memory_order_acq_rel) == 0) {
          _awaitingCoroutine.resume();
        }
      }

    protected:
      std::atomic<i32> _count;
      std::coroutine_handle<> _awaitingCoroutine;
    };
    template <typename TASK_CONTAINER> class when_all_ready_awaitable;

    template <> class when_all_ready_awaitable<zs::tuple<>> {
    public:
      constexpr when_all_ready_awaitable() noexcept {}
      explicit constexpr when_all_ready_awaitable(zs::tuple<>) noexcept {}

      constexpr bool await_ready() const noexcept { return true; }
      void await_suspend(std::coroutine_handle<>) noexcept {}
      zs::tuple<> await_resume() const noexcept { return {}; }
    };

    template <typename... TASKS> class when_all_ready_awaitable<zs::tuple<TASKS...>> {
    public:
      explicit when_all_ready_awaitable(TASKS&&... tasks) noexcept(
          std::conjunction_v<is_nothrow_move_constructible<TASKS>...>)
          : m_counter(sizeof...(TASKS)), m_tasks(zs::move(tasks)...) {}

      explicit when_all_ready_awaitable(zs::tuple<TASKS...>&& tasks) noexcept(
          is_nothrow_move_constructible_v<zs::tuple<TASKS...>>)
          : m_counter(sizeof...(TASKS)), m_tasks(zs::move(tasks)) {}

      when_all_ready_awaitable(when_all_ready_awaitable&& other) noexcept
          : m_counter(sizeof...(TASKS)), m_tasks(zs::move(other.m_tasks)) {}

      auto operator co_await() & noexcept {
        struct awaiter {
          awaiter(when_all_ready_awaitable& awaitable) noexcept : m_awaitable(awaitable) {}

          bool await_ready() const noexcept { return m_awaitable.is_ready(); }

          bool await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
            return m_awaitable.try_await(awaitingCoroutine);
          }

          zs::tuple<TASKS...>& await_resume() noexcept { return m_awaitable.m_tasks; }

        private:
          when_all_ready_awaitable& m_awaitable;
        };

        return awaiter{*this};
      }

      auto operator co_await() && noexcept {
        struct awaiter {
          awaiter(when_all_ready_awaitable& awaitable) noexcept : m_awaitable(awaitable) {}

          bool await_ready() const noexcept { return m_awaitable.is_ready(); }

          bool await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
            return m_awaitable.try_await(awaitingCoroutine);
          }

          zs::tuple<TASKS...>&& await_resume() noexcept { return zs::move(m_awaitable.m_tasks); }

        private:
          when_all_ready_awaitable& m_awaitable;
        };

        return awaiter{*this};
      }

    private:
      bool is_ready() const noexcept { return m_counter.is_ready(); }

      bool try_await(std::coroutine_handle<> awaitingCoroutine) noexcept {
        start_tasks(make_integer_sequence<size_t, sizeof...(TASKS)>{});
        return m_counter.try_await(awaitingCoroutine);
      }

      template <size_t... INDICES> void start_tasks(integer_sequence<size_t, INDICES...>) noexcept {
        ((void)zs::get<INDICES>(m_tasks).start(m_counter), ...);
      }

      when_all_counter m_counter;
      zs::tuple<TASKS...> m_tasks;
    };

    template <typename TASK_CONTAINER> class when_all_ready_awaitable {
    public:
      explicit when_all_ready_awaitable(TASK_CONTAINER&& tasks) noexcept
          : m_counter(tasks.size()), m_tasks(zs::forward<TASK_CONTAINER>(tasks)) {}

      when_all_ready_awaitable(when_all_ready_awaitable&& other) noexcept(
          is_nothrow_move_constructible_v<TASK_CONTAINER>)
          : m_counter(other.m_tasks.size()), m_tasks(zs::move(other.m_tasks)) {}

      when_all_ready_awaitable(const when_all_ready_awaitable&) = delete;
      when_all_ready_awaitable& operator=(const when_all_ready_awaitable&) = delete;

      auto operator co_await() & noexcept {
        class awaiter {
        public:
          awaiter(when_all_ready_awaitable& awaitable) : m_awaitable(awaitable) {}

          bool await_ready() const noexcept { return m_awaitable.is_ready(); }

          bool await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
            return m_awaitable.try_await(awaitingCoroutine);
          }

          TASK_CONTAINER& await_resume() noexcept { return m_awaitable.m_tasks; }

        private:
          when_all_ready_awaitable& m_awaitable;
        };

        return awaiter{*this};
      }

      auto operator co_await() && noexcept {
        class awaiter {
        public:
          explicit awaiter(when_all_ready_awaitable& awaitable) : m_awaitable(awaitable) {}

          bool await_ready() const noexcept { return m_awaitable.is_ready(); }

          bool await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept {
            return m_awaitable.try_await(awaitingCoroutine);
          }

          TASK_CONTAINER&& await_resume() noexcept { return zs::move(m_awaitable.m_tasks); }

        private:
          when_all_ready_awaitable& m_awaitable;
        };

        return awaiter{*this};
      }

    private:
      bool is_ready() const noexcept { return m_counter.is_ready(); }

      bool try_await(std::coroutine_handle<> awaitingCoroutine) noexcept {
        for (auto&& task : m_tasks) {
          task.start(m_counter);
        }

        return m_counter.try_await(awaitingCoroutine);
      }

      when_all_counter m_counter;
      TASK_CONTAINER m_tasks;
    };

    template <typename RESULT> class when_all_task;

    template <typename RESULT> class when_all_task_promise final {
    public:
      using coroutine_handle_t = std::coroutine_handle<when_all_task_promise<RESULT>>;

      when_all_task_promise() noexcept {}

      auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

      std::suspend_always initial_suspend() noexcept { return {}; }

      auto final_suspend() noexcept {
        class completion_notifier {
        public:
          bool await_ready() const noexcept { return false; }

          void await_suspend(coroutine_handle_t coro) const noexcept {
            coro.promise().m_counter->notify_awaitable_completed();
          }

          void await_resume() const noexcept {}
        };

        return completion_notifier{};
      }

      void unhandled_exception() noexcept { m_exception = std::current_exception(); }

      void return_void() noexcept {
        // We should have either suspended at co_yield point or
        // an exception was thrown before running off the end of
        // the coroutine.
        assert(false);
      }

      auto yield_value(RESULT&& result) noexcept {
        m_result = zs::addressof(result);
        return final_suspend();
      }

      void start(when_all_counter& counter) noexcept {
        m_counter = &counter;
        coroutine_handle_t::from_promise(*this).resume();
      }

      RESULT& result() & {
        rethrow_if_exception();
        return *m_result;
      }

      RESULT&& result() && {
        rethrow_if_exception();
        return forward<RESULT>(*m_result);
      }

    private:
      void rethrow_if_exception() {
        if (m_exception) {
          std::rethrow_exception(m_exception);
        }
      }

      when_all_counter* m_counter;
      std::exception_ptr m_exception;
      add_pointer_t<RESULT> m_result;
    };

    template <> class when_all_task_promise<void> final {
    public:
      using coroutine_handle_t = std::coroutine_handle<when_all_task_promise<void>>;

      when_all_task_promise() noexcept {}

      auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

      std::suspend_always initial_suspend() noexcept { return {}; }

      auto final_suspend() noexcept {
        class completion_notifier {
        public:
          bool await_ready() const noexcept { return false; }

          void await_suspend(coroutine_handle_t coro) const noexcept {
            coro.promise().m_counter->notify_awaitable_completed();
          }

          void await_resume() const noexcept {}
        };

        return completion_notifier{};
      }

      void unhandled_exception() noexcept { m_exception = std::current_exception(); }

      void return_void() noexcept {}

      void start(when_all_counter& counter) noexcept {
        m_counter = &counter;
        coroutine_handle_t::from_promise(*this).resume();
      }

      void result() {
        if (m_exception) {
          std::rethrow_exception(m_exception);
        }
      }

    private:
      when_all_counter* m_counter;
      std::exception_ptr m_exception;
    };

    template <typename RESULT> class when_all_task final {
    public:
      using promise_type = when_all_task_promise<RESULT>;

      using coroutine_handle_t = typename promise_type::coroutine_handle_t;

      when_all_task(coroutine_handle_t coroutine) noexcept : m_coroutine(coroutine) {}

      when_all_task(when_all_task&& other) noexcept
          : m_coroutine(zs::exchange(other.m_coroutine, coroutine_handle_t{})) {}

      ~when_all_task() {
        if (m_coroutine) m_coroutine.destroy();
      }

      when_all_task(const when_all_task&) = delete;
      when_all_task& operator=(const when_all_task&) = delete;

      decltype(auto) result() & { return m_coroutine.promise().result(); }

      decltype(auto) result() && { return zs::move(m_coroutine.promise()).result(); }

      decltype(auto) non_void_result() & {
        if constexpr (is_void_v<decltype(this->result())>) {
          this->result();
          return detail::void_value{};
        } else {
          return this->result();
        }
      }

      decltype(auto) non_void_result() && {
        if constexpr (is_void_v<decltype(this->result())>) {
          zs::move(*this).result();
          return detail::void_value{};
        } else {
          return zs::move(*this).result();
        }
      }

    private:
      template <typename TASK_CONTAINER> friend class when_all_ready_awaitable;

      void start(when_all_counter& counter) noexcept { m_coroutine.promise().start(counter); }

      coroutine_handle_t m_coroutine;
    };

    template <typename AWAITABLE,
              typename RESULT = typename awaitable_traits<AWAITABLE&&>::await_result_t,
              enable_if_t<!is_void_v<RESULT>> = 0>
    when_all_task<RESULT> make_when_all_task(AWAITABLE awaitable) {
      co_yield co_await static_cast<AWAITABLE&&>(awaitable);
    }

    template <typename AWAITABLE,
              typename RESULT = typename awaitable_traits<AWAITABLE&&>::await_result_t,
              enable_if_t<is_void_v<RESULT>> = 0>
    when_all_task<void> make_when_all_task(AWAITABLE awaitable) {
      co_await static_cast<AWAITABLE&&>(awaitable);
    }
  }  // namespace detail

  template <typename... AWAITABLES,
            enable_if_t<std::conjunction_v<is_awaitable<remove_reference_t<AWAITABLES>>...>> = 0>
  [[nodiscard]] inline auto when_all_ready(AWAITABLES&&... awaitables) {
    return detail::when_all_ready_awaitable<zs::tuple<detail::when_all_task<
        typename awaitable_traits<remove_reference_t<AWAITABLES>>::await_result_t>...>>(
        zs::make_tuple(detail::make_when_all_task(forward<AWAITABLES>(awaitables))...));
  }

  template <std::ranges::range AWAITABLES,
            typename RESULT
            = typename awaitable_traits<std::ranges::range_value_t<AWAITABLES>>::await_result_t>
  [[nodiscard]] auto when_all_ready(AWAITABLES awaitables) {
    std::vector<detail::when_all_task<RESULT>> tasks;

    tasks.reserve(awaitables.size());

    for (auto& awaitable : awaitables)
      tasks.emplace_back(detail::make_when_all_task(zs::move(awaitable)));

    return detail::when_all_ready_awaitable<std::vector<detail::when_all_task<RESULT>>>(
        zs::move(tasks));
  }

  /// when_any
  // @ref https://github.com/lewissbaker/cppcoro/issues/11
  ///
  /// @ref https://lewissbaker.github.io/2017/11/17/understanding-operator-co-await
  ///
  class AsyncFlag {
  public:
    AsyncFlag(bool initiallySet = false) noexcept
        : _awaitersOrEvent{initiallySet ? this : nullptr} {}

    // No copying/moving
    AsyncFlag(const AsyncFlag&) = delete;
    AsyncFlag(AsyncFlag&&) = delete;
    AsyncFlag& operator=(const AsyncFlag&) = delete;
    AsyncFlag& operator=(AsyncFlag&&) = delete;

    bool isSet() const noexcept { return _awaitersOrEvent.load(std::memory_order_acquire) == this; }

    struct Awaiter;
    inline Awaiter operator co_await() const noexcept;

    inline void set() noexcept;
    void reset() noexcept {
      void* oldValue = this;
      _awaitersOrEvent.compare_exchange_strong(oldValue, nullptr, std::memory_order_acquire);
    }

  private:
    friend struct Awaiter;

    // - 'this' => set state
    // - otherwise => not set, head of linked list of awaiter*.
    /// @note awaiters exist within coroutines' frames
    mutable std::atomic<void*> _awaitersOrEvent;
  };

  struct AsyncFlag::Awaiter {
    Awaiter(const AsyncFlag& event) : _event{event} {}
    bool await_ready() const noexcept { return _event.isSet(); }
    bool await_suspend(std::coroutine_handle<void> awaitingCoroutine) noexcept {
      const auto eventPtr = &_event;
      _pendingCoroutine = awaitingCoroutine;
      void* prevVal = _event._awaitersOrEvent.load(std::memory_order_acquire);
      do {
        if (prevVal == eventPtr) return false;  // resume immediately
        _nextAwaiter = static_cast<Awaiter*>(prevVal);

      } while (_event._awaitersOrEvent.compare_exchange_weak(
          prevVal, this, std::memory_order_release, std::memory_order_acquire));
      return true;  // suspend
    }
    void await_resume() noexcept {}

    const AsyncFlag& _event;
    std::coroutine_handle<void> _pendingCoroutine;
    Awaiter* _nextAwaiter;
  };

  AsyncFlag::Awaiter AsyncFlag::operator co_await() const noexcept {
    return AsyncFlag::Awaiter(*this);
  }
  void AsyncFlag::set() noexcept {
    void* val = _awaitersOrEvent.exchange(this, std::memory_order_acq_rel);
    if (val != this) {
      Awaiter* nextAwaiter = static_cast<Awaiter*>(val);
      while (nextAwaiter != nullptr) {
        auto* tmp = nextAwaiter->_nextAwaiter;
        nextAwaiter->_pendingCoroutine.resume();
        nextAwaiter = tmp;
      }
    }
  }

  ///
  /// composable parallel primitives
  ///
  template <typename F> auto suspend_callback(F&& f) {
    struct awaiter : std::suspend_always {
      remove_reference_t<F> _callback;
      awaiter(F&& c) : _callback{forward<F>(c)} {}

      void await_suspend(std::coroutine_handle<>) { _callback(); }
    };
    return awaiter{FWD(f)};
  }

  template <typename SignalArgs> struct AwaitableSignal;

  template <typename... Args> struct AwaitableSignalResult {
    template <typename T> using storage_type
        = conditional_t<is_reference_v<T>, add_pointer_t<remove_reference_t<T>>, T>;
    using storage_tuple_type = zs::tuple<storage_type<Args>...>;
    using dereference_tuple_type = zs::tuple<Args...>;
    static constexpr bool s_has_result = true;

    template <typename Arg> decltype(auto) wrap(Arg&& arg) {
      if constexpr (is_reference_v<Arg>)  // reference
        return zs::addressof(arg);
      else
        return zs::move(arg);
    }
    void emplace(Args&&... args) {
      _result = storage_tuple_type(wrap<Args>(zs::forward<Args>(args))...);
    }

    template <typename OriginalArg, typename WrappedArg> decltype(auto) unwrap(WrappedArg&& arg) {
      if constexpr (is_lvalue_reference_v<OriginalArg>)  // reference
        return *arg;
      else if constexpr (is_rvalue_reference_v<OriginalArg>)
        return zs::move(*arg);
      else
        // return zs::forward<WrappedArg>(arg);  // or zs::move?
        return zs::move(arg);  // or zs::move?
    }

    template <size_t... Is> dereference_tuple_type forwardResultImpl(index_sequence<Is...>) {
      // return dereference_tuple_type{unwrap<Args>(zs::get<Is>(_result))...};
      auto ret = dereference_tuple_type{unwrap<Args>(zs::get<Is>(_result))...};
      return ret;
    }
    auto forwardResult() { return forwardResultImpl(index_sequence_for<Args...>{}); }

    storage_tuple_type _result;
  };
  template <> struct AwaitableSignalResult<> {
    static constexpr bool s_has_result = false;
  };
  template <> struct AwaitableSignalResult<void> {
    static constexpr bool s_has_result = false;
  };

  template <typename... Args> struct AwaitableSignal<void(Args...)>
      : AwaitableSignalResult<Args...> {
    using signal_type = Signal<void(Args...)>;

    AwaitableSignal(signal_type& signal) noexcept : _signal{signal} {
      _connection.reset(_signal.newConnection([this](Args&&... args) {
        // fmt::print("again, awaitable signal address{}\n", (void*)awaitable);
        // fmt::print("conneciton id to remove: {}\n", _connection->getConnectionId());
        _signal.removeConnection(_connection->getConnectionId());
        if constexpr (AwaitableSignalResult<Args...>::s_has_result) {
          // AwaitableSignalResult<Args...>::_result = zs::forward_as_tuple(FWD(args)...);
          AwaitableSignalResult<Args...>::emplace(FWD(args)...);
        }
        _coroHandle.resume();
      }));
    }

    struct Awaiter {
      Awaiter(AwaitableSignal& awaitable) : _awaitable{awaitable} {}

      bool await_ready() const noexcept { return false; }

      void await_suspend(std::coroutine_handle<> handle) noexcept {
        _awaitable._coroHandle = handle;
      }

      decltype(auto) await_resume() noexcept {
        if constexpr (AwaitableSignalResult<Args...>::s_has_result) {
          // return _awaitable._result;
          return _awaitable.forwardResult();
        } else {
          return;
        }
      }

    private:
      AwaitableSignal& _awaitable;
    };

    Awaiter operator co_await() { return Awaiter{*this}; }

  private:
    signal_type& _signal;
    UniquePtr<Connection<signal_type>> _connection;
    std::coroutine_handle<> _coroHandle;
  };

  template <typename Signature> auto make_awaitable_signal(Signal<Signature>& signal) {
    return AwaitableSignal<Signature>(signal);
  }

}  // namespace zs
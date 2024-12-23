///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include <iterator>
#include <cassert>
#include <coroutine>
#include <exception>

#include "world/WorldExport.hpp"
#include "zensim/ZpcMeta.hpp"

namespace zs {

  template <typename T> class AsyncGenerator;

  namespace detail {
    template <typename T> class async_generator_iterator;
    class async_generator_yield_operation;
    class async_generator_advance_operation;

    /// promise_base
    class async_generator_promise_base {
    public:
      async_generator_promise_base() noexcept : m_exception(nullptr) {
        // Other variables left intentionally uninitialised as they're
        // only referenced in certain states by which time they should
        // have been initialised.
      }

      async_generator_promise_base(const async_generator_promise_base& other) = delete;
      async_generator_promise_base& operator=(const async_generator_promise_base& other) = delete;

      std::suspend_always initial_suspend() const noexcept { return {}; }

      async_generator_yield_operation final_suspend() noexcept;

      void unhandled_exception() noexcept { m_exception = std::current_exception(); }

      void return_void() noexcept {}

      /// Query if the generator has reached the end of the sequence.
      ///
      /// Only valid to call after resuming from an awaited advance operation.
      /// i.e. Either a begin() or iterator::operator++() operation.
      bool finished() const noexcept { return m_currentValue == nullptr; }

      void rethrow_if_unhandled_exception() {
        if (m_exception) {
          std::rethrow_exception(zs::move(m_exception));
        }
      }

    protected:
      async_generator_yield_operation internal_yield_value() noexcept;

    private:
      friend class async_generator_yield_operation;
      friend class async_generator_advance_operation;

      std::exception_ptr m_exception;

      std::coroutine_handle<> m_consumerCoroutine;

    protected:
      void* m_currentValue;
    };

    /// yield
    class async_generator_yield_operation final {
    public:
      async_generator_yield_operation(std::coroutine_handle<> consumer) noexcept
          : m_consumer(consumer) {}

      bool await_ready() const noexcept { return false; }

      std::coroutine_handle<> await_suspend(
          [[maybe_unused]] std::coroutine_handle<> producer) noexcept {
        return m_consumer;
      }

      void await_resume() noexcept {}

    private:
      std::coroutine_handle<> m_consumer;
    };

    inline async_generator_yield_operation async_generator_promise_base::final_suspend() noexcept {
      m_currentValue = nullptr;
      return internal_yield_value();
    }

    inline async_generator_yield_operation
    async_generator_promise_base::internal_yield_value() noexcept {
      return async_generator_yield_operation{m_consumerCoroutine};
    }

    /// advance
    class async_generator_advance_operation {
    protected:
      async_generator_advance_operation(std::nullptr_t) noexcept
          : m_promise(nullptr), m_producerCoroutine(nullptr) {}

      async_generator_advance_operation(async_generator_promise_base& promise,
                                        std::coroutine_handle<> producerCoroutine) noexcept
          : m_promise(addressof(promise)), m_producerCoroutine(producerCoroutine) {}

    public:
      bool await_ready() const noexcept { return false; }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<> consumerCoroutine) noexcept {
        m_promise->m_consumerCoroutine = consumerCoroutine;
        return m_producerCoroutine;
      }

    protected:
      async_generator_promise_base* m_promise;
      std::coroutine_handle<> m_producerCoroutine;
    };

    /// promise
    template <typename T> class async_generator_promise final
        : public async_generator_promise_base {
      using value_type = remove_reference_t<T>;

    public:
      async_generator_promise() noexcept = default;

      AsyncGenerator<T> get_return_object() noexcept;

      async_generator_yield_operation yield_value(value_type& value) noexcept {
        m_currentValue = addressof(value);
        return internal_yield_value();
      }

      async_generator_yield_operation yield_value(value_type&& value) noexcept {
        return yield_value(value);
      }

      T& value() const noexcept { return *static_cast<T*>(m_currentValue); }
    };

    template <typename T> class async_generator_promise<T&&> final
        : public async_generator_promise_base {
    public:
      async_generator_promise() noexcept = default;

      AsyncGenerator<T> get_return_object() noexcept;

      async_generator_yield_operation yield_value(T&& value) noexcept {
        m_currentValue = addressof(value);
        return internal_yield_value();
      }

      T&& value() const noexcept { return move(*static_cast<T*>(m_currentValue)); }
    };

    template <typename T> class async_generator_increment_operation final
        : public async_generator_advance_operation {
    public:
      async_generator_increment_operation(async_generator_iterator<T>& iterator) noexcept
          : async_generator_advance_operation(iterator.m_coroutine.promise(), iterator.m_coroutine),
            m_iterator(iterator) {}

      async_generator_iterator<T>& await_resume();

    private:
      async_generator_iterator<T>& m_iterator;
    };

    template <typename T> class async_generator_iterator final {
      using promise_type = async_generator_promise<T>;
      using handle_type = std::coroutine_handle<promise_type>;

    public:
      using iterator_category = std::input_iterator_tag;
      // Not sure what type should be used for difference_type as we don't
      // allow calculating difference between two iterators.
      using difference_type = ptrdiff_t;
      using value_type = remove_reference_t<T>;
      using reference = add_lvalue_reference_t<T>;
      using pointer = add_pointer_t<value_type>;

      async_generator_iterator(std::nullptr_t) noexcept : m_coroutine(nullptr) {}

      async_generator_iterator(handle_type coroutine) noexcept : m_coroutine(coroutine) {}

      async_generator_increment_operation<T> operator++() noexcept {
        return async_generator_increment_operation<T>{*this};
      }

      reference operator*() const noexcept { return m_coroutine.promise().value(); }

      bool operator==(const async_generator_iterator& other) const noexcept {
        return m_coroutine == other.m_coroutine;
      }

      bool operator!=(const async_generator_iterator& other) const noexcept {
        return !(*this == other);
      }

    private:
      friend class async_generator_increment_operation<T>;

      handle_type m_coroutine;
    };

    template <typename T>
    async_generator_iterator<T>& async_generator_increment_operation<T>::await_resume() {
      if (m_promise->finished()) {
        // Update iterator to end()
        m_iterator = async_generator_iterator<T>{nullptr};
        m_promise->rethrow_if_unhandled_exception();
      }

      return m_iterator;
    }

    template <typename T> class async_generator_begin_operation final
        : public async_generator_advance_operation {
      using promise_type = async_generator_promise<T>;
      using handle_type = std::coroutine_handle<promise_type>;

    public:
      async_generator_begin_operation(std::nullptr_t) noexcept
          : async_generator_advance_operation(nullptr) {}

      async_generator_begin_operation(handle_type producerCoroutine) noexcept
          : async_generator_advance_operation(producerCoroutine.promise(), producerCoroutine) {}

      bool await_ready() const noexcept {
        return m_promise == nullptr || async_generator_advance_operation::await_ready();
      }

      async_generator_iterator<T> await_resume() {
        if (m_promise == nullptr) {
          // Called begin() on the empty generator.
          return async_generator_iterator<T>{nullptr};
        } else if (m_promise->finished()) {
          // Completed without yielding any values.
          m_promise->rethrow_if_unhandled_exception();
          return async_generator_iterator<T>{nullptr};
        }

        return async_generator_iterator<T>{
            handle_type::from_promise(*static_cast<promise_type*>(m_promise))};
      }
    };
  }  // namespace detail

  template <typename T> class [[nodiscard]] AsyncGenerator {
  public:
    using promise_type = detail::async_generator_promise<T>;
    using iterator = detail::async_generator_iterator<T>;

    AsyncGenerator() noexcept : m_coroutine(nullptr) {}

    explicit AsyncGenerator(promise_type& promise) noexcept
        : m_coroutine(std::coroutine_handle<promise_type>::from_promise(promise)) {}

    AsyncGenerator(AsyncGenerator&& other) noexcept : m_coroutine(other.m_coroutine) {
      other.m_coroutine = nullptr;
    }

    ~AsyncGenerator() {
      if (m_coroutine) {
        m_coroutine.destroy();
      }
    }

    AsyncGenerator& operator=(AsyncGenerator&& other) noexcept {
      AsyncGenerator temp(zs::move(other));
      swap(temp);
      return *this;
    }

    AsyncGenerator(const AsyncGenerator&) = delete;
    AsyncGenerator& operator=(const AsyncGenerator&) = delete;

    auto begin() noexcept {
      if (!m_coroutine) {
        return detail::async_generator_begin_operation<T>{nullptr};
      }

      return detail::async_generator_begin_operation<T>{m_coroutine};
    }

    auto end() noexcept { return iterator{nullptr}; }

    void swap(AsyncGenerator& other) noexcept { zs_swap(m_coroutine, other.m_coroutine); }

  private:
    std::coroutine_handle<promise_type> m_coroutine;
  };

  template <typename T> void swap(AsyncGenerator<T>& a, AsyncGenerator<T>& b) noexcept {
    a.swap(b);
  }

  namespace detail {
    template <typename T>
    AsyncGenerator<T> async_generator_promise<T>::get_return_object() noexcept {
      return AsyncGenerator<T>{*this};
    }
  }  // namespace detail

}  // namespace zs
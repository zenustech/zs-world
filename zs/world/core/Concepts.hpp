#pragma once
#include <concepts>
#include <ranges>
#include <string>

namespace zs {

  namespace detail {
    template <class E>
    concept default_erasable = requires(E *p) { std::destroy_at(p); };

    template <class E, class T, class A>
    concept allocator_erasable = requires(A m, E *p) {
      requires std::same_as<typename T::allocator_type,
                            typename std::allocator_traits<A>::template rebind_alloc<E>>;
      std::allocator_traits<A>::destroy(m, p);
    };

    template <class T>
    concept allocator_aware = requires(T a) {
      { a.get_allocator() } -> std::same_as<typename T::allocator_type>;
    };

    template <class T> struct is_basic_string : std::false_type {};

    template <class C, class T, class A> struct is_basic_string<std::basic_string<C, T, A>>
        : std::true_type {};

    template <class T> constexpr bool is_basic_string_v = is_basic_string<T>::value;

    template <class E, class T>
    concept erasable
        = (is_basic_string_v<T> && default_erasable<E>)
          || (allocator_aware<T> && allocator_erasable<E, T, typename T::allocator_type>)
          || (!allocator_aware<T> && default_erasable<E>);
  }  // namespace detail

  template <class T>
  concept Container = requires(T a, const T b) {
    requires std::regular<T>;
    requires std::swappable<T>;
    requires detail::erasable<typename T::value_type, T>;
    requires std::same_as<typename T::reference, typename T::value_type &>;
    requires std::same_as<typename T::const_reference, const typename T::value_type &>;
    requires std::forward_iterator<typename T::iterator>;
    requires std::forward_iterator<typename T::const_iterator>;
    requires std::signed_integral<typename T::difference_type>;
    requires std::same_as<typename T::difference_type,
                          typename std::iterator_traits<typename T::iterator>::difference_type>;
    requires std::same_as<
        typename T::difference_type,
        typename std::iterator_traits<typename T::const_iterator>::difference_type>;
    { a.begin() } -> std::same_as<typename T::iterator>;
    { a.end() } -> std::same_as<typename T::iterator>;
    { b.begin() } -> std::same_as<typename T::const_iterator>;
    { b.end() } -> std::same_as<typename T::const_iterator>;
    { a.cbegin() } -> std::same_as<typename T::const_iterator>;
    { a.cend() } -> std::same_as<typename T::const_iterator>;
    { a.size() } -> std::same_as<typename T::size_type>;
    { a.max_size() } -> std::same_as<typename T::size_type>;
    { a.empty() } -> std::convertible_to<bool>;
  };

}  // namespace zs
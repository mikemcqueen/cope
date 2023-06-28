#ifndef INCLUDE_TUPLE_H
#define INCLUDE_TUPLE_H

#include <tuple>
#include <type_traits>
#include <variant>

namespace tuple {
  // Type alias for a tuple of concatenated types, given a list of tuple types.
  template<typename... Ts>
  using concat_t = decltype(std::tuple_cat(std::declval<Ts>()...));

  template <typename T, typename... Ts>
  struct distinct_impl : std::type_identity<T> {};

  template <typename... Ts, typename U, typename... Us>
  struct distinct_impl<std::tuple<Ts...>, U, Us...> :
    std::conditional_t<(std::is_same_v<U, Ts> || ...),
      distinct_impl<std::tuple<Ts...>, Us...>,
      distinct_impl<std::tuple<Ts..., U>, Us...>> {};

  // Type alias for a tuple of distinct types (all duplicate types removed),
  // given a list of element types.
  template <typename... Ts>
  using distinct_element_t = typename distinct_impl<std::tuple<>, Ts...>::type;

  template <typename T>
  struct distinct;

  template <typename... Ts>
  struct distinct<std::tuple<Ts...>> {
    using type = distinct_element_t<Ts...>;
  };

  // Type alias for a tuple of distinct types (all duplicate types removed),
  // given a tuple type.
  template <typename T>
  using distinct_t = distinct<T>::type;

  //
  // Convert a tuple type to a variant type.
  //
  template <class... Ts>
  struct to_variant;

  template <class... Ts>
  struct to_variant<std::tuple<Ts...>> {
    using type = std::variant<Ts...>;
  };

  template <typename T>
  using to_variant_t = typename to_variant<T>::type;
}

#endif // INCLUDE_TUPLE_H

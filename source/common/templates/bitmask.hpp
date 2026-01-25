#pragma once

#include <concepts>
#include <type_traits>

namespace gravity {

template <typename T>
concept BitmaskEnum = std::is_enum_v<T> && requires(T v) { enable_bitmask_operators(v); };

template <BitmaskEnum T>
constexpr auto operator|(T lhs, T rhs) -> T {
  using U = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <BitmaskEnum T>
constexpr auto has_flag(T value, T flag) -> bool {
    using U = std::underlying_type_t<T>;
    return (static_cast<U>(value) & static_cast<U>(flag)) != 0;
}

}  // namespace gravity
#pragma once

template<typename T, typename U>
constexpr T sc(U&& value) {
  return static_cast<T>(std::forward<U>(value));
}

template<typename T, typename U>
constexpr T dc(U&& value) {
  return dynamic_cast<T>(std::forward<U>(value));
}

template<typename T, typename U>
constexpr T rc(U&& value) {
  return reinterpret_cast<T>(std::forward<U>(value));
}

#pragma once

template<typename T>
struct use_once
{
  T value;

  constexpr explicit use_once(T v_) noexcept
    : value(v_)
  {
  }

  use_once(const use_once &) = delete;
  use_once(use_once &) = delete;
  use_once& operator=(const use_once &) = delete;
  use_once& operator=(use_once &&) = delete;

  constexpr
  operator T() && noexcept
  {
    return value;
  }

  T operator&() && = delete;
  const T& operator*() && = delete;
};

template<typename T>
use_once<T>
once(T v) noexcept
{
  return use_once<T>{v};
}

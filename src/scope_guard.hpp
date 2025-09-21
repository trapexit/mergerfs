//   _____                         _____                     _    _____
//  / ____|                       / ____|                   | |  / ____|_     _
// | (___   ___ ___  _ __   ___  | |  __ _   _  __ _ _ __ __| | | |   _| |_ _| |_
//  \___ \ / __/ _ \| '_ \ / _ \ | | |_ | | | |/ _` | '__/ _` | | |  |_   _|_   _|
//  ____) | (_| (_) | |_) |  __/ | |__| | |_| | (_| | | | (_| | | |____|_|   |_|
// |_____/ \___\___/| .__/ \___|  \_____|\__,_|\__,_|_|  \__,_|  \_____|
//                  | | https://github.com/Neargye/scope_guard
//                  |_| version 0.9.1
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2018 - 2021 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef NEARGYE_SCOPE_GUARD_HPP
#define NEARGYE_SCOPE_GUARD_HPP

#define SCOPE_GUARD_VERSION_MAJOR 0
#define SCOPE_GUARD_VERSION_MINOR 9
#define SCOPE_GUARD_VERSION_PATCH 1

#include <type_traits>
#if (defined(_MSC_VER) && _MSC_VER >= 1900) || ((defined(__clang__) || defined(__GNUC__)) && __cplusplus >= 201700L)
#include <exception>
#endif

// scope_guard throwable settings:
// SCOPE_GUARD_NO_THROW_CONSTRUCTIBLE requires nothrow constructible action.
// SCOPE_GUARD_MAY_THROW_ACTION action may throw exceptions.
// SCOPE_GUARD_NO_THROW_ACTION requires noexcept action.
// SCOPE_GUARD_SUPPRESS_THROW_ACTION exceptions during action will be suppressed.
// SCOPE_GUARD_CATCH_HANDLER exceptions handler. If SCOPE_GUARD_SUPPRESS_THROW_ACTIONS is not defined, it will do nothing.

#if !defined(SCOPE_GUARD_MAY_THROW_ACTION) && !defined(SCOPE_GUARD_NO_THROW_ACTION) && !defined(SCOPE_GUARD_SUPPRESS_THROW_ACTION)
#  define SCOPE_GUARD_MAY_THROW_ACTION
#elif (defined(SCOPE_GUARD_MAY_THROW_ACTION) + defined(SCOPE_GUARD_NO_THROW_ACTION) + defined(SCOPE_GUARD_SUPPRESS_THROW_ACTION)) > 1
#  error Only one of SCOPE_GUARD_MAY_THROW_ACTION and SCOPE_GUARD_NO_THROW_ACTION and SCOPE_GUARD_SUPPRESS_THROW_ACTION may be defined.
#endif

#if !defined(SCOPE_GUARD_CATCH_HANDLER)
#  define SCOPE_GUARD_CATCH_HANDLER /* Suppress exception.*/
#endif

namespace scope_guard {

namespace detail {

#if defined(SCOPE_GUARD_SUPPRESS_THROW_ACTION) && (defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (_HAS_EXCEPTIONS))
#  define NEARGYE_NOEXCEPT(...) noexcept
#  define NEARGYE_TRY           try {
#  define NEARGYE_CATCH         } catch (...) { SCOPE_GUARD_CATCH_HANDLER }
#else
#  define NEARGYE_NOEXCEPT(...) noexcept(__VA_ARGS__)
#  define NEARGYE_TRY
#  define NEARGYE_CATCH
#endif

#define NEARGYE_MOV(...) static_cast<typename std::remove_reference<decltype(__VA_ARGS__)>::type&&>(__VA_ARGS__)
#define NEARGYE_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

// NEARGYE_NODISCARD encourages the compiler to issue a warning if the return value is discarded.
#if !defined(NEARGYE_NODISCARD)
#  if defined(__clang__)
#    if (__clang_major__ * 10 + __clang_minor__) >= 39 && __cplusplus >= 201703L
#      define NEARGYE_NODISCARD [[nodiscard]]
#    else
#      define NEARGYE_NODISCARD __attribute__((__warn_unused_result__))
#    endif
#  elif defined(__GNUC__)
#    if __GNUC__ >= 7 && __cplusplus >= 201703L
#      define NEARGYE_NODISCARD [[nodiscard]]
#    else
#      define NEARGYE_NODISCARD __attribute__((__warn_unused_result__))
#    endif
#  elif defined(_MSC_VER)
#    if _MSC_VER >= 1911 && defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#      define NEARGYE_NODISCARD [[nodiscard]]
#    elif defined(_Check_return_)
#      define NEARGYE_NODISCARD _Check_return_
#    else
#      define NEARGYE_NODISCARD
#    endif
#  else
#    define NEARGYE_NODISCARD
#  endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
inline int uncaught_exceptions() noexcept {
  return *(reinterpret_cast<int*>(static_cast<char*>(static_cast<void*>(_getptd())) + (sizeof(void*) == 8 ? 0x100 : 0x90)));
}
#elif (defined(__clang__) || defined(__GNUC__)) && __cplusplus < 201700L
struct __cxa_eh_globals;
extern "C" __cxa_eh_globals* __cxa_get_globals() noexcept;
inline int uncaught_exceptions() noexcept {
  return static_cast<int>(*(reinterpret_cast<unsigned int*>(static_cast<char*>(static_cast<void*>(__cxa_get_globals())) + sizeof(void*))));
}
#else
inline int uncaught_exceptions() noexcept {
  return std::uncaught_exceptions();
}
#endif

class on_exit_policy {
  bool execute_;

 public:
  explicit on_exit_policy(bool execute) noexcept : execute_{execute} {}

  void dismiss() noexcept {
    execute_ = false;
  }

  bool should_execute() const noexcept {
    return execute_;
  }
};

class on_fail_policy {
  int ec_;

 public:
  explicit on_fail_policy(bool execute) noexcept : ec_{execute ? uncaught_exceptions() : -1} {}

  void dismiss() noexcept {
    ec_ = -1;
  }

  bool should_execute() const noexcept {
    return ec_ != -1 && ec_ < uncaught_exceptions();
  }
};

class on_success_policy {
  int ec_;

 public:
  explicit on_success_policy(bool execute) noexcept : ec_{execute ? uncaught_exceptions() : -1} {}

  void dismiss() noexcept {
    ec_ = -1;
  }

  bool should_execute() const noexcept {
    return ec_ != -1 && ec_ >= uncaught_exceptions();
  }
};

template <typename T, typename = void>
struct is_noarg_returns_void_action
    : std::false_type {};

template <typename T>
struct is_noarg_returns_void_action<T, decltype((std::declval<T>())())>
    : std::true_type {};

template <typename T, bool = is_noarg_returns_void_action<T>::value>
struct is_nothrow_invocable_action
    : std::false_type {};

template <typename T>
struct is_nothrow_invocable_action<T, true>
    : std::integral_constant<bool, noexcept((std::declval<T>())())> {};

template <typename F, typename P>
class scope_guard {
  using A = typename std::decay<F>::type;

  static_assert(is_noarg_returns_void_action<A>::value,
                "scope_guard requires no-argument action, that returns void.");
  static_assert(std::is_same<P, on_exit_policy>::value || std::is_same<P, on_fail_policy>::value || std::is_same<P, on_success_policy>::value,
                "scope_guard requires on_exit_policy, on_fail_policy or on_success_policy.");
#if defined(SCOPE_GUARD_NO_THROW_ACTION)
    static_assert(is_nothrow_invocable_action<A>::value,
                  "scope_guard requires noexcept invocable action.");
#endif
#if defined(SCOPE_GUARD_NO_THROW_CONSTRUCTIBLE)
  static_assert(std::is_nothrow_move_constructible<A>::value,
                "scope_guard requires nothrow constructible action.");
#endif

  P policy_;
  A action_;

  void* operator new(std::size_t) = delete;
  void operator delete(void*) = delete;

 public:
  scope_guard() = delete;
  scope_guard(const scope_guard&) = delete;
  scope_guard& operator=(const scope_guard&) = delete;
  scope_guard& operator=(scope_guard&&) = delete;

  scope_guard(scope_guard&& other) noexcept(std::is_nothrow_move_constructible<A>::value)
      : policy_{false},
        action_{NEARGYE_MOV(other.action_)} {
    policy_ = NEARGYE_MOV(other.policy_);
    other.policy_.dismiss();
  }

  scope_guard(const A& action) = delete;
  scope_guard(A& action) = delete;

  explicit scope_guard(A&& action) noexcept(std::is_nothrow_move_constructible<A>::value)
      : policy_{true},
        action_{NEARGYE_MOV(action)} {}

  void dismiss() noexcept {
    policy_.dismiss();
  }

  ~scope_guard() NEARGYE_NOEXCEPT(is_nothrow_invocable_action<A>::value) {
    if (policy_.should_execute()) {
      NEARGYE_TRY
        action_();
      NEARGYE_CATCH
    }
  }
};

template <typename F>
using scope_exit = scope_guard<F, on_exit_policy>;

template <typename F, typename std::enable_if<is_noarg_returns_void_action<F>::value, int>::type = 0>
NEARGYE_NODISCARD scope_exit<F> make_scope_exit(F&& action) noexcept(noexcept(scope_exit<F>{NEARGYE_FWD(action)})) {
  return scope_exit<F>{NEARGYE_FWD(action)};
}

template <typename F>
using scope_fail = scope_guard<F, on_fail_policy>;

template <typename F, typename std::enable_if<is_noarg_returns_void_action<F>::value, int>::type = 0>
NEARGYE_NODISCARD scope_fail<F> make_scope_fail(F&& action) noexcept(noexcept(scope_fail<F>{NEARGYE_FWD(action)})) {
  return scope_fail<F>{NEARGYE_FWD(action)};
}

template <typename F>
using scope_success = scope_guard<F, on_success_policy>;

template <typename F, typename std::enable_if<is_noarg_returns_void_action<F>::value, int>::type = 0>
NEARGYE_NODISCARD scope_success<F> make_scope_success(F&& action) noexcept(noexcept(scope_success<F>{NEARGYE_FWD(action)})) {
  return scope_success<F>{NEARGYE_FWD(action)};
}

struct scope_exit_tag {};

template <typename F, typename std::enable_if<is_noarg_returns_void_action<F>::value, int>::type = 0>
scope_exit<F> operator<<(scope_exit_tag, F&& action) noexcept(noexcept(scope_exit<F>{NEARGYE_FWD(action)})) {
  return scope_exit<F>{NEARGYE_FWD(action)};
}

struct scope_fail_tag {};

template <typename F, typename std::enable_if<is_noarg_returns_void_action<F>::value, int>::type = 0>
scope_fail<F> operator<<(scope_fail_tag, F&& action) noexcept(noexcept(scope_fail<F>{NEARGYE_FWD(action)})) {
  return scope_fail<F>{NEARGYE_FWD(action)};
}

struct scope_success_tag {};

template <typename F, typename std::enable_if<is_noarg_returns_void_action<F>::value, int>::type = 0>
scope_success<F> operator<<(scope_success_tag, F&& action) noexcept(noexcept(scope_success<F>{NEARGYE_FWD(action)})) {
  return scope_success<F>{NEARGYE_FWD(action)};
}

#undef NEARGYE_MOV
#undef NEARGYE_FWD
#undef NEARGYE_NOEXCEPT
#undef NEARGYE_TRY
#undef NEARGYE_CATCH
#undef NEARGYE_NODISCARD

} // namespace scope_guard::detail

using detail::make_scope_exit;
using detail::make_scope_fail;
using detail::make_scope_success;

} // namespace scope_guard

// NEARGYE_MAYBE_UNUSED suppresses compiler warnings on unused entities, if any.
#if !defined(NEARGYE_MAYBE_UNUSED)
#  if defined(__clang__)
#    if (__clang_major__ * 10 + __clang_minor__) >= 39 && __cplusplus >= 201703L
#      define NEARGYE_MAYBE_UNUSED [[maybe_unused]]
#    else
#      define NEARGYE_MAYBE_UNUSED __attribute__((__unused__))
#    endif
#  elif defined(__GNUC__)
#    if __GNUC__ >= 7 && __cplusplus >= 201703L
#      define NEARGYE_MAYBE_UNUSED [[maybe_unused]]
#    else
#      define NEARGYE_MAYBE_UNUSED __attribute__((__unused__))
#    endif
#  elif defined(_MSC_VER)
#    if _MSC_VER >= 1911 && defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#      define NEARGYE_MAYBE_UNUSED [[maybe_unused]]
#    else
#      define NEARGYE_MAYBE_UNUSED __pragma(warning(suppress : 4100 4101 4189))
#    endif
#  else
#    define NEARGYE_MAYBE_UNUSED
#  endif
#endif

#if !defined(NEARGYE_STR_CONCAT)
#  define NEARGYE_STR_CONCAT_(s1, s2) s1##s2
#  define NEARGYE_STR_CONCAT(s1, s2)  NEARGYE_STR_CONCAT_(s1, s2)
#endif

#if !defined(NEARGYE_COUNTER)
#  if defined(__COUNTER__)
#    define NEARGYE_COUNTER __COUNTER__
#  elif defined(__LINE__)
#    define NEARGYE_COUNTER __LINE__
#  endif
#endif

#if defined(SCOPE_GUARD_NO_THROW_ACTION)
#  define NEARGYE_MAKE_SCOPE_GUARD_ACTION [&]() noexcept -> void
#else
#  define NEARGYE_MAKE_SCOPE_GUARD_ACTION [&]() -> void
#endif

#define NEARGYE_MAKE_SCOPE_EXIT    ::scope_guard::detail::scope_exit_tag{}    << NEARGYE_MAKE_SCOPE_GUARD_ACTION
#define NEARGYE_MAKE_SCOPE_FAIL    ::scope_guard::detail::scope_fail_tag{}    << NEARGYE_MAKE_SCOPE_GUARD_ACTION
#define NEARGYE_MAKE_SCOPE_SUCCESS ::scope_guard::detail::scope_success_tag{} << NEARGYE_MAKE_SCOPE_GUARD_ACTION

#define NEARGYE_SCOPE_GUARD_WITH_(g, i) for (int i = 1; i--; g)
#define NEARGYE_SCOPE_GUARD_WITH(g)     NEARGYE_SCOPE_GUARD_WITH_(g, NEARGYE_STR_CONCAT(NEARGYE_INTERNAL_OBJECT_, NEARGYE_COUNTER))

// SCOPE_EXIT executing action on scope exit.
#define MAKE_SCOPE_EXIT(name)  auto name = NEARGYE_MAKE_SCOPE_EXIT
#define SCOPE_EXIT             NEARGYE_MAYBE_UNUSED const MAKE_SCOPE_EXIT(NEARGYE_STR_CONCAT(NEARGYE_SCOPE_EXIT_, NEARGYE_COUNTER))
#define WITH_SCOPE_EXIT(guard) NEARGYE_SCOPE_GUARD_WITH(NEARGYE_MAKE_SCOPE_EXIT{ guard })

// SCOPE_FAIL executing action on scope exit when an exception has been thrown before scope exit.
#define MAKE_SCOPE_FAIL(name)  auto name = NEARGYE_MAKE_SCOPE_FAIL
#define SCOPE_FAIL             NEARGYE_MAYBE_UNUSED const MAKE_SCOPE_FAIL(NEARGYE_STR_CONCAT(NEARGYE_SCOPE_FAIL_, NEARGYE_COUNTER))
#define WITH_SCOPE_FAIL(guard) NEARGYE_SCOPE_GUARD_WITH(NEARGYE_MAKE_SCOPE_FAIL{ guard })

// SCOPE_SUCCESS executing action on scope exit when no exceptions have been thrown before scope exit.
#define MAKE_SCOPE_SUCCESS(name)  auto name = NEARGYE_MAKE_SCOPE_SUCCESS
#define SCOPE_SUCCESS             NEARGYE_MAYBE_UNUSED const MAKE_SCOPE_SUCCESS(NEARGYE_STR_CONCAT(NEARGYE_SCOPE_SUCCESS_, NEARGYE_COUNTER))
#define WITH_SCOPE_SUCCESS(guard) NEARGYE_SCOPE_GUARD_WITH(NEARGYE_MAKE_SCOPE_SUCCESS{ guard })

// DEFER executing action on scope exit.
#define MAKE_DEFER(name)  MAKE_SCOPE_EXIT(name)
#define DEFER             SCOPE_EXIT
#define WITH_DEFER(guard) WITH_SCOPE_EXIT(guard)

#endif // NEARGYE_SCOPE_GUARD_HPP

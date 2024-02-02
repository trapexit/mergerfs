// This file is part of Better Enums, released under the BSD 2-clause license.
// See LICENSE.md for details, or visit http://github.com/aantron/better-enums.

#pragma once

#ifndef BETTER_ENUMS_ENUM_H
#define BETTER_ENUMS_ENUM_H



#include <cstddef>
#include <cstring>
#include <iosfwd>
#include <stdexcept>


// in-line, non-#pragma warning handling
// not supported in very old compilers (namely gcc 4.4 or less)
#ifdef __GNUC__
#   ifdef __clang__
#      define BETTER_ENUMS_IGNORE_OLD_CAST_HEADER _Pragma("clang diagnostic push")
#      define BETTER_ENUMS_IGNORE_OLD_CAST_BEGIN _Pragma("clang diagnostic ignored \"-Wold-style-cast\"")
#      define BETTER_ENUMS_IGNORE_OLD_CAST_END _Pragma("clang diagnostic pop")
#      define BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER
#      define BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN
#      define BETTER_ENUMS_IGNORE_ATTRIBUTES_END
#   else
#      define BETTER_ENUMS_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100)
#      if BETTER_ENUMS_GCC_VERSION > 40400
#         define BETTER_ENUMS_IGNORE_OLD_CAST_HEADER _Pragma("GCC diagnostic push")
#         define BETTER_ENUMS_IGNORE_OLD_CAST_BEGIN _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")
#         define BETTER_ENUMS_IGNORE_OLD_CAST_END _Pragma("GCC diagnostic pop")
#         if (BETTER_ENUMS_GCC_VERSION >= 70300)
#               define BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER _Pragma("GCC diagnostic push")
#               define BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN _Pragma("GCC diagnostic ignored \"-Wattributes\"")
#               define BETTER_ENUMS_IGNORE_ATTRIBUTES_END _Pragma("GCC diagnostic pop")
#         else
#               define BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER
#               define BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN
#               define BETTER_ENUMS_IGNORE_ATTRIBUTES_END
#         endif
#      else
#         define BETTER_ENUMS_IGNORE_OLD_CAST_HEADER
#         define BETTER_ENUMS_IGNORE_OLD_CAST_BEGIN
#         define BETTER_ENUMS_IGNORE_OLD_CAST_END
#         define BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER
#         define BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN
#         define BETTER_ENUMS_IGNORE_ATTRIBUTES_END
#      endif
#   endif
#else // empty definitions for compilers that don't support _Pragma
#   define BETTER_ENUMS_IGNORE_OLD_CAST_HEADER
#   define BETTER_ENUMS_IGNORE_OLD_CAST_BEGIN
#   define BETTER_ENUMS_IGNORE_OLD_CAST_END
#   define BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER
#   define BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN
#   define BETTER_ENUMS_IGNORE_ATTRIBUTES_END
#endif

// Feature detection.

#ifdef __GNUC__
#   ifdef __clang__
#       if __has_feature(cxx_constexpr)
#           define BETTER_ENUMS_HAVE_CONSTEXPR
#       endif
#       if !defined(__EXCEPTIONS) || !__has_feature(cxx_exceptions)
#           define BETTER_ENUMS_NO_EXCEPTIONS
#       endif
#   else
#       if defined(__GXX_EXPERIMENTAL_CXX0X__) || __cplusplus >= 201103L
#           if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 6))
#               define BETTER_ENUMS_HAVE_CONSTEXPR
#           endif
#       endif
#       ifndef __EXCEPTIONS
#           define BETTER_ENUMS_NO_EXCEPTIONS
#       endif
#   endif
#endif

#ifdef _MSC_VER
#   if _MSC_VER >= 1911
#       define BETTER_ENUMS_HAVE_CONSTEXPR
#   endif
#   ifdef __clang__
#       if __has_feature(cxx_constexpr)
#           define BETTER_ENUMS_HAVE_CONSTEXPR
#       endif
#   endif
#   ifndef _CPPUNWIND
#       define BETTER_ENUMS_NO_EXCEPTIONS
#   endif
#   if _MSC_VER < 1600
#       define BETTER_ENUMS_VC2008_WORKAROUNDS
#   endif
#endif

#ifdef BETTER_ENUMS_CONSTEXPR
#   define BETTER_ENUMS_HAVE_CONSTEXPR
#endif

#ifdef BETTER_ENUMS_NO_CONSTEXPR
#   ifdef BETTER_ENUMS_HAVE_CONSTEXPR
#       undef BETTER_ENUMS_HAVE_CONSTEXPR
#   endif
#endif

// GCC (and maybe clang) can be made to warn about using 0 or NULL when nullptr
// is available, so Better Enums tries to use nullptr. This passage uses
// availability of constexpr as a proxy for availability of nullptr, i.e. it
// assumes that nullptr is available when compiling on the right versions of gcc
// and clang with the right -std flag. This is actually slightly wrong, because
// nullptr is also available in Visual C++, but constexpr isn't. This
// imprecision doesn't matter, however, because VC++ doesn't have the warnings
// that make using nullptr necessary.
#ifdef BETTER_ENUMS_HAVE_CONSTEXPR
#   define BETTER_ENUMS_CONSTEXPR_     constexpr
#   define BETTER_ENUMS_NULLPTR        nullptr
#else
#   define BETTER_ENUMS_CONSTEXPR_
#   define BETTER_ENUMS_NULLPTR        NULL
#endif

#ifndef BETTER_ENUMS_NO_EXCEPTIONS
#   define BETTER_ENUMS_IF_EXCEPTIONS(x) x
#else
#   define BETTER_ENUMS_IF_EXCEPTIONS(x)
#endif

#ifdef __GNUC__
#   define BETTER_ENUMS_UNUSED __attribute__((__unused__))
#else
#   define BETTER_ENUMS_UNUSED
#endif



// Higher-order preprocessor macros.

#ifdef BETTER_ENUMS_MACRO_FILE
#   include BETTER_ENUMS_MACRO_FILE
#else

#define BETTER_ENUMS_PP_MAP(macro, data, ...) \
    BETTER_ENUMS_ID( \
        BETTER_ENUMS_APPLY( \
            BETTER_ENUMS_PP_MAP_VAR_COUNT, \
            BETTER_ENUMS_PP_COUNT(__VA_ARGS__)) \
        (macro, data, __VA_ARGS__))

#define BETTER_ENUMS_PP_MAP_VAR_COUNT(count) BETTER_ENUMS_M ## count

#define BETTER_ENUMS_APPLY(macro, ...) BETTER_ENUMS_ID(macro(__VA_ARGS__))

#define BETTER_ENUMS_ID(x) x

#define BETTER_ENUMS_M1(m, d, x) m(d,0,x)
#define BETTER_ENUMS_M2(m,d,x,...) m(d,1,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M1(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M3(m,d,x,...) m(d,2,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M2(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M4(m,d,x,...) m(d,3,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M3(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M5(m,d,x,...) m(d,4,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M4(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M6(m,d,x,...) m(d,5,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M5(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M7(m,d,x,...) m(d,6,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M6(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M8(m,d,x,...) m(d,7,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M7(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M9(m,d,x,...) m(d,8,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M8(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M10(m,d,x,...) m(d,9,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M9(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M11(m,d,x,...) m(d,10,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M10(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M12(m,d,x,...) m(d,11,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M11(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M13(m,d,x,...) m(d,12,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M12(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M14(m,d,x,...) m(d,13,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M13(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M15(m,d,x,...) m(d,14,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M14(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M16(m,d,x,...) m(d,15,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M15(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M17(m,d,x,...) m(d,16,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M16(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M18(m,d,x,...) m(d,17,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M17(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M19(m,d,x,...) m(d,18,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M18(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M20(m,d,x,...) m(d,19,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M19(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M21(m,d,x,...) m(d,20,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M20(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M22(m,d,x,...) m(d,21,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M21(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M23(m,d,x,...) m(d,22,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M22(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M24(m,d,x,...) m(d,23,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M23(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M25(m,d,x,...) m(d,24,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M24(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M26(m,d,x,...) m(d,25,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M25(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M27(m,d,x,...) m(d,26,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M26(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M28(m,d,x,...) m(d,27,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M27(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M29(m,d,x,...) m(d,28,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M28(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M30(m,d,x,...) m(d,29,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M29(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M31(m,d,x,...) m(d,30,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M30(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M32(m,d,x,...) m(d,31,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M31(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M33(m,d,x,...) m(d,32,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M32(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M34(m,d,x,...) m(d,33,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M33(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M35(m,d,x,...) m(d,34,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M34(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M36(m,d,x,...) m(d,35,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M35(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M37(m,d,x,...) m(d,36,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M36(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M38(m,d,x,...) m(d,37,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M37(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M39(m,d,x,...) m(d,38,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M38(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M40(m,d,x,...) m(d,39,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M39(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M41(m,d,x,...) m(d,40,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M40(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M42(m,d,x,...) m(d,41,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M41(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M43(m,d,x,...) m(d,42,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M42(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M44(m,d,x,...) m(d,43,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M43(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M45(m,d,x,...) m(d,44,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M44(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M46(m,d,x,...) m(d,45,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M45(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M47(m,d,x,...) m(d,46,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M46(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M48(m,d,x,...) m(d,47,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M47(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M49(m,d,x,...) m(d,48,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M48(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M50(m,d,x,...) m(d,49,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M49(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M51(m,d,x,...) m(d,50,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M50(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M52(m,d,x,...) m(d,51,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M51(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M53(m,d,x,...) m(d,52,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M52(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M54(m,d,x,...) m(d,53,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M53(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M55(m,d,x,...) m(d,54,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M54(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M56(m,d,x,...) m(d,55,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M55(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M57(m,d,x,...) m(d,56,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M56(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M58(m,d,x,...) m(d,57,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M57(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M59(m,d,x,...) m(d,58,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M58(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M60(m,d,x,...) m(d,59,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M59(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M61(m,d,x,...) m(d,60,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M60(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M62(m,d,x,...) m(d,61,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M61(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M63(m,d,x,...) m(d,62,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M62(m,d,__VA_ARGS__))
#define BETTER_ENUMS_M64(m,d,x,...) m(d,63,x) \
    BETTER_ENUMS_ID(BETTER_ENUMS_M63(m,d,__VA_ARGS__))

#define BETTER_ENUMS_PP_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10,    \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, \
    _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, \
    _56, _57, _58, _59, _60, _61, _62, _63, _64, count, ...) count

#define BETTER_ENUMS_PP_COUNT(...) \
    BETTER_ENUMS_ID(BETTER_ENUMS_PP_COUNT_IMPL(__VA_ARGS__, 64, 63, 62, 61, 60,\
        59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42,\
        41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,\
        23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, \
        4, 3, 2, 1))

#define BETTER_ENUMS_ITERATE(X, f, l) X(f, l, 0) X(f, l, 1) X(f, l, 2)         \
    X(f, l, 3) X(f, l, 4) X(f, l, 5) X(f, l, 6) X(f, l, 7) X(f, l, 8)          \
    X(f, l, 9) X(f, l, 10) X(f, l, 11) X(f, l, 12) X(f, l, 13) X(f, l, 14)     \
    X(f, l, 15) X(f, l, 16) X(f, l, 17) X(f, l, 18) X(f, l, 19) X(f, l, 20)    \
    X(f, l, 21) X(f, l, 22) X(f, l, 23)

#endif // #ifdef BETTER_ENUMS_MACRO_FILE else case



namespace better_enums {


// Optional type.

template <typename T>
BETTER_ENUMS_CONSTEXPR_ inline T _default()
{
    return static_cast<typename T::_enumerated>(0);
}

template <>
BETTER_ENUMS_CONSTEXPR_ inline const char* _default<const char*>()
{
    return BETTER_ENUMS_NULLPTR;
}

template <>
BETTER_ENUMS_CONSTEXPR_ inline std::size_t _default<std::size_t>()
{
    return 0;
}

template <typename T>
struct optional {
    BETTER_ENUMS_CONSTEXPR_ optional() :
        _valid(false), _value(_default<T>()) { }

    BETTER_ENUMS_CONSTEXPR_ optional(T v) : _valid(true), _value(v) { }

    BETTER_ENUMS_CONSTEXPR_ const T& operator *() const { return _value; }
    BETTER_ENUMS_CONSTEXPR_ const T* operator ->() const { return &_value; }

    BETTER_ENUMS_CONSTEXPR_ operator bool() const { return _valid; }

    BETTER_ENUMS_CONSTEXPR_ const T& value() const { return _value; }

  private:
    bool    _valid;
    T       _value;
};

template <typename CastTo, typename Element>
BETTER_ENUMS_CONSTEXPR_ static optional<CastTo>
_map_index(const Element *array, optional<std::size_t> index)
{
    return index ? static_cast<CastTo>(array[*index]) : optional<CastTo>();
}

#ifdef BETTER_ENUMS_VC2008_WORKAROUNDS

#define BETTER_ENUMS_OR_THROW                                                  \
    if (!maybe)                                                                \
        throw std::runtime_error(message);                                     \
                                                                               \
    return *maybe;

#else

#define BETTER_ENUMS_OR_THROW                                                  \
    return maybe ? *maybe : throw std::runtime_error(message);

#endif

BETTER_ENUMS_IF_EXCEPTIONS(
template <typename T>
BETTER_ENUMS_CONSTEXPR_ static T _or_throw(optional<T> maybe,
                                           const char *message)
{
    BETTER_ENUMS_OR_THROW
}
)

template <typename T>
BETTER_ENUMS_CONSTEXPR_ static T* _or_null(optional<T*> maybe)
{
    return maybe ? *maybe : BETTER_ENUMS_NULLPTR;
}

template <typename T>
BETTER_ENUMS_CONSTEXPR_ static T _or_zero(optional<T> maybe)
{
    return maybe ? *maybe : T::_from_integral_unchecked(0);
}



// Functional sequencing. This is essentially a comma operator wrapped in a
// constexpr function. g++ 4.7 doesn't "accept" integral constants in the second
// position for the comma operator, and emits an external symbol, which then
// causes a linking error.

template <typename T, typename U>
BETTER_ENUMS_CONSTEXPR_ U
continue_with(T, U value) { return value; }



// Values array declaration helper.

//! Get intrinsic value of an (Enum::value) by taking advantage of
// C-conversion's parentheses priority
template <typename EnumType>
struct _eat_assign {
    explicit BETTER_ENUMS_CONSTEXPR_ _eat_assign(EnumType value) : _value(value)
        { }

    template <typename Any>
    BETTER_ENUMS_CONSTEXPR_ const _eat_assign&
    operator =(Any) const { return *this; }

    BETTER_ENUMS_CONSTEXPR_ operator EnumType () const { return _value; }

  private:
    EnumType    _value;
};



// Iterables.

template <typename Element>
struct _iterable {
    typedef const Element*  iterator;

    BETTER_ENUMS_CONSTEXPR_ iterator begin() const { return iterator(_array); }
    BETTER_ENUMS_CONSTEXPR_ iterator end() const
        { return iterator(_array + _size); }
    BETTER_ENUMS_CONSTEXPR_ std::size_t size() const { return _size; }
    BETTER_ENUMS_CONSTEXPR_ const Element& operator [](std::size_t index) const
        { return _array[index]; }

    BETTER_ENUMS_CONSTEXPR_ _iterable(const Element *array, std::size_t s) :
        _array(array), _size(s) { }

  private:
    const Element * const   _array;
    const std::size_t       _size;
};



// String routines.

BETTER_ENUMS_CONSTEXPR_ static const char       *_name_enders = "= \t\n";

BETTER_ENUMS_CONSTEXPR_ inline bool _ends_name(char c, std::size_t index = 0)
{
    return
        c == _name_enders[index] ? true  :
        _name_enders[index] == '\0' ? false :
        _ends_name(c, index + 1);
}

BETTER_ENUMS_CONSTEXPR_ inline bool _has_initializer(const char *s,
                                                     std::size_t index = 0)
{
    return
        s[index] == '\0' ? false :
        s[index] == '=' ? true :
        _has_initializer(s, index + 1);
}

BETTER_ENUMS_CONSTEXPR_ inline std::size_t
_constant_length(const char *s, std::size_t index = 0)
{
    return _ends_name(s[index]) ? index : _constant_length(s, index + 1);
}

BETTER_ENUMS_CONSTEXPR_ inline char
_select(const char *from, std::size_t from_length, std::size_t index)
{
    return index >= from_length ? '\0' : from[index];
}

BETTER_ENUMS_CONSTEXPR_ inline char _to_lower_ascii(char c)
{
    return c >= 0x41 && c <= 0x5A ? static_cast<char>(c + 0x20) : c;
}

BETTER_ENUMS_CONSTEXPR_ inline bool _names_match(const char *stringizedName,
                                                 const char *referenceName,
                                                 std::size_t index = 0)
{
    return
        _ends_name(stringizedName[index]) ? referenceName[index] == '\0' :
        referenceName[index] == '\0' ? false :
        stringizedName[index] != referenceName[index] ? false :
        _names_match(stringizedName, referenceName, index + 1);
}

BETTER_ENUMS_CONSTEXPR_ inline bool
_names_match_nocase(const char *stringizedName, const char *referenceName,
                    std::size_t index = 0)
{
    return
        _ends_name(stringizedName[index]) ? referenceName[index] == '\0' :
        referenceName[index] == '\0' ? false :
        _to_lower_ascii(stringizedName[index]) !=
            _to_lower_ascii(referenceName[index]) ? false :
        _names_match_nocase(stringizedName, referenceName, index + 1);
}

inline void _trim_names(const char * const *raw_names,
                        const char **trimmed_names,
                        char *storage, std::size_t count)
{
    std::size_t     offset = 0;

    for (std::size_t index = 0; index < count; ++index) {
        trimmed_names[index] = storage + offset;

        std::size_t trimmed_length =
            std::strcspn(raw_names[index], _name_enders);
        storage[offset + trimmed_length] = '\0';

        std::size_t raw_length = std::strlen(raw_names[index]);
        offset += raw_length + 1;
    }
}



// Eager initialization.
template <typename Enum>
struct _initialize_at_program_start {
    _initialize_at_program_start() { Enum::initialize(); }
};

} // namespace better_enums



// Array generation macros.

#define BETTER_ENUMS_EAT_ASSIGN_SINGLE(EnumType, index, expression)            \
    (EnumType)((::better_enums::_eat_assign<EnumType>)EnumType::expression),

#define BETTER_ENUMS_EAT_ASSIGN(EnumType, ...)                                 \
    BETTER_ENUMS_ID(                                                           \
        BETTER_ENUMS_PP_MAP(                                                   \
            BETTER_ENUMS_EAT_ASSIGN_SINGLE, EnumType, __VA_ARGS__))



#ifdef BETTER_ENUMS_HAVE_CONSTEXPR



#define BETTER_ENUMS_SELECT_SINGLE_CHARACTER(from, from_length, index)         \
    ::better_enums::_select(from, from_length, index),

#define BETTER_ENUMS_SELECT_CHARACTERS(from, from_length)                      \
    BETTER_ENUMS_ITERATE(                                                      \
        BETTER_ENUMS_SELECT_SINGLE_CHARACTER, from, from_length)



#define BETTER_ENUMS_TRIM_SINGLE_STRING(ignored, index, expression)            \
constexpr std::size_t   _length_ ## index =                                    \
    ::better_enums::_constant_length(#expression);                             \
constexpr const char    _trimmed_ ## index [] =                                \
    { BETTER_ENUMS_SELECT_CHARACTERS(#expression, _length_ ## index) };        \
constexpr const char    *_final_ ## index =                                    \
    ::better_enums::_has_initializer(#expression) ?                            \
        _trimmed_ ## index : #expression;

#define BETTER_ENUMS_TRIM_STRINGS(...)                                         \
    BETTER_ENUMS_ID(                                                           \
        BETTER_ENUMS_PP_MAP(                                                   \
            BETTER_ENUMS_TRIM_SINGLE_STRING, ignored, __VA_ARGS__))



#define BETTER_ENUMS_REFER_TO_SINGLE_STRING(ignored, index, expression)        \
    _final_ ## index,

#define BETTER_ENUMS_REFER_TO_STRINGS(...)                                     \
    BETTER_ENUMS_ID(                                                           \
        BETTER_ENUMS_PP_MAP(                                                   \
            BETTER_ENUMS_REFER_TO_SINGLE_STRING, ignored, __VA_ARGS__))



#endif // #ifdef BETTER_ENUMS_HAVE_CONSTEXPR



#define BETTER_ENUMS_STRINGIZE_SINGLE(ignored, index, expression)  #expression,

#define BETTER_ENUMS_STRINGIZE(...)                                            \
    BETTER_ENUMS_ID(                                                           \
        BETTER_ENUMS_PP_MAP(                                                   \
            BETTER_ENUMS_STRINGIZE_SINGLE, ignored, __VA_ARGS__))

#define BETTER_ENUMS_RESERVE_STORAGE_SINGLE(ignored, index, expression)        \
    #expression ","

#define BETTER_ENUMS_RESERVE_STORAGE(...)                                      \
    BETTER_ENUMS_ID(                                                           \
        BETTER_ENUMS_PP_MAP(                                                   \
            BETTER_ENUMS_RESERVE_STORAGE_SINGLE, ignored, __VA_ARGS__))



// The enums proper.

#define BETTER_ENUMS_NS(EnumType)  better_enums_data_ ## EnumType

#ifdef BETTER_ENUMS_VC2008_WORKAROUNDS

#define BETTER_ENUMS_COPY_CONSTRUCTOR(Enum)                                    \
        BETTER_ENUMS_CONSTEXPR_ Enum(const Enum &other) :                      \
            _value(other._value) { }

#else

#define BETTER_ENUMS_COPY_CONSTRUCTOR(Enum)

#endif

#ifndef BETTER_ENUMS_CLASS_ATTRIBUTE
#   define BETTER_ENUMS_CLASS_ATTRIBUTE
#endif

#define BETTER_ENUMS_TYPE(SetUnderlyingType, SwitchType, GenerateSwitchType,   \
                          GenerateStrings, ToStringConstexpr,                  \
                          DeclareInitialize, DefineInitialize, CallInitialize, \
                          Enum, Underlying, ...)                               \
                                                                               \
namespace better_enums_data_ ## Enum {                                         \
                                                                               \
BETTER_ENUMS_ID(GenerateSwitchType(Underlying, __VA_ARGS__))                   \
                                                                               \
}                                                                              \
                                                                               \
class BETTER_ENUMS_CLASS_ATTRIBUTE Enum {                                      \
  private:                                                                     \
    typedef ::better_enums::optional<Enum>                  _optional;         \
    typedef ::better_enums::optional<std::size_t>           _optional_index;   \
                                                                               \
  public:                                                                      \
    typedef Underlying                                      _integral;         \
                                                                               \
    enum _enumerated SetUnderlyingType(Underlying) { __VA_ARGS__ };            \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ Enum(_enumerated value) : _value(value) { }        \
                                                                               \
    BETTER_ENUMS_COPY_CONSTRUCTOR(Enum)                                        \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ operator SwitchType(Enum)() const                  \
    {                                                                          \
        return SwitchType(Enum)(_value);                                       \
    }                                                                          \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ _integral _to_integral() const;                    \
    BETTER_ENUMS_IF_EXCEPTIONS(                                                \
    BETTER_ENUMS_CONSTEXPR_ static Enum _from_integral(_integral value);       \
    )                                                                          \
    BETTER_ENUMS_CONSTEXPR_ static Enum                                        \
    _from_integral_unchecked(_integral value);                                 \
    BETTER_ENUMS_CONSTEXPR_ static _optional                                   \
    _from_integral_nothrow(_integral value);                                   \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ std::size_t _to_index() const;                     \
    BETTER_ENUMS_IF_EXCEPTIONS(                                                \
    BETTER_ENUMS_CONSTEXPR_ static Enum _from_index(std::size_t index);        \
    )                                                                          \
    BETTER_ENUMS_CONSTEXPR_ static Enum                                        \
    _from_index_unchecked(std::size_t index);                                  \
    BETTER_ENUMS_CONSTEXPR_ static _optional                                   \
    _from_index_nothrow(std::size_t index);                                    \
                                                                               \
    ToStringConstexpr const char* _to_string() const;                          \
    BETTER_ENUMS_IF_EXCEPTIONS(                                                \
    BETTER_ENUMS_CONSTEXPR_ static Enum _from_string(const char *name);        \
    )                                                                          \
    BETTER_ENUMS_CONSTEXPR_ static _optional                                   \
    _from_string_nothrow(const char *name);                                    \
                                                                               \
    BETTER_ENUMS_IF_EXCEPTIONS(                                                \
    BETTER_ENUMS_CONSTEXPR_ static Enum _from_string_nocase(const char *name); \
    )                                                                          \
    BETTER_ENUMS_CONSTEXPR_ static _optional                                   \
    _from_string_nocase_nothrow(const char *name);                             \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ static bool _is_valid(_integral value);            \
    BETTER_ENUMS_CONSTEXPR_ static bool _is_valid(const char *name);           \
    BETTER_ENUMS_CONSTEXPR_ static bool _is_valid_nocase(const char *name);    \
                                                                               \
    typedef ::better_enums::_iterable<Enum>             _value_iterable;       \
    typedef ::better_enums::_iterable<const char*>      _name_iterable;        \
                                                                               \
    typedef _value_iterable::iterator                   _value_iterator;       \
    typedef _name_iterable::iterator                    _name_iterator;        \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ static const std::size_t _size_constant =          \
        BETTER_ENUMS_ID(BETTER_ENUMS_PP_COUNT(__VA_ARGS__));                   \
    BETTER_ENUMS_CONSTEXPR_ static std::size_t _size()                         \
        { return _size_constant; }                                             \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ static const char* _name();                        \
    BETTER_ENUMS_CONSTEXPR_ static _value_iterable _values();                  \
    ToStringConstexpr static _name_iterable _names();                          \
                                                                               \
    _integral      _value;                                                     \
                                                                               \
    BETTER_ENUMS_DEFAULT_CONSTRUCTOR(Enum)                                     \
                                                                               \
  private:                                                                     \
    explicit BETTER_ENUMS_CONSTEXPR_ Enum(const _integral &value) :            \
        _value(value) { }                                                      \
                                                                               \
    DeclareInitialize                                                          \
                                                                               \
    BETTER_ENUMS_CONSTEXPR_ static _optional_index                             \
    _from_value_loop(_integral value, std::size_t index = 0);                  \
    BETTER_ENUMS_CONSTEXPR_ static _optional_index                             \
    _from_string_loop(const char *name, std::size_t index = 0);                \
    BETTER_ENUMS_CONSTEXPR_ static _optional_index                             \
    _from_string_nocase_loop(const char *name, std::size_t index = 0);         \
                                                                               \
    friend struct ::better_enums::_initialize_at_program_start<Enum>;          \
};                                                                             \
                                                                               \
namespace better_enums_data_ ## Enum {                                         \
                                                                               \
static ::better_enums::_initialize_at_program_start<Enum>                      \
                                                _force_initialization;         \
                                                                               \
enum _putNamesInThisScopeAlso { __VA_ARGS__ };                                 \
                                                                               \
BETTER_ENUMS_IGNORE_OLD_CAST_HEADER                                            \
BETTER_ENUMS_IGNORE_OLD_CAST_BEGIN                                             \
BETTER_ENUMS_CONSTEXPR_ const Enum      _value_array[] =                       \
    { BETTER_ENUMS_ID(BETTER_ENUMS_EAT_ASSIGN(Enum, __VA_ARGS__)) };           \
BETTER_ENUMS_IGNORE_OLD_CAST_END                                               \
                                                                               \
BETTER_ENUMS_ID(GenerateStrings(Enum, __VA_ARGS__))                            \
                                                                               \
}                                                                              \
                                                                               \
BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER                                          \
BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN                                           \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline const Enum                                                              \
operator +(Enum::_enumerated enumerated)                                       \
{                                                                              \
    return static_cast<Enum>(enumerated);                                      \
}                                                                              \
BETTER_ENUMS_IGNORE_ATTRIBUTES_END                                             \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional_index                           \
Enum::_from_value_loop(Enum::_integral value, std::size_t index)               \
{                                                                              \
    return                                                                     \
        index == _size() ?                                                     \
            _optional_index() :                                                \
            BETTER_ENUMS_NS(Enum)::_value_array[index]._value == value ?       \
                _optional_index(index) :                                       \
                _from_value_loop(value, index + 1);                            \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional_index                           \
Enum::_from_string_loop(const char *name, std::size_t index)                   \
{                                                                              \
    return                                                                     \
        index == _size() ? _optional_index() :                                 \
        ::better_enums::_names_match(                                          \
            BETTER_ENUMS_NS(Enum)::_raw_names()[index], name) ?                \
            _optional_index(index) :                                           \
            _from_string_loop(name, index + 1);                                \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional_index                           \
Enum::_from_string_nocase_loop(const char *name, std::size_t index)            \
{                                                                              \
    return                                                                     \
        index == _size() ? _optional_index() :                                 \
            ::better_enums::_names_match_nocase(                               \
                BETTER_ENUMS_NS(Enum)::_raw_names()[index], name) ?            \
                    _optional_index(index) :                                   \
                    _from_string_nocase_loop(name, index + 1);                 \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_integral Enum::_to_integral() const      \
{                                                                              \
    return _integral(_value);                                                  \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline std::size_t Enum::_to_index() const             \
{                                                                              \
    return *_from_value_loop(_value);                                          \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum                                            \
Enum::_from_index_unchecked(std::size_t index)                                 \
{                                                                              \
    return                                                                     \
        ::better_enums::_or_zero(_from_index_nothrow(index));                  \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional                                 \
Enum::_from_index_nothrow(std::size_t index)                                   \
{                                                                              \
    return                                                                     \
        index >= _size() ?                                                     \
            _optional() :                                                      \
             _optional(BETTER_ENUMS_NS(Enum)::_value_array[index]);            \
}                                                                              \
                                                                               \
BETTER_ENUMS_IF_EXCEPTIONS(                                                    \
BETTER_ENUMS_CONSTEXPR_ inline Enum Enum::_from_index(std::size_t index)       \
{                                                                              \
    return                                                                     \
        ::better_enums::_or_throw(_from_index_nothrow(index),                  \
                                  #Enum "::_from_index: invalid argument");    \
}                                                                              \
)                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum                                            \
Enum::_from_integral_unchecked(_integral value)                                \
{                                                                              \
    return static_cast<_enumerated>(value);                                    \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional                                 \
Enum::_from_integral_nothrow(_integral value)                                  \
{                                                                              \
    return                                                                     \
        ::better_enums::_map_index<Enum>(BETTER_ENUMS_NS(Enum)::_value_array,  \
                                         _from_value_loop(value));             \
}                                                                              \
                                                                               \
BETTER_ENUMS_IF_EXCEPTIONS(                                                    \
BETTER_ENUMS_CONSTEXPR_ inline Enum Enum::_from_integral(_integral value)      \
{                                                                              \
    return                                                                     \
        ::better_enums::_or_throw(_from_integral_nothrow(value),               \
                                  #Enum "::_from_integral: invalid argument"); \
}                                                                              \
)                                                                              \
                                                                               \
ToStringConstexpr inline const char* Enum::_to_string() const                  \
{                                                                              \
    return                                                                     \
        ::better_enums::_or_null(                                              \
            ::better_enums::_map_index<const char*>(                           \
                BETTER_ENUMS_NS(Enum)::_name_array(),                          \
                _from_value_loop(CallInitialize(_value))));                    \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional                                 \
Enum::_from_string_nothrow(const char *name)                                   \
{                                                                              \
    return                                                                     \
        ::better_enums::_map_index<Enum>(                                      \
            BETTER_ENUMS_NS(Enum)::_value_array, _from_string_loop(name));     \
}                                                                              \
                                                                               \
BETTER_ENUMS_IF_EXCEPTIONS(                                                    \
BETTER_ENUMS_CONSTEXPR_ inline Enum Enum::_from_string(const char *name)       \
{                                                                              \
    return                                                                     \
        ::better_enums::_or_throw(_from_string_nothrow(name),                  \
                                  #Enum "::_from_string: invalid argument");   \
}                                                                              \
)                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_optional                                 \
Enum::_from_string_nocase_nothrow(const char *name)                            \
{                                                                              \
    return                                                                     \
        ::better_enums::_map_index<Enum>(BETTER_ENUMS_NS(Enum)::_value_array,  \
                                         _from_string_nocase_loop(name));      \
}                                                                              \
                                                                               \
BETTER_ENUMS_IF_EXCEPTIONS(                                                    \
BETTER_ENUMS_CONSTEXPR_ inline Enum Enum::_from_string_nocase(const char *name)\
{                                                                              \
    return                                                                     \
        ::better_enums::_or_throw(                                             \
            _from_string_nocase_nothrow(name),                                 \
            #Enum "::_from_string_nocase: invalid argument");                  \
}                                                                              \
)                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline bool Enum::_is_valid(_integral value)           \
{                                                                              \
    return _from_value_loop(value);                                            \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline bool Enum::_is_valid(const char *name)          \
{                                                                              \
    return _from_string_loop(name);                                            \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline bool Enum::_is_valid_nocase(const char *name)   \
{                                                                              \
    return _from_string_nocase_loop(name);                                     \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline const char* Enum::_name()                       \
{                                                                              \
    return #Enum;                                                              \
}                                                                              \
                                                                               \
BETTER_ENUMS_CONSTEXPR_ inline Enum::_value_iterable Enum::_values()           \
{                                                                              \
    return _value_iterable(BETTER_ENUMS_NS(Enum)::_value_array, _size());      \
}                                                                              \
                                                                               \
ToStringConstexpr inline Enum::_name_iterable Enum::_names()                   \
{                                                                              \
    return                                                                     \
        _name_iterable(BETTER_ENUMS_NS(Enum)::_name_array(),                   \
                       CallInitialize(_size()));                               \
}                                                                              \
                                                                               \
DefineInitialize(Enum)                                                         \
                                                                               \
BETTER_ENUMS_IGNORE_ATTRIBUTES_HEADER                                          \
BETTER_ENUMS_IGNORE_ATTRIBUTES_BEGIN                                           \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline bool operator ==(const Enum &a, const Enum &b)                          \
    { return a._to_integral() == b._to_integral(); }                           \
                                                                               \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline bool operator !=(const Enum &a, const Enum &b)                          \
    { return a._to_integral() != b._to_integral(); }                           \
                                                                               \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline bool operator <(const Enum &a, const Enum &b)                           \
    { return a._to_integral() < b._to_integral(); }                            \
                                                                               \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline bool operator <=(const Enum &a, const Enum &b)                          \
    { return a._to_integral() <= b._to_integral(); }                           \
                                                                               \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline bool operator >(const Enum &a, const Enum &b)                           \
    { return a._to_integral() > b._to_integral(); }                            \
                                                                               \
BETTER_ENUMS_UNUSED BETTER_ENUMS_CONSTEXPR_                                    \
inline bool operator >=(const Enum &a, const Enum &b)                          \
    { return a._to_integral() >= b._to_integral(); }                           \
BETTER_ENUMS_IGNORE_ATTRIBUTES_END                                             \
                                                                               \
                                                                               \
template <typename Char, typename Traits>                                      \
std::basic_ostream<Char, Traits>&                                              \
operator <<(std::basic_ostream<Char, Traits>& stream, const Enum &value)       \
{                                                                              \
    return stream << value._to_string();                                       \
}                                                                              \
                                                                               \
template <typename Char, typename Traits>                                      \
std::basic_istream<Char, Traits>&                                              \
operator >>(std::basic_istream<Char, Traits>& stream, Enum &value)             \
{                                                                              \
    std::basic_string<Char, Traits>     buffer;                                \
                                                                               \
    stream >> buffer;                                                          \
    ::better_enums::optional<Enum>      converted =                            \
        Enum::_from_string_nothrow(buffer.c_str());                            \
                                                                               \
    if (converted)                                                             \
        value = *converted;                                                    \
    else                                                                       \
        stream.setstate(std::basic_istream<Char, Traits>::failbit);            \
                                                                               \
    return stream;                                                             \
}



// Enum feature options.

// C++98, C++11
#define BETTER_ENUMS_CXX98_UNDERLYING_TYPE(Underlying)

// C++11
#define BETTER_ENUMS_CXX11_UNDERLYING_TYPE(Underlying)                         \
    : Underlying

#if defined(_MSC_VER) && _MSC_VER >= 1700
// VS 2012 and above fully support strongly typed enums and will warn about
// incorrect usage.
#   define BETTER_ENUMS_LEGACY_UNDERLYING_TYPE(Underlying) \
        BETTER_ENUMS_CXX11_UNDERLYING_TYPE(Underlying)
#else
#   define BETTER_ENUMS_LEGACY_UNDERLYING_TYPE(Underlying) \
        BETTER_ENUMS_CXX98_UNDERLYING_TYPE(Underlying)
#endif

// C++98, C++11
#define BETTER_ENUMS_REGULAR_ENUM_SWITCH_TYPE(Type)                            \
    _enumerated

// C++11
#define BETTER_ENUMS_ENUM_CLASS_SWITCH_TYPE(Type)                              \
    BETTER_ENUMS_NS(Type)::_enumClassForSwitchStatements

// C++98, C++11
#define BETTER_ENUMS_REGULAR_ENUM_SWITCH_TYPE_GENERATE(Underlying, ...)

// C++11
#define BETTER_ENUMS_ENUM_CLASS_SWITCH_TYPE_GENERATE(Underlying, ...)          \
    enum class _enumClassForSwitchStatements : Underlying { __VA_ARGS__ };

// C++98
#define BETTER_ENUMS_CXX98_TRIM_STRINGS_ARRAYS(Enum, ...)                      \
    inline const char** _raw_names()                                           \
    {                                                                          \
        static const char   *value[] =                                         \
            { BETTER_ENUMS_ID(BETTER_ENUMS_STRINGIZE(__VA_ARGS__)) };          \
        return value;                                                          \
    }                                                                          \
                                                                               \
    inline char* _name_storage()                                               \
    {                                                                          \
        static char         storage[] =                                        \
            BETTER_ENUMS_ID(BETTER_ENUMS_RESERVE_STORAGE(__VA_ARGS__));        \
        return storage;                                                        \
    }                                                                          \
                                                                               \
    inline const char** _name_array()                                          \
    {                                                                          \
        static const char   *value[Enum::_size_constant];                      \
        return value;                                                          \
    }                                                                          \
                                                                               \
    inline bool& _initialized()                                                \
    {                                                                          \
        static bool         value = false;                                     \
        return value;                                                          \
    }

// C++11 fast version
#define BETTER_ENUMS_CXX11_PARTIAL_CONSTEXPR_TRIM_STRINGS_ARRAYS(Enum, ...)    \
    constexpr const char    *_the_raw_names[] =                                \
        { BETTER_ENUMS_ID(BETTER_ENUMS_STRINGIZE(__VA_ARGS__)) };              \
                                                                               \
    constexpr const char * const * _raw_names()                                \
    {                                                                          \
        return _the_raw_names;                                                 \
    }                                                                          \
                                                                               \
    inline char* _name_storage()                                               \
    {                                                                          \
        static char         storage[] =                                        \
            BETTER_ENUMS_ID(BETTER_ENUMS_RESERVE_STORAGE(__VA_ARGS__));        \
        return storage;                                                        \
    }                                                                          \
                                                                               \
    inline const char** _name_array()                                          \
    {                                                                          \
        static const char   *value[Enum::_size_constant];                      \
        return value;                                                          \
    }                                                                          \
                                                                               \
    inline bool& _initialized()                                                \
    {                                                                          \
        static bool         value = false;                                     \
        return value;                                                          \
    }

// C++11 slow all-constexpr version
#define BETTER_ENUMS_CXX11_FULL_CONSTEXPR_TRIM_STRINGS_ARRAYS(Enum, ...)       \
    BETTER_ENUMS_ID(BETTER_ENUMS_TRIM_STRINGS(__VA_ARGS__))                    \
                                                                               \
    constexpr const char * const    _the_name_array[] =                        \
        { BETTER_ENUMS_ID(BETTER_ENUMS_REFER_TO_STRINGS(__VA_ARGS__)) };       \
                                                                               \
    constexpr const char * const * _name_array()                               \
    {                                                                          \
        return _the_name_array;                                                \
    }                                                                          \
                                                                               \
    constexpr const char * const * _raw_names()                                \
    {                                                                          \
        return _the_name_array;                                                \
    }

// C++98, C++11 fast version
#define BETTER_ENUMS_NO_CONSTEXPR_TO_STRING_KEYWORD

// C++11 slow all-constexpr version
#define BETTER_ENUMS_CONSTEXPR_TO_STRING_KEYWORD                               \
    constexpr

// C++98, C++11 fast version
#define BETTER_ENUMS_DO_DECLARE_INITIALIZE                                     \
    static int initialize();

// C++11 slow all-constexpr version
#define BETTER_ENUMS_DECLARE_EMPTY_INITIALIZE                                  \
    static int initialize() { return 0; }

// C++98, C++11 fast version
#define BETTER_ENUMS_DO_DEFINE_INITIALIZE(Enum)                                \
    inline int Enum::initialize()                                              \
    {                                                                          \
        if (BETTER_ENUMS_NS(Enum)::_initialized())                             \
            return 0;                                                          \
                                                                               \
        ::better_enums::_trim_names(BETTER_ENUMS_NS(Enum)::_raw_names(),       \
                                    BETTER_ENUMS_NS(Enum)::_name_array(),      \
                                    BETTER_ENUMS_NS(Enum)::_name_storage(),    \
                                    _size());                                  \
                                                                               \
        BETTER_ENUMS_NS(Enum)::_initialized() = true;                          \
                                                                               \
        return 0;                                                              \
    }

// C++11 slow all-constexpr version
#define BETTER_ENUMS_DO_NOT_DEFINE_INITIALIZE(Enum)

// C++98, C++11 fast version
#define BETTER_ENUMS_DO_CALL_INITIALIZE(value)                                 \
    ::better_enums::continue_with(initialize(), value)

// C++11 slow all-constexpr version
#define BETTER_ENUMS_DO_NOT_CALL_INITIALIZE(value)                             \
    value



// User feature selection.

#ifdef BETTER_ENUMS_STRICT_CONVERSION
#   define BETTER_ENUMS_DEFAULT_SWITCH_TYPE                                    \
        BETTER_ENUMS_ENUM_CLASS_SWITCH_TYPE
#   define BETTER_ENUMS_DEFAULT_SWITCH_TYPE_GENERATE                           \
        BETTER_ENUMS_ENUM_CLASS_SWITCH_TYPE_GENERATE
#else
#   define BETTER_ENUMS_DEFAULT_SWITCH_TYPE                                    \
        BETTER_ENUMS_REGULAR_ENUM_SWITCH_TYPE
#   define BETTER_ENUMS_DEFAULT_SWITCH_TYPE_GENERATE                           \
        BETTER_ENUMS_REGULAR_ENUM_SWITCH_TYPE_GENERATE
#endif



#ifndef BETTER_ENUMS_DEFAULT_CONSTRUCTOR
#   define BETTER_ENUMS_DEFAULT_CONSTRUCTOR(Enum)                              \
      private:                                                                 \
        Enum() : _value(0) { }
#endif



#ifdef BETTER_ENUMS_HAVE_CONSTEXPR

#ifdef BETTER_ENUMS_CONSTEXPR_TO_STRING
#   define BETTER_ENUMS_DEFAULT_TRIM_STRINGS_ARRAYS                            \
        BETTER_ENUMS_CXX11_FULL_CONSTEXPR_TRIM_STRINGS_ARRAYS
#   define BETTER_ENUMS_DEFAULT_TO_STRING_KEYWORD                              \
        BETTER_ENUMS_CONSTEXPR_TO_STRING_KEYWORD
#   define BETTER_ENUMS_DEFAULT_DECLARE_INITIALIZE                             \
        BETTER_ENUMS_DECLARE_EMPTY_INITIALIZE
#   define BETTER_ENUMS_DEFAULT_DEFINE_INITIALIZE                              \
        BETTER_ENUMS_DO_NOT_DEFINE_INITIALIZE
#   define BETTER_ENUMS_DEFAULT_CALL_INITIALIZE                                \
        BETTER_ENUMS_DO_NOT_CALL_INITIALIZE
#else
#   define BETTER_ENUMS_DEFAULT_TRIM_STRINGS_ARRAYS                            \
        BETTER_ENUMS_CXX11_PARTIAL_CONSTEXPR_TRIM_STRINGS_ARRAYS
#   define BETTER_ENUMS_DEFAULT_TO_STRING_KEYWORD                              \
        BETTER_ENUMS_NO_CONSTEXPR_TO_STRING_KEYWORD
#   define BETTER_ENUMS_DEFAULT_DECLARE_INITIALIZE                             \
        BETTER_ENUMS_DO_DECLARE_INITIALIZE
#   define BETTER_ENUMS_DEFAULT_DEFINE_INITIALIZE                              \
        BETTER_ENUMS_DO_DEFINE_INITIALIZE
#   define BETTER_ENUMS_DEFAULT_CALL_INITIALIZE                                \
        BETTER_ENUMS_DO_CALL_INITIALIZE
#endif



// Top-level macros.

#define BETTER_ENUM(Enum, Underlying, ...)                                     \
    BETTER_ENUMS_ID(BETTER_ENUMS_TYPE(                                         \
        BETTER_ENUMS_CXX11_UNDERLYING_TYPE,                                    \
        BETTER_ENUMS_DEFAULT_SWITCH_TYPE,                                      \
        BETTER_ENUMS_DEFAULT_SWITCH_TYPE_GENERATE,                             \
        BETTER_ENUMS_DEFAULT_TRIM_STRINGS_ARRAYS,                              \
        BETTER_ENUMS_DEFAULT_TO_STRING_KEYWORD,                                \
        BETTER_ENUMS_DEFAULT_DECLARE_INITIALIZE,                               \
        BETTER_ENUMS_DEFAULT_DEFINE_INITIALIZE,                                \
        BETTER_ENUMS_DEFAULT_CALL_INITIALIZE,                                  \
        Enum, Underlying, __VA_ARGS__))

#define SLOW_ENUM(Enum, Underlying, ...)                                       \
    BETTER_ENUMS_ID(BETTER_ENUMS_TYPE(                                         \
        BETTER_ENUMS_CXX11_UNDERLYING_TYPE,                                    \
        BETTER_ENUMS_DEFAULT_SWITCH_TYPE,                                      \
        BETTER_ENUMS_DEFAULT_SWITCH_TYPE_GENERATE,                             \
        BETTER_ENUMS_CXX11_FULL_CONSTEXPR_TRIM_STRINGS_ARRAYS,                 \
        BETTER_ENUMS_CONSTEXPR_TO_STRING_KEYWORD,                              \
        BETTER_ENUMS_DECLARE_EMPTY_INITIALIZE,                                 \
        BETTER_ENUMS_DO_NOT_DEFINE_INITIALIZE,                                 \
        BETTER_ENUMS_DO_NOT_CALL_INITIALIZE,                                   \
        Enum, Underlying, __VA_ARGS__))

#else

#define BETTER_ENUM(Enum, Underlying, ...)                                     \
    BETTER_ENUMS_ID(BETTER_ENUMS_TYPE(                                         \
        BETTER_ENUMS_LEGACY_UNDERLYING_TYPE,                                   \
        BETTER_ENUMS_DEFAULT_SWITCH_TYPE,                                      \
        BETTER_ENUMS_DEFAULT_SWITCH_TYPE_GENERATE,                             \
        BETTER_ENUMS_CXX98_TRIM_STRINGS_ARRAYS,                                \
        BETTER_ENUMS_NO_CONSTEXPR_TO_STRING_KEYWORD,                           \
        BETTER_ENUMS_DO_DECLARE_INITIALIZE,                                    \
        BETTER_ENUMS_DO_DEFINE_INITIALIZE,                                     \
        BETTER_ENUMS_DO_CALL_INITIALIZE,                                       \
        Enum, Underlying, __VA_ARGS__))

#endif



namespace better_enums {

// Maps.

template <typename T>
struct map_compare {
    BETTER_ENUMS_CONSTEXPR_ static bool less(const T& a, const T& b)
        { return a < b; }
};

template <>
struct map_compare<const char*> {
    BETTER_ENUMS_CONSTEXPR_ static bool less(const char *a, const char *b)
        { return less_loop(a, b); }

  private:
    BETTER_ENUMS_CONSTEXPR_ static bool
    less_loop(const char *a, const char *b, size_t index = 0)
    {
        return
            a[index] != b[index] ? a[index] < b[index] :
            a[index] == '\0' ? false :
            less_loop(a, b, index + 1);
    }
};

template <>
struct map_compare<const wchar_t*> {
    BETTER_ENUMS_CONSTEXPR_ static bool less(const wchar_t *a, const wchar_t *b)
        { return less_loop(a, b); }

  private:
    BETTER_ENUMS_CONSTEXPR_ static bool
    less_loop(const wchar_t *a, const wchar_t *b, size_t index = 0)
    {
        return
            a[index] != b[index] ? a[index] < b[index] :
            a[index] == L'\0' ? false :
            less_loop(a, b, index + 1);
    }
};

template <typename Enum, typename T, typename Compare = map_compare<T> >
struct map {
    typedef T (*function)(Enum);

    BETTER_ENUMS_CONSTEXPR_ explicit map(function f) : _f(f) { }

    BETTER_ENUMS_CONSTEXPR_ T from_enum(Enum value) const { return _f(value); }
    BETTER_ENUMS_CONSTEXPR_ T operator [](Enum value) const
        { return _f(value); }

    BETTER_ENUMS_CONSTEXPR_ Enum to_enum(T value) const
    {
        return
            _or_throw(to_enum_nothrow(value), "map::to_enum: invalid argument");
    }

    BETTER_ENUMS_CONSTEXPR_ optional<Enum>
    to_enum_nothrow(T value, size_t index = 0) const
    {
        return
            index >= Enum::_size() ? optional<Enum>() :
            Compare::less(_f(Enum::_values()[index]), value) ||
            Compare::less(value, _f(Enum::_values()[index])) ?
                to_enum_nothrow(value, index + 1) :
            Enum::_values()[index];
    }

  private:
    const function      _f;
};

template <typename Enum, typename T>
BETTER_ENUMS_CONSTEXPR_ map<Enum, T> make_map(T (*f)(Enum))
{
    return map<Enum, T>(f);
}

}

#define BETTER_ENUMS_DECLARE_STD_HASH(type)                                    \
	namespace std {                                                            \
    template <> struct hash<type>                                              \
    {                                                                          \
        size_t operator()(const type &x) const                                 \
        {                                                                      \
            return std::hash<size_t>()(x._to_integral());                      \
        }                                                                      \
    };                                                                         \
	}

#endif // #ifndef BETTER_ENUMS_ENUM_H

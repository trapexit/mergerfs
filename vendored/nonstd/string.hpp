//
// Copyright (c) 2025-2025 Martin Moene
//
// https://github.com/martinmoene/string-lite
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef NONSTD_STRING_BARE_HPP
#define NONSTD_STRING_BARE_HPP

#define string_bare_MAJOR  0
#define string_bare_MINOR  0
#define string_bare_PATCH  0

#define string_bare_VERSION  string_STRINGIFY(string_bare_MAJOR) "." string_STRINGIFY(string_bare_MINOR) "." string_STRINGIFY(string_bare_PATCH)

#define string_STRINGIFY(  x )  string_STRINGIFY_( x )
#define string_STRINGIFY_( x )  #x

// tweak header support:

#ifdef __has_include
# if __has_include(<nonstd/string.tweak.hpp>)
#  include <nonstd/string.tweak.hpp>
# endif
#define string_HAVE_TWEAK_HEADER  1
#else
#define string_HAVE_TWEAK_HEADER  0
//# pragma message("string.hpp: Note: Tweak header not supported.")
#endif

// string configuration:

// Select character types to provide:

#if !defined( string_CONFIG_PROVIDE_CHAR_T )
# define string_CONFIG_PROVIDE_CHAR_T  1
#endif

#if !defined( string_CONFIG_PROVIDE_WCHAR_T )
# define string_CONFIG_PROVIDE_WCHAR_T  0
#endif

#if !defined( string_CONFIG_PROVIDE_CHAR8_T )
# define string_CONFIG_PROVIDE_CHAR8_T  0
#endif

#if !defined( string_CONFIG_PROVIDE_CHAR16_T )
# define string_CONFIG_PROVIDE_CHAR16_T  0
#endif

#if !defined( string_CONFIG_PROVIDE_CHAR32_T )
# define string_CONFIG_PROVIDE_CHAR32_T  0
#endif

// Provide regex variants: default off, as it increases compile time considerably.

#if !defined( string_CONFIG_PROVIDE_REGEX )
# define string_CONFIG_PROVIDE_REGEX  0
#endif

// Control presence of exception handling (try and auto discover):

#ifndef string_CONFIG_NO_EXCEPTIONS
# if defined(_MSC_VER)
# include <cstddef>     // for _HAS_EXCEPTIONS
# endif
# if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (_HAS_EXCEPTIONS)
#  define string_CONFIG_NO_EXCEPTIONS  0
# else
#  define string_CONFIG_NO_EXCEPTIONS  1
# endif
#endif

// C++ language version detection:
// Note: VC14.0/1900 (VS2015) lacks too much from C++14.

#ifndef   string_CPLUSPLUS
# if defined(_MSVC_LANG ) && !defined(__clang__)
#  define string_CPLUSPLUS  (_MSC_VER == 1900 ? 201103L : _MSVC_LANG )
# else
#  define string_CPLUSPLUS  __cplusplus
# endif
#endif

#define string_CPP98_OR_GREATER  ( string_CPLUSPLUS >= 199711L )
#define string_CPP11_OR_GREATER  ( string_CPLUSPLUS >= 201103L )
#define string_CPP14_OR_GREATER  ( string_CPLUSPLUS >= 201402L )
#define string_CPP17_OR_GREATER  ( string_CPLUSPLUS >= 201703L )
#define string_CPP20_OR_GREATER  ( string_CPLUSPLUS >= 202002L )
#define string_CPP23_OR_GREATER  ( string_CPLUSPLUS >= 202302L )
#define string_CPP26_OR_GREATER  ( string_CPLUSPLUS >  202302L )  // not finalized yet

// MSVC version:

#if defined(_MSC_VER ) && !defined(__clang__)
# define string_COMPILER_MSVC_VER           (_MSC_VER )
# define string_COMPILER_MSVC_VERSION       (_MSC_VER / 10 - 10 * ( 5 + (_MSC_VER < 1900 ) ) )
# define string_COMPILER_MSVC_VERSION_FULL  (_MSC_VER - 100 * ( 5 + (_MSC_VER < 1900 ) ) )
#else
# define string_COMPILER_MSVC_VER           0
# define string_COMPILER_MSVC_VERSION       0
# define string_COMPILER_MSVC_VERSION_FULL  0
#endif

// clang version:

#define string_COMPILER_VERSION( major, minor, patch ) ( 10 * ( 10 * (major) + (minor) ) + (patch) )

#if defined( __apple_build_version__ )
# define string_COMPILER_APPLECLANG_VERSION string_COMPILER_VERSION( __clang_major__, __clang_minor__, __clang_patchlevel__ )
# define string_COMPILER_CLANG_VERSION 0
#elif defined( __clang__ )
# define string_COMPILER_APPLECLANG_VERSION 0
# define string_COMPILER_CLANG_VERSION string_COMPILER_VERSION( __clang_major__, __clang_minor__, __clang_patchlevel__ )
#else
# define string_COMPILER_APPLECLANG_VERSION 0
# define string_COMPILER_CLANG_VERSION 0
#endif

// GNUC version:

#if defined(__GNUC__) && !defined(__clang__)
# define string_COMPILER_GNUC_VERSION string_COMPILER_VERSION( __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ )
#else
# define string_COMPILER_GNUC_VERSION 0
#endif

// half-open range [lo..hi):
#define string_BETWEEN( v, lo, hi ) ( (lo) <= (v) && (v) < (hi) )

// Presence of language and library features:

#define string_CPP11_100  (string_CPP11_OR_GREATER || string_COMPILER_MSVC_VER >= 1600)
#define string_CPP11_110  (string_CPP11_OR_GREATER || string_COMPILER_MSVC_VER >= 1700)
#define string_CPP11_120  (string_CPP11_OR_GREATER || string_COMPILER_MSVC_VER >= 1800)
#define string_CPP11_140  (string_CPP11_OR_GREATER || string_COMPILER_MSVC_VER >= 1900)
#define string_CPP11_141  (string_CPP11_OR_GREATER || string_COMPILER_MSVC_VER >= 1910)

#define string_CPP11_000  (string_CPP11_OR_GREATER)

#define string_CPP14_000  (string_CPP14_OR_GREATER)
#define string_CPP14_120  (string_CPP14_OR_GREATER || string_COMPILER_MSVC_VER >= 1800)

#define string_CPP17_000  (string_CPP17_OR_GREATER)
#define string_CPP17_120  (string_CPP17_OR_GREATER || string_COMPILER_MSVC_VER >= 1800)
#define string_CPP17_140  (string_CPP17_OR_GREATER || string_COMPILER_MSVC_VER >= 1900)

#define string_CPP20_000  (string_CPP20_OR_GREATER)

// Presence of C++11 language features:

#define string_HAVE_FREE_BEGIN              string_CPP11_120
#define string_HAVE_CONSTEXPR_11           (string_CPP11_000 && !string_BETWEEN(string_COMPILER_MSVC_VER, 1, 1910))
#define string_HAVE_NOEXCEPT                string_CPP11_140
#define string_HAVE_NULLPTR                 string_CPP11_100
#define string_HAVE_DEFAULT_FN_TPL_ARGS     string_CPP11_120
#define string_HAVE_EXPLICIT_CONVERSION     string_CPP11_120
#define string_HAVE_CHAR16_T                string_CPP11_000
#define string_HAVE_CHAR32_T                string_HAVE_CHAR16_T

// Presence of C++14 language features:

#define string_HAVE_CONSTEXPR_14            string_CPP14_000
#define string_HAVE_FREE_CBEGIN             string_CPP14_120

// Presence of C++17 language features:

#define string_HAVE_FREE_SIZE               string_CPP17_140
#define string_HAVE_NODISCARD               string_CPP17_000
#define string_HAVE_STRING_VIEW             string_CPP17_000

// Presence of C++20 language features:

#define string_HAVE_CHAR8_T                 string_CPP20_000

// Presence of C++ library features:

#define string_HAVE_REGEX                  (string_CPP11_000 && !string_BETWEEN(string_COMPILER_GNUC_VERSION, 1, 490))
#define string_HAVE_TYPE_TRAITS             string_CPP11_110

// Usage of C++ language features:

#if string_HAVE_CONSTEXPR_11
# define string_constexpr constexpr
#else
# define string_constexpr /*constexpr*/
#endif

#if string_HAVE_CONSTEXPR_14
# define string_constexpr14  constexpr
#else
# define string_constexpr14  /*constexpr*/
#endif

#if string_HAVE_EXPLICIT_CONVERSION
# define string_explicit  explicit
#else
# define string_explicit  /*explicit*/
#endif

#if string_HAVE_NOEXCEPT && !string_CONFIG_NO_EXCEPTIONS
# define string_noexcept noexcept
#else
# define string_noexcept /*noexcept*/
#endif

#if string_HAVE_NODISCARD
# define string_nodiscard [[nodiscard]]
#else
# define string_nodiscard /*[[nodiscard]]*/
#endif

#if string_HAVE_NULLPTR
# define string_nullptr nullptr
#else
# define string_nullptr NULL
#endif

#if string_HAVE_EXPLICIT_CONVERSION
# define string_explicit_cv explicit
#else
# define string_explicit_cv /*explicit*/
#endif

// Additional includes:

#include <cassert>

#include <algorithm>    // std::transform()
#include <iterator>
#include <locale>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#if string_HAVE_STRING_VIEW
# include <string_view>
#else
// # pragma message("string.hpp: Using internal nonstd::std17::basic_string_view<CharT>.")
#endif

#if string_CONFIG_PROVIDE_REGEX && string_HAVE_REGEX
# include <regex>
#endif

namespace nonstd {

//
// string details:
//

namespace string {

// npos

#if string_HAVE_STRING_VIEW
    static string_constexpr std::size_t npos = std::string_view::npos;
#elif string_CPP17_OR_GREATER
    static string_constexpr std::size_t npos = std::string::npos;
#elif string_CPP11_OR_GREATER
    enum : std::size_t { npos = std::string::npos };
#else
    enum { npos = std::string::npos };
#endif  // string_HAVE_STRING_VIEW

namespace detail {

template< typename T >
string_constexpr std::size_t to_size_t(T value) string_noexcept
{
    return static_cast<std::size_t>( value );
}

} // namespace detail

namespace std14 {
} // namespace std14

namespace std17 {

#if string_HAVE_STRING_VIEW

using std::basic_string_view;

# if string_CONFIG_PROVIDE_CHAR_T
    using std::string_view;
# endif
# if string_CONFIG_PROVIDE_WCHAR_T
    using std::wstring_view;
# endif
# if string_CONFIG_PROVIDE_CHAR8_T
    using std::u8string_view;
# endif
# if string_CONFIG_PROVIDE_CHAR16_T
    using std::u16string_view;
# endif
# if string_CONFIG_PROVIDE_CHAR32_T
    using std::u32string_view;
# endif

#else // string_HAVE_STRING_VIEW

// Local basic_string_view.

template
<
    class CharT,
    class Traits = std::char_traits<CharT>
>
class basic_string_view
{
public:
    // Member types:

    typedef Traits traits_type;
    typedef CharT  value_type;

    typedef CharT       * pointer;
    typedef CharT const * const_pointer;
    typedef CharT       & reference;
    typedef CharT const & const_reference;

    typedef const_pointer iterator;
    typedef const_pointer const_iterator;
    typedef std::reverse_iterator< const_iterator > reverse_iterator;
    typedef	std::reverse_iterator< const_iterator > const_reverse_iterator;

    typedef std::size_t     size_type;
    typedef std::ptrdiff_t  difference_type;

    // Constants:

#if string_CPP17_OR_GREATER
    static string_constexpr std::size_t npos = string::npos;
#elif string_CPP11_OR_GREATER
    enum : std::size_t { npos = string::npos };
#else
    enum { npos = string::npos };
#endif

    // 24.4.2.1 Construction and assignment:

    string_constexpr basic_string_view() string_noexcept
        : data_( string_nullptr )
        , size_( 0 )
    {}

    string_constexpr basic_string_view( CharT const * s ) string_noexcept // non-standard noexcept
        : data_( s )
        , size_( Traits::length(s) )
    {}

    string_constexpr basic_string_view( CharT const * s, size_type count ) string_noexcept // non-standard noexcept
        : data_( s )
        , size_( count )
    {}

    string_constexpr basic_string_view( CharT const * b, CharT const * e ) string_noexcept // C++20, non-standard noexcept
        : data_( b )
        , size_( detail::to_size_t(e - b) )
    {}

    string_constexpr basic_string_view( std::basic_string<CharT> & s ) string_noexcept // non-standard noexcept
        : data_( s.data() )
        , size_( s.size() )
    {}

    string_constexpr basic_string_view( std::basic_string<CharT> const & s ) string_noexcept // non-standard noexcept
        : data_( s.data() )
        , size_( s.size() )
    {}

// #if string_HAVE_EXPLICIT_CONVERSION

    template< class Allocator >
    string_nodiscard string_explicit operator std::basic_string<CharT, Traits, Allocator>() const
    {
        return to_string( Allocator() );
    }

// #endif // string_HAVE_EXPLICIT_CONVERSION

#if string_CPP11_OR_GREATER

    template< class Allocator = std::allocator<CharT> >
    string_nodiscard std::basic_string<CharT, Traits, Allocator>
    to_string( Allocator const & a = Allocator() ) const
    {
        return std::basic_string<CharT, Traits, Allocator>( begin(), end(), a );
    }

#else

    string_nodiscard std::basic_string<CharT, Traits>
    to_string() const
    {
        return std::basic_string<CharT, Traits>( begin(), end() );
    }

    template< class Allocator >
    string_nodiscard std::basic_string<CharT, Traits, Allocator>
    to_string( Allocator const & a ) const
    {
        return std::basic_string<CharT, Traits, Allocator>( begin(), end(), a );
    }

#endif // string_CPP11_OR_GREATER

    string_nodiscard string_constexpr14 size_type find( basic_string_view v, size_type pos = 0 ) const string_noexcept  // (1)
    {
        return assert( v.size() == 0 || v.data() != string_nullptr )
            , pos >= size()
            ? npos
            : to_pos( std::search( cbegin() + pos, cend(), v.cbegin(), v.cend(), Traits::eq ) );
    }

    string_nodiscard string_constexpr14 size_type rfind( basic_string_view v, size_type pos = npos ) const string_noexcept  // (1)
    {
        if ( size() < v.size() )
        {
            return npos;
        }

        if ( v.empty() )
        {
            return (std::min)( size(), pos );
        }

        const_iterator last   = cbegin() + (std::min)( size() - v.size(), pos ) + v.size();
        const_iterator result = std::find_end( cbegin(), last, v.cbegin(), v.cend(), Traits::eq );

        return result != last ? size_type( result - cbegin() ) : npos;
    }

    string_nodiscard string_constexpr14 size_type find( CharT c, size_type pos = 0 ) const string_noexcept  // (2)
    {
        return find( basic_string_view( &c, 1 ), pos );
    }

    string_nodiscard string_constexpr size_type find_first_of( basic_string_view v, size_type pos = 0 ) const string_noexcept  // (1)
    {
        return pos >= size()
            ? npos
            : to_pos( std::find_first_of( cbegin() + pos, cend(), v.cbegin(), v.cend(), Traits::eq ) );
    }

    string_nodiscard string_constexpr size_type find_first_of( CharT c, size_type pos = 0 ) const string_noexcept  // (2)
    {
        return find_first_of( basic_string_view( &c, 1 ), pos );
    }

    string_nodiscard string_constexpr size_type find_last_of( basic_string_view v, size_type pos = npos ) const string_noexcept  // (1)
    {
        return empty()
            ? npos
            : pos >= size()
            ? find_last_of( v, size() - 1 )
            : to_pos( std::find_first_of( const_reverse_iterator( cbegin() + pos + 1 ), crend(), v.cbegin(), v.cend(), Traits::eq ) );
    }

    string_nodiscard string_constexpr size_type find_first_not_of( basic_string_view v, size_type pos = 0 ) const string_noexcept  // (1)
    {
        return pos >= size()
            ? npos
            : to_pos( std::find_if( cbegin() + pos, cend(), not_in_view( v ) ) );
    }

    string_nodiscard string_constexpr size_type find_last_not_of( basic_string_view v, size_type pos = npos ) const string_noexcept  // (1)
    {
        return empty()
            ? npos
            : pos >= size()
            ? find_last_not_of( v, size() - 1 )
            : to_pos( std::find_if( const_reverse_iterator( cbegin() + pos + 1 ), crend(), not_in_view( v ) ) );
    }

    string_nodiscard string_constexpr14 int compare( basic_string_view v ) const string_noexcept
    {
        auto const result = Traits::compare( data(), v.data(), (std::min)(size(), v.size()) );

        return result != 0
            ? result
            : size() == v.size() ? 0 : size() < v.size() ? -1 : +1;
    }

    string_nodiscard string_constexpr bool          empty()  const string_noexcept { return size_ == 0; }
    string_nodiscard string_constexpr size_type     size()   const string_noexcept { return size_; }
    string_nodiscard string_constexpr size_type     length() const string_noexcept { return size_; }
    string_nodiscard string_constexpr const_pointer data()   const string_noexcept { return data_; }

    string_nodiscard string_constexpr14 basic_string_view substr( size_type pos = 0, size_type n = npos ) const
    {
#if string_CONFIG_NO_EXCEPTIONS
        assert( pos <= size() );
#else
        if ( pos > size() )
        {
            throw std::out_of_range("string_view::substr()");
        }
#endif
        return basic_string_view( data() + pos, n == npos? size() - pos : (std::min)( n, size() - pos ) );
    }

    string_nodiscard string_constexpr const_iterator begin()  const string_noexcept { return data_;         }
    string_nodiscard string_constexpr const_iterator end()    const string_noexcept { return data_ + size_; }

    string_nodiscard string_constexpr const_iterator cbegin() const string_noexcept { return begin(); }
    string_nodiscard string_constexpr const_iterator cend()   const string_noexcept { return end();   }

    string_nodiscard string_constexpr const_reverse_iterator rbegin()  const string_noexcept { return const_reverse_iterator( end() );   }
    string_nodiscard string_constexpr const_reverse_iterator rend()    const string_noexcept { return const_reverse_iterator( begin() ); }

    string_nodiscard string_constexpr const_reverse_iterator crbegin() const string_noexcept { return rbegin(); }
    string_nodiscard string_constexpr const_reverse_iterator crend()   const string_noexcept { return rend();   }

private:
    struct not_in_view
    {
        const basic_string_view v;

        string_constexpr explicit not_in_view( basic_string_view v_ ) : v( v_ ) {}

        string_nodiscard string_constexpr bool operator()( CharT c ) const
        {
            return npos == v.find_first_of( c );
        }
    };

    string_nodiscard string_constexpr size_type to_pos( const_iterator it ) const
    {
        return it == cend() ? npos : size_type( it - cbegin() );
    }

    string_nodiscard string_constexpr size_type to_pos( const_reverse_iterator it ) const
    {
        return it == crend() ? npos : size_type( crend() - it - 1 );
    }

private:
    const_pointer data_;
    size_type     size_;
};

template< class CharT, class Traits >
string_nodiscard string_constexpr bool
operator==( basic_string_view<CharT,Traits> lhs, basic_string_view<CharT,Traits> rhs ) string_noexcept
{
    return lhs.compare( rhs ) == 0;
}

template< class CharT, class Traits >
string_nodiscard string_constexpr bool
operator!=( basic_string_view<CharT,Traits> lhs, basic_string_view<CharT,Traits> rhs ) string_noexcept
{
    return lhs.compare( rhs ) != 0;
}

template< class CharT, class Traits >
string_nodiscard string_constexpr bool
operator<( basic_string_view<CharT,Traits> lhs, basic_string_view<CharT,Traits> rhs ) string_noexcept
{
    return lhs.compare( rhs ) < 0;
}

template< class CharT, class Traits >
string_nodiscard string_constexpr bool
operator<=( basic_string_view<CharT,Traits> lhs, basic_string_view<CharT,Traits> rhs ) string_noexcept
{
    return lhs.compare( rhs ) <= 0;
}

template< class CharT, class Traits >
string_nodiscard string_constexpr bool
operator>( basic_string_view<CharT,Traits> lhs, basic_string_view<CharT,Traits> rhs ) string_noexcept
{
    return lhs.compare( rhs ) > 0;
}

template< class CharT, class Traits >
string_nodiscard string_constexpr bool
operator>=( basic_string_view<CharT,Traits> lhs, basic_string_view<CharT,Traits> rhs ) string_noexcept
{
    return compare( lhs, rhs ) >= 0;
}

#if string_CONFIG_PROVIDE_CHAR_T
typedef basic_string_view<char>     string_view;
#endif

#if string_CONFIG_PROVIDE_WCHAR_T
typedef basic_string_view<wchar_t>  wstring_view;
#endif

#if string_CONFIG_PROVIDE_CHAR8_T && string_HAVE_CHAR8_T
typedef basic_string_view<char8_t>  u8string_view;
#endif

#if string_CONFIG_PROVIDE_CHAR16_T
typedef basic_string_view<char16_t>  u16string_view;
#endif

#if string_CONFIG_PROVIDE_CHAR32_T
typedef basic_string_view<char32_t>  u32string_view;
#endif

template< typename CharT >
string_nodiscard inline std::size_t size( basic_string_view<CharT> const & sv ) string_noexcept
{
    return sv.size();
}

#endif // string_HAVE_STRING_VIEW

} // namespace std17

namespace std20 {

// type identity, to establish non-deduced contexts in template argument deduction:

template< typename T >
struct type_identity
{
    typedef T type;
};

} // namespace std20

namespace std23 {
} // namespace std23

namespace detail {

//
// Utilities:
//

// null character:

template< typename CharT >
string_nodiscard string_constexpr CharT nullchr() string_noexcept
{
    return 0;
}

// null C-string:

#if string_CONFIG_PROVIDE_CHAR_T
string_nodiscard inline string_constexpr char const * nullstr( char ) string_noexcept
{
    return "";
}
#endif

#if string_CONFIG_PROVIDE_WCHAR_T
string_nodiscard inline string_constexpr wchar_t const * nullstr( wchar_t ) string_noexcept
{
    return L"";
}
#endif

#if string_CONFIG_PROVIDE_CHAR8_T
string_nodiscard inline string_constexpr char8_t const * nullstr( char8_t ) string_noexcept
{
    return u8"";
}
#endif

#if string_CONFIG_PROVIDE_CHAR16_T
string_nodiscard inline string_constexpr char16_t const * nullstr( char16_t ) string_noexcept
{
    return u"";
}
#endif

#if string_CONFIG_PROVIDE_CHAR32_T
string_nodiscard inline string_constexpr char32_t const * nullstr( char32_t ) string_noexcept
{
    return U"";
}
#endif

// default strip set:

#if string_CONFIG_PROVIDE_CHAR_T
string_nodiscard inline string_constexpr char const * default_strip_set( char ) string_noexcept
{
    return " \t\n";
}
#endif

#if string_CONFIG_PROVIDE_WCHAR_T
string_nodiscard inline string_constexpr wchar_t const * default_strip_set( wchar_t ) string_noexcept
{
    return L" \t\n";
}
#endif

#if string_CONFIG_PROVIDE_CHAR8_T
string_nodiscard inline string_constexpr char8_t const * default_strip_set( char8_t ) string_noexcept
{
    return u8" \t\n";
}
#endif

#if string_CONFIG_PROVIDE_CHAR16_T
string_nodiscard inline string_constexpr char16_t const * default_strip_set( char16_t ) string_noexcept
{
    return u" \t\n";
}
#endif

#if string_CONFIG_PROVIDE_CHAR32_T
string_nodiscard inline string_constexpr char32_t const * default_strip_set( char32_t ) string_noexcept
{
    return U" \t\n";
}
#endif

// to_string(sv):

#if string_HAVE_STRING_VIEW
    #define MK_DETAIL_TO_STRING_SV(T)                           \
    string_nodiscard inline std::basic_string<T>                \
    to_string( std17::basic_string_view<T> sv )                 \
    {                                                           \
        return std::basic_string<T>( sv );                      \
    }
#else
    #define MK_DETAIL_TO_STRING_SV(T)                           \
    string_nodiscard inline std::basic_string<T>                \
    to_string( std17::basic_string_view<T> sv )                 \
    {                                                           \
        return std::basic_string<T>( sv.begin(), sv.end() );    \
    }
#endif

#if string_CONFIG_PROVIDE_CHAR_T
    MK_DETAIL_TO_STRING_SV( char )
#endif
#if string_CONFIG_PROVIDE_WCHAR_T
    MK_DETAIL_TO_STRING_SV( wchar_t )
#endif
#if string_CONFIG_PROVIDE_CHAR8_T
    MK_DETAIL_TO_STRING_SV( char8_t )
#endif
#if string_CONFIG_PROVIDE_CHAR16_T
    MK_DETAIL_TO_STRING_SV( char16_t )
#endif
#if string_CONFIG_PROVIDE_CHAR32_T
    MK_DETAIL_TO_STRING_SV( char32_t )
#endif

#undef MK_DETAIL_TO_STRING_SV

}  // namespace detail
}  // namespace string

// enable use of string-specific namespaces detail and stdxx:

using namespace string;

//
// Observers
//

// length()

#define string_MK_LENGTH(T)                                         \
    string_nodiscard inline string_constexpr std::size_t            \
    length( std17::basic_string_view<T> text ) string_noexcept      \
    {                                                               \
        return text.length();                                       \
    }

// size()

#define string_MK_SIZE(T)                                           \
    string_nodiscard inline string_constexpr std::size_t            \
    size( std17::basic_string_view<T> text ) string_noexcept        \
    {                                                               \
        return text.size();                                         \
    }

// is_empty()

#define string_MK_IS_EMPTY(T)                                       \
    string_nodiscard inline string_constexpr bool                   \
    is_empty( std17::basic_string_view<T> text ) string_noexcept    \
{                                                                   \
        return text.empty();                                        \
    }

//
// Searching:
//

namespace string {
namespace detail {

template< typename CharT, typename SeekT >
string_nodiscard std::size_t
find_first(
    std17::basic_string_view<CharT> text
    , SeekT const & seek, std::size_t pos ) string_noexcept
{
    return text.find( seek, pos );
}

} // namespace detail
} // namespace string

// find_first()

#define string_MK_FIND_FIRST(CharT)                 \
    template< typename SeekT >                      \
    string_nodiscard std::size_t                    \
    find_first(                                     \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek ) string_noexcept      \
    {                                               \
        return text.find( seek );                   \
    }

#if string_CPP17_000
# define string_MK_FIND_FIRST_CHAR(CharT)           \
    string_nodiscard inline std::size_t             \
    find_first(                                     \
        std17::basic_string_view<CharT> text        \
        , CharT seek ) string_noexcept              \
    {                                               \
        return find_first( text, std::basic_string<CharT>( &seek, &seek + 1 ) ); \
    }
#else
# define string_MK_FIND_FIRST_CHAR(CharT)           \
    string_nodiscard inline std::size_t             \
    find_first(                                     \
        std17::basic_string_view<CharT> text        \
        , CharT seek ) string_noexcept              \
    {                                               \
        return find_first( text, std17::basic_string_view<CharT>( &seek, &seek + 1 ) ); \
    }
#endif

// find_last()

#define string_MK_FIND_LAST(CharT)                  \
    template< typename SeekT >                      \
    string_nodiscard std::size_t                    \
    find_last(                                      \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek ) string_noexcept      \
    {                                               \
        return text.rfind( seek );                  \
    }

#if string_CPP17_000
# define string_MK_FIND_LAST_CHAR(CharT)            \
    string_nodiscard inline std::size_t             \
    find_last(                                      \
        std17::basic_string_view<CharT> text        \
        , CharT seek )                              \
    {                                               \
        return find_last( text, std::basic_string<CharT>( &seek, &seek + 1 ) ); \
    }
#else
# define string_MK_FIND_LAST_CHAR(CharT)            \
    string_nodiscard inline std::size_t             \
    find_last(                                      \
        std17::basic_string_view<CharT> text        \
        , CharT seek )                              \
    {                                               \
        return find_last( text, std17::basic_string_view<CharT>( &seek, &seek + 1 ) );  \
    }
#endif

// find_first_of()

#define string_MK_FIND_FIRST_OF(CharT)              \
    template< typename SeekT >                      \
    string_nodiscard std::size_t                    \
    find_first_of(                                  \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        return text.find_first_of( seek );          \
    }

// find_last_of()

#define string_MK_FIND_LAST_OF(CharT)               \
    template< typename SeekT >                      \
    string_nodiscard std::size_t                    \
    find_last_of(                                   \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        return text.find_last_of( seek );           \
    }

// find_first_not_of()

#define string_MK_FIND_FIRST_NOT_OF(CharT)          \
    template< typename SeekT >                      \
    string_nodiscard std::size_t                    \
    find_first_not_of(                              \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        return text.find_first_not_of( seek );      \
    }

// find_last_not_of()

#define string_MK_FIND_LAST_NOT_OF(CharT)           \
    template< typename SeekT >                      \
    string_nodiscard std::size_t                    \
    find_last_not_of(                               \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        return text.find_last_not_of( seek );       \
    }

// TODO: ??? find_if()

// TODO: ??? find_if_not()

// contains() - C++23

#if string_CPP23_OR_GREATER
# define string_MK_CONTAINS(CharT)                  \
    template< typename SeekT >                      \
    string_nodiscard bool                           \
    contains(                                       \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek ) string_noexcept      \
{                                                   \
        return text.contains( seek );               \
    }
#else
# define string_MK_CONTAINS(CharT)                  \
    template< typename SeekT >                      \
    string_nodiscard bool                           \
    contains(                                       \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek ) string_noexcept      \
    {                                               \
        return string::npos != find_first(text, seek);  \
    }
#endif

// contains_all_of()

# define string_MK_CONTAINS_ALL_OF(CharT)           \
    string_nodiscard inline bool                    \
    contains_all_of(                                \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        for ( auto const chr : set )                \
        {                                           \
            if ( ! contains( text, chr ) )          \
                return false;                       \
        }                                           \
        return true;                                \
    }

// contains_any_of()

# define string_MK_CONTAINS_ANY_OF(CharT)           \
    string_nodiscard inline bool                    \
    contains_any_of(                                \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        for ( auto const chr : set )                \
        {                                           \
            if ( contains( text, chr ) )            \
                return true;                        \
        }                                           \
        return false;                               \
    }

// contains_none_of()

# define string_MK_CONTAINS_NONE_OF(CharT)          \
    string_nodiscard inline bool                    \
    contains_none_of(                               \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        return ! contains_any_of( text, set );      \
    }

// starts_with() - C++20

#if string_CPP20_OR_GREATER
# define string_MK_STARTS_WITH(CharT)               \
    template< typename SeekT >                      \
    string_nodiscard bool                           \
    starts_with(                                    \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        return text.starts_with( seek );            \
    }
#else
# define string_MK_STARTS_WITH(CharT)               \
    template< typename SeekT >                      \
    string_nodiscard bool                           \
    starts_with(                                    \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        const std17::basic_string_view<CharT> look( seek );     \
                                                    \
        if ( size( look ) > size( text ) )          \
        {                                           \
            return false;                           \
        }                                           \
        return std::equal( look.begin(), look.end(), text.begin() );    \
    }
#endif  // string_CPP20_OR_GREATER

#if string_CPP17_000
# define string_MK_STARTS_WITH_CHAR(CharT)          \
    string_nodiscard inline bool                    \
    starts_with(                                    \
        std17::basic_string_view<CharT> text        \
        , CharT seek )                              \
    {                                               \
        return starts_with( text, std::basic_string<CharT>( &seek, &seek + 1) );    \
    }
#else
# define string_MK_STARTS_WITH_CHAR(CharT)          \
    string_nodiscard inline bool                    \
    starts_with(                                    \
        std17::basic_string_view<CharT> text        \
        , CharT seek )                              \
    {                                               \
        return starts_with( text, std17::basic_string_view<CharT>( &seek, &seek + 1) ); \
    }
#endif  // string_CPP17_000

// starts_with_all_of()

# define string_MK_STARTS_WITH_ALL_OF(CharT)        \
    string_nodiscard inline bool                    \
    starts_with_all_of(                             \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        if ( text.empty() )                         \
            return false;                           \
                                                    \
        std::basic_string<CharT> result;            \
                                                    \
        for ( auto const chr : text )               \
        {                                           \
            if ( !contains( set, chr ) )            \
                break;                              \
            if ( !contains( result, chr ) )         \
                result.append( 1, chr );            \
        }                                           \
        return contains_all_of( result, set );      \
    }

// starts_with_any_of()

# define string_MK_STARTS_WITH_ANY_OF(CharT)        \
    string_nodiscard inline bool                    \
    starts_with_any_of(                             \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        if ( text.empty() )                         \
            return false;                           \
                                                    \
        return contains( set, *text.cbegin() );     \
    }

// starts_with_none_of()

# define string_MK_STARTS_WITH_NONE_OF(CharT)       \
    string_nodiscard inline bool                    \
    starts_with_none_of(                            \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        return ! starts_with_any_of( text, set );   \
    }

// ends_with() - C++20

#if string_CPP20_OR_GREATER
# define string_MK_ENDS_WITH(CharT)                 \
    template< typename SeekT >                      \
    string_nodiscard bool                           \
    ends_with( std17::basic_string_view<CharT> text, SeekT const & seek )   \
    {                                               \
        return text.ends_with( seek );              \
    }
#else
# define string_MK_ENDS_WITH(CharT)                 \
    template< typename SeekT >                      \
    string_nodiscard bool                           \
    ends_with(                                      \
        std17::basic_string_view<CharT> text        \
        , SeekT const & seek )                      \
    {                                               \
        const std17::basic_string_view<CharT> look( seek ); \
                                                    \
        if ( size( look ) > size( text ) )          \
        {                                           \
            return false;                           \
        }                                           \
        return std::equal( look.rbegin(), look.rend(), text.rbegin() ); \
    }
#endif

#if string_CPP17_000
# define string_MK_ENDS_WITH_CHAR(CharT)            \
    string_nodiscard inline bool                    \
    ends_with(                                      \
        std17::basic_string_view<CharT> text        \
        , CharT seek )                              \
    {                                               \
        return ends_with( text, std::basic_string<CharT>( &seek, &seek + 1) );  \
    }
#else
# define string_MK_ENDS_WITH_CHAR(CharT)            \
    string_nodiscard inline bool                    \
    ends_with(                                      \
        std17::basic_string_view<CharT> text        \
        , CharT seek )                              \
    {                                               \
        return ends_with( text, std17::basic_string_view<CharT>( &seek, &seek + 1) );   \
    }
#endif

// ends_with_all_of()

# define string_MK_ENDS_WITH_ALL_OF(CharT)          \
    string_nodiscard inline bool                    \
    ends_with_all_of(                               \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        if ( text.empty() )                         \
            return false;                           \
                                                    \
        std::basic_string<CharT> result;            \
                                                    \
        for ( auto it = text.crbegin(); it != text.crend(); ++it )  \
        {                                           \
            auto const chr = *it;                   \
            if ( !contains( set, chr ) )            \
                break;                              \
            if ( !contains( result, chr ) )         \
                result.append( 1, chr );            \
        }                                           \
        return contains_all_of( result, set );      \
    }

// ends_with_any_of()

# define string_MK_ENDS_WITH_ANY_OF(CharT)          \
    string_nodiscard inline bool                    \
    ends_with_any_of(                               \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        if ( text.empty() )                         \
            return false;                           \
                                                    \
        return contains( set, *text.crbegin() );    \
    }

// ends_with_none_of()

# define string_MK_ENDS_WITH_NONE_OF(CharT)         \
    string_nodiscard inline bool                    \
    ends_with_none_of(                              \
        std17::basic_string_view<CharT> text        \
        , std17::basic_string_view<CharT> set )     \
    {                                               \
        return !ends_with_any_of( text, set );      \
    }

//
// Modifiers:
//

namespace string {
namespace detail {

// Transform case (character):

template< typename CharT >
string_nodiscard CharT to_lowercase( CharT chr )
{
    return std::tolower( chr, std::locale() );
}

template< typename CharT >
string_nodiscard CharT to_uppercase( CharT chr )
{
    return std::toupper( chr, std::locale() );
}

// Transform case; serve both CharT* and StringT&:

template< typename CharT, typename Fn >
string_nodiscard std::basic_string<CharT> to_case( std::basic_string<CharT> text, Fn fn )
{
    std::transform(
        std::begin( text ), std::end( text )
        , std::begin( text )
        , fn
    );
    return text;
}

} // namespace detail
} // namespace string

// capitalize():

#define string_MK_CAPITALIZE(CharT)                             \
    string_nodiscard inline std::basic_string<CharT>            \
    capitalize( std17::basic_string_view<CharT> text )          \
    {                                                           \
        if ( text.empty() )                                     \
            return {};                                          \
                                                                \
        std::basic_string<CharT> result{ to_string( text ) };   \
        result[0] = to_uppercase( result[0] );                  \
                                                                \
        return result;                                          \
    }

// to_lowercase(), to_uppercase()

#define string_MK_TO_CASE_CHAR(CharT, Function)                 \
    string_nodiscard inline CharT to_ ## Function( CharT chr )  \
    {                                                           \
        return detail::to_ ## Function<CharT>( chr );           \
    }

#define string_MK_TO_CASE_STRING(CharT, Function)               \
    string_nodiscard inline std::basic_string<CharT> to_ ## Function( std17::basic_string_view<CharT> text )    \
    {                                                           \
        return detail::to_case( std::basic_string<CharT>(text), detail::to_ ## Function<CharT> );   \
    }

// strip_left()

#define string_MK_STRIP_LEFT(CharT)                                                         \
    string_nodiscard inline std::basic_string<CharT>                                        \
    strip_left(                                                                             \
        std17::basic_string_view<CharT> text                                                \
        , std17::basic_string_view<CharT> set = detail::default_strip_set(CharT{}) )        \
    {                                                                                       \
        return std::basic_string<CharT>( text ).erase( 0, text.find_first_not_of( set ) );  \
    }

// strip_right()

#define string_MK_STRIP_RIGHT(CharT)                                                        \
    string_nodiscard inline std::basic_string<CharT>                                        \
    strip_right(                                                                            \
        std17::basic_string_view<CharT> text                                                \
        , std17::basic_string_view<CharT> set = detail::default_strip_set(CharT{}) )        \
    {                                                                                       \
        return std::basic_string<CharT>( text ).erase( text.find_last_not_of( set ) + 1 );  \
    }

// strip()

#define string_MK_STRIP(CharT)                                                              \
    string_nodiscard inline std::basic_string<CharT>                                        \
    strip(                                                                                  \
        std17::basic_string_view<CharT> text                                                \
        , std17::basic_string_view<CharT> set = detail::default_strip_set(CharT{}) )        \
    {                                                                                       \
        return strip_left( strip_right( text, set ), set );                                 \
    }

// erase_all()

namespace string {
namespace detail {

template< typename CharT >
string_nodiscard std::basic_string<CharT>
erase_all( std17::basic_string_view<CharT> text, std17::basic_string_view<CharT> what )
{
    std::basic_string<CharT> result( text );

    for ( auto pos = detail::find_first<CharT>( result, what, 0 ) ;; )
    {
        pos = detail::find_first<CharT>( result, what, pos );

        if ( pos == std::basic_string<CharT>::npos )
            break;

        result.erase( pos, what.length() );
    }
    return result;
}

} // detail
} // namespace string

// erase()

#define string_MK_ERASE(CharT)                          \
    string_nodiscard inline std::basic_string<CharT>    \
    erase(                                              \
        std17::basic_string_view<CharT> text            \
        , std::size_t pos                               \
        , std::size_t len = npos )                      \
        {                                               \
            return to_string( text ).erase( pos, len ); \
        }

// erase_all()

#define string_MK_ERASE_ALL(CharT)                      \
    string_nodiscard inline std::basic_string<CharT>    \
    erase_all(                                          \
        std17::basic_string_view<CharT> text            \
        , std17::basic_string_view<CharT> what )        \
    {                                                   \
        return detail::erase_all( text, what );         \
    }

// erase_first()

#define string_MK_ERASE_FIRST(CharT)                    \
    string_nodiscard inline std::basic_string<CharT>    \
    erase_first(                                        \
        std17::basic_string_view<CharT> text            \
        , std17::basic_string_view<CharT> what )        \
    {                                                   \
        std::basic_string<CharT> result( text );        \
                                                        \
        const auto pos = find_first( result, what );    \
                                                        \
        return pos != std::basic_string<CharT>::npos    \
            ? result.erase( pos, what.length() )        \
            : result;                                   \
    }

// erase_last()

#define string_MK_ERASE_LAST(CharT)                     \
    string_nodiscard inline std::basic_string<CharT>    \
    erase_last(                                         \
        std17::basic_string_view<CharT> text            \
        , std17::basic_string_view<CharT> what )        \
    {                                                   \
        std::basic_string<CharT> result( text );        \
                                                        \
        const auto pos = find_last( result, what );     \
                                                        \
        return pos != std::basic_string<CharT>::npos    \
            ? result.erase( pos, what.length() )        \
            : result;                                   \
    }

// insert()

#define string_MK_INSERT(CharT)                         \
    string_nodiscard inline std::basic_string<CharT>    \
    insert(                                             \
        std17::basic_string_view<CharT> text            \
        , std::size_t pos                               \
        , std17::basic_string_view<CharT> what )        \
        {                                               \
            return                                      \
                to_string( text.substr(0, pos ) )       \
                + to_string( what )                     \
                + to_string( text.substr(pos) );        \
        }

// replace_all()

namespace string {
namespace detail {

template< typename CharT >
string_nodiscard std::basic_string<CharT>
replace_all(
    std17::basic_string_view<CharT> text
    , std17::basic_string_view<CharT> what
    , std17::basic_string_view<CharT> with )
{
    std::basic_string<CharT> result( text );

    if ( with == what )
        return result;

    for ( auto pos = detail::find_first<CharT>( result, what, 0 ) ;; )
    {
        pos = detail::find_first<CharT>( result, what, pos );

        if ( pos == std::basic_string<CharT>::npos )
            break;

#if string_CPP17_OR_GREATER
        result.replace( pos, what.length(), with );
#else
        result.replace( pos, what.length(), std::basic_string<CharT>(with) );
#endif
    }
    return result;
}

} // detail
} // namespace string

// replace()

#define string_MK_REPLACE(CharT)                        \
    string_nodiscard inline std::basic_string<CharT>    \
    replace(                                            \
        std17::basic_string_view<CharT> text            \
        , std::size_t pos                               \
        , std::size_t len                               \
        , std17::basic_string_view<CharT> what )        \
        {                                               \
            return                                      \
                to_string( text.substr(0, pos ) )       \
                + to_string( what )                     \
                + to_string( text.substr(pos + len) );  \
        }

// replace_all()

#define string_MK_REPLACE_ALL(CharT)                    \
    string_nodiscard inline std::basic_string<CharT>    \
    replace_all(                                        \
        std17::basic_string_view<CharT> text            \
        , std17::basic_string_view<CharT> what          \
        , std17::basic_string_view<CharT> with )        \
    {                                                   \
        return detail::replace_all( text, what, with ); \
    }

// replace_first()

#define string_MK_REPLACE_FIRST(CharT)                  \
    string_nodiscard inline std::basic_string<CharT>    \
    replace_first(                                      \
        std17::basic_string_view<CharT> text            \
        , std17::basic_string_view<CharT> what          \
        , std17::basic_string_view<CharT> with )        \
    {                                                   \
        std::basic_string<CharT> result( text );        \
                                                        \
        const auto pos = find_first( result, what );    \
                                                        \
        return pos != std::basic_string<CharT>::npos    \
            ? result.replace( pos, what.length(), std::basic_string<CharT>(with) )  \
            : detail::nullstr(CharT{});                 \
    }

// replace_last()

#define string_MK_REPLACE_LAST(CharT)                   \
    string_nodiscard inline std::basic_string<CharT>    \
    replace_last(                                       \
        std17::basic_string_view<CharT> text            \
        , std17::basic_string_view<CharT> what          \
        , std17::basic_string_view<CharT> with )        \
    {                                                   \
        std::basic_string<CharT> result( text );        \
                                                        \
        const auto pos = find_last( result, what );     \
                                                        \
        return pos != std::basic_string<CharT>::npos    \
            ? result.replace( pos, what.length(), std::basic_string<CharT>(with) )  \
            : detail::nullstr(CharT{});                 \
    }

//
// Joining, splitting:
//

// append()

#if string_CPP17_OR_GREATER
# define string_MK_APPEND(CharT)                                            \
    template< typename TailT >                                              \
    string_nodiscard std::basic_string<CharT>                               \
    append( std17::basic_string_view<CharT> text, TailT const & tail )      \
    {                                                                       \
        return std::basic_string<CharT>( text ).append( tail );             \
    }
#else
# define string_MK_APPEND(CharT)                                            \
    template< typename TailT >                                              \
    string_nodiscard std::basic_string<CharT>                               \
    append( std17::basic_string_view<CharT> text, TailT const & tail )      \
    {                                                                       \
        return std::basic_string<CharT>( text ) + std::basic_string<CharT>(tail); \
    }
# endif

// substring()

#define string_MK_SUBSTRING(CharT)                                          \
    string_nodiscard inline std::basic_string<CharT>                        \
    substring(                                                              \
        std17::basic_string_view<CharT> text                                \
        , std::size_t pos = 0                                               \
        , std::size_t count = string::npos )                                \
    {                                                                       \
        return std::basic_string<CharT>( text ).substr( pos, count );       \
    }

// join()

#define string_MK_JOIN(CharT)                                               \
    template< typename Coll >                                               \
    string_nodiscard std::basic_string<CharT>                               \
    join( Coll const & coll, std17::basic_string_view<CharT> sep )          \
    {                                                                       \
        std::basic_string<CharT> result{};                                  \
        typename Coll::const_iterator const coll_begin = coll.cbegin();     \
        typename Coll::const_iterator const coll_end   = coll.cend();       \
        typename Coll::const_iterator pos = coll_begin;                     \
                                                                            \
        if ( pos != coll_end )                                              \
        {                                                                   \
            result = append( result, *pos++ );                              \
        }                                                                   \
                                                                            \
        for ( ; pos != coll_end; ++pos )                                    \
        {                                                                   \
            result = append( append( result,  sep ), *pos );                \
        }                                                                   \
                                                                            \
        return result;                                                      \
    }

// split():

namespace string {
namespace detail {

template< typename CharT >
string_nodiscard inline auto
split_left(
    std17::basic_string_view<CharT> text
    , std17::basic_string_view<CharT> set
    , std::size_t count = std::numeric_limits<std::size_t>::max() )
    -> std::tuple<std17::basic_string_view<CharT>, std17::basic_string_view<CharT>>
{
        auto const pos = text.find_first_of( set );

        if ( pos == npos )
            return { text, text };

        auto const n = (std::min)( count, text.substr( pos ).find_first_not_of( set ) );

        return { text.substr( 0, pos ), n != npos ? text.substr( pos + n ) : text.substr( text.size(), 0 ) };

        // Note: `text.substr( text.size(), 0 )` indicates empty and end of text, see `lhs.cend() == text.cend()` in detail::split().
}

template< typename CharT >
string_nodiscard inline auto
split_right(
    std17::basic_string_view<CharT> text
    , std17::basic_string_view<CharT> set
    , std::size_t count = std::numeric_limits<std::size_t>::max() )
    -> std::tuple<std17::basic_string_view<CharT>, std17::basic_string_view<CharT>>
{
        auto const pos = text.find_last_of( set );

        if ( pos == npos )
            return { text, text };

        auto const n = (std::min)( count, pos - text.substr( 0, pos ).find_last_not_of( set ) );

        return { text.substr( 0, pos - n + 1 ), text.substr( pos + 1 ) };
}

template< typename CharT >
string_nodiscard std::vector< std17::basic_string_view<CharT> >
split( std17::basic_string_view<CharT> text
    , std17::basic_string_view<CharT> set
    , std::size_t Nsplit )
{
    std::vector< std17::basic_string_view<CharT> > result;

    std17::basic_string_view<CharT> lhs = text;
    std17::basic_string_view<CharT> rhs;

    for( std::size_t cnt = 1; ; ++cnt )
    {
        if ( cnt >= Nsplit )
        {
            result.push_back( lhs );   // push tail:
            break;
        }

        std::tie(lhs, rhs) = split_left( lhs, set /*, Nset*/ );

        result.push_back( lhs );

        if ( lhs.cend() == text.cend() )
            break;

        lhs = rhs;
    }

    return result;
}

} // namespace detail
} // namespace string

// split() -> vector

#define string_MK_SPLIT(CharT)                                                                      \
    string_nodiscard inline std::vector< std17::basic_string_view<CharT>>                           \
    split(                                                                                          \
        std17::basic_string_view<CharT> text                                                        \
        , std17::basic_string_view<CharT> set                                                       \
        , std::size_t Nsplit = std::numeric_limits<std::size_t>::max() )                            \
    {                                                                                               \
        return detail::split(text, set, Nsplit );                                                   \
    }

#if string_CONFIG_PROVIDE_CHAR_T

// split_left() -> tuple

#define string_MK_SPLIT_LEFT( CharT )                                                               \
string_nodiscard inline auto                                                                        \
split_left(                                                                                         \
    std17::basic_string_view<CharT> text                                                            \
    , std17::basic_string_view<CharT> set                                                           \
    , std::size_t count = std::numeric_limits<std::size_t>::max() )                                 \
    -> std::tuple<std17::basic_string_view<CharT>, std17::basic_string_view<CharT>>                 \
{                                                                                                   \
    return detail::split_left( text, set, count );                                                  \
}

// split_right() -> tuple

#define string_MK_SPLIT_RIGHT( CharT )                                                              \
string_nodiscard inline auto                                                                        \
split_right(                                                                                        \
    std17::basic_string_view<CharT> text                                                            \
    , std17::basic_string_view<CharT> set                                                           \
    , std::size_t count = std::numeric_limits<std::size_t>::max() )                                 \
    -> std::tuple<std17::basic_string_view<CharT>, std17::basic_string_view<CharT>>                 \
{                                                                                                   \
    return detail::split_right( text, set, count );                                                 \
}

#endif // string_CONFIG_PROVIDE_CHAR_T

//
// Comparision:
//

// defined in namespace nonstd

#define string_MK_COMPARE( CharT )                  \
    string_nodiscard inline string_constexpr14 int  \
    compare( std17::basic_string_view<CharT> lhs, std17::basic_string_view<CharT> rhs ) string_noexcept \
    {                                               \
        return lhs.compare( rhs );                  \
    }

// defined in namespace nonstd::string::std17

#define string_MK_COMPARE_EQ( CharT )               \
    string_nodiscard inline string_constexpr14 bool \
    operator==( basic_string_view<CharT> lhs, basic_string_view<CharT> rhs )  \
    {                                               \
        return compare( lhs, rhs ) == 0;            \
    }

#define string_MK_COMPARE_NE( CharT )               \
    string_nodiscard inline string_constexpr14 bool \
    operator!=( basic_string_view<CharT> lhs, basic_string_view<CharT> rhs )  \
    {                                               \
        return compare( lhs, rhs ) != 0;            \
    }

#define string_MK_COMPARE_LT( CharT )               \
    string_nodiscard inline string_constexpr14 bool \
    operator<( basic_string_view<CharT> lhs, basic_string_view<CharT> rhs )   \
    {                                               \
        return compare( lhs, rhs ) < 0;             \
    }

#define string_MK_COMPARE_LE( CharT )               \
    string_nodiscard inline string_constexpr14 bool \
    operator<=( basic_string_view<CharT> lhs, basic_string_view<CharT> rhs )  \
    {                                               \
        return compare( lhs, rhs ) <= 0;            \
    }

#define string_MK_COMPARE_GE( CharT )               \
    string_nodiscard inline string_constexpr14 bool \
    operator>=( basic_string_view<CharT> lhs, basic_string_view<CharT> rhs )  \
    {                                               \
        return compare( lhs, rhs ) >= 0;            \
    }

#define string_MK_COMPARE_GT( CharT )               \
    string_nodiscard inline string_constexpr14 bool \
    operator>( basic_string_view<CharT> lhs, basic_string_view<CharT> rhs )   \
    {                                               \
        return compare( lhs, rhs ) > 0;             \
    }

//
// Define requested functions:
//

// Provide to_string() for std17::string_view:

using detail::to_string;

#if string_CONFIG_PROVIDE_CHAR_T

string_MK_IS_EMPTY           ( char )
string_MK_LENGTH             ( char )
string_MK_SIZE               ( char )
string_MK_FIND_FIRST         ( char )
string_MK_FIND_FIRST_CHAR    ( char )
string_MK_FIND_LAST          ( char )
string_MK_FIND_LAST_CHAR     ( char )
string_MK_FIND_FIRST_OF      ( char )
string_MK_FIND_LAST_OF       ( char )
string_MK_FIND_FIRST_NOT_OF  ( char )
string_MK_FIND_LAST_NOT_OF   ( char )
string_MK_APPEND             ( char )
string_MK_CONTAINS           ( char )      // includes char search type
string_MK_CONTAINS_ALL_OF    ( char )
string_MK_CONTAINS_ANY_OF    ( char )
string_MK_CONTAINS_NONE_OF   ( char )
string_MK_STARTS_WITH        ( char )
string_MK_STARTS_WITH_CHAR   ( char )
string_MK_STARTS_WITH_ALL_OF ( char )
string_MK_STARTS_WITH_ANY_OF ( char )
string_MK_STARTS_WITH_NONE_OF( char )
string_MK_ENDS_WITH          ( char )
string_MK_ENDS_WITH_CHAR     ( char )
string_MK_ENDS_WITH_ALL_OF   ( char )
string_MK_ENDS_WITH_ANY_OF   ( char )
string_MK_ENDS_WITH_NONE_OF  ( char )
string_MK_ERASE              ( char )
string_MK_ERASE_ALL          ( char )
string_MK_ERASE_FIRST        ( char )
string_MK_ERASE_LAST         ( char )
string_MK_INSERT             ( char )
string_MK_REPLACE            ( char )
string_MK_REPLACE_ALL        ( char )
string_MK_REPLACE_FIRST      ( char )
string_MK_REPLACE_LAST       ( char )
string_MK_STRIP_LEFT         ( char )
string_MK_STRIP_RIGHT        ( char )
string_MK_STRIP              ( char )
string_MK_SUBSTRING          ( char )
string_MK_TO_CASE_CHAR       ( char, lowercase )
string_MK_TO_CASE_CHAR       ( char, uppercase )
string_MK_TO_CASE_STRING     ( char, lowercase )
string_MK_TO_CASE_STRING     ( char, uppercase )
string_MK_CAPITALIZE         ( char )
string_MK_JOIN               ( char )
string_MK_SPLIT              ( char )
string_MK_SPLIT_LEFT         ( char )
string_MK_SPLIT_RIGHT        ( char )

string_MK_COMPARE            ( char )

// string_view operators:

namespace string { namespace std17 {

string_MK_COMPARE_EQ         ( char )
string_MK_COMPARE_NE         ( char )
string_MK_COMPARE_LT         ( char )
string_MK_COMPARE_LE         ( char )
string_MK_COMPARE_GE         ( char )
string_MK_COMPARE_GT         ( char )

}}  // namespace string::std17

#endif // string_CONFIG_PROVIDE_CHAR_T

#if string_CONFIG_PROVIDE_WCHAR_T

string_MK_IS_EMPTY           ( wchar_t )
string_MK_LENGTH             ( wchar_t )
string_MK_SIZE               ( wchar_t )
string_MK_FIND_FIRST         ( wchar_t )
string_MK_FIND_FIRST_CHAR    ( wchar_t )
string_MK_FIND_LAST          ( wchar_t )
string_MK_FIND_LAST_CHAR     ( wchar_t )
string_MK_FIND_FIRST_OF      ( wchar_t )
string_MK_FIND_LAST_OF       ( wchar_t )
string_MK_FIND_FIRST_NOT_OF  ( wchar_t )
string_MK_FIND_LAST_NOT_OF   ( wchar_t )
string_MK_APPEND             ( wchar_t )
string_MK_CONTAINS           ( wchar_t )      // includes wchar_t search type
string_MK_CONTAINS_ALL_OF    ( wchar_t )
string_MK_CONTAINS_ANY_OF    ( wchar_t )
string_MK_CONTAINS_NONE_OF   ( wchar_t )
string_MK_STARTS_WITH        ( wchar_t )
string_MK_STARTS_WITH_CHAR   ( wchar_t )
string_MK_STARTS_WITH_ALL_OF ( wchar_t )
string_MK_STARTS_WITH_ANY_OF ( wchar_t )
string_MK_STARTS_WITH_NONE_OF( wchar_t )
string_MK_ENDS_WITH          ( wchar_t )
string_MK_ENDS_WITH_CHAR     ( wchar_t )
string_MK_ENDS_WITH_ALL_OF   ( wchar_t )
string_MK_ENDS_WITH_ANY_OF   ( wchar_t )
string_MK_ENDS_WITH_NONE_OF  ( wchar_t )
string_MK_ERASE              ( wchar_t )
string_MK_ERASE_ALL          ( wchar_t )
string_MK_ERASE_FIRST        ( wchar_t )
string_MK_ERASE_LAST         ( wchar_t )
string_MK_INSERT             ( wchar_t )
string_MK_REPLACE            ( wchar_t )
string_MK_REPLACE_ALL        ( wchar_t )
string_MK_REPLACE_FIRST      ( wchar_t )
string_MK_REPLACE_LAST       ( wchar_t )
string_MK_STRIP_LEFT         ( wchar_t )
string_MK_STRIP_RIGHT        ( wchar_t )
string_MK_STRIP              ( wchar_t )
string_MK_SUBSTRING          ( wchar_t )
string_MK_TO_CASE_CHAR       ( wchar_t, lowercase )
string_MK_TO_CASE_CHAR       ( wchar_t, uppercase )
string_MK_TO_CASE_STRING     ( wchar_t, lowercase )
string_MK_TO_CASE_STRING     ( wchar_t, uppercase )
string_MK_CAPITALIZE         ( wchar_t )
string_MK_JOIN               ( wchar_t )
string_MK_SPLIT              ( wchar_t )
string_MK_SPLIT_LEFT         ( wchar_t )
string_MK_SPLIT_RIGHT        ( wchar_t )
// ...
string_MK_COMPARE            ( wchar_t )

// string_view operators:

namespace string { namespace std17 {

string_MK_COMPARE_EQ         ( wchar_t )
string_MK_COMPARE_NE         ( wchar_t )
string_MK_COMPARE_LT         ( wchar_t )
string_MK_COMPARE_LE         ( wchar_t )
string_MK_COMPARE_GE         ( wchar_t )
string_MK_COMPARE_GT         ( wchar_t )

}}  // namespace string::std17

#endif // string_CONFIG_PROVIDE_WCHAR_T

#if string_CONFIG_PROVIDE_CHAR8_T && string_HAVE_CHAR8_T

string_MK_IS_EMPTY           ( char8_t )
string_MK_LENGTH             ( char8_t )
string_MK_SIZE               ( char8_t )
string_MK_FIND_FIRST         ( char8_t )
string_MK_FIND_FIRST_CHAR    ( char8_t )
string_MK_FIND_LAST          ( char8_t )
string_MK_FIND_LAST_CHAR     ( char8_t )
string_MK_FIND_FIRST_OF      ( char8_t )
string_MK_FIND_LAST_OF       ( char8_t )
string_MK_FIND_FIRST_NOT_OF  ( char8_t )
string_MK_FIND_LAST_NOT_OF   ( char8_t )
string_MK_APPEND             ( char8_t )
string_MK_CONTAINS           ( char8_t )      // includes char search type
string_MK_CONTAINS_ALL_OF    ( char8_t )
string_MK_CONTAINS_ANY_OF    ( char8_t )
string_MK_CONTAINS_NONE_OF   ( char8_t )
string_MK_STARTS_WITH        ( char8_t )
string_MK_STARTS_WITH_CHAR   ( char8_t )
string_MK_STARTS_WITH_ALL_OF ( char8_t )
string_MK_STARTS_WITH_ANY_OF ( char8_t )
string_MK_STARTS_WITH_NONE_OF( char8_t )
string_MK_ENDS_WITH          ( char8_t )
string_MK_ENDS_WITH_CHAR     ( char8_t )
string_MK_ENDS_WITH_ALL_OF   ( char8_t )
string_MK_ENDS_WITH_ANY_OF   ( char8_t )
string_MK_ENDS_WITH_NONE_OF  ( char8_t )
string_MK_ERASE              ( char8_t )
string_MK_ERASE_ALL          ( char8_t )
string_MK_ERASE_FIRST        ( char8_t )
string_MK_ERASE_LAST         ( char8_t )
string_MK_INSERT             ( char8_t )
string_MK_REPLACE            ( char8_t )
string_MK_REPLACE_ALL        ( char8_t )
string_MK_REPLACE_FIRST      ( char8_t )
string_MK_REPLACE_LAST       ( char8_t )
string_MK_STRIP_LEFT         ( char8_t )
string_MK_STRIP_RIGHT        ( char8_t )
string_MK_STRIP              ( char8_t )
string_MK_SUBSTRING          ( char8_t )
string_MK_TO_CASE_CHAR       ( char8_t, lowercase )
string_MK_TO_CASE_CHAR       ( char8_t, uppercase )
string_MK_TO_CASE_STRING     ( char8_t, lowercase )
string_MK_TO_CASE_STRING     ( char8_t, uppercase )
string_MK_CAPITALIZE         ( char8_t )
string_MK_JOIN               ( char8_t )
string_MK_SPLIT              ( char8_t )
string_MK_SPLIT_LEFT         ( char8_t )
string_MK_SPLIT_RIGHT        ( char8_t )
// ...
string_MK_COMPARE            ( char8_t )

// string_view operators:

namespace string { namespace std17 {

string_MK_COMPARE_EQ         ( char8_t )
string_MK_COMPARE_NE         ( char8_t )
string_MK_COMPARE_LT         ( char8_t )
string_MK_COMPARE_LE         ( char8_t )
string_MK_COMPARE_GE         ( char8_t )
string_MK_COMPARE_GT         ( char8_t )

}}  // namespace string::std17

#endif // string_CONFIG_PROVIDE_CHAR8_T && string_HAVE_CHAR8_T

#if string_CONFIG_PROVIDE_CHAR16_T

string_MK_IS_EMPTY           ( char16_t )
string_MK_LENGTH             ( char16_t )
string_MK_SIZE               ( char16_t )
string_MK_FIND_FIRST         ( char16_t )
string_MK_FIND_FIRST_CHAR    ( char16_t )
string_MK_FIND_LAST          ( char16_t )
string_MK_FIND_LAST_CHAR     ( char16_t )
string_MK_FIND_FIRST_OF      ( char16_t )
string_MK_FIND_LAST_OF       ( char16_t )
string_MK_FIND_FIRST_NOT_OF  ( char16_t )
string_MK_FIND_LAST_NOT_OF   ( char16_t )
string_MK_APPEND             ( char16_t )
string_MK_CONTAINS           ( char16_t )      // includes char search type
string_MK_CONTAINS_ALL_OF    ( char16_t )
string_MK_CONTAINS_ANY_OF    ( char16_t )
string_MK_CONTAINS_NONE_OF   ( char16_t )
string_MK_STARTS_WITH        ( char16_t )
string_MK_STARTS_WITH_CHAR   ( char16_t )
string_MK_STARTS_WITH_ALL_OF ( char16_t )
string_MK_STARTS_WITH_ANY_OF ( char16_t )
string_MK_STARTS_WITH_NONE_OF( char16_t )
string_MK_ENDS_WITH          ( char16_t )
string_MK_ENDS_WITH_CHAR     ( char16_t )
string_MK_ENDS_WITH_ALL_OF   ( char16_t )
string_MK_ENDS_WITH_ANY_OF   ( char16_t )
string_MK_ENDS_WITH_NONE_OF  ( char16_t )
string_MK_ERASE              ( char16_t )
string_MK_ERASE_ALL          ( char16_t )
string_MK_ERASE_FIRST        ( char16_t )
string_MK_ERASE_LAST         ( char16_t )
string_MK_INSERT             ( char16_t )
string_MK_REPLACE            ( char16_t )
string_MK_REPLACE_ALL        ( char16_t )
string_MK_REPLACE_FIRST      ( char16_t )
string_MK_REPLACE_LAST       ( char16_t )
string_MK_STRIP_LEFT         ( char16_t )
string_MK_STRIP_RIGHT        ( char16_t )
string_MK_STRIP              ( char16_t )
string_MK_SUBSTRING          ( char16_t )
string_MK_TO_CASE_CHAR       ( char16_t, lowercase )
string_MK_TO_CASE_CHAR       ( char16_t, uppercase )
string_MK_TO_CASE_STRING     ( char16_t, lowercase )
string_MK_TO_CASE_STRING     ( char16_t, uppercase )
string_MK_CAPITALIZE         ( char16_t )
string_MK_JOIN               ( char16_t )
string_MK_SPLIT              ( char16_t )
string_MK_SPLIT_LEFT         ( char16_t )
string_MK_SPLIT_RIGHT        ( char16_t )
// ...
string_MK_COMPARE            ( char16_t )

// string_view operators:

namespace string { namespace std17 {

string_MK_COMPARE_EQ         ( char16_t )
string_MK_COMPARE_NE         ( char16_t )
string_MK_COMPARE_LT         ( char16_t )
string_MK_COMPARE_LE         ( char16_t )
string_MK_COMPARE_GE         ( char16_t )
string_MK_COMPARE_GT         ( char16_t )

}}  // namespace string::std17

#endif // string_CONFIG_PROVIDE_CHAR16_T

#if string_CONFIG_PROVIDE_CHAR32_T

string_MK_IS_EMPTY           ( char32_t )
string_MK_LENGTH             ( char32_t )
string_MK_SIZE               ( char32_t )
string_MK_FIND_FIRST         ( char32_t )
string_MK_FIND_FIRST_CHAR    ( char32_t )
string_MK_FIND_LAST          ( char32_t )
string_MK_FIND_LAST_CHAR     ( char32_t )
string_MK_FIND_FIRST_OF      ( char32_t )
string_MK_FIND_LAST_OF       ( char32_t )
string_MK_FIND_FIRST_NOT_OF  ( char32_t )
string_MK_FIND_LAST_NOT_OF   ( char32_t )
string_MK_APPEND             ( char32_t )
string_MK_CONTAINS           ( char32_t )      // includes char search type
string_MK_CONTAINS_ALL_OF    ( char32_t )
string_MK_CONTAINS_ANY_OF    ( char32_t )
string_MK_CONTAINS_NONE_OF   ( char32_t )
string_MK_STARTS_WITH        ( char32_t )
string_MK_STARTS_WITH_CHAR   ( char32_t )
string_MK_STARTS_WITH_ALL_OF ( char32_t )
string_MK_STARTS_WITH_ANY_OF ( char32_t )
string_MK_STARTS_WITH_NONE_OF( char32_t )
string_MK_ENDS_WITH          ( char32_t )
string_MK_ENDS_WITH_CHAR     ( char32_t )
string_MK_ENDS_WITH_ALL_OF   ( char32_t )
string_MK_ENDS_WITH_ANY_OF   ( char32_t )
string_MK_ENDS_WITH_NONE_OF  ( char32_t )
string_MK_ERASE              ( char32_t )
string_MK_ERASE_ALL          ( char32_t )
string_MK_ERASE_FIRST        ( char32_t )
string_MK_ERASE_LAST         ( char32_t )
string_MK_INSERT             ( char32_t )
string_MK_REPLACE            ( char32_t )
string_MK_REPLACE_ALL        ( char32_t )
string_MK_REPLACE_FIRST      ( char32_t )
string_MK_REPLACE_LAST       ( char32_t )
string_MK_STRIP_LEFT         ( char32_t )
string_MK_STRIP_RIGHT        ( char32_t )
string_MK_STRIP              ( char32_t )
string_MK_SUBSTRING          ( char32_t )
string_MK_TO_CASE_CHAR       ( char32_t, lowercase )
string_MK_TO_CASE_CHAR       ( char32_t, uppercase )
string_MK_TO_CASE_STRING     ( char32_t, lowercase )
string_MK_TO_CASE_STRING     ( char32_t, uppercase )
string_MK_CAPITALIZE         ( char32_t )
string_MK_JOIN               ( char32_t )
string_MK_SPLIT              ( char32_t )
string_MK_SPLIT_LEFT         ( char32_t )
string_MK_SPLIT_RIGHT        ( char32_t )
// ...
string_MK_COMPARE            ( char32_t )

// string_view operators:

namespace string { namespace std17 {

string_MK_COMPARE_EQ         ( char32_t )
string_MK_COMPARE_NE         ( char32_t )
string_MK_COMPARE_LT         ( char32_t )
string_MK_COMPARE_LE         ( char32_t )
string_MK_COMPARE_GE         ( char32_t )
string_MK_COMPARE_GT         ( char32_t )

}}  // namespace string::std17

#endif // string_CONFIG_PROVIDE_CHAR32_T

// #undef string_MK_*

#undef string_MK_IS_EMPTY
#undef string_MK_LENGTH
#undef string_MK_SIZE
#undef string_MK_APPEND
#undef string_MK_CONTAINS
#undef string_MK_CONTAINS_ALL_OF
#undef string_MK_CONTAINS_ANY_OF
#undef string_MK_CONTAINS_NONE_OF
#undef string_MK_STARTS_WITH
#undef string_MK_STARTS_WITH_CHAR
#undef string_MK_STARTS_WITH_ALL_OF
#undef string_MK_STARTS_WITH_ANY_OF
#undef string_MK_STARTS_WITH_NONE_OF
#undef string_MK_ENDS_WITH
#undef string_MK_ENDS_WITH_CHAR
#undef string_MK_ENDS_WITH_ALL_OF
#undef string_MK_ENDS_WITH_ANY_OF
#undef string_MK_ENDS_WITH_NONE_OF
#undef string_MK_FIND_FIRST
#undef string_MK_FIND_FIRST_CHAR
#undef string_MK_FIND_LAST
#undef string_MK_FIND_LAST_CHAR
#undef string_MK_FIND_FIRST_OF
#undef string_MK_FIND_LAST_OF
#undef string_MK_FIND_FIRST_NOT_OF
#undef string_MK_FIND_LAST_NOT_OF
#undef string_MK_ERASE
#undef string_MK_ERASE_ALL
#undef string_MK_ERASE_FIRST
#undef string_MK_ERASE_LAST
#undef string_MK_INSERT
#undef string_MK_REPLACE
#undef string_MK_REPLACE_ALL
#undef string_MK_REPLACE_FIRST
#undef string_MK_REPLACE_LAST
#undef string_MK_STRIP_LEFT
#undef string_MK_STRIP_RIGHT
#undef string_MK_STRIP
#undef string_MK_SUBSTRING
#undef string_MK_TO_CASE_CHAR
#undef string_MK_TO_CASE_CHAR
#undef string_MK_TO_CASE_STRING
#undef string_MK_TO_CASE_STRING
#undef string_MK_CAPITALIZE
#undef string_MK_JOIN
#undef string_MK_SPLIT
#undef string_MK_SPLIT_LEFT
#undef string_MK_SPLIT_RIGHT
#undef string_MK_COMPARE
#undef string_MK_COMPARE_EQ
#undef string_MK_COMPARE_NE
#undef string_MK_COMPARE_LT
#undef string_MK_COMPARE_LE
#undef string_MK_COMPARE_GE
#undef string_MK_COMPARE_GT

} // namespace nonstd

#endif  // NONSTD_STRING_BARE_HPP

/*
 * end of file
 */

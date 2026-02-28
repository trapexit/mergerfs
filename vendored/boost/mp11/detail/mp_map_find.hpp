#ifndef BOOST_MP11_DETAIL_MP_MAP_FIND_HPP_INCLUDED
#define BOOST_MP11_DETAIL_MP_MAP_FIND_HPP_INCLUDED

//  Copyright 2015 Peter Dimov.
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/utility.hpp>
#include <boost/mp11/detail/config.hpp>

#if BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, >= 140000 )

#include <boost/mp11/detail/mp_list.hpp>
#include <boost/mp11/detail/mp_append.hpp>
#include <boost/mp11/detail/mp_front.hpp>

#endif

#if BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1930 )

// not exactly good practice, but...
namespace std
{
    template<class... _Types> class tuple;
}

#endif

namespace boost
{
namespace mp11
{

#if BOOST_MP11_WORKAROUND( BOOST_MP11_GCC, >= 140000 )

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=120161

namespace detail
{

template<class M, class K> struct mp_map_find_impl;

template<template<class...> class M, class... T, class K> struct mp_map_find_impl<M<T...>, K>
{
    template<class U> using _f = mp_if<std::is_same<mp_front<U>, K>, mp_list<U>, mp_list<>>;

    using _l = mp_append<_f<T>..., mp_list<void>>;

    using type = mp_front<_l>;
};

} // namespace detail

template<class M, class K> using mp_map_find = typename detail::mp_map_find_impl<M, K>::type;

#else

// mp_map_find
namespace detail
{

#if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1930 )

template<class T> using mpmf_wrap = mp_identity<T>;
template<class T> using mpmf_unwrap = typename T::type;

#else

template<class... T> struct mpmf_tuple {};

template<class T> struct mpmf_wrap_impl
{
    using type = mp_identity<T>;
};

template<class... T> struct mpmf_wrap_impl< std::tuple<T...> >
{
    using type = mp_identity< mpmf_tuple<T...> >;
};

template<class T> using mpmf_wrap = typename mpmf_wrap_impl<T>::type;

template<class T> struct mpmf_unwrap_impl
{
    using type = typename T::type;
};

template<class... T> struct mpmf_unwrap_impl< mp_identity< mpmf_tuple<T...> > >
{
    using type = std::tuple<T...>;
};

template<class T> using mpmf_unwrap = typename mpmf_unwrap_impl<T>::type;

#endif // #if !BOOST_MP11_WORKAROUND( BOOST_MP11_MSVC, < 1930 )

template<class M, class K> struct mp_map_find_impl;

template<template<class...> class M, class... T, class K> struct mp_map_find_impl<M<T...>, K>
{
    using U = mp_inherit<mpmf_wrap<T>...>;

    template<template<class...> class L, class... U> static mp_identity<L<K, U...>> f( mp_identity<L<K, U...>>* );
    static mp_identity<void> f( ... );

    using type = mpmf_unwrap< decltype( f( static_cast<U*>(0) ) ) >;
};

} // namespace detail

template<class M, class K> using mp_map_find = typename detail::mp_map_find_impl<M, K>::type;

#endif

} // namespace mp11
} // namespace boost

#endif // #ifndef BOOST_MP11_DETAIL_MP_MAP_FIND_HPP_INCLUDED

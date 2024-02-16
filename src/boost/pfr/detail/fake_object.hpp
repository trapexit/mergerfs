// Copyright (c) 2023 Bela Schaum, X-Ryl669, Denis Mikhailov.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


// Initial implementation by Bela Schaum, https://github.com/schaumb
// The way to make it union and UB free by X-Ryl669, https://github.com/X-Ryl669
//

#ifndef BOOST_PFR_DETAIL_FAKE_OBJECT_HPP
#define BOOST_PFR_DETAIL_FAKE_OBJECT_HPP
#pragma once

#include <boost/pfr/detail/config.hpp>

namespace boost { namespace pfr { namespace detail {

// This variable serves as a compile-time assert. If you see any error here, then
// you're probably using `boost::pfr::get_name()` or `boost::pfr::names_as_array()` with a non-external linkage type.
template <class T>
extern const T passed_type_has_no_external_linkage;

// For returning non default constructible types, it's exclusively used in member name retrieval.
//
// Neither std::declval nor boost::pfr::detail::unsafe_declval are usable there.
// Limitation - T should have external linkage.
template <class T>
constexpr const T& fake_object() noexcept {
    return passed_type_has_no_external_linkage<T>;
}

}}} // namespace boost::pfr::detail

#endif // BOOST_PFR_DETAIL_FAKE_OBJECT_HPP


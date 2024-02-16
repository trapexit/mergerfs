//
// Copyright (c) 2023 Klemens Morgenstern (klemens.morgenstern@gmx.net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_BEAST_TEST_IMMEDIATE_EXECUTOR_HPP
#define BOOST_BEAST_TEST_IMMEDIATE_EXECUTOR_HPP

#include <boost/asio/any_io_executor.hpp>

namespace boost
{
namespace beast
{
namespace test
{

/** A immediate executor that directly invokes and counts how often that happened. */

class immediate_executor
{
    std::size_t &count_;
  public:
    immediate_executor(std::size_t & count) noexcept : count_(count) {}

    asio::execution_context &query(asio::execution::context_t) const
    {
        BOOST_ASSERT(false);
        return *static_cast<asio::execution_context*>(nullptr);
    }

    constexpr static asio::execution::blocking_t
    query(asio::execution::blocking_t) noexcept
    {
        return asio::execution::blocking_t::never_t{};
    }

    constexpr static asio::execution::relationship_t
    query(asio::execution::relationship_t) noexcept
    {
        return asio::execution::relationship_t::fork_t{};
    }
    // this function takes the function F and runs it on the event loop.
    template<class F>
    void
    execute(F f) const
    {
        count_++;
        std::forward<F>(f)();
    }

    bool
    operator==(immediate_executor const &other) const noexcept
    {
        return true;
    }

    bool
    operator!=(immediate_executor const &other) const noexcept
    {
        return false;
    }
};

}
}
}

#endif //BOOST_BEAST_TEST_IMMEDIATE_EXECUTOR_HPP

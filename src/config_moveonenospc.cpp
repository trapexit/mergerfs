/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "config_moveonenospc.hpp"

#include "errno.hpp"
#include "from_string.hpp"
#include "func_create_factory.hpp"


MoveOnENOSPC::MoveOnENOSPC(const bool enabled_)
  : enabled(enabled_),
    policy_name("pfrd")
{
}

int
MoveOnENOSPC::from_string(const std::string_view s_)
{
  bool tmp;
  const int rv = str::from(s_,&tmp);
  if(rv == 0)
    {
      enabled = tmp;
      if(enabled)
        policy_name = "pfrd";
      return 0;
    }

  // Not a bool — try a create-policy name.
  auto impl = Func2::CreateFactory::make(s_);
  if(!impl)
    return -EINVAL;

  policy_name = std::string(s_);
  enabled     = true;
  return 0;
}

std::string
MoveOnENOSPC::to_string(void) const
{
  if(enabled)
    return policy_name;
  return "false";
}

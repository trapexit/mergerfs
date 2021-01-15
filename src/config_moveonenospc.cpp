/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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
#include "ef.hpp"
#include "errno.hpp"
#include "from_string.hpp"


int
MoveOnENOSPC::from_string(const std::string &s_)
{
  int rv;
  std::string s;
  Policy::CreateImpl *tmp;

  rv = str::from(s_,&enabled);
  if((rv == 0) && (enabled == true))
    s = "mfs";
  ef(rv != 0)
    s = s_;
  else
    return 0;

  tmp = Policies::Create::find(s);
  if(tmp == NULL)
    return -EINVAL;

  policy  = tmp;
  enabled = true;

  return 0;
}

std::string
MoveOnENOSPC::to_string(void) const
{
  if(enabled)
    return policy.name();
  return "false";
}

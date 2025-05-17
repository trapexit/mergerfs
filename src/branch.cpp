/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "branch.hpp"
#include "ef.hpp"
#include "errno.hpp"
#include "num.hpp"

Branch::Branch()
{
}

Branch::Branch(const Branch &branch_)
  : _minfreespace(branch_._minfreespace),
    mode(branch_.mode),
    path(branch_.path)
{
}

Branch::Branch(const u64 &default_minfreespace_)
  : _minfreespace(&default_minfreespace_)
{
}

int
Branch::from_string(const std::string &str_)
{
  return -EINVAL;
}

std::string
Branch::to_string(void) const
{
  std::string rv;

  rv  = path;
  rv += '=';
  switch(mode)
    {
    default:
    case Branch::Mode::RW:
      rv += "RW";
      break;
    case Branch::Mode::RO:
      rv += "RO";
      break;
    case Branch::Mode::NC:
      rv += "NC";
      break;
    }

  if(std::holds_alternative<u64>(_minfreespace))
    {
      rv += ',';
      rv += num::humanize(std::get<u64>(_minfreespace));
    }

  return rv;
}

void
Branch::set_minfreespace(const u64 minfreespace_)
{
  _minfreespace = minfreespace_;
}

u64
Branch::minfreespace(void) const
{
  if(std::holds_alternative<const u64*>(_minfreespace))
    return *std::get<const u64*>(_minfreespace);
  return std::get<u64>(_minfreespace);
}

bool
Branch::ro(void) const
{
  return (mode == Branch::Mode::RO);
}

bool
Branch::nc(void) const
{
  return (mode == Branch::Mode::NC);
}

bool
Branch::ro_or_nc(void) const
{
  return ((mode == Branch::Mode::RO) ||
          (mode == Branch::Mode::NC));
}

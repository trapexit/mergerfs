/*
  ISC License

  Copyright (c) 2022, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fuse_mknod_policy_factory.hpp"
#include "fuse_mknod_policy_ff.hpp"
#include "fuse_mknod_policy_epff.hpp"

#include <stdexcept>


namespace FUSE::MKNOD::POLICY
{
  Base::Ptr
  factory(const toml::value &toml_)
  {
    std::string str;

    str = toml::find_or(toml_,"func","mknod","policy","ff");
    if(str == "ff")
      return std::make_shared<FF>(toml_);
    if(str == "epff")
      return std::make_shared<EPFF>(toml_);

    throw std::runtime_error("mknod");
  }
}

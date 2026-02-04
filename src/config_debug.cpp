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

#include "config_debug.hpp"

#include "debug.hpp"
#include "from_string.hpp"
#include "fuse_cfg.hpp"
#include "to_string.hpp"

#include <memory>
#include <string>

Debug::Debug(const bool v_)
{
  fuse_cfg.debug = v_;
}

std::string
Debug::to_string(void) const
{
  return str::to(fuse_cfg.debug);
}

int
Debug::from_string(const std::string_view s_)
{
  int rv;
  bool val;

  rv = str::from(s_,&val);
  if(rv < 0)
    return rv;

  fuse_cfg.debug = val;

  // If debug is being enabled, ensure output file is set
  if(val)
    {
      auto log_file_path = fuse_cfg.log_filepath();
      if(log_file_path && !log_file_path->empty())
        fuse_debug_set_output(*log_file_path);
      else
        fuse_debug_set_output({}); // Use stderr
    }

  return 0;
}

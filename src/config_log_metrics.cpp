/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config_log_metrics.hpp"
#include "from_string.hpp"
#include "to_string.hpp"

#include "fuse.h"

LogMetrics::LogMetrics(const bool val_)
{
  fuse_log_metrics_set(val_);
}

std::string
LogMetrics::to_string(void) const
{
  bool val;

  val = fuse_log_metrics_get();

  return str::to(val);
}

int
LogMetrics::from_string(const std::string &s_)
{
  int rv;
  bool val;

  rv = str::from(s_,&val);
  if(rv < 0)
    return rv;

  fuse_log_metrics_set(val);

  return 0;
}

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

#include "qos_class.hpp"


qos::Bucket *
qos::Class::bucket_for(const std::string &resource_) const
{
  std::lock_guard<std::mutex> lk(buckets_mutex);

  auto i = buckets.find(resource_);
  if(i != buckets.end())
    return i->second.get();

  // unique_ptr rather than the value type because a Bucket holds a
  // mutex: rehashing the map must not move one out from under a
  // thread waiting on it.
  auto [it,inserted] = buckets.emplace(resource_,std::make_unique<Bucket>());

  return it->second.get();
}

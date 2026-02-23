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

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "fileinfo.hpp"

#include <functional>
#include <map>
#include <string>


constexpr int INVALID_BACKING_ID = -1;

class State
{
public:
  State();

public:
  struct OpenFile
  {
    OpenFile()
      : ref_count(0),
        backing_id(INVALID_BACKING_ID),
        fi(nullptr)
    {
    }

    int ref_count;
    int backing_id;
    FileInfo *fi;
  };

public:
  struct GetSet
  {
    std::function<std::string()> get;
    std::function<int(const std::string_view)> set;
    std::function<int(const std::string_view)> valid;
  };

  void set_getset(const std::string   &name,
                  const State::GetSet &gs);

  int get(const std::string &key,
          std::string       &val);
  int set(const std::string      &key,
          const std::string_view  val);
  int valid(const std::string      &key,
            const std::string_view  val);

public:
  using OpenFileMap = boost::concurrent_flat_map<u64,OpenFile>;

private:
  std::map<std::string,GetSet> _getset;

public:
  OpenFileMap open_files;
};

extern State state;

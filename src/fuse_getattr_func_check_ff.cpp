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

#include "fuse_getattr_func_check_ff.hpp"

#include <cstdio>


namespace gfs = ghc::filesystem;


namespace l
{
  static
  void
  alert(const gfs::path &fullpath_,
        const char      *attr_,
        const uint64_t   val1_,
        const uint64_t   val2_)
  {
    fprintf(stderr,
            "%s: %s - %lx != %lx",
            fullpath_.c_str(),
            attr_,
            val1_,
            val2_);
  }
}

FUSE::GETATTR::FuncCheckFF::FuncCheckFF(const toml::value &toml_)
  : _branches(toml_)
{
  Stat::Func statfunc = Stat::no_follow;

  _statfunc = toml::find_or(toml_,"func","getattr","follow-symlinks",statfunc);
}

int
FUSE::GETATTR::FuncCheckFF::operator()(const char      *fusepath_,
                                       struct stat     *st_,
                                       fuse_timeouts_t *timeout_)
{
  int rv;
  size_t i, ei;
  size_t j, ej;
  gfs::path fullpath;
  struct stat st = {0};

  for(i = 0, ei = _branches.size(); i < ei; i++)
    {
      for(j = 0, ej = _branches[i].size(); j < ej; j++)
        {
          fullpath  = _branches[i][j].path;
          fullpath /= &fusepath_[1];

          rv = _statfunc(fullpath,&st);
          if(rv != 0)
            continue;

          j++;
          *st_ = st;
          goto main_loop;
        }
    }

 main_loop:
  for(; i < ei; i++)
    {
      for(; j < ej; j++)
        {
          fullpath  = _branches[i][j].path;
          fullpath /= &fusepath_[1];

          rv = _statfunc(fullpath,&st);
          if(rv != 0)
            continue;

          if(st.st_mode != st_->st_mode)
            l::alert(fullpath,"mode",st_->st_mode,st.st_mode);
        }
    }


  return 0;
}

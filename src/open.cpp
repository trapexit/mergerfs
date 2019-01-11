/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_open.hpp"
#include "fs_cow.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

#include <fcntl.h>

using std::string;
using std::vector;
using mergerfs::Policy;

namespace local
{
  static
  int
  open_core(const string &basepath_,
            const char   *fusepath_,
            const int     flags_,
            const bool    link_cow_,
            uint64_t     *fh_)
  {
    int fd;
    string fullpath;

    fs::path::make(basepath_,fusepath_,&fullpath);

    if(link_cow_ && fs::cow::is_eligible(fullpath.c_str(),flags_))
      fs::cow::break_link(fullpath.c_str());

    fd = fs::open(fullpath,flags_);
    if(fd == -1)
      return -errno;

    *fh_ = reinterpret_cast<uint64_t>(new FileInfo(fd,fusepath_));

    return 0;
  }

  static
  int
  open(Policy::Func::Search  searchFunc_,
       const Branches       &branches_,
       const uint64_t        minfreespace_,
       const char           *fusepath_,
       const int             flags_,
       const bool            link_cow_,
       uint64_t             *fh_)
  {
    int rv;
    vector<const string*> basepaths;

    rv = searchFunc_(branches_,fusepath_,minfreespace_,basepaths);
    if(rv == -1)
      return -errno;

    return local::open_core(*basepaths[0],fusepath_,flags_,link_cow_,fh_);
  }
}

namespace mergerfs
{
  namespace fuse
  {
    int
    open(const char     *fusepath_,
         fuse_file_info *ffi_)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.branches_lock);

      ffi_->direct_io = config.direct_io;
      return local::open(config.open,
                         config.branches,
                         config.minfreespace,
                         fusepath_,
                         ffi_->flags,
                         config.link_cow,
                         &ffi_->fh);
    }
  }
}

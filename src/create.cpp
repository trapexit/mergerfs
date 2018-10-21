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

#include <fuse.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_acl.hpp"
#include "fs_base_open.hpp"
#include "fs_clonepath.hpp"
#include "fs_path.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

using std::string;
using std::vector;
using namespace mergerfs;

static
int
_create_core(const string &fullpath,
             mode_t        mode,
             const mode_t  umask,
             const int     flags)
{
  if(!fs::acl::dir_has_defaults(fullpath))
    mode &= ~umask;

  return fs::open(fullpath,flags,mode);
}

static
int
_create_core(const string &createpath,
             const char   *fusepath,
             const mode_t  mode,
             const mode_t  umask,
             const int     flags,
             uint64_t     &fh)
{
  int rv;
  string fullpath;

  fs::path::make(&createpath,fusepath,fullpath);

  rv = _create_core(fullpath,mode,umask,flags);
  if(rv == -1)
    return -errno;

  fh = reinterpret_cast<uint64_t>(new FileInfo(rv,fusepath));

  return 0;
}

static
int
_create(Policy::Func::Search  searchFunc,
        Policy::Func::Create  createFunc,
        const Branches       &branches_,
        const uint64_t        minfreespace,
        const char           *fusepath,
        const mode_t          mode,
        const mode_t          umask,
        const int             flags,
        uint64_t             &fh)
{
  int rv;
  string fullpath;
  string fusedirpath;
  vector<const string*> createpaths;
  vector<const string*> existingpaths;

  fusedirpath = fs::path::dirname(fusepath);

  rv = searchFunc(branches_,fusedirpath,minfreespace,existingpaths);
  if(rv == -1)
    return -errno;

  rv = createFunc(branches_,fusedirpath,minfreespace,createpaths);
  if(rv == -1)
    return -errno;

  rv = fs::clonepath_as_root(*existingpaths[0],*createpaths[0],fusedirpath);
  if(rv == -1)
    return -errno;

  return _create_core(*createpaths[0],
                      fusepath,
                      mode,umask,flags,fh);
}

namespace mergerfs
{
  namespace fuse
  {
    int
    create(const char     *fusepath,
           mode_t          mode,
           fuse_file_info *ffi)
    {
      const fuse_context      *fc     = fuse_get_context();
      const Config            &config = Config::get(fc);
      const ugid::Set          ugid(fc->uid,fc->gid);
      const rwlock::ReadGuard  readlock(&config.branches_lock);

      return _create(config.getattr,
                     config.create,
                     config.branches,
                     config.minfreespace,
                     fusepath,
                     mode,
                     fc->umask,
                     ffi->flags,
                     ffi->fh);
    }
  }
}

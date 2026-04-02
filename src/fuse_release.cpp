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

/*
  RELEASE OF SHARED AND PER-HANDLE FILEINFO OBJECTS
  =================================================

  The open_files map stores one canonical FileInfo per nodeid together with a
  ref_count and optional passthrough backing_id. Overlapping opens of the same
  file increment that ref_count but still allocate their own FileInfo objects
  with duplicated file descriptors for the individual FUSE handles.

  That means release() must handle two ownership cases:
  - the canonical FileInfo stored in open_files
  - a per-handle FileInfo that exists only in fuse_file_info_t.fh

  On non-final closes, secondary handle FileInfo objects are freed
  immediately after the ref_count decrement while the canonical map entry stays
  alive.

  On the final close, the map entry is erased atomically and cleanup happens
  after the erase_if() callback returns so close() and delete do not execute
  while the map bucket is locked. If the final handle is different from the
  canonical map-owned FileInfo, both objects must be released.
*/

#include "fuse_release.hpp"

#include "state.hpp"

#include "config.hpp"
#include "fileinfo.hpp"
#include "fs_close.hpp"
#include "fs_fadvise.hpp"
#include "fuse_passthrough.hpp"

#include "fuse.h"


static
int
_release(cu64        nodeid_,
         FileInfo   *fi_,
         const bool  dropcacheonclose_)
{
  // The double fadvise is necessary insofar as according to nocache
  // author (https://github.com/Feh/nocache#limitations) the first one
  // doesn't always work due to timing issues.
  if(dropcacheonclose_)
    {
      fs::fadvise_dontneed(fi_->fd);
      fs::fadvise_dontneed(fi_->fd);
    }

  FUSE::release(nodeid_,fi_);

  return 0;
}

void
FUSE::release(cu64             nodeid_,
              FileInfo * const fi_)
{
  auto     &of            = state.open_files;
  int       backing_id    = INVALID_BACKING_ID;
  FileInfo *canonical_fi  = nullptr;
  FileInfo *fh_fi_to_free = nullptr;

  of.erase_if(nodeid_,
              [&](auto &v_)
              {
                v_.second.ref_count--;
                if(v_.second.ref_count > 0)
                  {
                    if(fi_ != v_.second.fi)
                      fh_fi_to_free = fi_;
                    return false;
                  }

                canonical_fi = v_.second.fi;
                backing_id = v_.second.backing_id;
                if(fi_ != canonical_fi)
                  fh_fi_to_free = fi_;

                return true;
              });

  FUSE::passthrough_close(backing_id);
  if(fh_fi_to_free)
    {
      fs::close(fh_fi_to_free->fd);
      delete fh_fi_to_free;
    }
  if(canonical_fi)
    {
      fs::close(canonical_fi->fd);
      delete canonical_fi;
    }
}

int
FUSE::release(const fuse_req_ctx_t   *ctx_,
              const fuse_file_info_t *ffi_)
{
  FileInfo *fi = FileInfo::from_fh(ffi_->fh);

  return ::_release(ctx_->nodeid,
                    fi,
                    cfg.dropcacheonclose);
}

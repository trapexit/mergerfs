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
  TWO-PHASE FILE RELEASE AND RESOURCE CLEANUP
  ===========================================

  This file implements FUSE release() with a sophisticated two-phase cleanup
  mechanism designed to handle concurrent access without deadlocks or leaks.

  THE PROBLEM: CONCURRENT RELEASE AND OPEN
  ----------------------------------------

  When multiple threads access the same file, we have reference counting to
  track how many opens are active. When the last close happens, we need to:
  1. Close the file descriptor
  2. Close the passthrough backing_id (if any)
  3. Delete the FileInfo object
  4. Remove the entry from the open_files map

  The challenge is that these operations can be slow (especially close()),
  and we CANNOT hold the map lock during slow operations. Other threads
  may need to open the same file or release other files.

  THE SOLUTION: TWO-PHASE CLEANUP
  -------------------------------

  Phase 1: _release_ref() - Atomic Reference Management (lines ~35-65)
    - Runs inside visit() callback (map is locked)
    - Decrements ref_count
    - If ref_count reaches 0, captures resources and sets fi = nullptr
    - This "zombie" state signals that cleanup is needed

  Phase 2: _release_cleanup() - Resource Destruction (lines ~69-88)
    - Runs OUTSIDE visit() callback (map is unlocked)
    - Performs slow operations: close(), passthrough_close(), delete
    - Conditionally erases the map entry using erase_if

  WHY TWO PHASES?
  --------------

  Without this split, a slow close() would block:
  - Other threads trying to open the same file
  - Other threads trying to release other files (global map lock)
  - The entire FUSE request pipeline

  By separating the fast reference counting from the slow cleanup, we achieve
  both correctness and high concurrency.

  THE ZOMBIE STATE
  ----------------

  When ref_count hits 0, we set fi = nullptr. This "zombie" state means:
  - The entry is marked for destruction
  - New opens should skip this entry (see fuse_open.cpp line ~305)
  - Cleanup is in progress or pending

  The erase_if() in _release_cleanup() only erases if fi is still nullptr,
  protecting against the ABA problem where another thread might have already
  created a new entry with the same nodeid.

  ORPHAN FILEINFO CLEANUP
  -----------------------

  The release() function (lines ~116-135) handles the case where a FileInfo
  was created but never inserted into the map (e.g., open failure). The
  fi_in_map flag distinguishes between:
  - FileInfo in map: wait for normal cleanup
  - FileInfo not in map (orphan): clean up immediately

  See commit 446e9a7b for the complete rationale behind this design.
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
  auto     &of         = state.open_files;
  int       backing_id = INVALID_BACKING_ID;
  FileInfo *fi_to_free = nullptr;

  of.erase_if(nodeid_,
              [&](auto &v_)
              {
                v_.second.ref_count--;
                if(v_.second.ref_count > 0)
                  {
                    if(fi_ != v_.second.fi)
                      fi_to_free = fi_;
                    return false;
                  }

                fi_to_free = v_.second.fi;
                backing_id = v_.second.backing_id;

                return true;
              });

  FUSE::passthrough_close(backing_id);
  if(fi_to_free)
    {
      fs::close(fi_to_free->fd);
      delete fi_to_free;
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

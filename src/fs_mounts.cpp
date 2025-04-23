#include "fs_mounts.hpp"

#include <cstdio>

#include <mntent.h>

#ifdef __linux__
void
fs::mounts(fs::MountVec &mounts_)
{
  FILE *f;

  f = setmntent("/proc/mounts","r");
  if(f == NULL)
    return;

  struct mntent *entry;
  while((entry = getmntent(f)) != NULL)
    {
      fs::Mount m;

      m.dir    = entry->mnt_dir;
      m.fsname = entry->mnt_fsname;
      m.type   = entry->mnt_type;
      m.opts   = entry->mnt_opts;

      mounts_.emplace_back(std::move(m));
    }

  endmntent(f);
}
#else
void
fs::mounts(fs::MountVec &mounts_)
{
}
#endif

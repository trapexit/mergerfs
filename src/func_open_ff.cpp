#include "func_open_ff.hpp"

#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_cow.hpp"
#include "fs_fchmod.hpp"
#include "fs_lchmod.hpp"
#include "fs_lstat.hpp"
#include "fs_open.hpp"
#include "fs_openat.hpp"
#include "stat_util.hpp"


static
bool
_rdonly(const int flags_)
{
  return ((flags_ & O_ACCMODE) == O_RDONLY);
}

static
int
_lchmod_and_open_if_not_writable_and_empty(const fs::path &fullpath_,
                                           const int       flags_)
{
  int rv;
  struct stat st;

  rv = fs::lstat(fullpath_,&st);
  if(rv < 0)
    return -EACCES;

  if(StatUtil::writable(st))
    return -EACCES;

  rv = fs::lchmod(fullpath_,(st.st_mode|S_IWUSR|S_IWGRP));
  if(rv < 0)
    return -EACCES;

  rv = fs::open(fullpath_,flags_);
  if(rv < 0)
    return -EACCES;

  fs::fchmod(rv,st.st_mode);

  return rv;
}

static
int
_nfsopenhack(const fs::path   &fullpath_,
             const int         flags_,
             const NFSOpenHack nfsopenhack_)
{
  switch(nfsopenhack_)
    {
    default:
    case NFSOpenHack::ENUM::OFF:
      return -EACCES;
    case NFSOpenHack::ENUM::GIT:
      if(::_rdonly(flags_))
        return -EACCES;
      if(fullpath_.string().find("/.git/") == std::string::npos)
        return -EACCES;
      return ::_lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
    case NFSOpenHack::ENUM::ALL:
      if(::_rdonly(flags_))
        return -EACCES;
      return ::_lchmod_and_open_if_not_writable_and_empty(fullpath_,flags_);
    }
}

static
int
_open_path(const fs::path    &fullpath_,
           const Branch      *branch_,
           const fs::path    &fusepath_,
           fuse_file_info_t  *ffi_,
           const NFSOpenHack  nfsopenhack_)
{
  int fd;
  FileInfo *fi;

  fd = fs::openat(AT_FDCWD,fullpath_,ffi_->flags);
  if(fd == -EACCES)
    fd = ::_nfsopenhack(fullpath_,ffi_->flags,nfsopenhack_);
  if(fd < 0)
    return fd;

  fi = new FileInfo(fd,branch_,fusepath_,ffi_->direct_io);

  ffi_->fh = fi->to_fh();

  return 0;
}


std::string_view
Func2::OpenFF::name() const
{
  return "ff";
}

int
Func2::OpenFF::operator()(const Branches &branches_,
                          const fs::path &fusepath_,
                          fuse_file_info_t *&ffi_,
                          const bool       &link_cow_,
                          const NFSOpenHack &nfsopenhack_)
{
  fs::path fullpath;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      if(link_cow_ && fs::cow::is_eligible(fullpath,ffi_->flags))
        fs::cow::break_link(fullpath);

      const int rv = ::_open_path(fullpath,
                                  &branch,
                                  fusepath_,
                                  ffi_,
                                  nfsopenhack_);
      if(rv == -ENOENT)
        continue;

      return rv;
    }

  return -ENOENT;
}

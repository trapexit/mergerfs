#include "fs_statx.hpp"

static
void
_set_stat_if_leads_to_dir(const std::string &path_,
                          struct fuse_statx *st_)
{
  int rv;
  struct fuse_statx st;

  rv = fs::statx(AT_FDCWD,path_,AT_SYMLINK_FOLLOW,STATX_TYPE,&st);
  if(rv < 0)
    return;

  if(S_ISDIR(st.mode))
    *st_ = st;

  return;
}

static
void
_set_stat_if_leads_to_reg(const std::string &path_,
                          struct fuse_statx *st_)
{
  int rv;
  struct fuse_statx st;

  rv = fs::statx(AT_FDCWD,path_,AT_SYMLINK_FOLLOW,STATX_TYPE,&st);
  if(rv < 0)
    return;

  if(S_ISREG(st.mode))
    *st_ = st;

  return;
}

int
fs::statx(const int           dirfd_,
          const char         *pathname_,
          const int           flags_,
          const unsigned int  mask_,
          struct fuse_statx  *st_,
          FollowSymlinksEnum  follow_)
{
  int rv;

  switch(follow_)
    {
    case FollowSymlinksEnum::NEVER:
      rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
      break;
    case FollowSymlinksEnum::DIRECTORY:
      rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
      if((rv >= 0) && S_ISLNK(st_->mode))
        _set_stat_if_leads_to_dir(pathname_,st_);
      break;
    case FollowSymlinksEnum::REGULAR:
      rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
      if((rv >= 0) && S_ISLNK(st_->mode))
        _set_stat_if_leads_to_reg(pathname_,st_);
      break;
    case FollowSymlinksEnum::ALL:
      rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_FOLLOW,mask_,st_);
      if(rv < 0)
        rv = fs::statx(AT_FDCWD,pathname_,flags_|AT_SYMLINK_NOFOLLOW,mask_,st_);
      break;
    }

  return rv;
}

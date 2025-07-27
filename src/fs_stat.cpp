#include "fs_stat.hpp"
#include "fs_lstat.hpp"

static
void
_set_stat_if_leads_to_dir(const char  *path_,
                          struct stat *st_)
{
  int rv;
  struct stat st;

  rv = fs::stat(path_,&st);
  if(rv < 0)
    return;

  if(S_ISDIR(st.st_mode))
    *st_ = st;

  return;
}

static
void
_set_stat_if_leads_to_reg(const char  *path_,
                          struct stat *st_)
{
  int rv;
  struct stat st;

  rv = fs::stat(path_,&st);
  if(rv < 0)
    return;

  if(S_ISREG(st.st_mode))
    *st_ = st;

  return;
}

int
fs::stat(const char         *path_,
         struct stat        *st_,
         FollowSymlinksEnum  follow_)
{
  int rv;

  switch(follow_)
    {
    case FollowSymlinksEnum::NEVER:
      rv = fs::lstat(path_,st_);
      break;
    case FollowSymlinksEnum::DIRECTORY:
      rv = fs::lstat(path_,st_);
      if(S_ISLNK(st_->st_mode))
        _set_stat_if_leads_to_dir(path_,st_);
      break;
    case FollowSymlinksEnum::REGULAR:
      rv = fs::lstat(path_,st_);
      if(S_ISLNK(st_->st_mode))
        _set_stat_if_leads_to_reg(path_,st_);
      break;
    case FollowSymlinksEnum::ALL:
      rv = fs::stat(path_,st_);
      if(rv != 0)
        rv = fs::lstat(path_,st_);
      break;
    }

  return rv;
}

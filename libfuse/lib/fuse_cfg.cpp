#include "fuse_cfg.hpp"

fuse_cfg_t fuse_cfg;

bool
fuse_cfg_t::valid_uid() const
{
  return (uid != FUSE_CFG_INVALID_ID);
}

bool
fuse_cfg_t::valid_gid() const
{
  return (gid != FUSE_CFG_INVALID_ID);
}

bool
fuse_cfg_t::valid_umask() const
{
  return (umask != FUSE_CFG_INVALID_UMASK);
}

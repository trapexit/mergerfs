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

std::shared_ptr<FILE>
fuse_cfg_t::log_file() const
{
  return std::atomic_load(&_log_file);
}

void
fuse_cfg_t::log_file(std::shared_ptr<FILE> f_)
{
  std::atomic_store(&_log_file, f_);
}

std::shared_ptr<std::string>
fuse_cfg_t::log_filepath() const
{
  return std::atomic_load(&_log_filepath);
}

void
fuse_cfg_t::log_filepath(const std::string &s_)
{
  std::atomic_store(&_log_filepath,
                    std::make_shared<std::string>(s_));
}

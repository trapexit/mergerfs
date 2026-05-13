#include "fuse_cfg.hpp"

#include <mutex>

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
  std::shared_lock<std::shared_mutex> lk(_log_mutex);

  return _log_file;
}

void
fuse_cfg_t::log_file(std::shared_ptr<FILE> f_)
{
  std::unique_lock<std::shared_mutex> lk(_log_mutex);

  _log_file = std::move(f_);
}

std::shared_ptr<std::string>
fuse_cfg_t::log_filepath() const
{
  std::shared_lock<std::shared_mutex> lk(_log_mutex);

  return _log_filepath;
}

void
fuse_cfg_t::log_filepath(const std::string &s_)
{
  auto filepath = std::make_shared<std::string>(s_);
  std::unique_lock<std::shared_mutex> lk(_log_mutex);

  _log_filepath = std::move(filepath);
}

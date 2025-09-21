#include "config_gidcache.hpp"

#include "gidcache.hpp"

#include "to_string.hpp"
#include "from_string.hpp"


GIDCacheExpireTimeout::GIDCacheExpireTimeout(const int i_)
{
  GIDCache::expire_timeout = i_;
}

GIDCacheExpireTimeout::GIDCacheExpireTimeout(const std::string &s_)
{
  from_string(s_);
}

std::string
GIDCacheExpireTimeout::to_string(void) const
{
  return str::to(GIDCache::expire_timeout);
}

int
GIDCacheExpireTimeout::from_string(const std::string_view s_)
{
  int rv;

  rv = str::from(s_,&GIDCache::expire_timeout);
  if(rv < 0)
    return rv;

  return 0;
}


GIDCacheRemoveTimeout::GIDCacheRemoveTimeout(const int i_)
{
  GIDCache::remove_timeout = i_;
}


GIDCacheRemoveTimeout::GIDCacheRemoveTimeout(const std::string &s_)
{
  from_string(s_);
}

std::string
GIDCacheRemoveTimeout::to_string(void) const
{
  return str::to(GIDCache::remove_timeout);
}

int
GIDCacheRemoveTimeout::from_string(const std::string_view s_)
{
  int rv;

  rv = str::from(s_,&GIDCache::remove_timeout);
  if(rv < 0)
    return rv;

  return 0;
}

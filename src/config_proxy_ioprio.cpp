#include "config_proxy_ioprio.hpp"

#include "ioprio.hpp"
#include "from_string.hpp"


ProxyIOPrio::ProxyIOPrio(const bool b_)
{
  ioprio::enable(b_);
}

std::string
ProxyIOPrio::to_string(void) const
{
  if(ioprio::enabled())
    return "true";
  return "false";
}

int
ProxyIOPrio::from_string(const std::string_view s_)
{
  int rv;
  bool enable;

  rv = str::from(s_,&enable);
  if(rv)
    return rv;

  ioprio::enable(enable);

  return 0;
}

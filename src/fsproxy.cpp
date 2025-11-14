#include "fsproxy.hpp"

#include "fs_close.hpp"

int
FSProxy::_spawn_proxy_if_needed(const uid_t uid_,
                                const gid_t gid_)
{
  int rv;
  FSProxy::Process process;

  if(_proxies.count({uid_,gid_}))
    return 0;

  rv = pipe(process.req_pipe);
  if(rv == -1)
    return rv;
  rv = pipe(process.res_pipe);
  if(rv == -1)
    return rv;

  process.pid = fork();
  if(process.pid != 0)
    {
      fs::close(process.req_pipe[1]);
      fs::close(process.res_pipe[0]);
      // listen on req in loop for instructions
      return 0;
    }

  fs::close(process.req_pipe[0]);
  fs::close(process.res_pipe[1]);
  _proxies[UGID{uid_,gid_}] = process;

  return 0;
}

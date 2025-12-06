#include <unistd.h>

namespace ugid
{
  thread_local uid_t currentuid  = 0;
  thread_local gid_t currentgid  = 0;
  thread_local bool  initialized = false;
}

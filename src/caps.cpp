#include "caps.hpp"

#if defined __linux__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <grp.h>
#include <linux/capability.h>
#include <linux/securebits.h>


static
int
capset(cap_user_header_t     header_,
       const cap_user_data_t data_)
{
  return ::syscall(SYS_capset,header_,data_);
}

static
int
capget(cap_user_header_t header_,
       cap_user_data_t   data_)
{
  return ::syscall(SYS_capget,header_,data_);
}

static
int
capset(int cap_bit_)
{
  int rv;
  struct __user_cap_header_struct header;
  struct __user_cap_data_struct data[2];

  header.version = _LINUX_CAPABILITY_VERSION_3;
  header.pid = 0;

  rv = capget(&header,data);
  if(rv < 0)
    return rv;

  int word = cap_bit_ / 32;
  int bit  = cap_bit_ % 32;

  data[word].permitted   |= (1 << bit);
  data[word].effective   |= (1 << bit);
  data[word].inheritable |= (1 << bit);

  rv = capset(&header,data);
  if(rv < 0)
    return rv;

  return 0;
}

int
caps::setup()
{
  int rv;

  rv = capset(CAP_DAC_OVERRIDE);
  if(rv < 0)
    return rv;
  rv = capset(CAP_DAC_READ_SEARCH);
  if(rv < 0)
    return rv;
  rv = capset(CAP_FOWNER);
  if(rv < 0)
    return rv;
  rv = capset(CAP_CHOWN);
  if(rv < 0)
    return rv;
  rv = capset(CAP_SETUID);
  if(rv < 0)
    return rv;
  rv = capset(CAP_SETGID);
  if(rv < 0)
    return rv;

  rv = prctl(PR_SET_SECUREBITS,
             SECBIT_KEEP_CAPS | SECBIT_NO_SETUID_FIXUP);
  if(rv < 0)
    return -errno;

  return 0;
}
#else
#include <errno.h>

int
caps::setup()
{
  return -ENOSYS;
}
#endif

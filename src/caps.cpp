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

static int capset(cap_user_header_t header, const cap_user_data_t data) {
    return syscall(SYS_capset, header, data);
}

static int capget(cap_user_header_t header, cap_user_data_t data) {
    return syscall(SYS_capget, header, data);
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

  int word = cap_bit_ / 32;     // Which 32-bit word (0 or 1)
  int bit  = cap_bit_ % 32;     // Which bit in that word

  // Set CAP_DAC_READ_SEARCH in permitted, effective, and inheritable
  data[word].permitted |= (1 << bit);
  data[word].effective |= (1 << bit);
  data[word].inheritable |= (1 << bit);


  return 0;
}



int
caps_setup()
{
  struct __user_cap_header_struct header;
  struct __user_cap_data_struct data[2];

  header.version = _LINUX_CAPABILITY_VERSION_3;
  header.pid = 0;

  // Get current capabilities
  if(capget(&header, data) < 0)
    {
      perror("capget");
      return -1;
    }

  // Calculate bit position for CAP_DAC_READ_SEARCH
  int cap_bit = CAP_DAC_READ_SEARCH;
  int word = cap_bit / 32;  // Which 32-bit word (0 or 1)
  int bit = cap_bit % 32;   // Which bit in that word

  // Set CAP_DAC_READ_SEARCH in permitted, effective, and inheritable
  data[word].permitted |= (1 << bit);
  data[word].effective |= (1 << bit);
  data[word].inheritable |= (1 << bit);

  cap_bit = CAP_DAC_OVERRIDE;
  word = cap_bit / 32;  // Which 32-bit word (0 or 1)
  bit = cap_bit % 32;   // Which bit in that word

  // Set CAP_DAC_READ_SEARCH in permitted, effective, and inheritable
  data[word].permitted |= (1 << bit);
  data[word].effective |= (1 << bit);
  data[word].inheritable |= (1 << bit);

  // Apply the capability set
  if(capset(&header, data) < 0)
    {
      perror("capset");
      return -1;
    }

  // Enable ambient capabilities (survives UID changes)
  // PR_CAP_AMBIENT = 47, PR_CAP_AMBIENT_RAISE = 2
  if(prctl(PR_CAP_AMBIENT,
           PR_CAP_AMBIENT_RAISE,
           CAP_DAC_READ_SEARCH,
           0,
           0) < 0)
    {
      perror("prctl PR_CAP_AMBIENT_RAISE");
      printf("Warning: Ambient capabilities not supported (kernel < 4.3?)\n");
    }
  else
    {
      printf("Ambient capabilities set successfully\n");
    }

  if(prctl(PR_CAP_AMBIENT,
           PR_CAP_AMBIENT_RAISE,
           CAP_DAC_OVERRIDE,
           0,
           0) < 0)
    {
      perror("prctl PR_CAP_AMBIENT_RAISE");
      printf("Warning: Ambient capabilities not supported (kernel < 4.3?)\n");
    }
  else
    {
      printf("Ambient capabilities set successfully\n");
    }

  // Keep capabilities across setuid
  if(prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0)
    {
      perror("prctl PR_SET_KEEPCAPS");
      return -1;
    }

  printf("PR_SET_KEEPCAPS enabled\n");

  return 0;
}

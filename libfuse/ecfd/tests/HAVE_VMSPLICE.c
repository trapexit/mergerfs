#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/uio.h>

int
main(int   argc,
     char *argv[])
{
  (void)vmsplice;

  return 0;
}

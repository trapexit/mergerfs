#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>

static
int
sys_get_pagesize_(void)
{
#ifdef _SC_PAGESIZE
  return sysconf(_SC_PAGESIZE);
#else
  return getpagesize();
#endif
}

int
sys_get_pagesize(void)
{
  static int pagesize = 0;

  if(pagesize == 0)
    pagesize = sys_get_pagesize_();

  return pagesize;
}

int
sys_alloc_pipe(int pipe_[2],
               int bufsize_)
{
#ifdef F_SETPIPE_SZ
  int rv;

  rv = pipe(pipe_);
  if(rv == -1)
    return -1;

  rv = fcntl(pipe_[0],F_SETPIPE_SZ,bufsize_);
  if(rv == -1)
    return -1;

  return rv;
#else
  errno = ENOSYS;
  return -1;
#endif
}

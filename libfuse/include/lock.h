#include <stdint.h>
#include <sys/types.h>

typedef struct lock_s lock_t;
struct lock_s
{
  int type;
  off_t start;
  off_t end;
  pid_t pid;
  uint64_t owner;
  lock_t *next;
};

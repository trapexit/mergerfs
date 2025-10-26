#include "fuse_req.hpp"

#include <cstddef>
#include <cstdlib>

typedef struct stack_t stack_t;
struct stack_t
{
  stack_t *next;
};

thread_local static stack_t *g_stack = NULL;

fuse_req_t*
fuse_req_alloc()
{
  if(g_stack == NULL)
    return (fuse_req_t*)calloc(1,sizeof(fuse_req_t));

  fuse_req_t *req;

  req = (fuse_req_t*)g_stack;
  g_stack = g_stack->next;

  return req;
}

void
fuse_req_free(fuse_req_t *req_)
{
  stack_t *stack;

  stack = (stack_t*)req_;
  stack->next = g_stack;
  g_stack = stack;
}

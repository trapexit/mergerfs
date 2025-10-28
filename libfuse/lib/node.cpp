#include "node.hpp"

#include <mutex>
#include <unordered_map>

#include <cstddef>
#include <cstdlib>

struct stack_t
{
  stack_t *next;
};

thread_local static stack_t *g_stack       = NULL;
thread_local bool            g_should_gc   = false;
thread_local bool            g_initialized = false;

static std::mutex g_mutex;
static std::unordered_map<stack_t*,bool*> g_all_stacks;

node_t*
node_alloc()
{
  if(g_stack == NULL)
    return (node_t*)calloc(1,sizeof(node_t));

  node_t *node;

  node = (node_t*)g_stack;
  g_stack = g_stack->next;

  return node;
}

void
node_free(node_t *node_)
{
  stack_t *stack;

  stack = (stack_t*)node_;
  stack->next = g_stack;
  g_stack = stack;
}

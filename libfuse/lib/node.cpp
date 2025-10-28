#include "node.hpp"

#include <mutex>
#include <vector>

#include <cstddef>
#include <cstdlib>

#include "fmt/core.h"
#include <unistd.h>


struct stack_t
{
  stack_t *next;
};

struct StackInfo
{
  stack_t *stack;
  bool    *should_gc;
};

thread_local static stack_t *g_stack       = NULL;
thread_local bool            g_should_gc   = false;
thread_local bool            g_initialized = false;

static std::mutex g_mutex;
static std::vector<StackInfo> g_all_stacks;


node_t*
node_alloc()
{
  if(g_initialized == false)
    {
      std::lock_guard<std::mutex> guard(g_mutex);
      g_all_stacks.push_back({g_stack,&g_should_gc});
      fmt::print("{} {}",

                 gettid(),
                 (void*)g_stack);
      g_initialized = true;
    }

  if(g_should_gc)
    {
      int i;
      stack_t *s;

      i = 0;
      s = g_stack;
      while(s)
        {
          i++;
          s = s->next;
        }
      fmt::print("{} {}",
                 gettid(),
                 i);

      g_should_gc = false;
    }

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


void
node_gc()
{
  std::lock_guard<std::mutex> _(g_mutex);

  for(auto &info : g_all_stacks)
    {
      *info.should_gc = true;
    }
}

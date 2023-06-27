#include "node.h"

#include "lfmp.h"

static lfmp_t g_NODE_FMP;

static
__attribute__((constructor))
void
node_constructor()
{
  lfmp_init(&g_NODE_FMP,sizeof(node_t),256);
}

static
__attribute__((destructor))
void
node_destructor()
{
  lfmp_destroy(&g_NODE_FMP);
}

node_t *
node_alloc()
{
  return lfmp_calloc(&g_NODE_FMP);
}

void
node_free(node_t *node_)
{
  lfmp_free(&g_NODE_FMP,node_);
}

int
node_gc1()
{
  return lfmp_gc(&g_NODE_FMP);
}

void
node_gc()
{
  int rv;
  int fails;

  fails = 0;
  do
    {
      rv = node_gc1();
      if(rv == 0)
        fails++;
    } while(rv || (fails < 3));
}

lfmp_t*
node_lfmp()
{
  return &g_NODE_FMP;
}

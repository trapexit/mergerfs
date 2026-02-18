#include "node.hpp"

#include "objpool.hpp"

static ObjPool<node_t> g_NODE_POOL;

node_t*
node_alloc()
{
  return g_NODE_POOL.alloc();
}

void
node_free(node_t *node_)
{
  g_NODE_POOL.free(node_);
}

void
node_gc()
{
  g_NODE_POOL.gc();
}

void
node_clear()
{
  g_NODE_POOL.clear();
}

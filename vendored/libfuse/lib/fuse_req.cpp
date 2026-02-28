#include "fuse_req.hpp"

#include "objpool.hpp"


static ObjPool<fuse_req_t> g_pool;

fuse_req_t*
fuse_req_alloc()
{
  return g_pool.alloc();
}

void
fuse_req_free(fuse_req_t *req_)
{
  return g_pool.free(req_);
}

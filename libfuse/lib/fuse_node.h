#include <stdint.h>

typedef struct fuse_node_t fuse_node_t;
struct fuse_node_t
{
  uint64_t     id;
  uint64_t     generation;  
  char        *name;
  fuse_node_t *parent;
  uint32_t     ref_count;
  uint64_t     lookup_count;
  uint64_t     open_count;
};

struct fuse_node_hashtable_t;
typedef struct fuse_node_hashtable_t fuse_node_hashtable_t;


fuse_node_hashtable_t *fuse_node_hashtable_init();

fuse_node_t *fuse_node_hashtable_put(fuse_node_hashtable_t *ht,
                                     const uint64_t         parent_id,
                                     const uint64_t         child_id,
                                     const char            *child_name);

fuse_node_t* fuse_node_hashtable_get(fuse_node_hashtable_t *ht,
                                     const uint64_t         id);
fuse_node_t* fuse_node_hashtable_get_child(fuse_node_hashtable_t *ht,
                                           const uint64_t         id,
                                           const char            *name);
void fuse_node_hashtable_del(fuse_node_hashtable_t *ht,
                             fuse_node_t           *node);

void fuse_node_hashtable_get_path(fuse_node_hashtable_t *ht,
                                  char                  *buf,
                                  uint32_t               buflen);

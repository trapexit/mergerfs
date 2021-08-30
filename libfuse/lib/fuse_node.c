#include "fuse_node.h"

#include "khash.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stdio.h> // for debugging

#define UNKNOWN_INO    UINT64_MAX
#define ROOT_NODE_ID   0
#define ROOT_NODE_NAME "/"

typedef struct node_idname_t node_idname_t;
struct node_idname_t
{
  uint64_t    id;
  const char *name;
};

static khint_t idname_hash_func(const node_idname_t idname);
static int     idname_hash_equal(const node_idname_t idname0, const node_idname_t idname1);

KHASH_INIT(node,node_idname_t,fuse_node_t*,1,idname_hash_func,idname_hash_equal);

typedef struct fuse_node_hashtable_t fuse_node_hashtable_t;
struct fuse_node_hashtable_t
{
  kh_node_t *ht;
  uint64_t   id;
  uint64_t   generation;
};

static
inline
khint_t
idname_hash_func(const node_idname_t idname_)
{
  if(idname_.name == NULL)
    return idname_.id;
  return (idname_.id ^ kh_str_hash_func(idname_.name));
}

static
inline
int
idname_hash_equal(const node_idname_t idname0_,
                  const node_idname_t idname1_)
{
  return ((idname0_.id == idname1_.id) &&
          ((idname0_.name == idname1_.name) ||
           (strcmp(idname0_.name,idname1_.name) == 0)));
}

static
inline
fuse_node_t*
fuse_node_alloc(const uint64_t  id_,
                const char     *name_)
{
  fuse_node_t *node;

  node = (fuse_node_t*)calloc(1,sizeof(fuse_node_t));

  node->id           = id_;
  node->name         = strdup(name_);
  node->ref_count    = 1;
  node->lookup_count = 1;

  return node;
}

static
inline
void
fuse_node_free(fuse_node_t *node_)
{
  free(node_->name);
  free(node_);
}

static
inline
uint64_t
rand64()
{
  uint64_t rv;

  rv   = rand();
  rv <<= 32;
  rv  |= rand();

  return rv;
}

static
inline
void
node_hashtable_gen_unique_id(fuse_node_hashtable_t *ht_)
{
  do
    {
      ht_->id++;
      if(ht_->id == 0)
        ht_->generation++;
    }
  while((ht_->id == 0) || (ht_->id == UNKNOWN_INO));
}

static
inline
void
node_hashtable_put_root(fuse_node_hashtable_t *ht_)
{
  int rv;
  khint_t k;
  fuse_node_t *root_node;
  const node_idname_t idname0 = {ROOT_NODE_ID,""};
  const node_idname_t idname1 = {ROOT_NODE_ID,ROOT_NODE_NAME};

  root_node = fuse_node_alloc(ROOT_NODE_ID,ROOT_NODE_NAME);

  k = kh_put_node(ht_->ht,idname0,&rv);
  kh_value(ht_->ht,k) = root_node;

  k = kh_put_node(ht_->ht,idname1,&rv);
  kh_value(ht_->ht,k) = root_node;
}

static
inline
void
node_hashtable_set_id_gen(fuse_node_hashtable_t *ht_,
                          fuse_node_t           *node_)
{
  node_hashtable_gen_unique_id(ht_);
  node_->id         = ht_->id;
  node_->generation = ht_->generation;
}

fuse_node_hashtable_t*
fuse_node_hashtable_init()
{
  fuse_node_hashtable_t *ht;

  ht = (fuse_node_hashtable_t*)calloc(sizeof(fuse_node_hashtable_t),1);
  if(ht == NULL)
    return NULL;

  ht->ht = kh_init_node();
  if(ht->ht == NULL)
    {
      free(ht);
      return NULL;
    }

  srand(time(NULL));
  ht->id         = 0;
  ht->generation = rand64();

  node_hashtable_put_root(ht);

  return ht;
}

fuse_node_t*
fuse_node_hashtable_put(fuse_node_hashtable_t *ht_,
                        const uint64_t         parent_id_,
                        const uint64_t         child_id_,
                        const char            *child_name_)
{
  int rv;
  khint_t k;
  fuse_node_t *child_node;
  const node_idname_t p_idname  = {parent_id_,""};
  const node_idname_t c0_idname = {child_id_,child_name_};
  const node_idname_t c1_idname = {parent_id_,child_name_};

  child_node = fuse_node_alloc(child_id_,child_name_);

  k = kh_get_node(ht_->ht,p_idname);
  child_node->parent = kh_value(ht_->ht,k);
  child_node->parent->ref_count++;

  k = kh_put_node(ht_->ht,c0_idname,&rv);
  kh_value(ht_->ht,k) = child_node;

  k = kh_put_node(ht_->ht,c1_idname,&rv);
  kh_value(ht_->ht,k) = child_node;

  return child_node;
}

fuse_node_t*
fuse_node_hashtable_get(fuse_node_hashtable_t *ht_,
                        const uint64_t         id_)
{
  return fuse_node_hashtable_get_child(ht_,id_,"");
}

fuse_node_t*
fuse_node_hashtable_get_child(fuse_node_hashtable_t *ht_,
                              const uint64_t         parent_id_,
                              const char            *child_name_)
{
  khint_t k;
  fuse_node_t *node;
  const node_idname_t idname = {parent_id_,child_name_};

  k = kh_get_node(ht_->ht,idname);
  node = ((k != kh_end(ht_->ht)) ?
          kh_value(ht_->ht,k) :
          NULL);

  return node;
}

void
fuse_node_hashtable_del(fuse_node_hashtable_t *ht_,
                        fuse_node_t           *node_)
{

}

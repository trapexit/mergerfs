#pragma once

#include "lock.h"

typedef struct node_t node_t;
struct node_t
{
  node_t *name_next;
  node_t *id_next;

  uint64_t nodeid;
  char *name;
  node_t *parent;

  uint64_t nlookup;
  uint32_t refctr;
  uint32_t open_count;

  int32_t treelock;
  lock_t *locks;

  uint32_t stat_crc32b;
  uint8_t is_stat_cache_valid:1;
};

node_t *node_alloc();
void    node_free(node_t*);

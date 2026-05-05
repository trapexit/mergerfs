#pragma once

#include "lock.h"

#include <cstdint>

struct node_t
{
  node_t *name_next;
  node_t *id_next;
  char *name;
  node_t *parent;
  lock_t *locks;

  uint64_t nodeid;
  uint64_t nlookup;

  uint32_t refctr;
  uint32_t open_count;
  uint32_t stat_crc32b;
  int32_t treelock;
};

node_t *node_alloc();
void    node_free(node_t*);
void    node_gc();
void    node_clear();

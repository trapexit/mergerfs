#pragma once

#include <cstdint>

struct node_t
{
  node_t *name_next;
  node_t *id_next;
  char *name;
  node_t *parent;

  uint64_t nodeid;
  uint64_t nlookup;

  uint32_t refctr;
  uint32_t open_count : 31;
  uint32_t remembered : 1;
  uint32_t stat_crc32b;
  int32_t treelock;
};

#if defined(__LP64__) || defined(_LP64)
static_assert(sizeof(node_t) == 64, "node_t expected to be 64 bytes on 64-bit");
#else
static_assert(sizeof(node_t) == 48, "node_t expected to be 48 bytes on 32-bit");
#endif

node_t *node_alloc();
void    node_free(node_t*);
void    node_gc();
void    node_clear();

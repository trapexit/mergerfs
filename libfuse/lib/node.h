#pragma once

#include "lock.h"

struct node
{
  struct node *name_next;
  struct node *id_next;

  uint64_t nodeid;
  char *name;
  struct node *parent;

  uint64_t nlookup;
  uint32_t refctr;
  uint32_t open_count;
  uint64_t hidden_fh;

  int32_t treelock;
  struct lock *locks;

  uint32_t stat_crc32b;
  uint8_t is_stat_cache_valid:1;
};

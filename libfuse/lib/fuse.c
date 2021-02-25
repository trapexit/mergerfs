/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

/* For pthread_rwlock_t */
#define _GNU_SOURCE

#include "config.h"
#include "fuse_i.h"
#include "fuse_lowlevel.h"
#include "fuse_opt.h"
#include "fuse_misc.h"
#include "fuse_kernel.h"
#include "fuse_dirents.h"

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#define FUSE_NODE_SLAB 1

#ifndef MAP_ANONYMOUS
#undef FUSE_NODE_SLAB
#endif

#define FUSE_UNKNOWN_INO UINT64_MAX
#define OFFSET_MAX 0x7fffffffffffffffLL

#define NODE_TABLE_MIN_SIZE 8192

struct fuse_config
{
  unsigned int uid;
  unsigned int gid;
  unsigned int umask;
  int remember;
  int debug;
  int use_ino;
  int set_mode;
  int set_uid;
  int set_gid;
  int help;
  int threads;
};

struct fuse_fs
{
  struct fuse_operations op;
  void *user_data;
  int debug;
};

struct lock_queue_element
{
  struct lock_queue_element *next;
  pthread_cond_t cond;
  fuse_ino_t nodeid1;
  const char *name1;
  char **path1;
  struct node **wnode1;
  fuse_ino_t nodeid2;
  const char *name2;
  char **path2;
  struct node **wnode2;
  int err;
  bool first_locked : 1;
  bool second_locked : 1;
  bool done : 1;
};

struct node_table
{
  struct node **array;
  size_t use;
  size_t size;
  size_t split;
};

#define container_of(ptr,type,member) ({                        \
      const typeof( ((type *)0)->member ) *__mptr = (ptr);      \
      (type *)( (char *)__mptr - offsetof(type,member) );})

#define list_entry(ptr,type,member)             \
  container_of(ptr,type,member)

struct list_head
{
  struct list_head *next;
  struct list_head *prev;
};

struct node_slab
{
  struct list_head list;  /* must be the first member */
  struct list_head freelist;
  int used;
};

struct fuse
{
  struct fuse_session *se;
  struct node_table name_table;
  struct node_table id_table;
  struct list_head lru_table;
  fuse_ino_t ctr;
  uint64_t generation;
  unsigned int hidectr;
  pthread_mutex_t lock;
  struct fuse_config conf;
  int intr_installed;
  struct fuse_fs *fs;
  struct lock_queue_element *lockq;
  int pagesize;
  struct list_head partial_slabs;
  struct list_head full_slabs;
  pthread_t prune_thread;
};

struct lock
{
  int type;
  off_t start;
  off_t end;
  pid_t pid;
  uint64_t owner;
  struct lock *next;
};

struct node
{
  struct node *name_next;
  struct node *id_next;
  fuse_ino_t nodeid;
  uint64_t generation;
  int refctr;
  struct node *parent;
  char *name;
  uint64_t nlookup;
  int open_count;
  struct lock *locks;
  uint64_t hidden_fh;
  char is_hidden;
  int treelock;
  ino_t ino;
  off_t size;
  struct timespec mtim;
  char stat_cache_valid;
  char inline_name[32];
};

#define TREELOCK_WRITE -1
#define TREELOCK_WAIT_OFFSET INT_MIN

struct node_lru
{
  struct node node;
  struct list_head lru;
  struct timespec forget_time;
};

struct fuse_dh
{
  pthread_mutex_t lock;
  uint64_t        fh;
  fuse_dirents_t  d;
};

struct fuse_context_i
{
  struct fuse_context ctx;
  fuse_req_t req;
};

static pthread_key_t fuse_context_key;
static pthread_mutex_t fuse_context_lock = PTHREAD_MUTEX_INITIALIZER;
static int fuse_context_ref;

static
void
init_list_head(struct list_head *list)
{
  list->next = list;
  list->prev = list;
}

static
int
list_empty(const struct list_head *head)
{
  return head->next == head;
}

static
void
list_add(struct list_head *new,
         struct list_head *prev,
         struct list_head *next)
{
  next->prev = new;
  new->next = next;
  new->prev = prev;
  prev->next = new;
}

static
inline
void
list_add_head(struct list_head *new,
              struct list_head *head)
{
  list_add(new,head,head->next);
}

static
inline
void
list_add_tail(struct list_head *new,
              struct list_head *head)
{
  list_add(new,head->prev,head);
}

static
inline
void
list_del(struct list_head *entry)
{
  struct list_head *prev = entry->prev;
  struct list_head *next = entry->next;

  next->prev = prev;
  prev->next = next;
}

static
inline
int
lru_enabled(struct fuse *f)
{
  return f->conf.remember > 0;
}

static
struct
node_lru*
node_lru(struct node *node)
{
  return (struct node_lru*)node;
}

static
size_t
get_node_size(struct fuse *f)
{
  if(lru_enabled(f))
    return sizeof(struct node_lru);
  else
    return sizeof(struct node);
}

#ifdef FUSE_NODE_SLAB
static
struct node_slab*
list_to_slab(struct list_head *head)
{
  return (struct node_slab *)head;
}

static
struct node_slab*
node_to_slab(struct fuse *f,
             struct node *node)
{
  return (struct node_slab *)(((uintptr_t)node) & ~((uintptr_t)f->pagesize - 1));
}

static
int
alloc_slab(struct fuse *f)
{
  void *mem;
  struct node_slab *slab;
  char *start;
  size_t num;
  size_t i;
  size_t node_size = get_node_size(f);

  mem = mmap(NULL,f->pagesize,PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS,-1,0);

  if(mem == MAP_FAILED)
    return -1;

  slab = mem;
  init_list_head(&slab->freelist);
  slab->used = 0;
  num = (f->pagesize - sizeof(struct node_slab)) / node_size;

  start = (char *)mem + f->pagesize - num * node_size;
  for(i = 0; i < num; i++)
    {
      struct list_head *n;

      n = (struct list_head *)(start + i * node_size);
      list_add_tail(n,&slab->freelist);
    }
  list_add_tail(&slab->list,&f->partial_slabs);

  return 0;
}

static
struct node*
alloc_node(struct fuse *f)
{
  struct node_slab *slab;
  struct list_head *node;

  if(list_empty(&f->partial_slabs))
    {
      int res = alloc_slab(f);
      if(res != 0)
        return NULL;
    }
  slab = list_to_slab(f->partial_slabs.next);
  slab->used++;
  node = slab->freelist.next;
  list_del(node);
  if(list_empty(&slab->freelist))
    {
      list_del(&slab->list);
      list_add_tail(&slab->list,&f->full_slabs);
    }
  memset(node,0,sizeof(struct node));

  return (struct node *)node;
}

static
void
free_slab(struct fuse      *f,
          struct node_slab *slab)
{
  int res;

  list_del(&slab->list);
  res = munmap(slab,f->pagesize);
  if(res == -1)
    fprintf(stderr,"fuse warning: munmap(%p) failed\n",slab);
}

static
void
free_node_mem(struct fuse *f,
              struct node *node)
{
  struct node_slab *slab = node_to_slab(f,node);
  struct list_head *n = (struct list_head *)node;

  slab->used--;
  if(slab->used)
    {
      if(list_empty(&slab->freelist))
        {
          list_del(&slab->list);
          list_add_tail(&slab->list,&f->partial_slabs);
        }
      list_add_head(n,&slab->freelist);
    }
  else
    {
      free_slab(f,slab);
    }
}
#else
static
struct node*
alloc_node(struct fuse *f)
{
  return (struct node *)calloc(1,get_node_size(f));
}

static
void
free_node_mem(struct fuse *f,
              struct node *node)
{
  (void)f;
  free(node);
}
#endif

static
size_t
id_hash(struct fuse *f,
        fuse_ino_t   ino)
{
  uint64_t hash = ((uint32_t)ino * 2654435761U) % f->id_table.size;
  uint64_t oldhash = hash % (f->id_table.size / 2);

  if(oldhash >= f->id_table.split)
    return oldhash;
  else
    return hash;
}

static
struct node*
get_node_nocheck(struct fuse *f,
                 fuse_ino_t   nodeid)
{
  size_t hash = id_hash(f,nodeid);
  struct node *node;

  for(node = f->id_table.array[hash]; node != NULL; node = node->id_next)
    if(node->nodeid == nodeid)
      return node;

  return NULL;
}

static
struct node*
get_node(struct fuse      *f,
         const fuse_ino_t  nodeid)
{
  struct node *node = get_node_nocheck(f,nodeid);

  if(!node)
    {
      fprintf(stderr,"fuse internal error: node %llu not found\n",
              (unsigned long long)nodeid);
      abort();
    }

  return node;
}

static void curr_time(struct timespec *now);
static double diff_timespec(const struct timespec *t1,
                            const struct timespec *t2);

static
void
remove_node_lru(struct node *node)
{
  struct node_lru *lnode = node_lru(node);
  list_del(&lnode->lru);
  init_list_head(&lnode->lru);
}

static
void
set_forget_time(struct fuse *f,
                struct node *node)
{
  struct node_lru *lnode = node_lru(node);

  list_del(&lnode->lru);
  list_add_tail(&lnode->lru,&f->lru_table);
  curr_time(&lnode->forget_time);
}

static
void
free_node(struct fuse *f_,
          struct node *node_)
{
  if(node_->name != node_->inline_name)
    free(node_->name);

  if(node_->is_hidden)
    fuse_fs_free_hide(f_->fs,node_->hidden_fh);

  free_node_mem(f_,node_);
}

static
void
node_table_reduce(struct node_table *t)
{
  size_t newsize = t->size / 2;
  void *newarray;

  if(newsize < NODE_TABLE_MIN_SIZE)
    return;

  newarray = realloc(t->array,sizeof(struct node *)* newsize);
  if(newarray != NULL)
    t->array = newarray;

  t->size = newsize;
  t->split = t->size / 2;
}

static
void
remerge_id(struct fuse *f)
{
  struct node_table *t = &f->id_table;
  int iter;

  if(t->split == 0)
    node_table_reduce(t);

  for(iter = 8; t->split > 0 && iter; iter--)
    {
      struct node **upper;

      t->split--;
      upper = &t->array[t->split + t->size / 2];
      if(*upper)
        {
          struct node **nodep;

          for(nodep = &t->array[t->split]; *nodep;
              nodep = &(*nodep)->id_next);

          *nodep = *upper;
          *upper = NULL;
          break;
        }
    }
}

static
void
unhash_id(struct fuse *f,
          struct node *node)
{
  struct node **nodep = &f->id_table.array[id_hash(f,node->nodeid)];

  for(; *nodep != NULL; nodep = &(*nodep)->id_next)
    if(*nodep == node)
      {
        *nodep = node->id_next;
        f->id_table.use--;

        if(f->id_table.use < f->id_table.size / 4)
          remerge_id(f);
        return;
      }
}

static
int
node_table_resize(struct node_table *t)
{
  size_t newsize = t->size * 2;
  void *newarray;

  newarray = realloc(t->array,sizeof(struct node *)* newsize);
  if(newarray == NULL)
    return -1;

  t->array = newarray;
  memset(t->array + t->size,0,t->size * sizeof(struct node *));
  t->size = newsize;
  t->split = 0;

  return 0;
}

static
void
rehash_id(struct fuse *f)
{
  struct node_table *t = &f->id_table;
  struct node **nodep;
  struct node **next;
  size_t hash;

  if(t->split == t->size / 2)
    return;

  hash = t->split;
  t->split++;
  for(nodep = &t->array[hash]; *nodep != NULL; nodep = next)
    {
      struct node *node = *nodep;
      size_t newhash = id_hash(f,node->nodeid);

      if(newhash != hash)
        {
          next = nodep;
          *nodep = node->id_next;
          node->id_next = t->array[newhash];
          t->array[newhash] = node;
        }
      else
        {
          next = &node->id_next;
        }
    }

  if(t->split == t->size / 2)
    node_table_resize(t);
}

static
void
hash_id(struct fuse *f,
        struct node *node)
{
  size_t hash;

  hash = id_hash(f,node->nodeid);
  node->id_next = f->id_table.array[hash];
  f->id_table.array[hash] = node;
  f->id_table.use++;

  if(f->id_table.use >= f->id_table.size / 2)
    rehash_id(f);
}

static
size_t
name_hash(struct fuse *f,
          fuse_ino_t   parent,
          const char  *name)
{
  uint64_t hash = parent;
  uint64_t oldhash;

  for(; *name; name++)
    hash = hash * 31 + (unsigned char)*name;

  hash %= f->name_table.size;
  oldhash = hash % (f->name_table.size / 2);
  if(oldhash >= f->name_table.split)
    return oldhash;
  else
    return hash;
}

static
void
unref_node(struct fuse *f,
           struct node *node);

static
void
remerge_name(struct fuse *f)
{
  int iter;
  struct node_table *t = &f->name_table;

  if(t->split == 0)
    node_table_reduce(t);

  for(iter = 8; t->split > 0 && iter; iter--)
    {
      struct node **upper;

      t->split--;
      upper = &t->array[t->split + t->size / 2];
      if(*upper)
        {
          struct node **nodep;

          for(nodep = &t->array[t->split]; *nodep; nodep = &(*nodep)->name_next);

          *nodep = *upper;
          *upper = NULL;
          break;
        }
    }
}

static
void
unhash_name(struct fuse *f,
            struct node *node)
{
  if(node->name)
    {
      size_t hash = name_hash(f,node->parent->nodeid,node->name);
      struct node **nodep = &f->name_table.array[hash];

      for(; *nodep != NULL; nodep = &(*nodep)->name_next)
        if(*nodep == node)
          {
            *nodep = node->name_next;
            node->name_next = NULL;
            unref_node(f,node->parent);
            if(node->name != node->inline_name)
              free(node->name);
            node->name = NULL;
            node->parent = NULL;
            f->name_table.use--;

            if(f->name_table.use < f->name_table.size / 4)
              remerge_name(f);
            return;
          }

      fprintf(stderr,
              "fuse internal error: unable to unhash node: %llu\n",
              (unsigned long long)node->nodeid);

      abort();
    }
}

static
void
rehash_name(struct fuse *f)
{
  struct node_table *t = &f->name_table;
  struct node **nodep;
  struct node **next;
  size_t hash;

  if(t->split == t->size / 2)
    return;

  hash = t->split;
  t->split++;
  for(nodep = &t->array[hash]; *nodep != NULL; nodep = next)
    {
      struct node *node = *nodep;
      size_t newhash = name_hash(f,node->parent->nodeid,node->name);

      if(newhash != hash)
        {
          next = nodep;
          *nodep = node->name_next;
          node->name_next = t->array[newhash];
          t->array[newhash] = node;
        }
      else
        {
          next = &node->name_next;
        }
    }

  if(t->split == t->size / 2)
    node_table_resize(t);
}

static
int
hash_name(struct fuse *f,
          struct node *node,
          fuse_ino_t   parentid,
          const char  *name)
{
  size_t hash = name_hash(f,parentid,name);
  struct node *parent = get_node(f,parentid);
  if(strlen(name) < sizeof(node->inline_name))
    {
      strcpy(node->inline_name,name);
      node->name = node->inline_name;
    }
  else
    {
      node->name = strdup(name);
      if(node->name == NULL)
        return -1;
    }

  parent->refctr ++;
  node->parent = parent;
  node->name_next = f->name_table.array[hash];
  f->name_table.array[hash] = node;
  f->name_table.use++;

  if(f->name_table.use >= f->name_table.size / 2)
    rehash_name(f);

  return 0;
}

static
void
delete_node(struct fuse *f,
            struct node *node)
{
  if(f->conf.debug)
    fprintf(stderr,"DELETE: %llu\n",(unsigned long long)node->nodeid);

  assert(node->treelock == 0);
  unhash_name(f,node);
  if(lru_enabled(f))
    remove_node_lru(node);
  unhash_id(f,node);
  free_node(f,node);
}

static
void
unref_node(struct fuse *f,
           struct node *node)
{
  assert(node->refctr > 0);
  node->refctr--;
  if(!node->refctr)
    delete_node(f,node);
}

static
uint64_t
rand64(void)
{
  uint64_t rv;

  rv   = rand();
  rv <<= 32;
  rv  |= rand();

  return rv;
}

static
fuse_ino_t
next_id(struct fuse *f)
{
  do
    {
      f->ctr = ((f->ctr + 1) & UINT64_MAX);
      if(f->ctr == 0)
        f->generation++;
    } while((f->ctr == 0)                ||
            (f->ctr == FUSE_UNKNOWN_INO) ||
            (get_node_nocheck(f,f->ctr) != NULL));

  return f->ctr;
}

static
struct node*
lookup_node(struct fuse *f,
            fuse_ino_t   parent,
            const char  *name)
{
  size_t hash;
  struct node *node;

  hash =  name_hash(f,parent,name);
  for(node = f->name_table.array[hash]; node != NULL; node = node->name_next)
    if(node->parent->nodeid == parent && strcmp(node->name,name) == 0)
      return node;

  return NULL;
}

static
void
inc_nlookup(struct node *node)
{
  if(!node->nlookup)
    node->refctr++;
  node->nlookup++;
}

static
struct node*
find_node(struct fuse *f,
          fuse_ino_t   parent,
          const char  *name)
{
  struct node *node;

  pthread_mutex_lock(&f->lock);
  if(!name)
    node = get_node(f,parent);
  else
    node = lookup_node(f,parent,name);
  if(node == NULL)
    {
      node = alloc_node(f);
      if(node == NULL)
        goto out_err;

      node->nodeid = next_id(f);
      node->generation = f->generation;
      if(f->conf.remember)
        inc_nlookup(node);

      if(hash_name(f,node,parent,name) == -1)
        {
          free_node(f,node);
          node = NULL;
          goto out_err;
        }
      hash_id(f,node);
      if(lru_enabled(f))
        {
          struct node_lru *lnode = node_lru(node);
          init_list_head(&lnode->lru);
        }
    }
  else if(lru_enabled(f) && node->nlookup == 1)
    {
      remove_node_lru(node);
    }
  inc_nlookup(node);
 out_err:
  pthread_mutex_unlock(&f->lock);
  return node;
}

static
char*
add_name(char       **buf,
         unsigned    *bufsize,
         char        *s,
         const char  *name)
{
  size_t len = strlen(name);

  if(s - len <= *buf)
    {
      unsigned pathlen = *bufsize - (s - *buf);
      unsigned newbufsize = *bufsize;
      char *newbuf;

      while(newbufsize < pathlen + len + 1)
        {
          if(newbufsize >= 0x80000000)
            newbufsize = 0xffffffff;
          else
            newbufsize *= 2;
        }

      newbuf = realloc(*buf,newbufsize);
      if(newbuf == NULL)
        return NULL;

      *buf = newbuf;
      s = newbuf + newbufsize - pathlen;
      memmove(s,newbuf + *bufsize - pathlen,pathlen);
      *bufsize = newbufsize;
    }
  s -= len;
  strncpy(s,name,len);
  s--;
  *s = '/';

  return s;
}

static
void
unlock_path(struct fuse *f,
            fuse_ino_t   nodeid,
            struct node *wnode,
            struct node *end)
{
  struct node *node;

  if(wnode)
    {
      assert(wnode->treelock == TREELOCK_WRITE);
      wnode->treelock = 0;
    }

  for(node = get_node(f,nodeid); node != end && node->nodeid != FUSE_ROOT_ID; node = node->parent)
    {
      assert(node->treelock != 0);
      assert(node->treelock != TREELOCK_WAIT_OFFSET);
      assert(node->treelock != TREELOCK_WRITE);
      node->treelock--;
      if(node->treelock == TREELOCK_WAIT_OFFSET)
        node->treelock = 0;
    }
}

static
int
try_get_path(struct fuse  *f,
             fuse_ino_t    nodeid,
             const char   *name,
             char        **path,
             struct node **wnodep,
             bool          need_lock)
{
  unsigned bufsize = 256;
  char *buf;
  char *s;
  struct node *node;
  struct node *wnode = NULL;
  int err;

  *path = NULL;

  err = -ENOMEM;
  buf = malloc(bufsize);
  if(buf == NULL)
    goto out_err;

  s = buf + bufsize - 1;
  *s = '\0';

  if(name != NULL)
    {
      s = add_name(&buf,&bufsize,s,name);
      err = -ENOMEM;
      if(s == NULL)
        goto out_free;
    }

  if(wnodep)
    {
      assert(need_lock);
      wnode = lookup_node(f,nodeid,name);
      if(wnode)
        {
          if(wnode->treelock != 0)
            {
              if(wnode->treelock > 0)
                wnode->treelock += TREELOCK_WAIT_OFFSET;
              err = -EAGAIN;
              goto out_free;
            }
          wnode->treelock = TREELOCK_WRITE;
        }
    }

  for(node = get_node(f,nodeid); node->nodeid != FUSE_ROOT_ID; node = node->parent)
    {
      err = -ENOENT;
      if(node->name == NULL || node->parent == NULL)
        goto out_unlock;

      err = -ENOMEM;
      s = add_name(&buf,&bufsize,s,node->name);
      if(s == NULL)
        goto out_unlock;

      if(need_lock)
        {
          err = -EAGAIN;
          if(node->treelock < 0)
            goto out_unlock;

          node->treelock++;
        }
    }

  if(s[0])
    memmove(buf,s,bufsize - (s - buf));
  else
    strcpy(buf,"/");

  *path = buf;
  if(wnodep)
    *wnodep = wnode;

  return 0;

 out_unlock:
  if(need_lock)
    unlock_path(f,nodeid,wnode,node);
 out_free:
  free(buf);

 out_err:
  return err;
}

static
void
queue_element_unlock(struct fuse               *f,
                     struct lock_queue_element *qe)
{
  struct node *wnode;

  if(qe->first_locked)
    {
      wnode = qe->wnode1 ? *qe->wnode1 : NULL;
      unlock_path(f,qe->nodeid1,wnode,NULL);
      qe->first_locked = false;
    }

  if(qe->second_locked)
    {
      wnode = qe->wnode2 ? *qe->wnode2 : NULL;
      unlock_path(f,qe->nodeid2,wnode,NULL);
      qe->second_locked = false;
    }
}

static
void
queue_element_wakeup(struct fuse               *f,
                     struct lock_queue_element *qe)
{
  int err;
  bool first = (qe == f->lockq);

  if(!qe->path1)
    {
      /* Just waiting for it to be unlocked */
      if(get_node(f,qe->nodeid1)->treelock == 0)
        pthread_cond_signal(&qe->cond);

      return;
    }

  if(!qe->first_locked)
    {
      err = try_get_path(f,qe->nodeid1,qe->name1,qe->path1,qe->wnode1,true);
      if(!err)
        qe->first_locked = true;
      else if(err != -EAGAIN)
        goto err_unlock;
    }

  if(!qe->second_locked && qe->path2)
    {
      err = try_get_path(f,qe->nodeid2,qe->name2,qe->path2,qe->wnode2,true);
      if(!err)
        qe->second_locked = true;
      else if(err != -EAGAIN)
        goto err_unlock;
    }

  if(qe->first_locked && (qe->second_locked || !qe->path2))
    {
      err = 0;
      goto done;
    }

  /*
   * Only let the first element be partially locked otherwise there could
   * be a deadlock.
   *
   * But do allow the first element to be partially locked to prevent
   * starvation.
   */
  if(!first)
    queue_element_unlock(f,qe);

  /* keep trying */
  return;

 err_unlock:
  queue_element_unlock(f,qe);
 done:
  qe->err = err;
  qe->done = true;
  pthread_cond_signal(&qe->cond);
}

static
void
wake_up_queued(struct fuse *f)
{
  struct lock_queue_element *qe;

  for(qe = f->lockq; qe != NULL; qe = qe->next)
    queue_element_wakeup(f,qe);
}

static
void
debug_path(struct fuse *f,
           const char  *msg,
           fuse_ino_t   nodeid,
           const char  *name,
           bool         wr)
{
  if(f->conf.debug)
    {
      struct node *wnode = NULL;

      if(wr)
        wnode = lookup_node(f,nodeid,name);

      if(wnode)
        fprintf(stderr,"%s %li (w)\n",	msg,wnode->nodeid);
      else
        fprintf(stderr,"%s %li\n",msg,nodeid);
    }
}

static
void
queue_path(struct fuse               *f,
           struct lock_queue_element *qe)
{
  struct lock_queue_element **qp;

  qe->done = false;
  qe->first_locked = false;
  qe->second_locked = false;
  pthread_cond_init(&qe->cond,NULL);
  qe->next = NULL;
  for(qp = &f->lockq; *qp != NULL; qp = &(*qp)->next);
  *qp = qe;
}

static
void
dequeue_path(struct fuse               *f,
             struct lock_queue_element *qe)
{
  struct lock_queue_element **qp;

  pthread_cond_destroy(&qe->cond);
  for(qp = &f->lockq; *qp != qe; qp = &(*qp)->next);
  *qp = qe->next;
}

static
int
wait_path(struct fuse               *f,
          struct lock_queue_element *qe)
{
  queue_path(f,qe);

  do
    {
      pthread_cond_wait(&qe->cond,&f->lock);
    } while(!qe->done);

  dequeue_path(f,qe);

  return qe->err;
}

static
int
get_path_common(struct fuse  *f,
                fuse_ino_t    nodeid,
                const char   *name,
                char        **path,
                struct node **wnode)
{
  int err;

  pthread_mutex_lock(&f->lock);
  err = try_get_path(f,nodeid,name,path,wnode,true);
  if(err == -EAGAIN)
    {
      struct lock_queue_element qe = {0};

      qe.nodeid1 = nodeid;
      qe.name1   = name;
      qe.path1   = path;
      qe.wnode1  = wnode;

      debug_path(f,"QUEUE PATH",nodeid,name,!!wnode);
      err = wait_path(f,&qe);
      debug_path(f,"DEQUEUE PATH",nodeid,name,!!wnode);
    }
  pthread_mutex_unlock(&f->lock);

  return err;
}

static
int
get_path(struct fuse  *f,
         fuse_ino_t    nodeid,
         char        **path)
{
  return get_path_common(f,nodeid,NULL,path,NULL);
}

static
int
get_path_name(struct fuse  *f,
              fuse_ino_t    nodeid,
              const char   *name,
              char        **path)
{
  return get_path_common(f,nodeid,name,path,NULL);
}

static
int
get_path_wrlock(struct fuse  *f,
                fuse_ino_t    nodeid,
                const char   *name,
                char        **path,
                struct node **wnode)
{
  return get_path_common(f,nodeid,name,path,wnode);
}

static
int
try_get_path2(struct fuse  *f,
              fuse_ino_t    nodeid1,
              const char   *name1,
              fuse_ino_t    nodeid2,
              const char   *name2,
              char        **path1,
              char        **path2,
              struct node **wnode1,
              struct node **wnode2)
{
  int err;

  /* FIXME: locking two paths needs deadlock checking */
  err = try_get_path(f,nodeid1,name1,path1,wnode1,true);
  if(!err)
    {
      err = try_get_path(f,nodeid2,name2,path2,wnode2,true);
      if(err)
        {
          struct node *wn1 = wnode1 ? *wnode1 : NULL;

          unlock_path(f,nodeid1,wn1,NULL);
          free(*path1);
        }
    }

  return err;
}

static
int
get_path2(struct fuse  *f,
          fuse_ino_t    nodeid1,
          const char   *name1,
          fuse_ino_t    nodeid2,
          const char   *name2,
          char        **path1,
          char        **path2,
          struct node **wnode1,
          struct node **wnode2)
{
  int err;

  pthread_mutex_lock(&f->lock);
  err = try_get_path2(f,nodeid1,name1,nodeid2,name2,
                      path1,path2,wnode1,wnode2);
  if(err == -EAGAIN)
    {
      struct lock_queue_element qe = {0};

      qe.nodeid1 = nodeid1;
      qe.name1   = name1;
      qe.path1   = path1;
      qe.wnode1  = wnode1;
      qe.nodeid2 = nodeid2;
      qe.name2   = name2;
      qe.path2   = path2;
      qe.wnode2  = wnode2;

      debug_path(f,"QUEUE PATH1",nodeid1,name1,!!wnode1);
      debug_path(f,"      PATH2",nodeid2,name2,!!wnode2);
      err = wait_path(f,&qe);
      debug_path(f,"DEQUEUE PATH1",nodeid1,name1,!!wnode1);
      debug_path(f,"        PATH2",nodeid2,name2,!!wnode2);
    }
  pthread_mutex_unlock(&f->lock);

  return err;
}

static
void
free_path_wrlock(struct fuse *f,
                 fuse_ino_t   nodeid,
                 struct node *wnode,
                 char        *path)
{
  pthread_mutex_lock(&f->lock);
  unlock_path(f,nodeid,wnode,NULL);
  if(f->lockq)
    wake_up_queued(f);
  pthread_mutex_unlock(&f->lock);
  free(path);
}

static
void
free_path(struct fuse *f,
          fuse_ino_t   nodeid,
          char        *path)
{
  if(path)
    free_path_wrlock(f,nodeid,NULL,path);
}

static
void
free_path2(struct fuse *f,
           fuse_ino_t   nodeid1,
           fuse_ino_t   nodeid2,
           struct node *wnode1,
           struct node *wnode2,
           char        *path1,
           char        *path2)
{
  pthread_mutex_lock(&f->lock);
  unlock_path(f,nodeid1,wnode1,NULL);
  unlock_path(f,nodeid2,wnode2,NULL);
  wake_up_queued(f);
  pthread_mutex_unlock(&f->lock);
  free(path1);
  free(path2);
}

static
void
forget_node(struct fuse      *f,
            const fuse_ino_t  nodeid,
            const uint64_t    nlookup)
{
  struct node *node;

  if(nodeid == FUSE_ROOT_ID)
    return;

  pthread_mutex_lock(&f->lock);
  node = get_node(f,nodeid);

  /*
   * Node may still be locked due to interrupt idiocy in open,
   * create and opendir
   */
  while(node->nlookup == nlookup && node->treelock)
    {
      struct lock_queue_element qe = {0};

      qe.nodeid1 = nodeid;

      debug_path(f,"QUEUE PATH (forget)",nodeid,NULL,false);
      queue_path(f,&qe);

      do
        {
          pthread_cond_wait(&qe.cond,&f->lock);
        }
      while((node->nlookup == nlookup) && node->treelock);

      dequeue_path(f,&qe);
      debug_path(f,"DEQUEUE_PATH (forget)",nodeid,NULL,false);
    }

  assert(node->nlookup >= nlookup);
  node->nlookup -= nlookup;
  if(!node->nlookup)
    unref_node(f,node);
  else if(lru_enabled(f) && node->nlookup == 1)
    set_forget_time(f,node);

  pthread_mutex_unlock(&f->lock);
}

static
void
unlink_node(struct fuse *f,
            struct node *node)
{
  if(f->conf.remember)
    {
      assert(node->nlookup > 1);
      node->nlookup--;
    }
  unhash_name(f,node);
}

static
void
remove_node(struct fuse *f,
            fuse_ino_t   dir,
            const char  *name)
{
  struct node *node;

  pthread_mutex_lock(&f->lock);
  node = lookup_node(f,dir,name);
  if(node != NULL)
    unlink_node(f,node);
  pthread_mutex_unlock(&f->lock);
}

static
int
rename_node(struct fuse *f,
            fuse_ino_t   olddir,
            const char  *oldname,
            fuse_ino_t   newdir,
            const char  *newname)
{
  struct node *node;
  struct node *newnode;
  int err = 0;

  pthread_mutex_lock(&f->lock);
  node = lookup_node(f,olddir,oldname);
  newnode = lookup_node(f,newdir,newname);
  if(node == NULL)
    goto out;

  if(newnode != NULL)
    unlink_node(f,newnode);

  unhash_name(f,node);
  if(hash_name(f,node,newdir,newname) == -1)
    {
      err = -ENOMEM;
      goto out;
    }

 out:
  pthread_mutex_unlock(&f->lock);
  return err;
}

static
void
set_stat(struct fuse *f,
         fuse_ino_t   nodeid,
         struct stat *stbuf)
{
  if(!f->conf.use_ino)
    stbuf->st_ino = nodeid;
  if(f->conf.set_mode)
    stbuf->st_mode = (stbuf->st_mode & S_IFMT) | (0777 & ~f->conf.umask);
  if(f->conf.set_uid)
    stbuf->st_uid = f->conf.uid;
  if(f->conf.set_gid)
    stbuf->st_gid = f->conf.gid;
}

static
struct fuse*
req_fuse(fuse_req_t req)
{
  return (struct fuse*)fuse_req_userdata(req);
}

int
fuse_fs_getattr(struct fuse_fs  *fs,
                const char      *path,
                struct stat     *buf,
                fuse_timeouts_t *timeout)
{
  if(fs->op.getattr == NULL)
    return -ENOSYS;

  if(fs->debug)
    fprintf(stderr,"getattr %s\n",path);

  fuse_get_context()->private_data = fs->user_data;

  return fs->op.getattr(path,buf,timeout);
}

int
fuse_fs_fgetattr(struct fuse_fs   *fs,
                 struct stat      *buf,
                 fuse_file_info_t *fi,
                 fuse_timeouts_t  *timeout)
{
  if(fs->op.fgetattr == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;
  if(fs->debug)
    fprintf(stderr,"fgetattr[%llu]\n",(unsigned long long)fi->fh);

  return fs->op.fgetattr(fi,buf,timeout);
}

int
fuse_fs_rename(struct fuse_fs *fs,
               const char     *oldpath,
               const char     *newpath)
{
  if(fs->op.rename == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  return fs->op.rename(oldpath,newpath);
}

int
fuse_fs_prepare_hide(struct fuse_fs *fs_,
                     const char     *path_,
                     uint64_t       *fh_)
{
  if(fs_->op.prepare_hide == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.prepare_hide(path_,fh_);
}

int
fuse_fs_free_hide(struct fuse_fs *fs_,
                  uint64_t        fh_)
{
  if(fs_->op.free_hide == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.free_hide(fh_);
}

int
fuse_fs_unlink(struct fuse_fs *fs,
               const char     *path)
{
  if(fs->op.unlink == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"unlink %s\n",path);

  return fs->op.unlink(path);
}

int
fuse_fs_rmdir(struct fuse_fs *fs,
              const char     *path)
{
  if(fs->op.rmdir == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"rmdir %s\n",path);

  return fs->op.rmdir(path);
}

int
fuse_fs_symlink(struct fuse_fs  *fs_,
                const char      *linkname_,
                const char      *path_,
                struct stat     *st_,
                fuse_timeouts_t *timeouts_)
{

  if(fs_->op.symlink == NULL)
    return -ENOSYS;

  if(fs_->debug)
    fprintf(stderr,"symlink %s %s\n",linkname_,path_);

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.symlink(linkname_,path_,st_,timeouts_);
}

int
fuse_fs_link(struct fuse_fs  *fs,
             const char      *oldpath,
             const char      *newpath,
             struct stat     *st_,
             fuse_timeouts_t *timeouts_)
{
  if(fs->op.link == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"link %s %s\n",oldpath,newpath);

  return fs->op.link(oldpath,newpath,st_,timeouts_);
}

int
fuse_fs_release(struct fuse_fs   *fs,
                fuse_file_info_t *fi)
{
  if(fs->op.release == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"release%s[%llu] flags: 0x%x\n",
            fi->flush ? "+flush" : "",
            (unsigned long long)fi->fh,fi->flags);

  return fs->op.release(fi);
}

int
fuse_fs_opendir(struct fuse_fs   *fs,
                const char       *path,
                fuse_file_info_t *fi)
{
  int err;

  if(fs->op.opendir == NULL)
    return 0;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"opendir flags: 0x%x %s\n",fi->flags,path);

  err = fs->op.opendir(path,fi);

  if(fs->debug && !err)
    fprintf(stderr,"   opendir[%lli] flags: 0x%x %s\n",
            (unsigned long long)fi->fh,fi->flags,path);

  return err;
}

int
fuse_fs_open(struct fuse_fs   *fs,
             const char       *path,
             fuse_file_info_t *fi)
{
  int err;

  if(fs->op.open == NULL)
    return 0;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"open flags: 0x%x %s\n",fi->flags,path);

  err = fs->op.open(path,fi);

  if(fs->debug && !err)
    fprintf(stderr,"   open[%lli] flags: 0x%x %s\n",
            (unsigned long long)fi->fh,fi->flags,path);

  return err;
}

static
void
fuse_free_buf(struct fuse_bufvec *buf)
{
  if(buf != NULL)
    {
      size_t i;

      for(i = 0; i < buf->count; i++)
        free(buf->buf[i].mem);
      free(buf);
    }
}

int
fuse_fs_read_buf(struct fuse_fs      *fs,
                 struct fuse_bufvec **bufp,
                 size_t               size,
                 off_t                off,
                 fuse_file_info_t    *fi)
{
  fuse_get_context()->private_data = fs->user_data;
  if(fs->op.read || fs->op.read_buf)
    {
      int res;

      if(fs->debug)
        fprintf(stderr,
                "read[%llu] %zu bytes from %llu flags: 0x%x\n",
                (unsigned long long)fi->fh,
                size,(unsigned long long)off,fi->flags);

      if(fs->op.read_buf)
        {
          res = fs->op.read_buf(fi,bufp,size,off);
        }
      else
        {
          struct fuse_bufvec *buf;
          void *mem;

          buf = malloc(sizeof(struct fuse_bufvec));
          if(buf == NULL)
            return -ENOMEM;

          mem = malloc(size);
          if(mem == NULL)
            {
            free(buf);
            return -ENOMEM;
          }

          *buf = FUSE_BUFVEC_INIT(size);
          buf->buf[0].mem = mem;
          *bufp = buf;

          res = fs->op.read(fi,mem,size,off);
          if(res >= 0)
            buf->buf[0].size = res;
        }

      if(fs->debug && res >= 0)
        fprintf(stderr,"   read[%llu] %zu bytes from %llu\n",
                (unsigned long long)fi->fh,
                fuse_buf_size(*bufp),
                (unsigned long long)off);
      if(res >= 0 && fuse_buf_size(*bufp) > (int)size)
        fprintf(stderr,"fuse: read too many bytes\n");

      if(res < 0)
        return res;

      return 0;
    }
  else
    {
      return -ENOSYS;
    }
}

int
fuse_fs_read(struct fuse_fs   *fs,
             char             *mem,
             size_t            size,
             off_t             off,
             fuse_file_info_t *fi)
{
  int res;
  struct fuse_bufvec *buf = NULL;

  res = fuse_fs_read_buf(fs,&buf,size,off,fi);
  if(res == 0)
    {
      struct fuse_bufvec dst = FUSE_BUFVEC_INIT(size);

      dst.buf[0].mem = mem;
      res = fuse_buf_copy(&dst,buf,0);
    }
  fuse_free_buf(buf);

  return res;
}

int
fuse_fs_write_buf(struct fuse_fs     *fs,
                  struct fuse_bufvec *buf,
                  off_t               off,
                  fuse_file_info_t   *fi)
{
  fuse_get_context()->private_data = fs->user_data;
  if(fs->op.write_buf || fs->op.write)
    {
      int res;
      size_t size = fuse_buf_size(buf);

      assert(buf->idx == 0 && buf->off == 0);
      if(fs->debug)
        fprintf(stderr,
                "write%s[%llu] %zu bytes to %llu flags: 0x%x\n",
                fi->writepage ? "page" : "",
                (unsigned long long)fi->fh,
                size,
                (unsigned long long)off,
                fi->flags);

      if(fs->op.write_buf)
        {
          res = fs->op.write_buf(fi,buf,off);
        }
      else
        {
          void *mem = NULL;
          struct fuse_buf *flatbuf;
          struct fuse_bufvec tmp = FUSE_BUFVEC_INIT(size);

          if(buf->count == 1 && !(buf->buf[0].flags & FUSE_BUF_IS_FD))
            {
              flatbuf = &buf->buf[0];
            }
          else
            {
              res = -ENOMEM;
              mem = malloc(size);
              if(mem == NULL)
                goto out;

              tmp.buf[0].mem = mem;
              res = fuse_buf_copy(&tmp,buf,0);
              if(res <= 0)
                goto out_free;

              tmp.buf[0].size = res;
              flatbuf = &tmp.buf[0];
            }

          res = fs->op.write(fi,flatbuf->mem,flatbuf->size,off);
        out_free:
          free(mem);
        }
    out:
      if(fs->debug && res >= 0)
        fprintf(stderr,"   write%s[%llu] %u bytes to %llu\n",
                fi->writepage ? "page" : "",
                (unsigned long long)fi->fh,res,
                (unsigned long long)off);
      if(res > (int)size)
        fprintf(stderr,"fuse: wrote too many bytes\n");

      return res;
    }
  else
    {
      return -ENOSYS;
    }
}

int
fuse_fs_write(struct fuse_fs   *fs,
              const char       *mem,
              size_t            size,
              off_t             off,
              fuse_file_info_t *fi)
{
  struct fuse_bufvec bufv = FUSE_BUFVEC_INIT(size);

  bufv.buf[0].mem = (void *)mem;

  return fuse_fs_write_buf(fs,&bufv,off,fi);
}

int
fuse_fs_fsync(struct fuse_fs   *fs,
              int               datasync,
              fuse_file_info_t *fi)
{
  if(fs->op.fsync == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"fsync[%llu] datasync: %i\n",
            (unsigned long long)fi->fh,datasync);

  return fs->op.fsync(fi,datasync);
}

int
fuse_fs_fsyncdir(struct fuse_fs   *fs,
                 int               datasync,
                 fuse_file_info_t *fi)
{
  if(fs->op.fsyncdir == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"fsyncdir[%llu] datasync: %i\n",
            (unsigned long long)fi->fh,datasync);

  return fs->op.fsyncdir(fi,datasync);
}

int
fuse_fs_flush(struct fuse_fs   *fs,
              fuse_file_info_t *fi)
{
  if(fs->op.flush == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"flush[%llu]\n",
            (unsigned long long)fi->fh);

  return fs->op.flush(fi);
}

int
fuse_fs_statfs(struct fuse_fs *fs,
               const char     *path,
               struct statvfs *buf)
{
  if(fs->op.statfs == NULL)
    {
      buf->f_namemax = 255;
      buf->f_bsize = 512;
      return 0;
    }

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"statfs %s\n",path);

  return fs->op.statfs(path,buf);
}

int
fuse_fs_releasedir(struct fuse_fs   *fs,
                   fuse_file_info_t *fi)
{
  if(fs->op.releasedir == NULL)
    return 0;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"releasedir[%llu] flags: 0x%x\n",
            (unsigned long long)fi->fh,fi->flags);

  return fs->op.releasedir(fi);
}

int
fuse_fs_readdir(struct fuse_fs   *fs,
                fuse_file_info_t *fi,
                fuse_dirents_t   *buf)
{
  if(fs->op.readdir == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  return fs->op.readdir(fi,buf);
}

int
fuse_fs_readdir_plus(struct fuse_fs   *fs_,
                     fuse_file_info_t *ffi_,
                     fuse_dirents_t   *buf_)
{
  if(fs_->op.readdir_plus == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.readdir_plus(ffi_,buf_);
}

int
fuse_fs_create(struct fuse_fs   *fs,
               const char       *path,
               mode_t            mode,
               fuse_file_info_t *fi)
{
  int err;

  if(fs->op.create == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,
            "create flags: 0x%x %s 0%o umask=0%03o\n",
            fi->flags,path,mode,
            fuse_get_context()->umask);

  err = fs->op.create(path,mode,fi);

  if(fs->debug && !err)
    fprintf(stderr,"   create[%llu] flags: 0x%x %s\n",
            (unsigned long long)fi->fh,fi->flags,path);

  return err;
}

int
fuse_fs_lock(struct fuse_fs   *fs,
             fuse_file_info_t *fi,
             int               cmd,
             struct flock     *lock)
{
  if(fs->op.lock == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"lock[%llu] %s %s start: %llu len: %llu pid: %llu\n",
            (unsigned long long)fi->fh,
            (cmd == F_GETLK ? "F_GETLK" :
             (cmd == F_SETLK ? "F_SETLK" :
              (cmd == F_SETLKW ? "F_SETLKW" : "???"))),
            (lock->l_type == F_RDLCK ? "F_RDLCK" :
             (lock->l_type == F_WRLCK ? "F_WRLCK" :
              (lock->l_type == F_UNLCK ? "F_UNLCK" :
               "???"))),
            (unsigned long long)lock->l_start,
            (unsigned long long)lock->l_len,
            (unsigned long long)lock->l_pid);

  return fs->op.lock(fi,cmd,lock);
}

int
fuse_fs_flock(struct fuse_fs   *fs,
              fuse_file_info_t *fi,
              int               op)
{
  if(fs->op.flock == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    {
      int xop = op & ~LOCK_NB;

      fprintf(stderr,"lock[%llu] %s%s\n",
              (unsigned long long)fi->fh,
              xop == LOCK_SH ? "LOCK_SH" :
              (xop == LOCK_EX ? "LOCK_EX" :
               (xop == LOCK_UN ? "LOCK_UN" : "???")),
              (op & LOCK_NB) ? "|LOCK_NB" : "");
    }

  return fs->op.flock(fi,op);
}

int
fuse_fs_chown(struct fuse_fs *fs,
              const char     *path,
              uid_t           uid,
              gid_t           gid)
{
  if(fs->op.chown == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"chown %s %lu %lu\n",path,
            (unsigned long)uid,(unsigned long)gid);

  return fs->op.chown(path,uid,gid);
}

int
fuse_fs_fchown(struct fuse_fs         *fs_,
               const fuse_file_info_t *ffi_,
               const uid_t             uid_,
               const gid_t             gid_)
{
  if(fs_->op.fchown == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;


  return fs_->op.fchown(ffi_,uid_,gid_);
}

int
fuse_fs_truncate(struct fuse_fs *fs,
                 const char     *path,
                 off_t           size)
{
  if(fs->op.truncate == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"truncate %s %llu\n",path,
            (unsigned long long)size);

  return fs->op.truncate(path,size);
}

int
fuse_fs_ftruncate(struct fuse_fs   *fs,
                  off_t             size,
                  fuse_file_info_t *fi)
{
  if(fs->op.ftruncate == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"ftruncate[%llu] %llu\n",
            (unsigned long long)fi->fh,
            (unsigned long long)size);

  return fs->op.ftruncate(fi,size);
}

int
fuse_fs_utimens(struct fuse_fs        *fs,
                const char            *path,
                const struct timespec  tv[2])
{
  fuse_get_context()->private_data = fs->user_data;
  if(fs->op.utimens)
    {
      if(fs->debug)
        fprintf(stderr,"utimens %s %li.%09lu %li.%09lu\n",
                path,tv[0].tv_sec,tv[0].tv_nsec,
                tv[1].tv_sec,tv[1].tv_nsec);

      return fs->op.utimens(path,tv);
    }
  else if(fs->op.utime)
    {
      struct utimbuf buf;

      if(fs->debug)
        fprintf(stderr,"utime %s %li %li\n",path,
                tv[0].tv_sec,tv[1].tv_sec);

      buf.actime = tv[0].tv_sec;
      buf.modtime = tv[1].tv_sec;
      return fs->op.utime(path,&buf);
    }
  else
    {
      return -ENOSYS;
    }
}

int
fuse_fs_futimens(struct fuse_fs         *fs_,
                 const fuse_file_info_t *ffi_,
                 const struct timespec   tv_[2])
{
  if(fs_->op.futimens == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.futimens(ffi_,tv_);
}

int
fuse_fs_access(struct fuse_fs *fs,
               const char     *path,
               int             mask)
{
  if(fs->op.access == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"access %s 0%o\n",path,mask);

  return fs->op.access(path,mask);
}

int
fuse_fs_readlink(struct fuse_fs *fs,
                 const char     *path,
                 char           *buf,
                 size_t          len)
{
  if(fs->op.readlink == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"readlink %s %lu\n",path,
            (unsigned long)len);

  return fs->op.readlink(path,buf,len);
}

int
fuse_fs_mknod(struct fuse_fs *fs,
              const char     *path,
              mode_t          mode,
              dev_t           rdev)
{
  if(fs->op.mknod == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"mknod %s 0%o 0x%llx umask=0%03o\n",
            path,mode,(unsigned long long)rdev,
            fuse_get_context()->umask);

  return fs->op.mknod(path,mode,rdev);
}

int
fuse_fs_mkdir(struct fuse_fs *fs,
              const char     *path,
              mode_t          mode)
{
  if(fs->op.mkdir == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"mkdir %s 0%o umask=0%03o\n",
            path,mode,fuse_get_context()->umask);

  return fs->op.mkdir(path,mode);
}

int
fuse_fs_setxattr(struct fuse_fs *fs,
                 const char     *path,
                 const char     *name,
		 const char     *value,
                 size_t          size,
                 int             flags)
{
  if(fs->op.setxattr == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"setxattr %s %s %lu 0x%x\n",
            path,name,(unsigned long)size,flags);

  return fs->op.setxattr(path,name,value,size,flags);
}

int
fuse_fs_getxattr(struct fuse_fs *fs,
                 const char     *path,
                 const char     *name,
                 char           *value,
                 size_t          size)
{
  if(fs->op.getxattr == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"getxattr %s %s %lu\n",
            path,name,(unsigned long)size);

  return fs->op.getxattr(path,name,value,size);
}

int
fuse_fs_listxattr(struct fuse_fs *fs,
                  const char     *path,
                  char           *list,
                  size_t          size)
{
  if(fs->op.listxattr == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"listxattr %s %lu\n",
            path,(unsigned long)size);

  return fs->op.listxattr(path,list,size);
}

int
fuse_fs_bmap(struct fuse_fs *fs,
             const char     *path,
             size_t          blocksize,
             uint64_t       *idx)
{
  if(fs->op.bmap == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;
  if(fs->debug)
    fprintf(stderr,"bmap %s blocksize: %lu index: %llu\n",
            path,(unsigned long)blocksize,
            (unsigned long long)*idx);

  return fs->op.bmap(path,blocksize,idx);
}

int
fuse_fs_removexattr(struct fuse_fs *fs,
                    const char     *path,
                    const char     *name)
{
  if(fs->op.removexattr == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"removexattr %s %s\n",path,name);

  return fs->op.removexattr(path,name);
}

int
fuse_fs_ioctl(struct fuse_fs   *fs,
              unsigned long     cmd,
              void             *arg,
              fuse_file_info_t *fi,
              unsigned int      flags,
              void             *data,
              uint32_t         *out_size)
{
  if(fs->op.ioctl == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"ioctl[%llu] 0x%lx flags: 0x%x\n",
            (unsigned long long)fi->fh,cmd,flags);

  return fs->op.ioctl(fi,cmd,arg,flags,data,out_size);
}

int
fuse_fs_poll(struct fuse_fs    *fs,
             fuse_file_info_t  *fi,
             fuse_pollhandle_t *ph,
             unsigned          *reventsp)
{
  int res;

  if(fs->op.poll == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"poll[%llu] ph: %p\n",
            (unsigned long long)fi->fh,ph);

  res = fs->op.poll(fi,ph,reventsp);

  if(fs->debug && !res)
    fprintf(stderr,"   poll[%llu] revents: 0x%x\n",
            (unsigned long long)fi->fh,*reventsp);

  return res;
}

int
fuse_fs_fallocate(struct fuse_fs   *fs,
                  int               mode,
                  off_t             offset,
                  off_t             length,
                  fuse_file_info_t *fi)
{
  if(fs->op.fallocate == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  if(fs->debug)
    fprintf(stderr,"fallocate mode %x,offset: %llu,length: %llu\n",
            mode,
            (unsigned long long)offset,
            (unsigned long long)length);

  return fs->op.fallocate(fi,mode,offset,length);
}

ssize_t
fuse_fs_copy_file_range(struct fuse_fs   *fs_,
                        fuse_file_info_t *ffi_in_,
                        off_t             off_in_,
                        fuse_file_info_t *ffi_out_,
                        off_t             off_out_,
                        size_t            len_,
                        int               flags_)
{
  if(fs_->op.copy_file_range == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.copy_file_range(ffi_in_,
                                 off_in_,
                                 ffi_out_,
                                 off_out_,
                                 len_,
                                 flags_);
}

static
int
node_open(const struct node *node_)
{
  return ((node_ != NULL) && (node_->open_count > 0));
}

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

static
void
curr_time(struct timespec *now)
{
  static clockid_t clockid = CLOCK_MONOTONIC;
  int res = clock_gettime(clockid,now);
  if(res == -1 && errno == EINVAL)
    {
      clockid = CLOCK_REALTIME;
      res = clock_gettime(clockid,now);
    }

  if(res == -1)
    {
      perror("fuse: clock_gettime");
      abort();
    }
}

static
void
update_stat(struct node       *node_,
            const struct stat *stnew_)
{
  if((node_->stat_cache_valid) &&
     ((node_->mtim.tv_sec  != stnew_->st_mtim.tv_sec)  ||
      (node_->mtim.tv_nsec != stnew_->st_mtim.tv_nsec) ||
      (node_->ino          != stnew_->st_ino) ||
      (node_->size         != stnew_->st_size)))
    node_->stat_cache_valid = 0;

  node_->ino  = stnew_->st_ino;
  node_->size = stnew_->st_size;
  node_->mtim = stnew_->st_mtim;
}

static
int
set_path_info(struct fuse             *f,
              fuse_ino_t               nodeid,
              const char              *name,
              struct fuse_entry_param *e)
{
  struct node *node;

  node = find_node(f,nodeid,name);
  if(node == NULL)
    return -ENOMEM;

  e->ino        = node->nodeid;
  e->generation = node->generation;

  pthread_mutex_lock(&f->lock);
  update_stat(node,&e->attr);
  pthread_mutex_unlock(&f->lock);

  set_stat(f,e->ino,&e->attr);
  if(f->conf.debug)
    fprintf(stderr,
            "   NODEID: %llu\n"
            "   GEN: %llu\n",
            (unsigned long long)e->ino,
            (unsigned long long)e->generation);

  return 0;
}

static
int
lookup_path(struct fuse             *f,
            fuse_ino_t               nodeid,
            const char              *name,
            const char              *path,
            struct fuse_entry_param *e,
            fuse_file_info_t        *fi)
{
  int rv;

  memset(e,0,sizeof(struct fuse_entry_param));

  rv = ((fi == NULL) ?
        fuse_fs_getattr(f->fs,path,&e->attr,&e->timeout) :
        fuse_fs_fgetattr(f->fs,&e->attr,fi,&e->timeout));

  if(rv)
    return rv;

  return set_path_info(f,nodeid,name,e);
}

static
struct fuse_context_i*
fuse_get_context_internal(void)
{
  struct fuse_context_i *c;

  c = (struct fuse_context_i *)pthread_getspecific(fuse_context_key);
  if(c == NULL)
    {
      c = (struct fuse_context_i*)calloc(1,sizeof(struct fuse_context_i));
      if(c == NULL)
        {
          /* This is hard to deal with properly,so just
             abort.  If memory is so low that the
             context cannot be allocated,there's not
             much hope for the filesystem anyway */
          fprintf(stderr,"fuse: failed to allocate thread specific data\n");
          abort();
        }
      pthread_setspecific(fuse_context_key,c);
    }
  return c;
}

static
void
fuse_freecontext(void *data)
{
  free(data);
}

static
int
fuse_create_context_key(void)
{
  int err = 0;
  pthread_mutex_lock(&fuse_context_lock);
  if(!fuse_context_ref)
    {
      err = pthread_key_create(&fuse_context_key,fuse_freecontext);
      if(err)
        {
          fprintf(stderr,"fuse: failed to create thread specific key: %s\n",
                  strerror(err));
          pthread_mutex_unlock(&fuse_context_lock);
          return -1;
        }
    }
  fuse_context_ref++;
  pthread_mutex_unlock(&fuse_context_lock);
  return 0;
}

static
void
fuse_delete_context_key(void)
{
  pthread_mutex_lock(&fuse_context_lock);
  fuse_context_ref--;
  if(!fuse_context_ref)
    {
      free(pthread_getspecific(fuse_context_key));
      pthread_key_delete(fuse_context_key);
    }
  pthread_mutex_unlock(&fuse_context_lock);
}

static
struct fuse*
req_fuse_prepare(fuse_req_t req)
{
  struct fuse_context_i *c = fuse_get_context_internal();
  const struct fuse_ctx *ctx = fuse_req_ctx(req);
  c->req = req;
  c->ctx.fuse = req_fuse(req);
  c->ctx.uid = ctx->uid;
  c->ctx.gid = ctx->gid;
  c->ctx.pid = ctx->pid;
  c->ctx.umask = ctx->umask;
  return c->ctx.fuse;
}

static
inline
void
reply_err(fuse_req_t req,
          int        err)
{
  /* fuse_reply_err() uses non-negated errno values */
  fuse_reply_err(req,-err);
}

static
void
reply_entry(fuse_req_t                     req,
            const struct fuse_entry_param *e,
            int                            err)
{
  if(!err)
    {
      struct fuse *f = req_fuse(req);
      if(fuse_reply_entry(req,e) == -ENOENT)
        {
          /* Skip forget for negative result */
          if(e->ino != 0)
            forget_node(f,e->ino,1);
        }
    }
  else
    {
      reply_err(req,err);
    }
}

void
fuse_fs_init(struct fuse_fs        *fs,
             struct fuse_conn_info *conn)
{
  fuse_get_context()->private_data = fs->user_data;
  if(!fs->op.write_buf)
    conn->want &= ~FUSE_CAP_SPLICE_READ;
  if(!fs->op.lock)
    conn->want &= ~FUSE_CAP_POSIX_LOCKS;
  if(!fs->op.flock)
    conn->want &= ~FUSE_CAP_FLOCK_LOCKS;
  if(fs->op.init)
    fs->user_data = fs->op.init(conn);
}

static
void
fuse_lib_init(void                  *data,
              struct fuse_conn_info *conn)
{
  struct fuse *f = (struct fuse *)data;
  struct fuse_context_i *c = fuse_get_context_internal();

  memset(c,0,sizeof(*c));
  c->ctx.fuse = f;
  conn->want |= FUSE_CAP_EXPORT_SUPPORT;
  fuse_fs_init(f->fs,conn);
}

void
fuse_fs_destroy(struct fuse_fs *fs)
{
  fuse_get_context()->private_data = fs->user_data;
  if(fs->op.destroy)
    fs->op.destroy(fs->user_data);
  free(fs);
}

static
void
fuse_lib_destroy(void *data)
{
  struct fuse *f = (struct fuse *)data;
  struct fuse_context_i *c = fuse_get_context_internal();

  memset(c,0,sizeof(*c));
  c->ctx.fuse = f;
  fuse_fs_destroy(f->fs);
  f->fs = NULL;
}

static
void
fuse_lib_lookup(fuse_req_t  req,
                fuse_ino_t  parent,
                const char *name)
{
  struct fuse *f = req_fuse_prepare(req);
  struct fuse_entry_param e;
  char *path;
  int err;
  struct node *dot = NULL;

  if(name[0] == '.')
    {
      int len = strlen(name);

      if(len == 1 || (name[1] == '.' && len == 2))
        {
          pthread_mutex_lock(&f->lock);
          if(len == 1)
            {
              if(f->conf.debug)
                fprintf(stderr,"LOOKUP-DOT\n");
              dot = get_node_nocheck(f,parent);
              if(dot == NULL)
                {
                  pthread_mutex_unlock(&f->lock);
                  reply_entry(req,&e,-ESTALE);
                  return;
                }
              dot->refctr++;
            }
          else
            {
              if(f->conf.debug)
                fprintf(stderr,"LOOKUP-DOTDOT\n");
              parent = get_node(f,parent)->parent->nodeid;
            }
          pthread_mutex_unlock(&f->lock);
          name = NULL;
        }
    }

  err = get_path_name(f,parent,name,&path);
  if(!err)
    {
      if(f->conf.debug)
        fprintf(stderr,"LOOKUP %s\n",path);
      err = lookup_path(f,parent,name,path,&e,NULL);
      if(err == -ENOENT)
        {
          e.ino = 0;
          err = 0;
        }
      free_path(f,parent,path);
    }
  if(dot)
    {
      pthread_mutex_lock(&f->lock);
      unref_node(f,dot);
      pthread_mutex_unlock(&f->lock);
    }
  reply_entry(req,&e,err);
}

static
void
do_forget(struct fuse      *f,
          const fuse_ino_t  ino,
          const uint64_t    nlookup)
{
  if(f->conf.debug)
    fprintf(stderr,
            "FORGET %llu/%llu\n",
            (unsigned long long)ino,
            (unsigned long long)nlookup);
  forget_node(f,ino,nlookup);
}

static
void
fuse_lib_forget(fuse_req_t       req,
                const fuse_ino_t ino,
                const uint64_t   nlookup)
{
  do_forget(req_fuse(req),ino,nlookup);
  fuse_reply_none(req);
}

static
void
fuse_lib_forget_multi(fuse_req_t               req,
                      size_t                   count,
                      struct fuse_forget_data *forgets)
{
  struct fuse *f = req_fuse(req);
  size_t i;

  for(i = 0; i < count; i++)
    do_forget(f,forgets[i].ino,forgets[i].nlookup);

  fuse_reply_none(req);
}


static
void
fuse_lib_getattr(fuse_req_t        req,
                 fuse_ino_t        ino,
                 fuse_file_info_t *fi)
{

  int err;
  char *path;
  struct fuse *f;
  struct stat buf;
  struct node *node;
  fuse_timeouts_t timeout;
  fuse_file_info_t ffi = {0};

  f = req_fuse_prepare(req);
  if(fi == NULL)
    {
      pthread_mutex_lock(&f->lock);
      node = get_node(f,ino);
      if(node->is_hidden)
        {
          fi = &ffi;
          fi->fh = node->hidden_fh;
        }
      pthread_mutex_unlock(&f->lock);
    }

  memset(&buf,0,sizeof(buf));

  err = 0;
  path = NULL;
  if((fi == NULL) || (f->fs->op.fgetattr == NULL))
    err = get_path(f,ino,&path);

  if(!err)
    {
      err = ((fi == NULL) ?
             fuse_fs_getattr(f->fs,path,&buf,&timeout) :
             fuse_fs_fgetattr(f->fs,&buf,fi,&timeout));

      free_path(f,ino,path);
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      node = get_node(f,ino);
      update_stat(node,&buf);
      pthread_mutex_unlock(&f->lock);
      set_stat(f,ino,&buf);
      fuse_reply_attr(req,&buf,timeout.attr);
    }
  else
    {
      reply_err(req,err);
    }
}

int
fuse_fs_chmod(struct fuse_fs *fs,
              const char     *path,
              mode_t          mode)
{
  if(fs->op.chmod == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs->user_data;

  return fs->op.chmod(path,mode);
}

int
fuse_fs_fchmod(struct fuse_fs         *fs_,
               const fuse_file_info_t *ffi_,
               const mode_t            mode_)
{
  if(fs_->op.fchmod == NULL)
    return -ENOSYS;

  fuse_get_context()->private_data = fs_->user_data;

  return fs_->op.fchmod(ffi_,mode_);
}

static
void
fuse_lib_setattr(fuse_req_t        req,
                 fuse_ino_t        ino,
                 struct stat      *attr,
                 int               valid,
                 fuse_file_info_t *fi)
{
  struct fuse *f = req_fuse_prepare(req);
  struct stat buf;
  char *path;
  int err;
  struct node *node;
  fuse_timeouts_t timeout;
  fuse_file_info_t ffi = {0};

  if(fi == NULL)
    {
      pthread_mutex_lock(&f->lock);
      node = get_node(f,ino);
      if(node->is_hidden)
        {
          fi = &ffi;
          fi->fh = node->hidden_fh;
        }
      pthread_mutex_unlock(&f->lock);
    }

  memset(&buf,0,sizeof(buf));

  err = 0;
  path = NULL;
  if(fi == NULL)
    err = get_path(f,ino,&path);

  if(!err)
    {
      err = 0;
      if(!err && (valid & FATTR_MODE))
        err = ((fi == NULL) ?
               fuse_fs_chmod(f->fs,path,attr->st_mode) :
               fuse_fs_fchmod(f->fs,fi,attr->st_mode));

      if(!err && (valid & (FATTR_UID | FATTR_GID)))
        {
          uid_t uid = ((valid & FATTR_UID) ? attr->st_uid : (uid_t)-1);
          gid_t gid = ((valid & FATTR_GID) ? attr->st_gid : (gid_t)-1);

          err = ((fi == NULL) ?
                 fuse_fs_chown(f->fs,path,uid,gid) :
                 fuse_fs_fchown(f->fs,fi,uid,gid));
        }

      if(!err && (valid & FATTR_SIZE))
        err = ((fi == NULL) ?
               fuse_fs_truncate(f->fs,path,attr->st_size) :
               fuse_fs_ftruncate(f->fs,attr->st_size,fi));

#ifdef HAVE_UTIMENSAT
      if(!err && (valid & (FATTR_ATIME | FATTR_MTIME)))
        {
          struct timespec tv[2];

          tv[0].tv_sec = 0;
          tv[1].tv_sec = 0;
          tv[0].tv_nsec = UTIME_OMIT;
          tv[1].tv_nsec = UTIME_OMIT;

          if(valid & FATTR_ATIME_NOW)
            tv[0].tv_nsec = UTIME_NOW;
          else if(valid & FATTR_ATIME)
            tv[0] = attr->st_atim;

          if(valid & FATTR_MTIME_NOW)
            tv[1].tv_nsec = UTIME_NOW;
          else if(valid & FATTR_MTIME)
            tv[1] = attr->st_mtim;

          err = ((fi == NULL) ?
                 fuse_fs_utimens(f->fs,path,tv) :
                 fuse_fs_futimens(f->fs,fi,tv));
        }
      else
#endif
        if(!err && ((valid & (FATTR_ATIME|FATTR_MTIME)) == (FATTR_ATIME|FATTR_MTIME)))
          {
            struct timespec tv[2];
            tv[0].tv_sec = attr->st_atime;
            tv[0].tv_nsec = ST_ATIM_NSEC(attr);
            tv[1].tv_sec = attr->st_mtime;
            tv[1].tv_nsec = ST_MTIM_NSEC(attr);
            err = ((fi == NULL) ?
                   fuse_fs_utimens(f->fs,path,tv) :
                   fuse_fs_futimens(f->fs,fi,tv));
          }

      if(!err)
        err = ((fi == NULL) ?
               fuse_fs_getattr(f->fs,path,&buf,&timeout) :
               fuse_fs_fgetattr(f->fs,&buf,fi,&timeout));

      free_path(f,ino,path);
    }


  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      update_stat(get_node(f,ino),&buf);
      pthread_mutex_unlock(&f->lock);
      set_stat(f,ino,&buf);
      fuse_reply_attr(req,&buf,timeout.attr);
    }
  else
    {
      reply_err(req,err);
    }
}

static
void
fuse_lib_access(fuse_req_t req,
                fuse_ino_t ino,
                int        mask)
{
  struct fuse *f = req_fuse_prepare(req);
  char *path;
  int err;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_access(f->fs,path,mask);
      free_path(f,ino,path);
    }

  reply_err(req,err);
}

static
void
fuse_lib_readlink(fuse_req_t req,
                  fuse_ino_t ino)
{
  struct fuse *f = req_fuse_prepare(req);
  char linkname[PATH_MAX + 1];
  char *path;
  int err;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_readlink(f->fs,path,linkname,sizeof(linkname));
      free_path(f,ino,path);
    }

  if(!err)
    {
      linkname[PATH_MAX] = '\0';
      fuse_reply_readlink(req,linkname);
    }
  else
    {
      reply_err(req,err);
    }
}

static
void
fuse_lib_mknod(fuse_req_t  req,
               fuse_ino_t  parent,
               const char *name,
               mode_t      mode,
               dev_t       rdev)
{
  struct fuse *f = req_fuse_prepare(req);
  struct fuse_entry_param e;
  char *path;
  int err;

  err = get_path_name(f,parent,name,&path);
  if(!err)
    {
      err = -ENOSYS;
      if(S_ISREG(mode))
        {
          fuse_file_info_t fi;

          memset(&fi,0,sizeof(fi));
          fi.flags = O_CREAT | O_EXCL | O_WRONLY;
          err = fuse_fs_create(f->fs,path,mode,&fi);
          if(!err)
            {
              err = lookup_path(f,parent,name,path,&e,
                                &fi);
              fuse_fs_release(f->fs,&fi);
            }
        }
      if(err == -ENOSYS)
        {
          err = fuse_fs_mknod(f->fs,path,mode,rdev);
          if(!err)
            err = lookup_path(f,parent,name,path,&e,NULL);
        }
      free_path(f,parent,path);
    }
  reply_entry(req,&e,err);
}

static
void
fuse_lib_mkdir(fuse_req_t  req,
               fuse_ino_t  parent,
               const char *name,
               mode_t      mode)
{
  struct fuse *f = req_fuse_prepare(req);
  struct fuse_entry_param e;
  char *path;
  int err;

  err = get_path_name(f,parent,name,&path);
  if(!err)
    {
      err = fuse_fs_mkdir(f->fs,path,mode);
      if(!err)
        err = lookup_path(f,parent,name,path,&e,NULL);
      free_path(f,parent,path);
    }
  reply_entry(req,&e,err);
}

static
void
fuse_lib_unlink(fuse_req_t  req,
                fuse_ino_t  parent,
                const char *name)
{
  int err;
  char *path;
  struct fuse *f;
  struct node *wnode;

  f = req_fuse_prepare(req);
  err = get_path_wrlock(f,parent,name,&path,&wnode);

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      if(node_open(wnode))
        {
          err = fuse_fs_prepare_hide(f->fs,path,&wnode->hidden_fh);
          if(!err)
            wnode->is_hidden = 1;
        }
      pthread_mutex_unlock(&f->lock);

      err = fuse_fs_unlink(f->fs,path);
      if(!err)
        remove_node(f,parent,name);

      free_path_wrlock(f,parent,wnode,path);
    }

  reply_err(req,err);
}

static
void
fuse_lib_rmdir(fuse_req_t  req,
               fuse_ino_t  parent,
               const char *name)
{
  struct fuse *f = req_fuse_prepare(req);
  struct node *wnode;
  char *path;
  int err;

  err = get_path_wrlock(f,parent,name,&path,&wnode);
  if(!err)
    {
      err = fuse_fs_rmdir(f->fs,path);
      if(!err)
        remove_node(f,parent,name);
      free_path_wrlock(f,parent,wnode,path);
    }

  reply_err(req,err);
}

static
void
fuse_lib_symlink(fuse_req_t  req_,
                 const char *linkname_,
                 fuse_ino_t  parent_,
                 const char *name_)
{
  int rv;
  char *path;
  struct fuse *f;
  struct fuse_entry_param e = {0};

  f = req_fuse_prepare(req_);

  rv = get_path_name(f,parent_,name_,&path);
  if(rv == 0)
    {
      rv = fuse_fs_symlink(f->fs,linkname_,path,&e.attr,&e.timeout);
      if(rv == 0)
        rv = set_path_info(f,parent_,name_,&e);
      free_path(f,parent_,path);
    }

  reply_entry(req_,&e,rv);
}

static
void
fuse_lib_rename(fuse_req_t  req,
                fuse_ino_t  olddir,
                const char *oldname,
                fuse_ino_t  newdir,
                const char *newname)
{
  int err;
  struct fuse *f;
  char *oldpath;
  char *newpath;
  struct node *wnode1;
  struct node *wnode2;

  f = req_fuse_prepare(req);
  err = get_path2(f,olddir,oldname,newdir,newname,
                  &oldpath,&newpath,&wnode1,&wnode2);

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      if(node_open(wnode2))
        {
          err = fuse_fs_prepare_hide(f->fs,newpath,&wnode2->hidden_fh);
          if(!err)
            wnode2->is_hidden = 1;
        }
      pthread_mutex_unlock(&f->lock);

      err = fuse_fs_rename(f->fs,oldpath,newpath);
      if(!err)
        err = rename_node(f,olddir,oldname,newdir,newname);

      free_path2(f,olddir,newdir,wnode1,wnode2,oldpath,newpath);
    }

  reply_err(req,err);
}

static
void
fuse_lib_link(fuse_req_t  req,
              fuse_ino_t  ino,
              fuse_ino_t  newparent,
              const char *newname)
{
  int rv;
  char *oldpath;
  char *newpath;
  struct fuse *f;
  struct fuse_entry_param e = {0};

  f = req_fuse_prepare(req);

  rv = get_path2(f,ino,NULL,newparent,newname,
                  &oldpath,&newpath,NULL,NULL);
  if(!rv)
    {
      rv = fuse_fs_link(f->fs,oldpath,newpath,&e.attr,&e.timeout);
      if(rv == 0)
        rv = set_path_info(f,newparent,newname,&e);
      free_path2(f,ino,newparent,NULL,NULL,oldpath,newpath);
    }

  reply_entry(req,&e,rv);
}

static
void
fuse_do_release(struct fuse      *f,
                fuse_ino_t        ino,
                fuse_file_info_t *fi)
{
  struct node *node;
  uint64_t fh;
  int was_hidden;

  fh = 0;
  fuse_fs_release(f->fs,fi);

  pthread_mutex_lock(&f->lock);
  node = get_node(f,ino);
  assert(node->open_count > 0);
  node->open_count--;
  was_hidden = 0;
  if(node->is_hidden && (node->open_count == 0))
    {
      was_hidden = 1;
      node->is_hidden = 0;
      fh = node->hidden_fh;
    }
  pthread_mutex_unlock(&f->lock);

  if(was_hidden)
    fuse_fs_free_hide(f->fs,fh);
}

static
void
fuse_lib_create(fuse_req_t        req,
                fuse_ino_t        parent,
                const char       *name,
                mode_t            mode,
                fuse_file_info_t *fi)
{
  int err;
  char *path;
  struct fuse *f;
  struct fuse_entry_param e;

  f = req_fuse_prepare(req);
  err = get_path_name(f,parent,name,&path);
  if(!err)
    {
      err = fuse_fs_create(f->fs,path,mode,fi);
      if(!err)
        {
          err = lookup_path(f,parent,name,path,&e,fi);
          if(err)
            {
              fuse_fs_release(f->fs,fi);
            }
          else if(!S_ISREG(e.attr.st_mode))
            {
              err = -EIO;
              fuse_fs_release(f->fs,fi);
              forget_node(f,e.ino,1);
            }
        }
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      get_node(f,e.ino)->open_count++;
      pthread_mutex_unlock(&f->lock);

      if(fuse_reply_create(req,&e,fi) == -ENOENT)
        {
          /* The open syscall was interrupted,so it
             must be cancelled */
          fuse_do_release(f,e.ino,fi);
          forget_node(f,e.ino,1);
        }
    }
  else
    {
      reply_err(req,err);
    }

  free_path(f,parent,path);
}

static
double
diff_timespec(const struct timespec *t1,
              const struct timespec *t2)
{
  return (t1->tv_sec - t2->tv_sec) +
    ((double)t1->tv_nsec - (double)t2->tv_nsec) / 1000000000.0;
}

static
void
open_auto_cache(struct fuse      *f,
                fuse_ino_t        ino,
                const char       *path,
                fuse_file_info_t *fi)
{
  struct node *node;
  fuse_timeouts_t timeout;

  pthread_mutex_lock(&f->lock);

  node = get_node(f,ino);
  if(node->stat_cache_valid)
    {
      int err;
      struct stat stbuf;

      pthread_mutex_unlock(&f->lock);
      err = fuse_fs_fgetattr(f->fs,&stbuf,fi,&timeout);
      pthread_mutex_lock(&f->lock);

      if(!err)
        update_stat(node,&stbuf);
      else
        node->stat_cache_valid = 0;
    }

  if(node->stat_cache_valid)
    fi->keep_cache = 1;

  node->stat_cache_valid = 1;

  pthread_mutex_unlock(&f->lock);
}

static
void
fuse_lib_open(fuse_req_t        req,
              fuse_ino_t        ino,
              fuse_file_info_t *fi)
{
  int err;
  char *path;
  struct fuse *f;

  f = req_fuse_prepare(req);
  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_open(f->fs,path,fi);
      if(!err)
        {
          if(fi && fi->auto_cache)
            open_auto_cache(f,ino,path,fi);
        }
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      get_node(f,ino)->open_count++;
      pthread_mutex_unlock(&f->lock);
      /* The open syscall was interrupted,so it must be cancelled */
      if(fuse_reply_open(req,fi) == -ENOENT)
        fuse_do_release(f,ino,fi);
    }
  else
    {
      reply_err(req,err);
    }

  free_path(f,ino,path);
}

static
void
fuse_lib_read(fuse_req_t        req,
              fuse_ino_t        ino,
              size_t            size,
              off_t             off,
              fuse_file_info_t *fi)
{
  struct fuse *f = req_fuse_prepare(req);
  struct fuse_bufvec *buf = NULL;
  int res;

  res = fuse_fs_read_buf(f->fs,&buf,size,off,fi);

  if(res == 0)
    fuse_reply_data(req,buf,FUSE_BUF_SPLICE_MOVE);
  else
    reply_err(req,res);

  fuse_free_buf(buf);
}

static
void
fuse_lib_write_buf(fuse_req_t          req,
                   fuse_ino_t          ino,
                   struct fuse_bufvec *buf,
                   off_t               off,
                   fuse_file_info_t   *fi)
{
  int res;
  struct fuse *f = req_fuse_prepare(req);

  res = fuse_fs_write_buf(f->fs,buf,off,fi);
  free_path(f,ino,NULL);

  if(res >= 0)
    fuse_reply_write(req,res);
  else
    reply_err(req,res);
}

static
void
fuse_lib_fsync(fuse_req_t        req,
               fuse_ino_t        ino,
               int               datasync,
               fuse_file_info_t *fi)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = fuse_fs_fsync(f->fs,datasync,fi);

  reply_err(req,err);
}

static
struct fuse_dh*
get_dirhandle(const fuse_file_info_t *llfi,
              fuse_file_info_t       *fi)
{
  struct fuse_dh *dh = (struct fuse_dh *)(uintptr_t)llfi->fh;
  memset(fi,0,sizeof(fuse_file_info_t));
  fi->fh = dh->fh;
  return dh;
}

static
void
fuse_lib_opendir(fuse_req_t        req,
                 fuse_ino_t        ino,
                 fuse_file_info_t *llfi)
{
  int err;
  char *path;
  struct fuse_dh *dh;
  fuse_file_info_t fi;
  struct fuse *f = req_fuse_prepare(req);

  dh = (struct fuse_dh *)calloc(1,sizeof(struct fuse_dh));
  if(dh == NULL)
    {
      reply_err(req,-ENOMEM);
      return;
    }

  fuse_dirents_init(&dh->d);
  fuse_mutex_init(&dh->lock);

  llfi->fh = (uintptr_t)dh;

  memset(&fi,0,sizeof(fi));
  fi.flags = llfi->flags;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_opendir(f->fs,path,&fi);
      dh->fh = fi.fh;
      llfi->keep_cache    = fi.keep_cache;
      llfi->cache_readdir = fi.cache_readdir;
    }

  if(!err)
    {
      if(fuse_reply_open(req,llfi) == -ENOENT)
        {
          /* The opendir syscall was interrupted,so it
             must be cancelled */
          fuse_fs_releasedir(f->fs,&fi);
          pthread_mutex_destroy(&dh->lock);
          free(dh);
        }
    }
  else
    {
      reply_err(req,err);
      pthread_mutex_destroy(&dh->lock);
      free(dh);
    }
  free_path(f,ino,path);
}

static
int
readdir_fill(struct fuse      *f_,
             fuse_req_t        req_,
             fuse_dirents_t   *d_,
             fuse_file_info_t *fi_)
{
  int rv;

  rv = fuse_fs_readdir(f_->fs,fi_,d_);

  return rv;
}

static
int
readdir_plus_fill(struct fuse      *f_,
                  fuse_req_t        req_,
                  fuse_dirents_t   *d_,
                  fuse_file_info_t *fi_)
{
  int rv;

  rv = fuse_fs_readdir_plus(f_->fs,fi_,d_);

  return rv;
}

static
size_t
readdir_buf_size(fuse_dirents_t *d_,
                 size_t          size_,
                 off_t           off_)
{
  if(off_ >= kv_size(d_->offs))
    return 0;
  if((kv_A(d_->offs,off_) + size_) > d_->data_len)
    return (d_->data_len - kv_A(d_->offs,off_));
  return size_;
}

static
char*
readdir_buf(fuse_dirents_t *d_,
            off_t           off_)
{
  return &d_->buf[kv_A(d_->offs,off_)];
}

static
void
fuse_lib_readdir(fuse_req_t        req_,
                 fuse_ino_t        ino_,
                 size_t            size_,
                 off_t             off_,
                 fuse_file_info_t *llffi_)
{
  int rv;
  struct fuse *f;
  fuse_dirents_t *d;
  struct fuse_dh *dh;
  fuse_file_info_t fi;

  f  = req_fuse_prepare(req_);
  dh = get_dirhandle(llffi_,&fi);
  d  = &dh->d;

  pthread_mutex_lock(&dh->lock);

  rv = 0;
  if((off_ == 0) || (d->data_len == 0))
    rv = readdir_fill(f,req_,d,&fi);

  if(rv)
    {
      reply_err(req_,rv);
      goto out;
    }

  size_ = readdir_buf_size(d,size_,off_);

  fuse_reply_buf(req_,
                 readdir_buf(d,off_),
                 size_);

 out:
  pthread_mutex_unlock(&dh->lock);
}

static
void
fuse_lib_readdir_plus(fuse_req_t        req_,
                      fuse_ino_t        ino_,
                      size_t            size_,
                      off_t             off_,
                      fuse_file_info_t *llffi_)
{
  int rv;
  struct fuse *f;
  fuse_dirents_t *d;
  struct fuse_dh *dh;
  fuse_file_info_t fi;

  f  = req_fuse_prepare(req_);
  dh = get_dirhandle(llffi_,&fi);
  d  = &dh->d;

  pthread_mutex_lock(&dh->lock);

  rv = 0;
  if((off_ == 0) || (d->data_len == 0))
    rv = readdir_plus_fill(f,req_,d,&fi);

  if(rv)
    {
      reply_err(req_,rv);
      goto out;
    }

  size_ = readdir_buf_size(d,size_,off_);

  fuse_reply_buf(req_,
                 readdir_buf(d,off_),
                 size_);

 out:
  pthread_mutex_unlock(&dh->lock);
}

static
void
fuse_lib_releasedir(fuse_req_t        req_,
                    fuse_ino_t        ino_,
                    fuse_file_info_t *llfi_)
{
  struct fuse *f;
  struct fuse_dh *dh;
  fuse_file_info_t fi;

  f  = req_fuse_prepare(req_);
  dh = get_dirhandle(llfi_,&fi);

  fuse_fs_releasedir(f->fs,&fi);

  /* Done to keep race condition between last readdir reply and the unlock */
  pthread_mutex_lock(&dh->lock);
  pthread_mutex_unlock(&dh->lock);
  pthread_mutex_destroy(&dh->lock);
  fuse_dirents_free(&dh->d);
  free(dh);
  reply_err(req_,0);
}

static
void
fuse_lib_fsyncdir(fuse_req_t        req,
                  fuse_ino_t        ino,
                  int               datasync,
                  fuse_file_info_t *llfi)
{
  int err;
  fuse_file_info_t fi;
  struct fuse *f = req_fuse_prepare(req);

  get_dirhandle(llfi,&fi);

  err = fuse_fs_fsyncdir(f->fs,datasync,&fi);

  reply_err(req,err);
}

static
void
fuse_lib_statfs(fuse_req_t req,
                fuse_ino_t ino)
{
  struct fuse *f = req_fuse_prepare(req);
  struct statvfs buf;
  char *path = NULL;
  int err = 0;

  memset(&buf,0,sizeof(buf));
  if(ino)
    err = get_path(f,ino,&path);

  if(!err)
    {
      err = fuse_fs_statfs(f->fs,path ? path : "/",&buf);
      free_path(f,ino,path);
    }

  if(!err)
    fuse_reply_statfs(req,&buf);
  else
    reply_err(req,err);
}

static
void
fuse_lib_setxattr(fuse_req_t  req,
                  fuse_ino_t  ino,
                  const char *name,
                  const char *value,
                  size_t      size,
                  int         flags)
{
  struct fuse *f = req_fuse_prepare(req);
  char *path;
  int err;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_setxattr(f->fs,path,name,value,size,flags);
      free_path(f,ino,path);
    }

  reply_err(req,err);
}

static
int
common_getxattr(struct fuse *f,
                fuse_req_t   req,
                fuse_ino_t   ino,
                const char  *name,
                char        *value,
                size_t       size)
{
  int err;
  char *path;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_getxattr(f->fs,path,name,value,size);

      free_path(f,ino,path);
    }

  return err;
}

static
void
fuse_lib_getxattr(fuse_req_t  req,
                  fuse_ino_t  ino,
                  const char *name,
                  size_t      size)
{
  struct fuse *f = req_fuse_prepare(req);
  int res;

  if(size)
    {
      char *value = (char *)malloc(size);
      if(value == NULL)
        {
          reply_err(req,-ENOMEM);
          return;
        }

      res = common_getxattr(f,req,ino,name,value,size);
      if(res > 0)
        fuse_reply_buf(req,value,res);
      else
        reply_err(req,res);
      free(value);
    }
  else
    {
      res = common_getxattr(f,req,ino,name,NULL,0);
      if(res >= 0)
        fuse_reply_xattr(req,res);
      else
        reply_err(req,res);
    }
}

static
int
common_listxattr(struct fuse *f,
                 fuse_req_t   req,
                 fuse_ino_t   ino,
                 char        *list,
                 size_t       size)
{
  char *path;
  int err;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_listxattr(f->fs,path,list,size);
      free_path(f,ino,path);
    }

  return err;
}

static
void
fuse_lib_listxattr(fuse_req_t req,
                   fuse_ino_t ino,
                   size_t     size)
{
  struct fuse *f = req_fuse_prepare(req);
  int res;

  if(size)
    {
      char *list = (char *)malloc(size);
      if(list == NULL)
        {
          reply_err(req,-ENOMEM);
          return;
        }

      res = common_listxattr(f,req,ino,list,size);
      if(res > 0)
        fuse_reply_buf(req,list,res);
      else
        reply_err(req,res);
      free(list);
    }
  else
    {
      res = common_listxattr(f,req,ino,NULL,0);
      if(res >= 0)
        fuse_reply_xattr(req,res);
      else
        reply_err(req,res);
    }
}

static
void
fuse_lib_removexattr(fuse_req_t  req,
                     fuse_ino_t  ino,
                     const char *name)
{
  struct fuse *f = req_fuse_prepare(req);
  char *path;
  int err;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_removexattr(f->fs,path,name);
      free_path(f,ino,path);
    }

  reply_err(req,err);
}

static
void
fuse_lib_copy_file_range(fuse_req_t        req_,
                         fuse_ino_t        nodeid_in_,
                         off_t             off_in_,
                         fuse_file_info_t *ffi_in_,
                         fuse_ino_t        nodeid_out_,
                         off_t             off_out_,
                         fuse_file_info_t *ffi_out_,
                         size_t            len_,
                         int               flags_)
{
  ssize_t rv;
  struct fuse *f;

  f = req_fuse_prepare(req_);

  rv = fuse_fs_copy_file_range(f->fs,
                               ffi_in_,
                               off_in_,
                               ffi_out_,
                               off_out_,
                               len_,
                               flags_);

  if(rv >= 0)
    fuse_reply_write(req_,rv);
  else
    reply_err(req_,rv);
}

static
struct lock*
locks_conflict(struct node       *node,
               const struct lock *lock)
{
  struct lock *l;

  for(l = node->locks; l; l = l->next)
    if(l->owner != lock->owner &&
       lock->start <= l->end && l->start <= lock->end &&
       (l->type == F_WRLCK || lock->type == F_WRLCK))
      break;

  return l;
}

static
void
delete_lock(struct lock **lockp)
{
  struct lock *l = *lockp;
  *lockp = l->next;
  free(l);
}

static
void
insert_lock(struct lock **pos,
            struct lock  *lock)
{
  lock->next = *pos;
  *pos       = lock;
}

static
int
locks_insert(struct node *node,
             struct lock *lock)
{
  struct lock **lp;
  struct lock  *newl1 = NULL;
  struct lock  *newl2 = NULL;

  if(lock->type != F_UNLCK || lock->start != 0 || lock->end != OFFSET_MAX)
    {
      newl1 = malloc(sizeof(struct lock));
      newl2 = malloc(sizeof(struct lock));

      if(!newl1 || !newl2)
        {
          free(newl1);
          free(newl2);
          return -ENOLCK;
        }
    }

  for(lp = &node->locks; *lp;)
    {
      struct lock *l = *lp;
      if(l->owner != lock->owner)
        goto skip;

      if(lock->type == l->type)
        {
          if(l->end < lock->start - 1)
            goto skip;
          if(lock->end < l->start - 1)
            break;
          if(l->start <= lock->start && lock->end <= l->end)
            goto out;
          if(l->start < lock->start)
            lock->start = l->start;
          if(lock->end < l->end)
            lock->end   = l->end;
          goto delete;
        }
      else
        {
          if(l->end < lock->start)
            goto skip;
          if(lock->end < l->start)
            break;
          if(lock->start <= l->start && l->end <= lock->end)
            goto delete;
          if(l->end <= lock->end)
            {
              l->end = lock->start - 1;
              goto skip;
            }
          if(lock->start <= l->start)
            {
              l->start = lock->end + 1;
              break;
            }
          *newl2       = *l;
          newl2->start = lock->end + 1;
          l->end       = lock->start - 1;
          insert_lock(&l->next,newl2);
          newl2        = NULL;
        }
    skip:
      lp = &l->next;
      continue;

    delete:
      delete_lock(lp);
    }
  if(lock->type != F_UNLCK)
    {
      *newl1 = *lock;
      insert_lock(lp,newl1);
      newl1 = NULL;
    }
 out:
  free(newl1);
  free(newl2);
  return 0;
}

static
void
flock_to_lock(struct flock *flock,
              struct lock  *lock)
{
  memset(lock,0,sizeof(struct lock));
  lock->type = flock->l_type;
  lock->start = flock->l_start;
  lock->end = flock->l_len ? flock->l_start + flock->l_len - 1 : OFFSET_MAX;
  lock->pid = flock->l_pid;
}

static
void
lock_to_flock(struct lock  *lock,
              struct flock *flock)
{
  flock->l_type = lock->type;
  flock->l_start = lock->start;
  flock->l_len = (lock->end == OFFSET_MAX) ? 0 : lock->end - lock->start + 1;
  flock->l_pid = lock->pid;
}

static
int
fuse_flush_common(struct fuse      *f,
                  fuse_req_t        req,
                  fuse_ino_t        ino,
                  fuse_file_info_t *fi)
{
  struct flock lock;
  struct lock l;
  int err;
  int errlock;

  memset(&lock,0,sizeof(lock));
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  err = fuse_fs_flush(f->fs,fi);
  errlock = fuse_fs_lock(f->fs,fi,F_SETLK,&lock);

  if(errlock != -ENOSYS)
    {
      flock_to_lock(&lock,&l);
      l.owner = fi->lock_owner;
      pthread_mutex_lock(&f->lock);
      locks_insert(get_node(f,ino),&l);
      pthread_mutex_unlock(&f->lock);

      /* if op.lock() is defined FLUSH is needed regardless
         of op.flush() */
      if(err == -ENOSYS)
        err = 0;
    }

  return err;
}

static
void
fuse_lib_release(fuse_req_t        req,
                 fuse_ino_t        ino,
                 fuse_file_info_t *fi)
{
  int err = 0;
  struct fuse *f = req_fuse_prepare(req);

  if(fi->flush)
    {
      err = fuse_flush_common(f,req,ino,fi);
      if(err == -ENOSYS)
        err = 0;
    }

  fuse_do_release(f,ino,fi);

  reply_err(req,err);
}

static
void
fuse_lib_flush(fuse_req_t        req,
               fuse_ino_t        ino,
               fuse_file_info_t *fi)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = fuse_flush_common(f,req,ino,fi);

  reply_err(req,err);
}

static
int
fuse_lock_common(fuse_req_t        req,
                 fuse_ino_t        ino,
                 fuse_file_info_t *fi,
                 struct flock     *lock,
                 int               cmd)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = fuse_fs_lock(f->fs,fi,cmd,lock);

  return err;
}

static
void
fuse_lib_getlk(fuse_req_t        req,
               fuse_ino_t        ino,
               fuse_file_info_t *fi,
               struct flock     *lock)
{
  int err;
  struct lock l;
  struct lock *conflict;
  struct fuse *f = req_fuse(req);

  flock_to_lock(lock,&l);
  l.owner = fi->lock_owner;
  pthread_mutex_lock(&f->lock);
  conflict = locks_conflict(get_node(f,ino),&l);
  if(conflict)
    lock_to_flock(conflict,lock);
  pthread_mutex_unlock(&f->lock);
  if(!conflict)
    err = fuse_lock_common(req,ino,fi,lock,F_GETLK);
  else
    err = 0;

  if(!err)
    fuse_reply_lock(req,lock);
  else
    reply_err(req,err);
}

static
void
fuse_lib_setlk(fuse_req_t        req,
               fuse_ino_t        ino,
               fuse_file_info_t *fi,
               struct flock     *lock,
               int               sleep)
{
  int err = fuse_lock_common(req,ino,fi,lock,
                             sleep ? F_SETLKW : F_SETLK);
  if(!err)
    {
      struct fuse *f = req_fuse(req);
      struct lock l;
      flock_to_lock(lock,&l);
      l.owner = fi->lock_owner;
      pthread_mutex_lock(&f->lock);
      locks_insert(get_node(f,ino),&l);
      pthread_mutex_unlock(&f->lock);
    }

  reply_err(req,err);
}

static
void
fuse_lib_flock(fuse_req_t        req,
               fuse_ino_t        ino,
               fuse_file_info_t *fi,
               int               op)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = fuse_fs_flock(f->fs,fi,op);

  reply_err(req,err);
}

static
void
fuse_lib_bmap(fuse_req_t req,
              fuse_ino_t ino,
              size_t     blocksize,
              uint64_t   idx)
{
  int err;
  char *path;
  struct fuse *f = req_fuse_prepare(req);

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = fuse_fs_bmap(f->fs,path,blocksize,&idx);
      free_path(f,ino,path);
    }

  if(!err)
    fuse_reply_bmap(req,idx);
  else
    reply_err(req,err);
}

static
void
fuse_lib_ioctl(fuse_req_t        req,
               fuse_ino_t        ino,
               unsigned long     cmd,
               void             *arg,
               fuse_file_info_t *llfi,
               unsigned int      flags,
               const void       *in_buf,
               uint32_t          in_bufsz,
               uint32_t          out_bufsz_)
{
  int err;
  char *out_buf = NULL;
  struct fuse *f = req_fuse_prepare(req);
  fuse_file_info_t fi;
  uint32_t out_bufsz = out_bufsz_;

  err = -EPERM;
  if(flags & FUSE_IOCTL_UNRESTRICTED)
    goto err;

  if(flags & FUSE_IOCTL_DIR)
    get_dirhandle(llfi,&fi);
  else
    fi = *llfi;

  if(out_bufsz)
    {
      err = -ENOMEM;
      out_buf = malloc(out_bufsz);
      if(!out_buf)
        goto err;
    }

  assert(!in_bufsz || !out_bufsz || in_bufsz == out_bufsz);
  if(out_buf)
    memcpy(out_buf,in_buf,in_bufsz);

  err = fuse_fs_ioctl(f->fs,cmd,arg,&fi,flags,
                      out_buf ?: (void *)in_buf,&out_bufsz);

  fuse_reply_ioctl(req,err,out_buf,out_bufsz);
  goto out;
 err:
  reply_err(req,err);
 out:
  free(out_buf);
}

static
void
fuse_lib_poll(fuse_req_t         req,
              fuse_ino_t         ino,
              fuse_file_info_t  *fi,
              fuse_pollhandle_t *ph)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);
  unsigned revents = 0;

  err = fuse_fs_poll(f->fs,fi,ph,&revents);

  if(!err)
    fuse_reply_poll(req,revents);
  else
    reply_err(req,err);
}

static
void
fuse_lib_fallocate(fuse_req_t        req,
                   fuse_ino_t        ino,
                   int               mode,
                   off_t             offset,
                   off_t             length,
                   fuse_file_info_t *fi)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = fuse_fs_fallocate(f->fs,mode,offset,length,fi);

  reply_err(req,err);
}

static
int
clean_delay(struct fuse *f)
{
  /*
   * This is calculating the delay between clean runs.  To
   * reduce the number of cleans we are doing them 10 times
   * within the remember window.
   */
  int min_sleep = 60;
  int max_sleep = 3600;
  int sleep_time = f->conf.remember / 10;

  if(sleep_time > max_sleep)
    return max_sleep;
  if(sleep_time < min_sleep)
    return min_sleep;
  return sleep_time;
}

int
fuse_clean_cache(struct fuse *f)
{
  struct node_lru *lnode;
  struct list_head *curr,*next;
  struct node *node;
  struct timespec now;

  pthread_mutex_lock(&f->lock);

  curr_time(&now);

  for(curr = f->lru_table.next; curr != &f->lru_table; curr = next)
    {
    double age;

    next = curr->next;
    lnode = list_entry(curr,struct node_lru,lru);
    node = &lnode->node;

    age = diff_timespec(&now,&lnode->forget_time);
    if(age <= f->conf.remember)
      break;

    assert(node->nlookup == 1);

    /* Don't forget active directories */
    if(node->refctr > 1)
      continue;

    node->nlookup = 0;
    unhash_name(f,node);
    unref_node(f,node);
  }
  pthread_mutex_unlock(&f->lock);

  return clean_delay(f);
}

static struct fuse_lowlevel_ops fuse_path_ops =
  {
    .init            = fuse_lib_init,
    .destroy         = fuse_lib_destroy,
    .lookup          = fuse_lib_lookup,
    .forget          = fuse_lib_forget,
    .forget_multi    = fuse_lib_forget_multi,
    .getattr         = fuse_lib_getattr,
    .setattr         = fuse_lib_setattr,
    .access          = fuse_lib_access,
    .readlink        = fuse_lib_readlink,
    .mknod           = fuse_lib_mknod,
    .mkdir           = fuse_lib_mkdir,
    .unlink          = fuse_lib_unlink,
    .rmdir           = fuse_lib_rmdir,
    .symlink         = fuse_lib_symlink,
    .rename          = fuse_lib_rename,
    .link            = fuse_lib_link,
    .create          = fuse_lib_create,
    .open            = fuse_lib_open,
    .read            = fuse_lib_read,
    .write_buf       = fuse_lib_write_buf,
    .flush           = fuse_lib_flush,
    .release         = fuse_lib_release,
    .fsync           = fuse_lib_fsync,
    .opendir         = fuse_lib_opendir,
    .readdir         = fuse_lib_readdir,
    .readdir_plus    = fuse_lib_readdir_plus,
    .releasedir      = fuse_lib_releasedir,
    .fsyncdir        = fuse_lib_fsyncdir,
    .statfs          = fuse_lib_statfs,
    .setxattr        = fuse_lib_setxattr,
    .getxattr        = fuse_lib_getxattr,
    .listxattr       = fuse_lib_listxattr,
    .removexattr     = fuse_lib_removexattr,
    .getlk           = fuse_lib_getlk,
    .setlk           = fuse_lib_setlk,
    .flock           = fuse_lib_flock,
    .bmap            = fuse_lib_bmap,
    .ioctl           = fuse_lib_ioctl,
    .poll            = fuse_lib_poll,
    .fallocate       = fuse_lib_fallocate,
    .copy_file_range = fuse_lib_copy_file_range,
  };

int
fuse_notify_poll(fuse_pollhandle_t *ph)
{
  return fuse_lowlevel_notify_poll(ph);
}

static
void
free_cmd(struct fuse_cmd *cmd)
{
  free(cmd->buf);
  free(cmd);
}

void
fuse_process_cmd(struct fuse     *f,
                 struct fuse_cmd *cmd)
{
  fuse_session_process(f->se,cmd->buf,cmd->buflen,cmd->ch);
  free_cmd(cmd);
}

int
fuse_exited(struct fuse *f)
{
  return fuse_session_exited(f->se);
}

struct fuse_session*
fuse_get_session(struct fuse *f)
{
  return f->se;
}

static
struct fuse_cmd*
fuse_alloc_cmd(size_t bufsize)
{
  struct fuse_cmd *cmd = (struct fuse_cmd *)malloc(sizeof(*cmd));
  if(cmd == NULL)
    {
      fprintf(stderr,"fuse: failed to allocate cmd\n");
      return NULL;
    }

  cmd->buf = (char *)malloc(bufsize);
  if(cmd->buf == NULL)
    {
      fprintf(stderr,"fuse: failed to allocate read buffer\n");
      free(cmd);
      return NULL;
    }

  return cmd;
}

struct fuse_cmd*
fuse_read_cmd(struct fuse *f)
{
  struct fuse_chan *ch = fuse_session_next_chan(f->se,NULL);
  size_t bufsize = fuse_chan_bufsize(ch);
  struct fuse_cmd *cmd = fuse_alloc_cmd(bufsize);

  if(cmd != NULL)
    {
      int res = fuse_chan_recv(&ch,cmd->buf,bufsize);
      if(res <= 0)
        {
          free_cmd(cmd);
          if(res < 0 && res != -EINTR && res != -EAGAIN)
            fuse_exit(f);
          return NULL;
        }
      cmd->buflen = res;
      cmd->ch = ch;
    }

  return cmd;
}

void
fuse_exit(struct fuse *f)
{
  fuse_session_exit(f->se);
}

struct fuse_context*
fuse_get_context(void)
{
  return &fuse_get_context_internal()->ctx;
}

enum {
  KEY_HELP,
};

#define FUSE_LIB_OPT(t,p,v) { t,offsetof(struct fuse_config,p),v }

static const struct fuse_opt fuse_lib_opts[] =
  {
    FUSE_OPT_KEY("-h",		      KEY_HELP),
    FUSE_OPT_KEY("--help",	      KEY_HELP),
    FUSE_OPT_KEY("debug",		      FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("-d",		      FUSE_OPT_KEY_KEEP),
    FUSE_LIB_OPT("debug",		      debug,1),
    FUSE_LIB_OPT("-d",		      debug,1),
    FUSE_LIB_OPT("umask=",	      set_mode,1),
    FUSE_LIB_OPT("umask=%o",	      umask,0),
    FUSE_LIB_OPT("uid=",		      set_uid,1),
    FUSE_LIB_OPT("uid=%d",	      uid,0),
    FUSE_LIB_OPT("gid=",		      set_gid,1),
    FUSE_LIB_OPT("gid=%d",	      gid,0),
    FUSE_LIB_OPT("noforget",           remember,-1),
    FUSE_LIB_OPT("remember=%u",        remember,0),
    FUSE_LIB_OPT("threads=%d",         threads,0),
    FUSE_LIB_OPT("use_ino",            use_ino,1),
    FUSE_OPT_END
  };

static void fuse_lib_help(void)
{
  fprintf(stderr,
          "    -o umask=M             set file permissions (octal)\n"
          "    -o uid=N               set file owner\n"
          "    -o gid=N               set file group\n"
          "    -o noforget            never forget cached inodes\n"
          "    -o remember=T          remember cached inodes for T seconds (0s)\n"
          "    -o threads=NUM         number of worker threads. 0 = autodetect.\n"
          "                           Negative values autodetect then divide by\n"
          "                           absolute value. default = 0\n"
          "\n");
}

static
int
fuse_lib_opt_proc(void             *data,
                  const char       *arg,
                  int               key,
                  struct fuse_args *outargs)
{
  (void)arg; (void)outargs;

  if(key == KEY_HELP)
    {
      struct fuse_config *conf = (struct fuse_config *)data;
      fuse_lib_help();
      conf->help = 1;
    }

  return 1;
}

int
fuse_is_lib_option(const char *opt)
{
  return fuse_lowlevel_is_lib_option(opt) || fuse_opt_match(fuse_lib_opts,opt);
}

struct fuse_fs*
fuse_fs_new(const struct fuse_operations *op,
            size_t                        op_size,
            void                         *user_data)
{
  struct fuse_fs *fs;

  if(sizeof(struct fuse_operations) < op_size)
    {
      fprintf(stderr,"fuse: warning: library too old,some operations may not not work\n");
      op_size = sizeof(struct fuse_operations);
    }

  fs = (struct fuse_fs *)calloc(1,sizeof(struct fuse_fs));
  if(!fs)
    {
      fprintf(stderr,"fuse: failed to allocate fuse_fs object\n");
      return NULL;
    }

  fs->user_data = user_data;
  if(op)
    memcpy(&fs->op,op,op_size);

  return fs;
}

static
int
node_table_init(struct node_table *t)
{
  t->size = NODE_TABLE_MIN_SIZE;
  t->array = (struct node **)calloc(1,sizeof(struct node *) * t->size);
  if(t->array == NULL)
    {
      fprintf(stderr,"fuse: memory allocation failed\n");
      return -1;
    }
  t->use = 0;
  t->split = 0;

  return 0;
}

static
void*
fuse_prune_nodes(void *fuse)
{
  struct fuse *f = fuse;
  int sleep_time;

  while(1)
    {
      sleep_time = fuse_clean_cache(f);
      sleep(sleep_time);
    }
  return NULL;
}

int
fuse_start_cleanup_thread(struct fuse *f)
{
  if(lru_enabled(f))
    return fuse_start_thread(&f->prune_thread,fuse_prune_nodes,f);

  return 0;
}

void
fuse_stop_cleanup_thread(struct fuse *f)
{
  if(lru_enabled(f))
    {
      pthread_mutex_lock(&f->lock);
      pthread_cancel(f->prune_thread);
      pthread_mutex_unlock(&f->lock);
      pthread_join(f->prune_thread,NULL);
    }
}

struct fuse*
fuse_new_common(struct fuse_chan             *ch,
                struct fuse_args             *args,
                const struct fuse_operations *op,
                size_t                        op_size,
                void                         *user_data)
{
  struct fuse *f;
  struct node *root;
  struct fuse_fs *fs;
  struct fuse_lowlevel_ops llop = fuse_path_ops;

  if(fuse_create_context_key() == -1)
    goto out;

  f = (struct fuse *)calloc(1,sizeof(struct fuse));
  if(f == NULL)
    {
      fprintf(stderr,"fuse: failed to allocate fuse object\n");
      goto out_delete_context_key;
    }

  fs = fuse_fs_new(op,op_size,user_data);
  if(!fs)
    goto out_free;

  f->fs = fs;

  /* Oh f**k,this is ugly! */
  if(!fs->op.lock)
    {
      llop.getlk = NULL;
      llop.setlk = NULL;
    }

  f->pagesize = getpagesize();
  init_list_head(&f->partial_slabs);
  init_list_head(&f->full_slabs);
  init_list_head(&f->lru_table);

  if(fuse_opt_parse(args,&f->conf,fuse_lib_opts,fuse_lib_opt_proc) == -1)
    goto out_free_fs;

  f->se = fuse_lowlevel_new_common(args,&llop,sizeof(llop),f);
  if(f->se == NULL)
    goto out_free_fs;

  fuse_session_add_chan(f->se,ch);

  /* Trace topmost layer by default */
  srand(time(NULL));
  f->fs->debug = f->conf.debug;
  f->ctr = 0;
  f->generation = rand64();
  if(node_table_init(&f->name_table) == -1)
    goto out_free_session;

  if(node_table_init(&f->id_table) == -1)
    goto out_free_name_table;

  fuse_mutex_init(&f->lock);

  root = alloc_node(f);
  if(root == NULL)
    {
      fprintf(stderr,"fuse: memory allocation failed\n");
      goto out_free_id_table;
    }

  if(lru_enabled(f))
    {
      struct node_lru *lnode = node_lru(root);
      init_list_head(&lnode->lru);
    }

  strcpy(root->inline_name,"/");
  root->name = root->inline_name;

  root->parent = NULL;
  root->nodeid = FUSE_ROOT_ID;
  inc_nlookup(root);
  hash_id(f,root);

  return f;

 out_free_id_table:
  free(f->id_table.array);
 out_free_name_table:
  free(f->name_table.array);
 out_free_session:
  fuse_session_destroy(f->se);
 out_free_fs:
  /* Horrible compatibility hack to stop the destructor from being
     called on the filesystem without init being called first */
  fs->op.destroy = NULL;
  fuse_fs_destroy(f->fs);
 out_free:
  free(f);
 out_delete_context_key:
  fuse_delete_context_key();
 out:
  return NULL;
}

struct fuse*
fuse_new(struct fuse_chan             *ch,
         struct fuse_args             *args,
         const struct fuse_operations *op,
         size_t                        op_size,
         void                         *user_data)
{
  return fuse_new_common(ch,args,op,op_size,user_data);
}

void
fuse_destroy(struct fuse *f)
{
  size_t i;

  if(f->fs)
    {
      struct fuse_context_i *c = fuse_get_context_internal();

      memset(c,0,sizeof(*c));
      c->ctx.fuse = f;

      for(i = 0; i < f->id_table.size; i++)
        {
          struct node *node;

          for(node = f->id_table.array[i]; node != NULL; node = node->id_next)
            {
              if(node->is_hidden)
                fuse_fs_free_hide(f->fs,node->hidden_fh);
            }
        }
    }

  for(i = 0; i < f->id_table.size; i++)
    {
      struct node *node;
      struct node *next;

      for(node = f->id_table.array[i]; node != NULL; node = next)
        {
          next = node->id_next;
          free_node(f,node);
          f->id_table.use--;
        }
    }

  assert(list_empty(&f->partial_slabs));
  assert(list_empty(&f->full_slabs));

  free(f->id_table.array);
  free(f->name_table.array);
  pthread_mutex_destroy(&f->lock);
  fuse_session_destroy(f->se);
  free(f);
  fuse_delete_context_key();
}

int
fuse_config_num_threads(const struct fuse *fuse_)
{
  return fuse_->conf.threads;
}

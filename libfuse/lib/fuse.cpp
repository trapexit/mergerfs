/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

/* For pthread_rwlock_t */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "syslog.hpp"

#include "crc32b.h"
#include "khash.h"
#include "kvec.h"
#include "mutex.hpp"
#include "node.hpp"

#include "fuse_cfg.hpp"
#include "fuse_req.h"
#include "fuse_dirents.hpp"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_opt.h"
#include "fuse_pollhandle.h"
#include "fuse_msgbuf.hpp"

#include "maintenance_thread.hpp"

#include <string>
#include <vector>

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#ifdef __GLIBC__
#include <malloc.h>
#endif

#define FUSE_UNKNOWN_INO UINT64_MAX
#define OFFSET_MAX 0x7fffffffffffffffLL

#define NODE_TABLE_MIN_SIZE 8192

#define PARAM(inarg) ((void*)(((char*)(inarg)) + sizeof(*(inarg))))

static int g_LOG_METRICS = 0;

struct fuse_config
{
};

struct lock_queue_element
{
  struct lock_queue_element *next;
  pthread_cond_t cond;
  uint64_t   nodeid1;
  const char *name1;
  char **path1;
  node_t **wnode1;
  uint64_t   nodeid2;
  const char *name2;
  char **path2;
  node_t **wnode2;
  int err;
  bool done : 1;
};

struct node_table
{
  node_t **array;
  size_t   use;
  size_t   size;
  size_t   split;
};

struct list_head
{
  struct list_head *next;
  struct list_head *prev;
};

typedef struct remembered_node_t remembered_node_t;
struct remembered_node_t
{
  node_t *node;
  time_t  time;
};

typedef struct nodeid_gen_t nodeid_gen_t;
struct nodeid_gen_t
{
  uint64_t nodeid;
  uint64_t generation;
};

struct fuse
{
  struct fuse_session *se;
  struct node_table name_table;
  struct node_table id_table;
  nodeid_gen_t nodeid_gen;
  unsigned int hidectr;
  pthread_mutex_t lock;
  struct fuse_config conf;
  fuse_operations ops;
  struct lock_queue_element *lockq;

  kvec_t(remembered_node_t) remembered_nodes;
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

#define TREELOCK_WRITE -1
#define TREELOCK_WAIT_OFFSET INT_MIN

struct fuse_dh
{
  pthread_mutex_t lock;
  uint64_t        fh;
  fuse_dirents_t  d;
};

static struct fuse f = {};

/*
  Why was the nodeid:generation logic simplified?

  nodeid is uint64_t: max value of 18446744073709551616
  If nodes were created at a rate of 1048576 per second it would take
  over 500 thousand years to roll over. I'm fine with risking that.
*/
static
uint64_t
generate_nodeid(nodeid_gen_t *ng_)
{
  ng_->nodeid++;

  return ng_->nodeid;
}

static
void*
fuse_hdr_arg(const struct fuse_in_header *hdr_)
{
  return (void*)&hdr_[1];
}

static
size_t
id_hash(uint64_t ino)
{
  uint64_t hash = ((uint32_t)ino * 2654435761U) % f.id_table.size;
  uint64_t oldhash = hash % (f.id_table.size / 2);

  if(oldhash >= f.id_table.split)
    return oldhash;
  else
    return hash;
}

static
node_t*
get_node_nocheck(uint64_t nodeid)
{
  size_t hash = id_hash(nodeid);
  node_t *node;

  for(node = f.id_table.array[hash]; node != NULL; node = node->id_next)
    if(node->nodeid == nodeid)
      return node;

  return NULL;
}

static
node_t*
get_node(const uint64_t nodeid)
{
  node_t *node = get_node_nocheck(nodeid);

  if(!node)
    {
      fprintf(stderr,"fuse internal error: node %llu not found\n",
              (unsigned long long)nodeid);
      abort();
    }

  return node;
}

static
void
remove_remembered_node(node_t *node_)
{
  for(size_t i = 0; i < kv_size(f.remembered_nodes); i++)
    {
      if(kv_A(f.remembered_nodes,i).node != node_)
        continue;

      kv_delete(f.remembered_nodes,i);
      break;
    }
}

static
uint32_t
stat_crc32b(const struct stat *st_)
{
  uint32_t crc;

  crc = crc32b_start();
  crc = crc32b_continue(&st_->st_ino,sizeof(st_->st_ino),crc);
  crc = crc32b_continue(&st_->st_size,sizeof(st_->st_size),crc);
  crc = crc32b_continue(&st_->st_mtim,sizeof(st_->st_mtim),crc);
  crc = crc32b_finish(crc);

  return crc;
}

#ifndef CLOCK_MONOTONIC
# define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

static
time_t
current_time()
{
  int rv;
  struct timespec now;
  static clockid_t clockid = CLOCK_MONOTONIC;

  rv = clock_gettime(clockid,&now);
  if((rv == -1) && (errno == EINVAL))
    {
      clockid = CLOCK_REALTIME;
      rv = clock_gettime(clockid,&now);
    }

  if(rv == -1)
    now.tv_sec = time(NULL);

  return now.tv_sec;
}

static
void
free_node(node_t *node_)
{
  free(node_->name);

  node_free(node_);
}

static
void
node_table_reduce(struct node_table *t)
{
  size_t newsize = t->size / 2;
  void *newarray;

  if(newsize < NODE_TABLE_MIN_SIZE)
    return;

  newarray = realloc(t->array,sizeof(node_t*) * newsize);
  if(newarray != NULL)
    t->array = (node_t**)newarray;

  t->size = newsize;
  t->split = t->size / 2;
}

static
void
remerge_id()
{
  struct node_table *t = &f.id_table;
  int iter;

  if(t->split == 0)
    node_table_reduce(t);

  for(iter = 8; t->split > 0 && iter; iter--)
    {
      node_t **upper;

      t->split--;
      upper = &t->array[t->split + t->size / 2];
      if(*upper)
        {
          node_t **nodep;

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
unhash_id(node_t *node)
{
  node_t **nodep = &f.id_table.array[id_hash(node->nodeid)];

  for(; *nodep != NULL; nodep = &(*nodep)->id_next)
    if(*nodep == node)
      {
        *nodep = node->id_next;
        f.id_table.use--;

        if(f.id_table.use < (f.id_table.size / 4))
          remerge_id();
        return;
      }
}

static
int
node_table_resize(struct node_table *t)
{
  size_t newsize = t->size * 2;
  void *newarray;

  newarray = realloc(t->array,sizeof(node_t*) * newsize);
  if(newarray == NULL)
    return -1;

  t->array = (node_t**)newarray;
  memset(t->array + t->size,0,t->size * sizeof(node_t*));
  t->size = newsize;
  t->split = 0;

  return 0;
}

static
void
rehash_id()
{
  struct node_table *t = &f.id_table;
  node_t **nodep;
  node_t **next;
  size_t hash;

  if(t->split == t->size / 2)
    return;

  hash = t->split;
  t->split++;
  for(nodep = &t->array[hash]; *nodep != NULL; nodep = next)
    {
      node_t *node = *nodep;
      size_t newhash = id_hash(node->nodeid);

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
hash_id(node_t *node)
{
  size_t hash;

  hash = id_hash(node->nodeid);
  node->id_next = f.id_table.array[hash];
  f.id_table.array[hash] = node;
  f.id_table.use++;

  if(f.id_table.use >= (f.id_table.size / 2))
    rehash_id();
}

static
size_t
name_hash(uint64_t    parent,
          const char *name)
{
  uint64_t hash = parent;
  uint64_t oldhash;

  for(; *name; name++)
    hash = hash * 31 + (unsigned char)*name;

  hash %= f.name_table.size;
  oldhash = hash % (f.name_table.size / 2);
  if(oldhash >= f.name_table.split)
    return oldhash;
  else
    return hash;
}

static
void
unref_node(node_t *node);

static
void
remerge_name()
{
  int iter;
  struct node_table *t = &f.name_table;

  if(t->split == 0)
    node_table_reduce(t);

  for(iter = 8; t->split > 0 && iter; iter--)
    {
      node_t **upper;

      t->split--;
      upper = &t->array[t->split + t->size / 2];
      if(*upper)
        {
          node_t **nodep;

          for(nodep = &t->array[t->split]; *nodep; nodep = &(*nodep)->name_next);

          *nodep = *upper;
          *upper = NULL;
          break;
        }
    }
}

static
void
unhash_name(node_t *node)
{
  if(node->name)
    {
      size_t hash = name_hash(node->parent->nodeid,node->name);
      node_t **nodep = &f.name_table.array[hash];

      for(; *nodep != NULL; nodep = &(*nodep)->name_next)
        if(*nodep == node)
          {
            *nodep = node->name_next;
            node->name_next = NULL;
            unref_node(node->parent);
            free(node->name);
            node->name = NULL;
            node->parent = NULL;
            f.name_table.use--;

            if(f.name_table.use < f.name_table.size / 4)
              remerge_name();
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
rehash_name()
{
  struct node_table *t = &f.name_table;
  node_t **nodep;
  node_t **next;
  size_t hash;

  if(t->split == t->size / 2)
    return;

  hash = t->split;
  t->split++;
  for(nodep = &t->array[hash]; *nodep != NULL; nodep = next)
    {
      node_t *node = *nodep;
      size_t newhash = name_hash(node->parent->nodeid,node->name);

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
hash_name(node_t     *node,
          uint64_t    parentid,
          const char *name)
{
  size_t hash = name_hash(parentid,name);
  node_t *parent = get_node(parentid);
  node->name = strdup(name);
  if(node->name == NULL)
    return -1;

  parent->refctr++;
  node->parent = parent;
  node->name_next = f.name_table.array[hash];
  f.name_table.array[hash] = node;
  f.name_table.use++;

  if(f.name_table.use >= f.name_table.size / 2)
    rehash_name();

  return 0;
}

static
inline
int
remember_nodes()
{
  return (fuse_cfg.remember_nodes > 0);
}

static
void
delete_node(node_t *node)
{
  assert(node->treelock == 0);
  unhash_name(node);
  if(remember_nodes())
    remove_remembered_node(node);
  unhash_id(node);
  node_free(node);
}

static
void
unref_node(node_t *node)
{
  assert(node->refctr > 0);
  node->refctr--;
  if(!node->refctr)
    delete_node(node);
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
node_t*
lookup_node(uint64_t    parent,
            const char *name)
{
  size_t hash;
  node_t *node;

  hash =  name_hash(parent,name);
  for(node = f.name_table.array[hash]; node != NULL; node = node->name_next)
    if(node->parent->nodeid == parent && strcmp(node->name,name) == 0)
      return node;

  return NULL;
}

static
void
inc_nlookup(node_t *node)
{
  if(!node->nlookup)
    node->refctr++;
  node->nlookup++;
}

static
node_t*
find_node(uint64_t    parent,
          const char *name)
{
  node_t *node;

  mutex_lock(&f.lock);
  if(!name)
    node = get_node(parent);
  else
    node = lookup_node(parent,name);

  if(node == NULL)
    {
      node = node_alloc();
      if(node == NULL)
        goto out_err;

      node->nodeid = generate_nodeid(&f.nodeid_gen);
      if(fuse_cfg.remember_nodes)
        inc_nlookup(node);

      if(hash_name(node,parent,name) == -1)
        {
          free_node(node);
          node = NULL;
          goto out_err;
        }
      hash_id(node);
    }
  else if((node->nlookup == 1) && remember_nodes())
    {
      remove_remembered_node(node);
    }
  inc_nlookup(node);
 out_err:
  mutex_unlock(&f.lock);
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

      newbuf = (char*)realloc(*buf,newbufsize);
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
unlock_path(uint64_t  nodeid,
            node_t   *wnode,
            node_t   *end)
{
  node_t *node;

  if(wnode)
    {
      assert(wnode->treelock == TREELOCK_WRITE);
      wnode->treelock = 0;
    }

  for(node = get_node(nodeid);
      node != end && node->nodeid != FUSE_ROOT_ID;
      node = node->parent)
    {
      assert(node->treelock != 0);
      assert(node->treelock != TREELOCK_WAIT_OFFSET);
      assert(node->treelock != TREELOCK_WRITE);
      node->treelock--;
      if(node->treelock == TREELOCK_WAIT_OFFSET)
        node->treelock = 0;
    }
}

/*
  Throughout this file all calls to the op.func() callbacks will be
  '&fusepath[1]' because the current path generation always prefixes a
  '/' for each path but we need a relative path for usage with
  `openat()` functions and would rather do that here than in the
  called function.
*/

static
int
try_get_path(uint64_t      nodeid,
             const char   *name,
             char        **path,
             node_t      **wnodep,
             bool          need_lock)
{
  unsigned bufsize = 256;
  char *buf;
  char *s;
  node_t *node;
  node_t *wnode = NULL;
  int err;

  *path = NULL;

  err = -ENOMEM;
  buf = (char*)malloc(bufsize);
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
      wnode = lookup_node(nodeid,name);
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

  for(node = get_node(nodeid); node->nodeid != FUSE_ROOT_ID; node = node->parent)
    {
      err = -ESTALE;
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
    unlock_path(nodeid,wnode,node);
 out_free:
  free(buf);

 out_err:
  return err;
}

static
int
try_get_path2(uint64_t     nodeid1,
              const char  *name1,
              uint64_t     nodeid2,
              const char  *name2,
              char       **path1,
              char       **path2,
              node_t     **wnode1,
              node_t     **wnode2)
{
  int err;

  err = try_get_path(nodeid1,name1,path1,wnode1,true);
  if(!err)
    {
      err = try_get_path(nodeid2,name2,path2,wnode2,true);
      if(err)
        {
          node_t *wn1 = wnode1 ? *wnode1 : NULL;

          unlock_path(nodeid1,wn1,NULL);
          free(*path1);
        }
    }

  return err;
}

static
void
queue_element_wakeup(struct lock_queue_element *qe)
{
  int err;

  if(!qe->path1)
    {
      /* Just waiting for it to be unlocked */
      if(get_node(qe->nodeid1)->treelock == 0)
        pthread_cond_signal(&qe->cond);

      return;
    }

  if(qe->done)
    return;

  if(!qe->path2)
    {
      err = try_get_path(qe->nodeid1,
                         qe->name1,
                         qe->path1,
                         qe->wnode1,
                         true);
    }
  else
    {
      err = try_get_path2(qe->nodeid1,
                          qe->name1,
                          qe->nodeid2,
                          qe->name2,
                          qe->path1,
                          qe->path2,
                          qe->wnode1,
                          qe->wnode2);
    }

  if(err == -EAGAIN)
    return;

  qe->err = err;
  qe->done = true;
  pthread_cond_signal(&qe->cond);
}

static
void
wake_up_queued()
{
  struct lock_queue_element *qe;

  for(qe = f.lockq; qe != NULL; qe = qe->next)
    queue_element_wakeup(qe);
}

static
void
queue_path(struct lock_queue_element *qe)
{
  struct lock_queue_element **qp;

  qe->done = false;
  pthread_cond_init(&qe->cond,NULL);
  qe->next = NULL;
  for(qp = &f.lockq; *qp != NULL; qp = &(*qp)->next);
  *qp = qe;
}

static
void
dequeue_path(struct lock_queue_element *qe)
{
  struct lock_queue_element **qp;

  pthread_cond_destroy(&qe->cond);
  for(qp = &f.lockq; *qp != qe; qp = &(*qp)->next);
  *qp = qe->next;
}

static
int
wait_path(struct lock_queue_element *qe)
{
  queue_path(qe);

  do
    {
      pthread_cond_wait(&qe->cond,&f.lock);
    } while(!qe->done);

  dequeue_path(qe);

  return qe->err;
}

static
int
get_path_common(uint64_t     nodeid,
                const char  *name,
                char       **path,
                node_t     **wnode)
{
  int err;

  mutex_lock(&f.lock);
  err = try_get_path(nodeid,name,path,wnode,true);
  if(err == -EAGAIN)
    {
      struct lock_queue_element qe = {};

      qe.nodeid1 = nodeid;
      qe.name1   = name;
      qe.path1   = path;
      qe.wnode1  = wnode;

      err = wait_path(&qe);
    }
  mutex_unlock(&f.lock);

  return err;
}

static
int
get_path(uint64_t   nodeid,
         char     **path)
{
  return get_path_common(nodeid,NULL,path,NULL);
}

static
int
get_path_name(uint64_t     nodeid,
              const char  *name,
              char       **path)
{
  return get_path_common(nodeid,name,path,NULL);
}

static
int
get_path_wrlock(uint64_t     nodeid,
                const char  *name,
                char       **path,
                node_t     **wnode)
{
  return get_path_common(nodeid,name,path,wnode);
}

static
int
get_path2(uint64_t     nodeid1,
          const char  *name1,
          uint64_t     nodeid2,
          const char  *name2,
          char       **path1,
          char       **path2,
          node_t     **wnode1,
          node_t     **wnode2)
{
  int err;

  mutex_lock(&f.lock);
  err = try_get_path2(nodeid1,name1,nodeid2,name2,
                      path1,path2,wnode1,wnode2);
  if(err == -EAGAIN)
    {
      struct lock_queue_element qe = {};

      qe.nodeid1 = nodeid1;
      qe.name1   = name1;
      qe.path1   = path1;
      qe.wnode1  = wnode1;
      qe.nodeid2 = nodeid2;
      qe.name2   = name2;
      qe.path2   = path2;
      qe.wnode2  = wnode2;

      err = wait_path(&qe);
    }
  mutex_unlock(&f.lock);

  return err;
}

static
void
free_path_wrlock(uint64_t  nodeid,
                 node_t   *wnode,
                 char     *path)
{
  mutex_lock(&f.lock);
  unlock_path(nodeid,wnode,NULL);
  if(f.lockq)
    wake_up_queued();
  mutex_unlock(&f.lock);
  free(path);
}

static
void
free_path(uint64_t  nodeid,
          char     *path)
{
  if(path)
    free_path_wrlock(nodeid,NULL,path);
}

static
void
free_path2(uint64_t  nodeid1,
           uint64_t  nodeid2,
           node_t   *wnode1,
           node_t   *wnode2,
           char     *path1,
           char     *path2)
{
  mutex_lock(&f.lock);
  unlock_path(nodeid1,wnode1,NULL);
  unlock_path(nodeid2,wnode2,NULL);
  wake_up_queued();
  mutex_unlock(&f.lock);
  free(path1);
  free(path2);
}

static
void
forget_node(const uint64_t nodeid,
            const uint64_t nlookup)
{
  node_t *node;

  if(nodeid == FUSE_ROOT_ID)
    return;

  mutex_lock(&f.lock);
  node = get_node(nodeid);

  /*
   * Node may still be locked due to interrupt idiocy in open,
   * create and opendir
   */
  while(node->nlookup == nlookup && node->treelock)
    {
      struct lock_queue_element qe = {};

      qe.nodeid1 = nodeid;

      queue_path(&qe);

      do
        {
          pthread_cond_wait(&qe.cond,&f.lock);
        }
      while((node->nlookup == nlookup) && node->treelock);

      dequeue_path(&qe);
    }

  assert(node->nlookup >= nlookup);
  node->nlookup -= nlookup;

  if(node->nlookup == 0)
    {
      unref_node(node);
    }
  else if((node->nlookup == 1) && remember_nodes())
    {
      remembered_node_t fn;

      fn.node = node;
      fn.time = current_time();

      kv_push(remembered_node_t,f.remembered_nodes,fn);
    }

  mutex_unlock(&f.lock);
}

static
void
unlink_node(node_t *node)
{
  if(remember_nodes())
    {
      assert(node->nlookup > 1);
      node->nlookup--;
    }
  unhash_name(node);
}

static
void
remove_node(uint64_t    dir,
            const char *name)
{
  node_t *node;

  mutex_lock(&f.lock);
  node = lookup_node(dir,name);
  if(node != NULL)
    unlink_node(node);
  mutex_unlock(&f.lock);
}

static
int
rename_node(uint64_t    olddir,
            const char *oldname,
            uint64_t    newdir,
            const char *newname)
{
  int err = 0;
  node_t *node;
  node_t *newnode;

  mutex_lock(&f.lock);
  node = lookup_node(olddir,oldname);
  newnode = lookup_node(newdir,newname);
  if(node == NULL)
    goto out;

  if(newnode != NULL)
    unlink_node(newnode);

  unhash_name(node);
  if(hash_name(node,newdir,newname) == -1)
    {
      err = -ENOMEM;
      goto out;
    }

 out:
  mutex_unlock(&f.lock);
  return err;
}

static
void
set_stat(uint64_t     nodeid,
         struct stat *stbuf)
{
  if(fuse_cfg.valid_uid())
    stbuf->st_uid = fuse_cfg.uid;
  if(fuse_cfg.valid_gid())
    stbuf->st_gid = fuse_cfg.gid;
  if(fuse_cfg.valid_umask())
    stbuf->st_mode = (stbuf->st_mode & S_IFMT) | (0777 & ~fuse_cfg.umask);
}

static
int
node_open(const node_t *node_)
{
  return ((node_ != NULL) && (node_->open_count > 0));
}

static
void
update_stat(node_t       *node_,
            const struct stat *stnew_)
{
  uint32_t crc32b;

  crc32b = stat_crc32b(stnew_);

  if(node_->is_stat_cache_valid && (crc32b != node_->stat_crc32b))
    node_->is_stat_cache_valid = 0;

  node_->stat_crc32b = crc32b;
}

static
int
set_path_info(uint64_t                 nodeid,
              const char              *name,
              struct fuse_entry_param *e)
{
  node_t *node;

  node = find_node(nodeid,name);
  if(node == NULL)
    return -ENOMEM;

  e->ino        = node->nodeid;
  e->generation = ((e->ino == FUSE_ROOT_ID) ? 0 : f.nodeid_gen.generation);

  mutex_lock(&f.lock);
  update_stat(node,&e->attr);
  mutex_unlock(&f.lock);

  set_stat(e->ino,&e->attr);

  return 0;
}

/*
  lookup requests only come in for FUSE_ROOT_ID when a "parent of
  child of root node" request is made. This can happen when using
  EXPORT_SUPPORT=true and a file handle is used to keep a reference to
  a node which has been forgotten. Mostly a NFS concern but not
  excluslively. Root node always has a nodeid of 1 and generation of
  0. To ensure this set_path_info() explicitly ensures the root id has
  a generation of 0.
*/
static
int
lookup_path(fuse_req_t              *req_,
            uint64_t                 nodeid,
            const char              *name,
            const char              *fusepath,
            struct fuse_entry_param *e,
            fuse_file_info_t        *fi)
{
  int rv;

  assert(req_ != NULL);

  memset(e,0,sizeof(struct fuse_entry_param));

  rv = ((fi == NULL) ?
        f.ops.getattr(&req_->ctx,&fusepath[1],&e->attr,&e->timeout) :
        f.ops.fgetattr(&req_->ctx,fi->fh,&e->attr,&e->timeout));

  if(rv)
    return rv;

  return set_path_info(nodeid,name,e);
}

static
void
reply_entry(fuse_req_t                    *req_,
            const struct fuse_entry_param *e,
            int                            err)
{
  if(!err)
    {
      if(fuse_reply_entry(req_,e) == -ENOENT)
        {
          /* Skip forget for negative result */
          if(e->ino != 0)
            forget_node(e->ino,1);
        }
    }
  else
    {
      fuse_reply_err(req_,err);
    }
}

static
void
fuse_lib_init(void                  *data,
              struct fuse_conn_info *conn)
{
  f.ops.init(conn);
}

static
void
fuse_lib_destroy(void *data)
{
  f.ops.destroy();
}

static
void
fuse_lib_lookup(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  int err;
  uint64_t nodeid;
  char *fusepath;
  const char *name;
  node_t *dot = NULL;
  struct fuse_entry_param e = {};

  name   = (const char*)fuse_hdr_arg(hdr_);
  nodeid = hdr_->nodeid;

  if(name[0] == '.')
    {
      if(name[1] == '\0')
        {
          name = NULL;
          mutex_lock(&f.lock);
          dot = get_node_nocheck(nodeid);
          if(dot == NULL)
            {
              mutex_unlock(&f.lock);
              reply_entry(req_,&e,-ESTALE);
              return;
            }
          dot->refctr++;
          mutex_unlock(&f.lock);
        }
      else if((name[1] == '.') && (name[2] == '\0'))
        {
          if(nodeid == 1)
            {
              reply_entry(req_,&e,-ENOENT);
              return;
            }

          name = NULL;
          mutex_lock(&f.lock);
          nodeid = get_node(nodeid)->parent->nodeid;
          mutex_unlock(&f.lock);
        }
    }

  err = get_path_name(nodeid,name,&fusepath);
  if(!err)
    {
      err = lookup_path(req_,nodeid,name,fusepath,&e,NULL);
      if(err == -ENOENT)
        {
          e.ino = 0;
          err = 0;
        }
      free_path(nodeid,fusepath);
    }

  if(dot)
    {
      mutex_lock(&f.lock);
      unref_node(dot);
      mutex_unlock(&f.lock);
    }

  reply_entry(req_,&e,err);
}

static
void
fuse_lib_lseek(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

static
void
fuse_lib_forget(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  struct fuse_forget_in *arg;

  arg = (fuse_forget_in*)fuse_hdr_arg(hdr_);

  forget_node(hdr_->nodeid,arg->nlookup);

  fuse_reply_none(req_);
}

static
void
fuse_lib_forget_multi(fuse_req_t            *req_,
                      struct fuse_in_header *hdr_)
{
  struct fuse_batch_forget_in *arg;
  struct fuse_forget_one      *entry;

  arg   = (fuse_batch_forget_in*)fuse_hdr_arg(hdr_);
  entry = (fuse_forget_one*)PARAM(arg);

  for(uint32_t i = 0; i < arg->count; i++)
    {
      forget_node(entry[i].nodeid,
                  entry[i].nlookup);
    }

  fuse_reply_none(req_);
}


static
void
fuse_lib_getattr(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  uint64_t fh;
  struct stat buf;
  node_t *node;
  fuse_timeouts_t timeout;
  const struct fuse_getattr_in *arg;

  arg = (fuse_getattr_in*)fuse_hdr_arg(hdr_);

  fh = 0;
  if(arg->getattr_flags & FUSE_GETATTR_FH)
    fh = arg->fh;

  memset(&buf,0,sizeof(buf));

  err = 0;
  fusepath = NULL;
  if(fh == 0)
    {
      err = get_path(hdr_->nodeid,&fusepath);
      if(err == -ESTALE) // unlinked but open
        err = 0;
    }

  if(!err)
    {
      fmt::println("getattr: fh={}; filepath={};",
                   fh,filepath);
      err = ((fusepath != NULL) ?
             f.ops.getattr(&req_->ctx,&fusepath[1],&buf,&timeout) :
             f.ops.fgetattr(&req_->ctx,fh,&buf,&timeout));

      free_path(hdr_->nodeid,fusepath);
    }

  if(!err)
    {
      mutex_lock(&f.lock);
      node = get_node(hdr_->nodeid);
      update_stat(node,&buf);
      mutex_unlock(&f.lock);
      set_stat(hdr_->nodeid,&buf);
      fuse_reply_attr(req_,&buf,timeout.attr);
    }
  else
    {
      fuse_reply_err(req_,err);
    }
}

static
void
fuse_lib_statx_path(fuse_req_t            *req_,
                    struct fuse_in_header *hdr_,
                    fuse_statx_in         *inarg_)
{
  int err;
  char *fusepath;
  struct fuse_statx st{};
  fuse_timeouts_t timeouts{};

  // TODO: if node unlinked... but FUSE may not send it
  err = get_path(hdr_->nodeid,&fusepath);
  if(err)
    {
      fuse_reply_err(req_,err);
      return;
    }

  err = f.ops.statx(&req_->ctx,
                    &fusepath[1],
                    inarg_->sx_flags,
                    inarg_->sx_mask,
                    &st,
                    &timeouts);

  free_path(hdr_->nodeid,fusepath);

  if(err)
    fuse_reply_err(req_,err);
  else
    fuse_reply_statx(req_,0,&st,timeouts.attr);
}

static
void
fuse_lib_statx_fh(fuse_req_t            *req_,
                  struct fuse_in_header *hdr_,
                  fuse_statx_in         *inarg_)
{
  int err;
  struct fuse_statx st{};
  fuse_timeouts_t timeouts{};

  err = f.ops.statx_fh(&req_->ctx,
                       inarg_->fh,
                       inarg_->sx_flags,
                       inarg_->sx_mask,
                       &st,
                       &timeouts);

  if(err)
    fuse_reply_err(req_,err);
  else
    fuse_reply_statx(req_,0,&st,timeouts.attr);
}

static
void
fuse_lib_statx(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  fuse_statx_in *inarg;

  inarg = (fuse_statx_in*)fuse_hdr_arg(hdr_);

  if(inarg->getattr_flags & FUSE_GETATTR_FH)
    fuse_lib_statx_fh(req_,hdr_,inarg);
  else
    fuse_lib_statx_path(req_,hdr_,inarg);
}

static
void
fuse_lib_setattr(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  uint64_t fh;
  struct stat stbuf = {};
  char *fusepath;
  int err;
  fuse_timeouts_t timeout;
  struct fuse_setattr_in *arg;

  arg = (fuse_setattr_in*)fuse_hdr_arg(hdr_);

  fh = 0;
  if(arg->valid & FATTR_FH)
    fh = arg->fh;

  err = 0;
  fusepath = NULL;
  if(fh == 0)
    {
      err = get_path(hdr_->nodeid,&fusepath);
      if(err == -ESTALE)
        err = 0;
    }

  if(!err)
    {
      err = 0;
      if(!err && (arg->valid & FATTR_MODE))
        err = ((fusepath != NULL) ?
               f.ops.chmod(&req_->ctx,&fusepath[1],arg->mode) :
               f.ops.fchmod(&req_->ctx,fh,arg->mode));

      if(!err && (arg->valid & (FATTR_UID | FATTR_GID)))
        {
          uid_t uid = ((arg->valid & FATTR_UID) ? arg->uid : (uid_t)-1);
          gid_t gid = ((arg->valid & FATTR_GID) ? arg->gid : (gid_t)-1);

          err = ((fusepath != NULL) ?
                 f.ops.chown(&req_->ctx,&fusepath[1],uid,gid) :
                 f.ops.fchown(&req_->ctx,fh,uid,gid));
        }

      if(!err && (arg->valid & FATTR_SIZE))
        err = ((fusepath != NULL) ?
               f.ops.truncate(&req_->ctx,&fusepath[1],arg->size) :
               f.ops.ftruncate(&req_->ctx,fh,arg->size));

      if(!err && (arg->valid & (FATTR_ATIME | FATTR_MTIME)))
        {
          struct timespec tv[2];

          tv[0].tv_sec = 0;
          tv[1].tv_sec = 0;
          tv[0].tv_nsec = UTIME_OMIT;
          tv[1].tv_nsec = UTIME_OMIT;

          if(arg->valid & FATTR_ATIME_NOW)
            tv[0].tv_nsec = UTIME_NOW;
          else if(arg->valid & FATTR_ATIME)
            tv[0] = (struct timespec){ static_cast<time_t>(arg->atime), static_cast<long>(arg->atimensec) };

          if(arg->valid & FATTR_MTIME_NOW)
            tv[1].tv_nsec = UTIME_NOW;
          else if(arg->valid & FATTR_MTIME)
            tv[1] = (struct timespec){ static_cast<time_t>(arg->mtime), static_cast<long>(arg->mtimensec) };

          err = ((fusepath != NULL) ?
                 f.ops.utimens(&req_->ctx,&fusepath[1],tv) :
                 f.ops.futimens(&req_->ctx,fh,tv));
        }
      else if(!err && ((arg->valid & (FATTR_ATIME|FATTR_MTIME)) == (FATTR_ATIME|FATTR_MTIME)))
        {
          struct timespec tv[2];
          tv[0].tv_sec  = arg->atime;
          tv[0].tv_nsec = arg->atimensec;
          tv[1].tv_sec  = arg->mtime;
          tv[1].tv_nsec = arg->mtimensec;
          err = ((fusepath != NULL) ?
                 f.ops.utimens(&req_->ctx,&fusepath[1],tv) :
                 f.ops.futimens(&req_->ctx,fh,tv));
        }

      if(!err)
        err = ((fusepath != NULL) ?
               f.ops.getattr(&req_->ctx,&fusepath[1],&stbuf,&timeout) :
               f.ops.fgetattr(&req_->ctx,fh,&stbuf,&timeout));

      free_path(hdr_->nodeid,fusepath);
    }

  if(!err)
    {
      mutex_lock(&f.lock);
      update_stat(get_node(hdr_->nodeid),&stbuf);
      mutex_unlock(&f.lock);
      set_stat(hdr_->nodeid,&stbuf);
      fuse_reply_attr(req_,&stbuf,timeout.attr);
    }
  else
    {
      fuse_reply_err(req_,err);
    }
}

static
void
fuse_lib_access(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  struct fuse_access_in *arg;

  arg = (fuse_access_in*)fuse_hdr_arg(hdr_);

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      err = f.ops.access(&req_->ctx,
                         &fusepath[1],
                         arg->mask);
      free_path(hdr_->nodeid,fusepath);
    }

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_readlink(fuse_req_t            *req_,
                  struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  char linkname[PATH_MAX + 1];

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      err = f.ops.readlink(&req_->ctx,
                           &fusepath[1],
                           linkname,
                           sizeof(linkname));
      free_path(hdr_->nodeid,fusepath);
    }

  if(!err)
    {
      linkname[PATH_MAX] = '\0';
      fuse_reply_readlink(req_,linkname);
    }
  else
    {
      fuse_reply_err(req_,err);
    }
}

static
void
fuse_lib_mknod(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char* name;
  struct fuse_entry_param e;
  struct fuse_mknod_in *arg;

  arg  = (fuse_mknod_in*)fuse_hdr_arg(hdr_);
  name = (const char*)PARAM(arg);

  if(req_->f->conn.proto_minor >= 12)
    req_->ctx.umask = arg->umask;
  else
    name = (char*)arg + FUSE_COMPAT_MKNOD_IN_SIZE;

  err = get_path_name(hdr_->nodeid,name,&fusepath);
  if(!err)
    {
      err = -ENOSYS;
      if(S_ISREG(arg->mode))
        {
          fuse_file_info_t fi;

          memset(&fi,0,sizeof(fi));
          fi.flags = O_CREAT | O_EXCL | O_WRONLY;
          err = f.ops.create(&req_->ctx,
                             &fusepath[1],
                             arg->mode,
                             &fi);
          if(!err)
            {
              err = lookup_path(req_,hdr_->nodeid,name,fusepath,&e,&fi);
              f.ops.release(&req_->ctx,
                            &fi);
            }
        }

      if(err == -ENOSYS)
        {
          err = f.ops.mknod(&req_->ctx,
                            &fusepath[1],
                            arg->mode,
                            arg->rdev);
          if(!err)
            err = lookup_path(req_,hdr_->nodeid,name,fusepath,&e,NULL);
        }

      free_path(hdr_->nodeid,fusepath);
    }

  reply_entry(req_,&e,err);
}

static
void
fuse_lib_mkdir(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;
  struct fuse_entry_param e;
  struct fuse_mkdir_in *arg;

  arg  = (fuse_mkdir_in*)fuse_hdr_arg(hdr_);
  name = (const char*)PARAM(arg);
  if(req_->f->conn.proto_minor >= 12)
    req_->ctx.umask = arg->umask;

  err = get_path_name(hdr_->nodeid,name,&fusepath);
  if(!err)
    {
      err = f.ops.mkdir(&req_->ctx,
                        &fusepath[1],
                        arg->mode);
      if(!err)
        err = lookup_path(req_,hdr_->nodeid,name,fusepath,&e,NULL);
      free_path(hdr_->nodeid,fusepath);
    }

  reply_entry(req_,&e,err);
}

static
void
fuse_lib_unlink(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;
  node_t *wnode;

  name = (const char*)PARAM(hdr_);

  err = get_path_wrlock(hdr_->nodeid,name,&fusepath,&wnode);

  if(!err)
    {
      if(node_open(wnode))
        req_->ctx.nodeid = wnode->nodeid;

      err = f.ops.unlink(&req_->ctx,
                         &fusepath[1]);
      if(!err)
        remove_node(hdr_->nodeid,name);

      free_path_wrlock(hdr_->nodeid,wnode,fusepath);
    }

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_rmdir(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;
  node_t *wnode;

  name = (const char*)PARAM(hdr_);

  err = get_path_wrlock(hdr_->nodeid,name,&fusepath,&wnode);
  if(!err)
    {
      fmt::print("rmdir: {}, {}\n",
                 (bool)node_open(wnode),
                 wnode->nodeid);
      if(node_open(wnode))
        req_->ctx.nodeid = wnode->nodeid;

      err = f.ops.rmdir(&req_->ctx,
                        &fusepath[1]);
      if(!err)
        remove_node(hdr_->nodeid,name);

      free_path_wrlock(hdr_->nodeid,wnode,fusepath);
    }

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_symlink(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  int rv;
  char *fusepath;
  const char *name;
  const char *linkname;
  struct fuse_entry_param e = {};

  name     = (const char*)fuse_hdr_arg(hdr_);
  linkname = (name + strlen(name) + 1);

  rv = get_path_name(hdr_->nodeid,name,&fusepath);
  if(rv == 0)
    {
      rv = f.ops.symlink(&req_->ctx,
                         linkname,
                         &fusepath[1],
                         &e.attr,
                         &e.timeout);
      if(rv == 0)
        rv = set_path_info(hdr_->nodeid,name,&e);
      free_path(hdr_->nodeid,fusepath);
    }

  reply_entry(req_,&e,rv);
}

static
void
fuse_lib_rename(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  int err;
  char *oldpath;
  char *newpath;
  const char *oldname;
  const char *newname;
  node_t *wnode1;
  node_t *wnode2;
  struct fuse_rename_in *arg;

  arg     = (fuse_rename_in*)fuse_hdr_arg(hdr_);
  oldname = (const char*)PARAM(arg);
  newname = (oldname + strlen(oldname) + 1);

  err = get_path2(hdr_->nodeid,
                  oldname,
                  arg->newdir,
                  newname,
                  &oldpath,
                  &newpath,
                  &wnode1,
                  &wnode2);

  if(!err)
    {
      err = f.ops.rename(&req_->ctx,
                         &oldpath[1],
                         &newpath[1]);
      if(!err)
        err = rename_node(hdr_->nodeid,oldname,arg->newdir,newname);

      free_path2(hdr_->nodeid,arg->newdir,wnode1,wnode2,oldpath,newpath);
    }

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_rename2(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

static
void
fuse_lib_retrieve_reply(fuse_req_t *req_,
                        void       *cookie_,
                        uint64_t    ino_,
                        off_t       offset_)
{

}

static
void
fuse_lib_link(fuse_req_t            *req_,
              struct fuse_in_header *hdr_)
{
  int rv;
  char *oldpath;
  char *newpath;
  const char *newname;
  struct fuse_link_in *arg;
  struct fuse_entry_param e = {};

  arg     = (fuse_link_in*)fuse_hdr_arg(hdr_);
  newname = (const char*)PARAM(arg);

  rv = get_path2(arg->oldnodeid,
                 NULL,
                 hdr_->nodeid,
                 newname,
                 &oldpath,
                 &newpath,
                 NULL,
                 NULL);
  if(!rv)
    {
      rv = f.ops.link(&req_->ctx,
                      &oldpath[1],
                      &newpath[1],
                      &e.attr,
                      &e.timeout);
      if(rv == 0)
        rv = set_path_info(hdr_->nodeid,newname,&e);
      free_path2(arg->oldnodeid,hdr_->nodeid,NULL,NULL,oldpath,newpath);
    }

  reply_entry(req_,&e,rv);
}

static
void
fuse_do_release(fuse_req_ctx_t   *req_ctx_,
                uint64_t          ino_,
                fuse_file_info_t *ffi_)
{
  f.ops.release(req_ctx_,
                ffi_);

  mutex_lock(&f.lock);
  {
    node_t *node;

    node = get_node(ino_);
    assert(node->open_count > 0);
    node->open_count--;
  }
  mutex_unlock(&f.lock);
}

static
void
fuse_do_releasedir(fuse_req_ctx_t   *req_ctx_,
                   uint64_t          ino_,
                   fuse_file_info_t *ffi_)
{
  f.ops.releasedir(req_ctx_,
                   ffi_);

  mutex_lock(&f.lock);
  {
    node_t *node;

    node = get_node(ino_);
    assert(node->open_count > 0);
    node->open_count--;
  }
  mutex_unlock(&f.lock);
}

static
void
fuse_lib_create(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;
  uint64_t new_nodeid;
  fuse_file_info_t ffi = {};
  struct fuse_entry_param e;
  struct fuse_create_in *arg;

  arg     = (fuse_create_in*)fuse_hdr_arg(hdr_);
  name    = (const char*)PARAM(arg);

  ffi.flags = arg->flags;

  if(req_->f->conn.proto_minor >= 12)
    req_->ctx.umask = arg->umask;
  else
    name = ((char*)arg + sizeof(struct fuse_open_in));

  // opportunistically allocate a node so we have a new nodeid for
  // later in the actual create. Added for use with passthrough.
  {
    node_t *node = find_node(hdr_->nodeid,name);
    new_nodeid = node->nodeid;
    req_->ctx.nodeid = new_nodeid;
  }

  err = get_path_name(hdr_->nodeid,name,&fusepath);
  if(!err)
    {
      err = f.ops.create(&req_->ctx,
                         &fusepath[1],
                         arg->mode,
                         &ffi);
      if(!err)
        {
          err = lookup_path(req_,hdr_->nodeid,name,fusepath,&e,&ffi);
          if(err)
            {
              f.ops.release(&req_->ctx,&ffi);
            }
          else if(!S_ISREG(e.attr.st_mode))
            {
              err = -EIO;
              f.ops.release(&req_->ctx,&ffi);
              forget_node(e.ino,1);
            }
        }
    }

  if(!err)
    {
      mutex_lock(&f.lock);
      get_node(e.ino)->open_count++;
      mutex_unlock(&f.lock);

      if(fuse_reply_create(req_,&e,&ffi) == -ENOENT)
        {
          /* The open syscall was interrupted,so it
             must be cancelled */
          fuse_do_release(&req_->ctx,e.ino,&ffi);
          forget_node(e.ino,1);
        }
    }
  else
    {
      forget_node(new_nodeid,1);
      fuse_reply_err(req_,err);
    }

  free_path(hdr_->nodeid,fusepath);
}

static
void
open_auto_cache(fuse_req_ctx_t   *req_ctx_,
                uint64_t          ino,
                const char       *path,
                fuse_file_info_t *fi)
{
  node_t *node;
  fuse_timeouts_t timeout;

  mutex_lock(&f.lock);

  node = get_node(ino);
  if(node->is_stat_cache_valid)
    {
      int err;
      struct stat stbuf;

      mutex_unlock(&f.lock);
      err = f.ops.fgetattr(req_ctx_,
                           fi->fh,
                           &stbuf,
                           &timeout);
      mutex_lock(&f.lock);

      if(!err)
        update_stat(node,&stbuf);
      else
        node->is_stat_cache_valid = 0;
    }

  if(node->is_stat_cache_valid)
    fi->keep_cache = 1;

  node->is_stat_cache_valid = 1;

  mutex_unlock(&f.lock);
}

static
void
fuse_lib_open(fuse_req_t            *req_,
              struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  fuse_file_info_t ffi = {};
  struct fuse_open_in *arg;

  arg     = (fuse_open_in*)fuse_hdr_arg(hdr_);

  ffi.flags = arg->flags;

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      err = f.ops.open(&req_->ctx,
                       &fusepath[1],
                       &ffi);
      if(!err)
        {
          if(ffi.auto_cache && (ffi.passthrough == false))
            open_auto_cache(&req_->ctx,hdr_->nodeid,fusepath,&ffi);
        }
    }

  if(!err)
    {
      mutex_lock(&f.lock);
      get_node(hdr_->nodeid)->open_count++;
      mutex_unlock(&f.lock);

      /* The open syscall was interrupted,so it must be cancelled */
      if(fuse_reply_open(req_,&ffi) == -ENOENT)
        fuse_do_release(&req_->ctx,hdr_->nodeid,&ffi);
    }
  else
    {
      fuse_reply_err(req_,err);
    }

  free_path(hdr_->nodeid,fusepath);
}

static
void
fuse_lib_read(fuse_req_t            *req_,
              struct fuse_in_header *hdr_)
{
  int res;
  fuse_file_info_t ffi = {};
  struct fuse_read_in *arg;
  fuse_msgbuf_t *msgbuf;

  arg = (fuse_read_in*)fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;
  if(req_->f->conn.proto_minor >= 9)
    {
      ffi.flags      = arg->flags;
      ffi.lock_owner = arg->lock_owner;
    }

  msgbuf = msgbuf_alloc_page_aligned();

  res = f.ops.read(&req_->ctx,
                   &ffi,
                   msgbuf->mem,
                   arg->size,
                   arg->offset);

  if(res >= 0)
    fuse_reply_data(req_,msgbuf->mem,res);
  else
    fuse_reply_err(req_,res);

  msgbuf_free(msgbuf);
}

static
void
fuse_lib_write(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  int res;
  char *data;
  fuse_file_info_t ffi = {};
  struct fuse_write_in *arg;

  arg     = (fuse_write_in*)fuse_hdr_arg(hdr_);
  ffi.fh  = arg->fh;
  ffi.writepage = !!(arg->write_flags & 1);
  if(req_->f->conn.proto_minor < 9)
    {
      data = ((char*)arg) + FUSE_COMPAT_WRITE_IN_SIZE;
    }
  else
    {
      ffi.flags = arg->flags;
      ffi.lock_owner = arg->lock_owner;
      data = (char*)PARAM(arg);
    }

  res = f.ops.write(&req_->ctx,
                    &ffi,
                    data,
                    arg->size,
                    arg->offset);
  free_path(hdr_->nodeid,NULL);

  if(res >= 0)
    fuse_reply_write(req_,res);
  else
    fuse_reply_err(req_,res);
}

static
void
fuse_lib_fsync(fuse_req_t            *req_,
               struct fuse_in_header *hdr_)
{
  int err;
  int is_datasync;
  struct fuse_fsync_in *arg;

  arg = (fuse_fsync_in*)fuse_hdr_arg(hdr_);

  is_datasync = !!(arg->fsync_flags & FUSE_FSYNC_FDATASYNC);

  err = f.ops.fsync(&req_->ctx,
                    arg->fh,
                    is_datasync);

  fuse_reply_err(req_,err);
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
fuse_lib_opendir(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  struct fuse_dh *dh;
  fuse_file_info_t llffi = {};
  fuse_file_info_t ffi = {};
  struct fuse_open_in *arg;

  arg = (fuse_open_in*)fuse_hdr_arg(hdr_);
  llffi.flags = arg->flags;

  dh = (struct fuse_dh *)calloc(1,sizeof(struct fuse_dh));
  if(dh == NULL)
    {
      fuse_reply_err(req_,ENOMEM);
      return;
    }

  fuse_dirents_init(&dh->d);
  mutex_init(&dh->lock);

  llffi.fh = (uintptr_t)dh;
  ffi.flags = llffi.flags;

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      fmt::print("opendir: {}\n",
                 req_->ctx.nodeid);
      err = f.ops.opendir(&req_->ctx,
                          &fusepath[1],
                          &ffi);
      dh->fh = ffi.fh;
      llffi.keep_cache    = ffi.keep_cache;
      llffi.cache_readdir = ffi.cache_readdir;
    }

  if(!err)
    {
      mutex_lock(&f.lock);
      get_node(hdr_->nodeid)->open_count++;
      mutex_unlock(&f.lock);

      if(fuse_reply_open(req_,&llffi) == -ENOENT)
        {
          /* The opendir syscall was interrupted,so it
             must be cancelled */
          fuse_do_releasedir(&req_->ctx,hdr_->nodeid,&ffi);
          mutex_destroy(&dh->lock);
          free(dh);
        }
    }
  else
    {
      fuse_reply_err(req_,err);
      mutex_destroy(&dh->lock);
      free(dh);
    }

  free_path(hdr_->nodeid,fusepath);
}

static
size_t
readdir_buf_size(fuse_dirents_t *d_,
                 size_t          size_,
                 off_t           off_)
{
  if((size_t)off_ >= kv_size(d_->offs))
    return 0;
  if((kv_A(d_->offs,off_) + size_) > kv_size(d_->data))
    return (kv_size(d_->data) - kv_A(d_->offs,off_));
  return size_;
}

static
char*
readdir_buf(fuse_dirents_t *d_,
            off_t           off_)
{
  size_t i;

  i = kv_A(d_->offs,off_);

  return &kv_A(d_->data,i);
}

static
void
fuse_lib_readdir(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  int rv;
  size_t size;
  fuse_dirents_t *d;
  struct fuse_dh *dh;
  fuse_file_info_t ffi = {};
  fuse_file_info_t llffi = {};
  struct fuse_read_in *arg;

  arg      = (fuse_read_in*)fuse_hdr_arg(hdr_);
  size     = arg->size;
  llffi.fh = arg->fh;

  dh = get_dirhandle(&llffi,&ffi);
  d  = &dh->d;

  mutex_lock(&dh->lock);

  rv = 0;
  if((arg->offset == 0) || (kv_size(d->data) == 0))
    rv = f.ops.readdir(&req_->ctx,
                       &ffi,
                       d);

  if(rv)
    {
      fuse_reply_err(req_,rv);
      goto out;
    }

  size = readdir_buf_size(d,size,arg->offset);

  fuse_reply_buf(req_,
                 readdir_buf(d,arg->offset),
                 size);

 out:
  mutex_unlock(&dh->lock);
}

static
void
fuse_lib_readdir_plus(fuse_req_t            *req_,
                      struct fuse_in_header *hdr_)
{
  int rv;
  size_t size;
  fuse_dirents_t *d;
  struct fuse_dh *dh;
  fuse_file_info_t ffi = {};
  fuse_file_info_t llffi = {};
  struct fuse_read_in *arg;

  arg      = (fuse_read_in*)fuse_hdr_arg(hdr_);
  size     = arg->size;
  llffi.fh = arg->fh;

  dh = get_dirhandle(&llffi,&ffi);
  d  = &dh->d;

  mutex_lock(&dh->lock);

  rv = 0;
  if((arg->offset == 0) || (kv_size(d->data) == 0))
    rv = f.ops.readdir_plus(&req_->ctx,
                            &ffi,
                            d);

  if(rv)
    {
      fuse_reply_err(req_,rv);
      goto out;
    }

  size = readdir_buf_size(d,size,arg->offset);

  fuse_reply_buf(req_,
                 readdir_buf(d,arg->offset),
                 size);

 out:
  mutex_unlock(&dh->lock);
}

static
void
fuse_lib_releasedir(fuse_req_t            *req_,
                    struct fuse_in_header *hdr_)
{
  struct fuse_dh *dh;
  fuse_file_info_t ffi;
  fuse_file_info_t llffi = {};
  struct fuse_release_in *arg;

  arg = (fuse_release_in*)fuse_hdr_arg(hdr_);
  llffi.fh    = arg->fh;
  llffi.flags = arg->flags;

  dh = get_dirhandle(&llffi,&ffi);

  fuse_do_releasedir(&req_->ctx,hdr_->nodeid,&ffi);

  /* Done to keep race condition between last readdir reply and the unlock */
  mutex_lock(&dh->lock);
  mutex_unlock(&dh->lock);
  mutex_destroy(&dh->lock);
  fuse_dirents_free(&dh->d);
  free(dh);
  fuse_reply_err(req_,0);
}

static
void
fuse_lib_fsyncdir(fuse_req_t            *req_,
                  struct fuse_in_header *hdr_)
{
  int err;
  int is_datasync;
  fuse_file_info_t ffi;
  fuse_file_info_t llffi = {};
  struct fuse_fsync_in *arg;

  arg         = (fuse_fsync_in*)fuse_hdr_arg(hdr_);
  is_datasync = !!(arg->fsync_flags & FUSE_FSYNC_FDATASYNC);
  llffi.fh    = arg->fh;

  get_dirhandle(&llffi,&ffi);

  err = f.ops.fsyncdir(&req_->ctx,
                       &ffi,
                       is_datasync);

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_statfs(fuse_req_t            *req_,
                struct fuse_in_header *hdr_)
{
  int err = 0;
  char *fusepath;
  struct statvfs buf = {};

  fusepath = NULL;
  if(hdr_->nodeid)
    err = get_path(hdr_->nodeid,&fusepath);

  if(!err)
    {
      err = f.ops.statfs(&req_->ctx,
                         fusepath ? &fusepath[1] : "",
                         &buf);
      free_path(hdr_->nodeid,fusepath);
    }

  if(!err)
    fuse_reply_statfs(req_,&buf);
  else
    fuse_reply_err(req_,err);
}

static
void
fuse_lib_setxattr(fuse_req_t            *req_,
                  struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;
  const char *value;
  struct fuse_setxattr_in *arg;

  arg = (fuse_setxattr_in*)fuse_hdr_arg(hdr_);
  if((req_->f->conn.capable & FUSE_SETXATTR_EXT) &&
     (req_->f->conn.want & FUSE_SETXATTR_EXT))
    name = (const char*)PARAM(arg);
  else
    name = (((char*)arg) + FUSE_COMPAT_SETXATTR_IN_SIZE);

  value = (name + strlen(name) + 1);

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      err = f.ops.setxattr(&req_->ctx,
                           &fusepath[1],
                           name,
                           value,
                           arg->size,
                           arg->flags);
      free_path(hdr_->nodeid,fusepath);
    }

  fuse_reply_err(req_,err);
}

static
int
common_getxattr(fuse_req_t *req_,
                uint64_t    ino,
                const char *name,
                char       *value,
                size_t      size)
{
  int err;
  char *fusepath;

  err = get_path(ino,&fusepath);
  if(!err)
    {
      err = f.ops.getxattr(&req_->ctx,
                           &fusepath[1],
                           name,
                           value,
                           size);

      free_path(ino,fusepath);
    }

  return err;
}

static
void
fuse_lib_getxattr(fuse_req_t            *req_,
                  struct fuse_in_header *hdr_)
{
  int res;
  const char* name;
  struct fuse_getxattr_in *arg;

  arg  = (fuse_getxattr_in*)fuse_hdr_arg(hdr_);
  name = (const char*)PARAM(arg);

  if(arg->size)
    {
      char *value = (char*)malloc(arg->size);
      if(value == NULL)
        {
          fuse_reply_err(req_,ENOMEM);
          return;
        }

      res = common_getxattr(req_,hdr_->nodeid,name,value,arg->size);
      if(res > 0)
        fuse_reply_buf(req_,value,res);
      else
        fuse_reply_err(req_,res);
      free(value);
    }
  else
    {
      res = common_getxattr(req_,hdr_->nodeid,name,NULL,0);
      if(res >= 0)
        fuse_reply_xattr(req_,res);
      else
        fuse_reply_err(req_,res);
    }
}

static
int
common_listxattr(fuse_req_t *req_,
                 uint64_t    ino,
                 char       *list,
                 size_t      size)
{
  int err;
  char *fusepath;

  err = get_path(ino,&fusepath);
  if(!err)
    {
      err = f.ops.listxattr(&req_->ctx,
                            &fusepath[1],
                            list,
                            size);
      free_path(ino,fusepath);
    }

  return err;
}

static
void
fuse_lib_listxattr(fuse_req_t            *req_,
                   struct fuse_in_header *hdr_)
{
  int res;
  struct fuse_getxattr_in *arg;

  arg = (fuse_getxattr_in*)fuse_hdr_arg(hdr_);

  if(arg->size)
    {
      char *list = (char*)malloc(arg->size);
      if(list == NULL)
        {
          fuse_reply_err(req_,ENOMEM);
          return;
        }

      res = common_listxattr(req_,hdr_->nodeid,list,arg->size);
      if(res > 0)
        fuse_reply_buf(req_,list,res);
      else
        fuse_reply_err(req_,res);
      free(list);
    }
  else
    {
      res = common_listxattr(req_,hdr_->nodeid,NULL,0);
      if(res >= 0)
        fuse_reply_xattr(req_,res);
      else
        fuse_reply_err(req_,res);
    }
}

static
void
fuse_lib_removexattr(fuse_req_t                  *req_,
                     const struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;

  name = (const char*)fuse_hdr_arg(hdr_);

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      err = f.ops.removexattr(&req_->ctx,
                              &fusepath[1],
                              name);
      free_path(hdr_->nodeid,fusepath);
    }

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_copy_file_range(fuse_req_t                  *req_,
                         const struct fuse_in_header *hdr_)
{
  ssize_t rv;
  fuse_file_info_t ffi_in = {};
  fuse_file_info_t ffi_out = {};
  const struct fuse_copy_file_range_in *arg;

  arg = (fuse_copy_file_range_in*)fuse_hdr_arg(hdr_);
  ffi_in.fh  = arg->fh_in;
  ffi_out.fh = arg->fh_out;

  rv = f.ops.copy_file_range(&req_->ctx,
                             &ffi_in,
                             arg->off_in,
                             &ffi_out,
                             arg->off_out,
                             arg->len,
                             arg->flags);

  if(rv >= 0)
    fuse_reply_write(req_,rv);
  else
    fuse_reply_err(req_,rv);
}

static
void
fuse_lib_setupmapping(fuse_req_t                  *req_,
                      const struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

static
void
fuse_lib_removemapping(fuse_req_t                  *req_,
                       const struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

static
void
fuse_lib_syncfs(fuse_req_t                  *req_,
                const struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

// TODO: This is just a copy of fuse_lib_create. Needs to be rewritten
// so a nameless node can be setup.
// name is always '/'
// nodeid is the base directory
static
void
fuse_lib_tmpfile(fuse_req_t                  *req_,
                 const struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  const char *name;
  fuse_file_info_t ffi = {};
  struct fuse_entry_param e;
  struct fuse_create_in *arg;

  arg  = (fuse_create_in*)fuse_hdr_arg(hdr_);
  name = (const char*)PARAM(arg);

  ffi.flags = arg->flags;

  if(req_->f->conn.proto_minor >= 12)
    req_->ctx.umask = arg->umask;
  else
    name = (char*)arg + sizeof(struct fuse_open_in);

  err = get_path_name(hdr_->nodeid,name,&fusepath);
  if(!err)
    {
      err = f.ops.tmpfile(&req_->ctx,
                          &fusepath[1],
                          arg->mode,
                          &ffi);
      if(!err)
        {
          err = lookup_path(req_,hdr_->nodeid,name,fusepath,&e,&ffi);
          if(err)
            {
              f.ops.release(&req_->ctx,
                            &ffi);
            }
          else if(!S_ISREG(e.attr.st_mode))
            {
              err = -EIO;
              f.ops.release(&req_->ctx,
                            &ffi);
              forget_node(e.ino,1);
            }
        }
    }

  if(!err)
    {
      mutex_lock(&f.lock);
      get_node(e.ino)->open_count++;
      mutex_unlock(&f.lock);

      if(fuse_reply_create(req_,&e,&ffi) == -ENOENT)
        {
          /* The open syscall was interrupted,so it
             must be cancelled */
          fuse_do_release(&req_->ctx,e.ino,&ffi);
          forget_node(e.ino,1);
        }
    }
  else
    {
      fuse_reply_err(req_,err);
    }

  free_path(hdr_->nodeid,fusepath);
}

static
lock_t*
locks_conflict(node_t       *node,
               const lock_t *lock)
{
  lock_t *l;

  for(l = node->locks; l; l = l->next)
    if(l->owner != lock->owner &&
       lock->start <= l->end && l->start <= lock->end &&
       (l->type == F_WRLCK || lock->type == F_WRLCK))
      break;

  return l;
}

static
void
delete_lock(lock_t **lockp)
{
  lock_t *l = *lockp;
  *lockp = l->next;
  free(l);
}

static
void
insert_lock(lock_t **pos,
            lock_t  *lock)
{
  lock->next = *pos;
  *pos       = lock;
}

static
int
locks_insert(node_t *node,
             lock_t *lock)
{
  lock_t **lp;
  lock_t  *newl1 = NULL;
  lock_t  *newl2 = NULL;

  if(lock->type != F_UNLCK || lock->start != 0 || lock->end != OFFSET_MAX)
    {
      newl1 = (lock_t*)malloc(sizeof(lock_t));
      newl2 = (lock_t*)malloc(sizeof(lock_t));

      if(!newl1 || !newl2)
        {
          free(newl1);
          free(newl2);
          return -ENOLCK;
        }
    }

  for(lp = &node->locks; *lp;)
    {
      lock_t *l = *lp;
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
          goto delete_lock;
        }
      else
        {
          if(l->end < lock->start)
            goto skip;
          if(lock->end < l->start)
            break;
          if(lock->start <= l->start && l->end <= lock->end)
            goto delete_lock;
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

    delete_lock:
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
              lock_t  *lock)
{
  memset(lock,0,sizeof(lock_t));
  lock->type = flock->l_type;
  lock->start = flock->l_start;
  lock->end = flock->l_len ? flock->l_start + flock->l_len - 1 : OFFSET_MAX;
  lock->pid = flock->l_pid;
}

static
void
lock_to_flock(lock_t  *lock,
              struct flock *flock)
{
  flock->l_type = lock->type;
  flock->l_start = lock->start;
  flock->l_len = (lock->end == OFFSET_MAX) ? 0 : lock->end - lock->start + 1;
  flock->l_pid = lock->pid;
}

static
int
fuse_flush_common(fuse_req_t       *req_,
                  uint64_t          ino_,
                  fuse_file_info_t *ffi_)
{
  struct flock lock;
  lock_t l;
  int err;
  int errlock;

  memset(&lock,0,sizeof(lock));
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  err = f.ops.flush(&req_->ctx,
                    ffi_);
  errlock = f.ops.lock(&req_->ctx,
                       ffi_,
                       F_SETLK,
                       &lock);

  if(errlock != -ENOSYS)
    {
      flock_to_lock(&lock,&l);
      l.owner = ffi_->lock_owner;
      mutex_lock(&f.lock);
      locks_insert(get_node(ino_),&l);
      mutex_unlock(&f.lock);

      /* if op.lock() is defined FLUSH is needed regardless
         of op.flush() */
      if(err == -ENOSYS)
        err = 0;
    }

  return err;
}

static
void
fuse_lib_release(fuse_req_t            *req_,
                 struct fuse_in_header *hdr_)
{
  int err = 0;
  fuse_file_info_t ffi = {};
  struct fuse_release_in *arg;

  arg = (fuse_release_in*)fuse_hdr_arg(hdr_);
  ffi.fh    = arg->fh;
  ffi.flags = arg->flags;
  if(req_->f->conn.proto_minor >= 8)
    {
      ffi.flush      = !!(arg->release_flags & FUSE_RELEASE_FLUSH);
      ffi.lock_owner = arg->lock_owner;
    }
  else
    {
      ffi.flock_release = 1;
      ffi.lock_owner    = arg->lock_owner;
    }

  if(ffi.flush)
    {
      err = fuse_flush_common(req_,hdr_->nodeid,&ffi);
      if(err == -ENOSYS)
        err = 0;
    }

  fuse_do_release(&req_->ctx,
                  hdr_->nodeid,
                  &ffi);

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_flush(fuse_req_t             *req_,
               struct fuse_in_header *hdr_)
{
  int err;
  fuse_file_info_t ffi = {};
  struct fuse_flush_in *arg;

  arg = (fuse_flush_in*)fuse_hdr_arg(hdr_);

  ffi.fh = arg->fh;
  ffi.flush = 1;
  if(req_->f->conn.proto_minor >= 7)
    ffi.lock_owner = arg->lock_owner;

  err = fuse_flush_common(req_,hdr_->nodeid,&ffi);

  fuse_reply_err(req_,err);
}

static
int
fuse_lock_common(fuse_req_t       *req_,
                 uint64_t          ino_,
                 fuse_file_info_t *ffi_,
                 struct flock     *lock_,
                 int               cmd_)
{
  int err;

  err = f.ops.lock(&req_->ctx,
                   ffi_,
                   cmd_,
                   lock_);

  return err;
}

static
void
convert_fuse_file_lock(const struct fuse_file_lock *fl,
                       struct flock                *flock)
{
  memset(flock, 0, sizeof(struct flock));
  flock->l_type = fl->type;
  flock->l_whence = SEEK_SET;
  flock->l_start = fl->start;
  if (fl->end == OFFSET_MAX)
    flock->l_len = 0;
  else
    flock->l_len = fl->end - fl->start + 1;
  flock->l_pid = fl->pid;
}

static
void
fuse_lib_getlk(fuse_req_t                  *req_,
               const struct fuse_in_header *hdr_)
{
  int err;
  lock_t lk;
  struct flock flk;
  lock_t *conflict;
  fuse_file_info_t ffi = {};
  const struct fuse_lk_in *arg;

  arg            = (fuse_lk_in*)fuse_hdr_arg(hdr_);
  ffi.fh         = arg->fh;
  ffi.lock_owner = arg->owner;

  convert_fuse_file_lock(&arg->lk,&flk);

  flock_to_lock(&flk,&lk);
  lk.owner = ffi.lock_owner;
  mutex_lock(&f.lock);
  conflict = locks_conflict(get_node(hdr_->nodeid),&lk);
  if(conflict)
    lock_to_flock(conflict,&flk);
  mutex_unlock(&f.lock);
  if(!conflict)
    err = fuse_lock_common(req_,hdr_->nodeid,&ffi,&flk,F_GETLK);
  else
    err = 0;

  if(!err)
    fuse_reply_lock(req_,&flk);
  else
    fuse_reply_err(req_,err);
}

static
void
fuse_lib_setlk(fuse_req_t        *req_,
               uint64_t          ino,
               fuse_file_info_t *fi,
               struct flock     *lock,
               int               sleep)
{
  int err = fuse_lock_common(req_,ino,fi,lock,
                             sleep ? F_SETLKW : F_SETLK);
  if(!err)
    {
      lock_t l;
      flock_to_lock(lock,&l);
      l.owner = fi->lock_owner;
      mutex_lock(&f.lock);
      locks_insert(get_node(ino),&l);
      mutex_unlock(&f.lock);
    }

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_flock(fuse_req_t       *req_,
               uint64_t          ino_,
               fuse_file_info_t *ffi_,
               int               op_)
{
  int err;

  err = f.ops.flock(&req_->ctx,
                    ffi_,
                    op_);

  fuse_reply_err(req_,err);
}

static
void
fuse_lib_bmap(fuse_req_t                  *req_,
              const struct fuse_in_header *hdr_)
{
  int err;
  char *fusepath;
  uint64_t block;
  const struct fuse_bmap_in *arg;

  arg = (fuse_bmap_in*)fuse_hdr_arg(hdr_);
  block = arg->block;

  err = get_path(hdr_->nodeid,&fusepath);
  if(!err)
    {
      err = f.ops.bmap(&req_->ctx,
                       &fusepath[1],
                       arg->blocksize,
                       &block);
      free_path(hdr_->nodeid,fusepath);
    }

  if(!err)
    fuse_reply_bmap(req_,block);
  else
    fuse_reply_err(req_,err);
}

static
void
fuse_lib_ioctl(fuse_req_t                  *req_,
               const struct fuse_in_header *hdr_)
{
  int err;
  char *out_buf = NULL;
  fuse_file_info_t ffi;
  fuse_file_info_t llffi = {};
  const void *in_buf;
  uint32_t out_size;
  const struct fuse_ioctl_in *arg;

  arg = (fuse_ioctl_in*)fuse_hdr_arg(hdr_);
  if((arg->flags & FUSE_IOCTL_DIR) && !(req_->f->conn.want & FUSE_CAP_IOCTL_DIR))
    {
      fuse_reply_err(req_,ENOTTY);
      return;
    }

  if((sizeof(void*) == 4)              &&
     (req_->f->conn.proto_minor >= 16) &&
     !(arg->flags & FUSE_IOCTL_32BIT))
    {
      req_->ioctl_64bit = 1;
    }

  llffi.fh = arg->fh;
  out_size = arg->out_size;
  in_buf   = (arg->in_size ? PARAM(arg) : NULL);

  err = -EPERM;
  if(arg->flags & FUSE_IOCTL_UNRESTRICTED)
    goto err;

  if(arg->flags & FUSE_IOCTL_DIR)
    get_dirhandle(&llffi,&ffi);
  else
    ffi = llffi;

  if(out_size)
    {
      err = -ENOMEM;
      out_buf = (char*)malloc(out_size);
      if(!out_buf)
        goto err;
    }

  assert(!arg->in_size || !out_size || arg->in_size == out_size);
  if(out_buf)
    memcpy(out_buf,in_buf,arg->in_size);

  err = f.ops.ioctl(&req_->ctx,
                    &ffi,
                    arg->cmd,
                    (void*)(uintptr_t)arg->arg,
                    arg->flags,
                    out_buf ?: (void *)in_buf,
                    &out_size);
  if(err < 0)
    goto err;

  fuse_reply_ioctl(req_,err,out_buf,out_size);
  goto out;
 err:
  fuse_reply_err(req_,err);
 out:
  free(out_buf);
}

static
void
fuse_lib_poll(fuse_req_t                  *req_,
              const struct fuse_in_header *hdr_)
{
  int err;
  unsigned revents = 0;
  fuse_file_info_t ffi = {};
  fuse_pollhandle_t *ph = NULL;
  const struct fuse_poll_in *arg;

  arg = (fuse_poll_in*)fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;

  if(arg->flags & FUSE_POLL_SCHEDULE_NOTIFY)
    {
      ph = (fuse_pollhandle_t*)malloc(sizeof(fuse_pollhandle_t));
      if(ph == NULL)
        {
          fuse_reply_err(req_,ENOMEM);
          return;
        }

      ph->kh = arg->kh;
      ph->ch = req_->ch;
      ph->f  = req_->f;
    }

  err = f.ops.poll(&req_->ctx,
                   &ffi,
                   ph,
                   &revents);

  if(!err)
    fuse_reply_poll(req_,revents);
  else
    fuse_reply_err(req_,err);
}

static
void
fuse_lib_fallocate(fuse_req_t                  *req_,
                   const struct fuse_in_header *hdr_)
{
  int err;
  const struct fuse_fallocate_in *arg;

  arg = (fuse_fallocate_in*)fuse_hdr_arg(hdr_);

  err = f.ops.fallocate(&req_->ctx,
                        arg->fh,
                        arg->mode,
                        arg->offset,
                        arg->length);

  fuse_reply_err(req_,err);
}

static
int
remembered_node_cmp(const void *a_,
                    const void *b_)
{
  const remembered_node_t *a = (const remembered_node_t*)a_;
  const remembered_node_t *b = (const remembered_node_t*)b_;

  return (a->time - b->time);
}

static
void
remembered_nodes_sort()
{
  mutex_lock(&f.lock);
  qsort(&kv_first(f.remembered_nodes),
        kv_size(f.remembered_nodes),
        sizeof(remembered_node_t),
        remembered_node_cmp);
  mutex_unlock(&f.lock);
}

#define MAX_PRUNE 100
#define MAX_CHECK 1000

int
fuse_prune_some_remembered_nodes(int *offset_)
{
  time_t now;
  int pruned;
  int checked;

  mutex_lock(&f.lock);

  pruned = 0;
  checked = 0;
  now = current_time();
  while(*offset_ < (int)kv_size(f.remembered_nodes))
    {
      time_t age;
      remembered_node_t *fn = &kv_A(f.remembered_nodes,*offset_);

      if(pruned >= MAX_PRUNE)
        break;
      if(checked >= MAX_CHECK)
        break;

      checked++;
      age = (now - fn->time);
      if(fuse_cfg.remember_nodes > age)
        break;

      assert(fn->node->nlookup == 1);

      /* Don't forget active directories */
      if(fn->node->refctr > 1)
        {
          (*offset_)++;
          continue;
        }

      fn->node->nlookup = 0;
      unref_node(fn->node);
      kv_delete(f.remembered_nodes,*offset_);
      pruned++;
    }

  mutex_unlock(&f.lock);

  if((pruned < MAX_PRUNE) && (checked < MAX_CHECK))
    *offset_ = -1;

  return pruned;
}

#undef MAX_PRUNE
#undef MAX_CHECK

static
void
sleep_100ms(void)
{
  const struct timespec ms100 = {0,100 * 1000000};

  nanosleep(&ms100,NULL);
}

void
fuse_prune_remembered_nodes()
{
  int offset;
  int pruned;

  offset = 0;
  pruned = 0;
  for(;;)
    {
      pruned += fuse_prune_some_remembered_nodes(&offset);
      if(offset >= 0)
        {
          sleep_100ms();
          continue;
        }

      break;
    }

  if(pruned > 0)
    remembered_nodes_sort();
}

static struct fuse_lowlevel_ops fuse_path_ops =
  {
    .access          = fuse_lib_access,
    .bmap            = fuse_lib_bmap,
    .copy_file_range = fuse_lib_copy_file_range,
    .create          = fuse_lib_create,
    .destroy         = fuse_lib_destroy,
    .fallocate       = fuse_lib_fallocate,
    .flock           = fuse_lib_flock,
    .flush           = fuse_lib_flush,
    .forget          = fuse_lib_forget,
    .forget_multi    = fuse_lib_forget_multi,
    .fsync           = fuse_lib_fsync,
    .fsyncdir        = fuse_lib_fsyncdir,
    .getattr         = fuse_lib_getattr,
    .getlk           = fuse_lib_getlk,
    .getxattr        = fuse_lib_getxattr,
    .init            = fuse_lib_init,
    .ioctl           = fuse_lib_ioctl,
    .link            = fuse_lib_link,
    .listxattr       = fuse_lib_listxattr,
    .lookup          = fuse_lib_lookup,
    .lseek           = fuse_lib_lseek,
    .mkdir           = fuse_lib_mkdir,
    .mknod           = fuse_lib_mknod,
    .open            = fuse_lib_open,
    .opendir         = fuse_lib_opendir,
    .poll            = fuse_lib_poll,
    .read            = fuse_lib_read,
    .readdir         = fuse_lib_readdir,
    .readdir_plus    = fuse_lib_readdir_plus,
    .readlink        = fuse_lib_readlink,
    .release         = fuse_lib_release,
    .releasedir      = fuse_lib_releasedir,
    .removemapping   = fuse_lib_removemapping,
    .removexattr     = fuse_lib_removexattr,
    .rename          = fuse_lib_rename,
    .rename2         = fuse_lib_rename2,
    .retrieve_reply  = fuse_lib_retrieve_reply,
    .rmdir           = fuse_lib_rmdir,
    .setattr         = fuse_lib_setattr,
    .setlk           = fuse_lib_setlk,
    .setupmapping    = fuse_lib_setupmapping,
    .setxattr        = fuse_lib_setxattr,
    .statfs          = fuse_lib_statfs,
    .statx           = fuse_lib_statx,
    .symlink         = fuse_lib_symlink,
    .syncfs          = fuse_lib_syncfs,
    .tmpfile         = fuse_lib_tmpfile,
    .unlink          = fuse_lib_unlink,
    .write           = fuse_lib_write,
  };

int
fuse_notify_poll(fuse_pollhandle_t *ph)
{
  return fuse_lowlevel_notify_poll(ph);
}

int
fuse_exited()
{
  return fuse_session_exited(f.se);
}

struct fuse_session*
fuse_get_session()
{
  return f.se;
}

void
fuse_exit()
{
  f.se->exited = 1;
}

enum
  {
    KEY_HELP,
  };

#define FUSE_LIB_OPT(t,p,v) { t,offsetof(struct fuse_config,p),v }

static const struct fuse_opt fuse_lib_opts[] =
  {
    FUSE_OPT_END
  };

static
int
fuse_lib_opt_proc(void             *data,
                  const char       *arg,
                  int               key,
                  struct fuse_args *outargs)
{
  return 1;
}

static
int
node_table_init(struct node_table *t)
{
  t->size = NODE_TABLE_MIN_SIZE;
  t->array = (node_t **)calloc(1,sizeof(node_t *) * t->size);
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
void
metrics_log_nodes_info(FILE *file_)
{
  char buf[1024];
  char time_str[64];
  struct tm tm;
  struct timeval tv;
  uint64_t sizeof_node;
  float node_usage_ratio;
  uint64_t node_slab_count;
  uint64_t node_avail_objs;
  uint64_t node_total_alloc_mem;

  gettimeofday(&tv,NULL);
  localtime_r(&tv.tv_sec,&tm);
  strftime(time_str,sizeof(time_str),"%Y-%m-%dT%H:%M:%S.000%z",&tm);

  sizeof_node = sizeof(node_t);

  lfmp_t *lfmp;
  lfmp = node_lfmp();
  lfmp_lock(lfmp);
  node_slab_count = fmp_slab_count(&lfmp->fmp);
  node_usage_ratio = fmp_slab_usage_ratio(&lfmp->fmp);
  node_avail_objs = fmp_avail_objs(&lfmp->fmp);
  node_total_alloc_mem = fmp_total_allocated_memory(&lfmp->fmp);
  lfmp_unlock(lfmp);

  snprintf(buf,sizeof(buf),
           "time: %s\n"
           "sizeof(node): %" PRIu64 "\n"
           "node id_table size: %" PRIu64 "\n"
           "node id_table usage: %" PRIu64 "\n"
           "node id_table total allocated memory: %" PRIu64 "\n"
           "node name_table size: %" PRIu64 "\n"
           "node name_table usage: %" PRIu64 "\n"
           "node name_table total allocated memory: %" PRIu64 "\n"
           "node memory pool slab count: %" PRIu64 "\n"
           "node memory pool usage ratio: %f\n"
           "node memory pool avail objs: %" PRIu64 "\n"
           "node memory pool total allocated memory: %" PRIu64 "\n"
           "msgbuf bufsize: %" PRIu64 "\n"
           "msgbuf allocation count: %" PRIu64 "\n"
           "msgbuf available count: %" PRIu64 "\n"
           "msgbuf total allocated memory: %" PRIu64 "\n"
           "\n"
           ,
           time_str,
           sizeof_node,
           (uint64_t)f.id_table.size,
           (uint64_t)f.id_table.use,
           (uint64_t)(f.id_table.size * sizeof(node_t*)),
           (uint64_t)f.name_table.size,
           (uint64_t)f.name_table.use,
           (uint64_t)(f.name_table.size * sizeof(node_t*)),
           node_slab_count,
           node_usage_ratio,
           node_avail_objs,
           node_total_alloc_mem,
           msgbuf_get_bufsize(),
           msgbuf_alloc_count(),
           msgbuf_avail_count(),
           msgbuf_alloc_count() * msgbuf_get_bufsize()
           );

  fputs(buf,file_);
}

static
void
metrics_log_nodes_info_to_tmp_dir(struct fuse *f_)
{
  int rv;
  FILE *file;
  char filepath[256];
  struct stat st;
  char const *mode = "a";
  off_t const max_size = (1024 * 1024);

  sprintf(filepath,"/tmp/mergerfs.%d.info",getpid());

  rv = lstat(filepath,&st);
  if((rv == 0) && (st.st_size > max_size))
    mode = "w";

  file = fopen(filepath,mode);
  if(file == NULL)
    return;

  metrics_log_nodes_info(file);

  fclose(file);
}

static
void
fuse_malloc_trim(void)
{
#ifdef __GLIBC__
  malloc_trim(1024 * 1024);
#endif
}

void
fuse_invalidate_all_nodes()
{
  std::vector<std::string> names;

  mutex_lock(&f.lock);
  for(size_t i = 0; i < f.id_table.size; i++)
    {
      node_t *node;

      for(node = f.id_table.array[i]; node != NULL; node = node->id_next)
        {
          if(node->nodeid == FUSE_ROOT_ID)
            continue;
          if(node->name == NULL)
            continue;
          if(node->parent == NULL)
            continue;
          if(node->parent->nodeid != FUSE_ROOT_ID)
            continue;

          names.emplace_back(node->name);
        }
    }
  mutex_unlock(&f.lock);

  SysLog::info("invalidating {} file entries",
               names.size());
  for(auto &name : names)
    {
      fuse_lowlevel_notify_inval_entry(f.se->ch,
                                       FUSE_ROOT_ID,
                                       name.c_str(),
                                       name.size());
    }
}

void
fuse_gc()
{
  SysLog::info("running thorough garbage collection");
  node_gc();
  msgbuf_gc();
  fuse_malloc_trim();
}

void
fuse_gc1()
{
  SysLog::info("running basic garbage collection");
  node_gc1();
  msgbuf_gc_10percent();
  fuse_malloc_trim();
}

void
fuse_populate_maintenance_thread(struct fuse *f_)
{
  MaintenanceThread::push_job([=](int count_)
  {
    if(remember_nodes())
      fuse_prune_remembered_nodes();
  });

  MaintenanceThread::push_job([](int count_)
  {
    if((count_ % 15) == 0)
      fuse_gc1();
  });

  MaintenanceThread::push_job([=](int count_)
  {
    if(g_LOG_METRICS)
      metrics_log_nodes_info_to_tmp_dir(f_);
  });
}

struct fuse*
fuse_new(struct fuse_chan             *ch,
         struct fuse_args             *args,
         const struct fuse_operations *ops_)
{
  node_t *root;
  struct fuse_lowlevel_ops llop = fuse_path_ops;

  f.ops = *ops_;

  /* Oh f**k,this is ugly! */
  if(!f.ops.lock)
    {
      llop.getlk = NULL;
      llop.setlk = NULL;
    }

  if(fuse_opt_parse(args,&f.conf,fuse_lib_opts,fuse_lib_opt_proc) == -1)
    goto out_free_fs;

  g_LOG_METRICS = fuse_cfg.debug;

  f.se = fuse_lowlevel_new_common(args,&llop,sizeof(llop),&f);
  if(f.se == NULL)
    goto out_free_fs;

  fuse_session_add_chan(f.se,ch);

  /* Trace topmost layer by default */
  srand(time(NULL));
  f.nodeid_gen.nodeid = FUSE_ROOT_ID;
  f.nodeid_gen.generation = rand64();
  if(node_table_init(&f.name_table) == -1)
    goto out_free_session;

  if(node_table_init(&f.id_table) == -1)
    goto out_free_name_table;

  mutex_init(&f.lock);

  kv_init(f.remembered_nodes);

  root = node_alloc();
  if(root == NULL)
    {
      fprintf(stderr,"fuse: memory allocation failed\n");
      goto out_free_id_table;
    }

  root->name = strdup("/");

  root->parent = NULL;
  root->nodeid = FUSE_ROOT_ID;
  inc_nlookup(root);
  hash_id(root);

  return &f;

 out_free_id_table:
  free(f.id_table.array);
 out_free_name_table:
  free(f.name_table.array);
 out_free_session:
  fuse_session_destroy(f.se);
 out_free_fs:
  /* Horrible compatibility hack to stop the destructor from being
     called on the filesystem without init being called first */
  f.ops.destroy = NULL;

  return NULL;
}

void
fuse_destroy(struct fuse *)
{
  size_t i;

  for(i = 0; i < f.id_table.size; i++)
    {
      node_t *node;
      node_t *next;

      for(node = f.id_table.array[i]; node != NULL; node = next)
        {
          next = node->id_next;
          free_node(node);
          f.id_table.use--;
        }
    }

  free(f.id_table.array);
  free(f.name_table.array);
  mutex_destroy(&f.lock);
  fuse_session_destroy(f.se);
  kv_destroy(f.remembered_nodes);
}

void
fuse_log_metrics_set(int log_)
{
  g_LOG_METRICS = log_;
}

int
fuse_log_metrics_get(void)
{
  return g_LOG_METRICS;
}

int
fuse_passthrough_open(const int fd_)
{
  int rv;
  int dev_fuse_fd;
  struct fuse_backing_map bm = {};

  dev_fuse_fd = fuse_chan_fd(f.se->ch);
  bm.fd       = fd_;

  rv = ::ioctl(dev_fuse_fd,FUSE_DEV_IOC_BACKING_OPEN,&bm);

  return rv;
}

int
fuse_passthrough_close(const int backing_id_)
{
  int dev_fuse_fd;

  dev_fuse_fd = fuse_chan_fd(f.se->ch);

  return ::ioctl(dev_fuse_fd,FUSE_DEV_IOC_BACKING_CLOSE,&backing_id_);
}

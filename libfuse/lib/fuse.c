/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

/* For pthread_rwlock_t */
#define _GNU_SOURCE

#include "crc32b.h"
#include "fuse_node.h"
#include "khash.h"
#include "kvec.h"

#include "node.h"
#include "config.h"
#include "fuse_dirents.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"
#include "fuse_opt.h"
#include "fuse_pollhandle.h"
#include "fuse_msgbuf.hpp"

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_MALLOC_TRIM
#include <malloc.h>
#endif

#define FUSE_UNKNOWN_INO UINT64_MAX
#define OFFSET_MAX 0x7fffffffffffffffLL

#define NODE_TABLE_MIN_SIZE 8192

#define PARAM(inarg) ((void*)(((char*)(inarg)) + sizeof(*(inarg))))

static int g_LOG_METRICS = 0;

struct fuse_config
{
  unsigned int uid;
  unsigned int gid;
  unsigned int umask;
  int remember;
  int debug;
  int nogc;
  int set_mode;
  int set_uid;
  int set_gid;
  int help;
};

struct fuse_fs
{
  struct fuse_operations op;
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
  struct fuse_fs *fs;
  struct lock_queue_element *lockq;

  pthread_t maintenance_thread;
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

struct fuse_context_i
{
  struct fuse_context ctx;
  fuse_req_t req;
};

static pthread_key_t fuse_context_key;
static pthread_mutex_t fuse_context_lock = PTHREAD_MUTEX_INITIALIZER;
static int fuse_context_ref;

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
char*
filename_strdup(struct fuse *f_,
                const char  *fn_)
{
  return strdup(fn_);
}

static
void
filename_free(struct fuse *f_,
              char        *fn_)
{
  free(fn_);
}

static
void*
fuse_hdr_arg(const struct fuse_in_header *hdr_)
{
  return (void*)&hdr_[1];
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
size_t
id_hash(struct fuse *f,
        uint64_t     ino)
{
  uint64_t hash = ((uint32_t)ino * 2654435761U) % f->id_table.size;
  uint64_t oldhash = hash % (f->id_table.size / 2);

  if(oldhash >= f->id_table.split)
    return oldhash;
  else
    return hash;
}

static
node_t*
get_node_nocheck(struct fuse *f,
                 uint64_t     nodeid)
{
  size_t hash = id_hash(f,nodeid);
  node_t *node;

  for(node = f->id_table.array[hash]; node != NULL; node = node->id_next)
    if(node->nodeid == nodeid)
      return node;

  return NULL;
}

static
node_t*
get_node(struct fuse      *f,
         const uint64_t    nodeid)
{
  node_t *node = get_node_nocheck(f,nodeid);

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
remove_remembered_node(struct fuse *f_,
                       node_t *node_)
{
  for(size_t i = 0; i < kv_size(f_->remembered_nodes); i++)
    {
      if(kv_A(f_->remembered_nodes,i).node != node_)
        continue;

      kv_delete(f_->remembered_nodes,i);
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
free_node(struct fuse *f_,
          node_t      *node_)
{
  filename_free(f_,node_->name);

  if(node_->hidden_fh)
    f_->fs->op.free_hide(node_->hidden_fh);

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
unhash_id(struct fuse *f,
          node_t      *node)
{
  node_t **nodep = &f->id_table.array[id_hash(f,node->nodeid)];

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

  newarray = realloc(t->array,sizeof(node_t*) * newsize);
  if(newarray == NULL)
    return -1;

  t->array = newarray;
  memset(t->array + t->size,0,t->size * sizeof(node_t*));
  t->size = newsize;
  t->split = 0;

  return 0;
}

static
void
rehash_id(struct fuse *f)
{
  struct node_table *t = &f->id_table;
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
        node_t      *node)
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
          uint64_t     parent,
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
           node_t      *node);

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
unhash_name(struct fuse *f,
            node_t      *node)
{
  if(node->name)
    {
      size_t hash = name_hash(f,node->parent->nodeid,node->name);
      node_t **nodep = &f->name_table.array[hash];

      for(; *nodep != NULL; nodep = &(*nodep)->name_next)
        if(*nodep == node)
          {
            *nodep = node->name_next;
            node->name_next = NULL;
            unref_node(f,node->parent);
            filename_free(f,node->name);
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
          node_t      *node,
          uint64_t     parentid,
          const char  *name)
{
  size_t hash = name_hash(f,parentid,name);
  node_t *parent = get_node(f,parentid);
  node->name = filename_strdup(f,name);
  if(node->name == NULL)
    return -1;

  parent->refctr++;
  node->parent = parent;
  node->name_next = f->name_table.array[hash];
  f->name_table.array[hash] = node;
  f->name_table.use++;

  if(f->name_table.use >= f->name_table.size / 2)
    rehash_name(f);

  return 0;
}

static
inline
int
remember_nodes(struct fuse *f_)
{
  return (f_->conf.remember > 0);
}

static
void
delete_node(struct fuse *f,
            node_t      *node)
{
  assert(node->treelock == 0);
  unhash_name(f,node);
  if(remember_nodes(f))
    remove_remembered_node(f,node);
  unhash_id(f,node);
  node_free(node);
}

static
void
unref_node(struct fuse *f,
           node_t *node)
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
node_t*
lookup_node(struct fuse *f,
            uint64_t     parent,
            const char  *name)
{
  size_t hash;
  node_t *node;

  hash =  name_hash(f,parent,name);
  for(node = f->name_table.array[hash]; node != NULL; node = node->name_next)
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
find_node(struct fuse *f,
          uint64_t     parent,
          const char  *name)
{
  node_t *node;

  pthread_mutex_lock(&f->lock);
  if(!name)
    node = get_node(f,parent);
  else
    node = lookup_node(f,parent,name);

  if(node == NULL)
    {
      node = node_alloc();
      if(node == NULL)
        goto out_err;

      node->nodeid = generate_nodeid(&f->nodeid_gen);
      if(f->conf.remember)
        inc_nlookup(node);

      if(hash_name(f,node,parent,name) == -1)
        {
          free_node(f,node);
          node = NULL;
          goto out_err;
        }
      hash_id(f,node);
    }
  else if((node->nlookup == 1) && remember_nodes(f))
    {
      remove_remembered_node(f,node);
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
            uint64_t     nodeid,
            node_t *wnode,
            node_t *end)
{
  node_t *node;

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
             uint64_t      nodeid,
             const char   *name,
             char        **path,
             node_t **wnodep,
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
    unlock_path(f,nodeid,wnode,node);
 out_free:
  free(buf);

 out_err:
  return err;
}

static
int
try_get_path2(struct fuse  *f,
              uint64_t      nodeid1,
              const char   *name1,
              uint64_t      nodeid2,
              const char   *name2,
              char        **path1,
              char        **path2,
              node_t **wnode1,
              node_t **wnode2)
{
  int err;

  err = try_get_path(f,nodeid1,name1,path1,wnode1,true);
  if(!err)
    {
      err = try_get_path(f,nodeid2,name2,path2,wnode2,true);
      if(err)
        {
          node_t *wn1 = wnode1 ? *wnode1 : NULL;

          unlock_path(f,nodeid1,wn1,NULL);
          free(*path1);
        }
    }

  return err;
}

static
void
queue_element_wakeup(struct fuse               *f,
                     struct lock_queue_element *qe)
{
  int err;

  if(!qe->path1)
    {
      /* Just waiting for it to be unlocked */
      if(get_node(f,qe->nodeid1)->treelock == 0)
        pthread_cond_signal(&qe->cond);

      return;
    }

  if(qe->done)
    return;

  if(!qe->path2)
    {
      err = try_get_path(f,
                         qe->nodeid1,
                         qe->name1,
                         qe->path1,
                         qe->wnode1,
                         true);
    }
  else
    {
      err = try_get_path2(f,
                          qe->nodeid1,
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
wake_up_queued(struct fuse *f)
{
  struct lock_queue_element *qe;

  for(qe = f->lockq; qe != NULL; qe = qe->next)
    queue_element_wakeup(f,qe);
}

static
void
queue_path(struct fuse               *f,
           struct lock_queue_element *qe)
{
  struct lock_queue_element **qp;

  qe->done = false;
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
                uint64_t      nodeid,
                const char   *name,
                char        **path,
                node_t **wnode)
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

      err = wait_path(f,&qe);
    }
  pthread_mutex_unlock(&f->lock);

  return err;
}

static
int
get_path(struct fuse  *f,
         uint64_t      nodeid,
         char        **path)
{
  return get_path_common(f,nodeid,NULL,path,NULL);
}

static
int
get_path_name(struct fuse  *f,
              uint64_t      nodeid,
              const char   *name,
              char        **path)
{
  return get_path_common(f,nodeid,name,path,NULL);
}

static
int
get_path_wrlock(struct fuse  *f,
                uint64_t      nodeid,
                const char   *name,
                char        **path,
                node_t **wnode)
{
  return get_path_common(f,nodeid,name,path,wnode);
}

static
int
get_path2(struct fuse  *f,
          uint64_t      nodeid1,
          const char   *name1,
          uint64_t      nodeid2,
          const char   *name2,
          char        **path1,
          char        **path2,
          node_t **wnode1,
          node_t **wnode2)
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

      err = wait_path(f,&qe);
    }
  pthread_mutex_unlock(&f->lock);

  return err;
}

static
void
free_path_wrlock(struct fuse *f,
                 uint64_t     nodeid,
                 node_t *wnode,
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
          uint64_t     nodeid,
          char        *path)
{
  if(path)
    free_path_wrlock(f,nodeid,NULL,path);
}

static
void
free_path2(struct fuse *f,
           uint64_t     nodeid1,
           uint64_t     nodeid2,
           node_t *wnode1,
           node_t *wnode2,
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
            const uint64_t    nodeid,
            const uint64_t    nlookup)
{
  node_t *node;

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

      queue_path(f,&qe);

      do
        {
          pthread_cond_wait(&qe.cond,&f->lock);
        }
      while((node->nlookup == nlookup) && node->treelock);

      dequeue_path(f,&qe);
    }

  assert(node->nlookup >= nlookup);
  node->nlookup -= nlookup;

  if(node->nlookup == 0)
    {
      unref_node(f,node);
    }
  else if((node->nlookup == 1) && remember_nodes(f))
    {
      remembered_node_t fn;

      fn.node = node;
      fn.time = current_time();

      kv_push(remembered_node_t,f->remembered_nodes,fn);
    }

  pthread_mutex_unlock(&f->lock);
}

static
void
unlink_node(struct fuse *f,
            node_t *node)
{
  if(remember_nodes(f))
    {
      assert(node->nlookup > 1);
      node->nlookup--;
    }
  unhash_name(f,node);
}

static
void
remove_node(struct fuse *f,
            uint64_t     dir,
            const char  *name)
{
  node_t *node;

  pthread_mutex_lock(&f->lock);
  node = lookup_node(f,dir,name);
  if(node != NULL)
    unlink_node(f,node);
  pthread_mutex_unlock(&f->lock);
}

static
int
rename_node(struct fuse *f,
            uint64_t     olddir,
            const char  *oldname,
            uint64_t     newdir,
            const char  *newname)
{
  node_t *node;
  node_t *newnode;
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
         uint64_t     nodeid,
         struct stat *stbuf)
{
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
set_path_info(struct fuse             *f,
              uint64_t                 nodeid,
              const char              *name,
              struct fuse_entry_param *e)
{
  node_t *node;

  node = find_node(f,nodeid,name);
  if(node == NULL)
    return -ENOMEM;

  e->ino        = node->nodeid;
  e->generation = ((e->ino == FUSE_ROOT_ID) ? 0 : f->nodeid_gen.generation);

  pthread_mutex_lock(&f->lock);
  update_stat(node,&e->attr);
  pthread_mutex_unlock(&f->lock);

  set_stat(f,e->ino,&e->attr);

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
lookup_path(struct fuse             *f,
            uint64_t                 nodeid,
            const char              *name,
            const char              *path,
            struct fuse_entry_param *e,
            fuse_file_info_t        *fi)
{
  int rv;

  memset(e,0,sizeof(struct fuse_entry_param));

  rv = ((fi == NULL) ?
        f->fs->op.getattr(path,&e->attr,&e->timeout) :
        f->fs->op.fgetattr(fi,&e->attr,&e->timeout));

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
      fuse_reply_err(req,err);
    }
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
  f->fs->op.init(conn);
}

static
void
fuse_lib_destroy(void *data)
{
  struct fuse *f = (struct fuse *)data;
  struct fuse_context_i *c = fuse_get_context_internal();

  memset(c,0,sizeof(*c));
  c->ctx.fuse = f;
  f->fs->op.destroy();
  free(f->fs);
  f->fs = NULL;
}

static
void
fuse_lib_lookup(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  int err;
  uint64_t nodeid;
  char *path;
  const char *name;
  struct fuse *f;
  node_t *dot = NULL;
  struct fuse_entry_param e = {0};

  name   = fuse_hdr_arg(hdr_);
  nodeid = hdr_->nodeid;
  f      = req_fuse_prepare(req);

  if(name[0] == '.')
    {
      if(name[1] == '\0')
        {
          name = NULL;
          pthread_mutex_lock(&f->lock);
          dot = get_node_nocheck(f,nodeid);
          if(dot == NULL)
            {
              pthread_mutex_unlock(&f->lock);
              reply_entry(req,&e,-ESTALE);
              return;
            }
          dot->refctr++;
          pthread_mutex_unlock(&f->lock);
        }
      else if((name[1] == '.') && (name[2] == '\0'))
        {
          if(nodeid == 1)
            {
              reply_entry(req,&e,-ENOENT);
              return;
            }

          name = NULL;
          pthread_mutex_lock(&f->lock);
          nodeid = get_node(f,nodeid)->parent->nodeid;
          pthread_mutex_unlock(&f->lock);
        }
    }

  err = get_path_name(f,nodeid,name,&path);
  if(!err)
    {
      err = lookup_path(f,nodeid,name,path,&e,NULL);
      if(err == -ENOENT)
        {
          e.ino = 0;
          err = 0;
        }
      free_path(f,nodeid,path);
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
fuse_lib_forget(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  struct fuse *f;
  struct fuse_forget_in *arg;

  f   = req_fuse(req);
  arg = fuse_hdr_arg(hdr_);

  forget_node(f,hdr_->nodeid,arg->nlookup);

  fuse_reply_none(req);
}

static
void
fuse_lib_forget_multi(fuse_req_t             req,
                      struct fuse_in_header *hdr_)
{
  struct fuse *f;
  struct fuse_batch_forget_in *arg;
  struct fuse_forget_one      *entry;

  f     = req_fuse(req);
  arg   = fuse_hdr_arg(hdr_);
  entry = PARAM(arg);

  for(uint32_t i = 0; i < arg->count; i++)
    {
      forget_node(f,
                entry[i].nodeid,
                entry[i].nlookup);
    }

  fuse_reply_none(req);
}


static
void
fuse_lib_getattr(fuse_req_t             req,
                 struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  struct stat buf;
  node_t *node;
  fuse_timeouts_t timeout;
  fuse_file_info_t ffi = {0};
  const struct fuse_getattr_in *arg;

  arg = fuse_hdr_arg(hdr_);
  f   = req_fuse_prepare(req);

  if(arg->getattr_flags & FUSE_GETATTR_FH)
    {
      ffi.fh = arg->fh;
    }
  else
    {
      pthread_mutex_lock(&f->lock);
      node = get_node(f,hdr_->nodeid);
      if(node->hidden_fh)
        ffi.fh = node->hidden_fh;
      pthread_mutex_unlock(&f->lock);
    }

  memset(&buf,0,sizeof(buf));

  err = 0;
  path = NULL;
  if(ffi.fh == 0)
    err = get_path(f,hdr_->nodeid,&path);

  if(!err)
    {
      err = ((ffi.fh == 0) ?
             f->fs->op.getattr(path,&buf,&timeout) :
             f->fs->op.fgetattr(&ffi,&buf,&timeout));

      free_path(f,hdr_->nodeid,path);
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      node = get_node(f,hdr_->nodeid);
      update_stat(node,&buf);
      pthread_mutex_unlock(&f->lock);
      set_stat(f,hdr_->nodeid,&buf);
      fuse_reply_attr(req,&buf,timeout.attr);
    }
  else
    {
      fuse_reply_err(req,err);
    }
}

static
void
fuse_lib_setattr(fuse_req_t             req,
                 struct fuse_in_header *hdr_)
{
  struct fuse *f = req_fuse_prepare(req);
  struct stat stbuf = {0};
  char *path;
  int err;
  node_t *node;
  fuse_timeouts_t timeout;
  fuse_file_info_t *fi;
  fuse_file_info_t ffi = {0};
  struct fuse_setattr_in *arg;

  arg = fuse_hdr_arg(hdr_);

  fi = NULL;
  if(arg->valid & FATTR_FH)
    {
      fi = &ffi;
      fi->fh = arg->fh;
    }
  else
    {
      pthread_mutex_lock(&f->lock);
      node = get_node(f,hdr_->nodeid);
      if(node->hidden_fh)
        {
          fi = &ffi;
          fi->fh = node->hidden_fh;
        }
      pthread_mutex_unlock(&f->lock);
    }

  err = 0;
  path = NULL;
  if(fi == NULL)
    err = get_path(f,hdr_->nodeid,&path);

  if(!err)
    {
      err = 0;
      if(!err && (arg->valid & FATTR_MODE))
        err = ((fi == NULL) ?
               f->fs->op.chmod(path,arg->mode) :
               f->fs->op.fchmod(fi,arg->mode));

      if(!err && (arg->valid & (FATTR_UID | FATTR_GID)))
        {
          uid_t uid = ((arg->valid & FATTR_UID) ? arg->uid : (uid_t)-1);
          gid_t gid = ((arg->valid & FATTR_GID) ? arg->gid : (gid_t)-1);

          err = ((fi == NULL) ?
                 f->fs->op.chown(path,uid,gid) :
                 f->fs->op.fchown(fi,uid,gid));
        }

      if(!err && (arg->valid & FATTR_SIZE))
        err = ((fi == NULL) ?
               f->fs->op.truncate(path,arg->size) :
               f->fs->op.ftruncate(fi,arg->size));

#ifdef HAVE_UTIMENSAT
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
            tv[0] = (struct timespec){ arg->atime, arg->atimensec };

          if(arg->valid & FATTR_MTIME_NOW)
            tv[1].tv_nsec = UTIME_NOW;
          else if(arg->valid & FATTR_MTIME)
            tv[1] = (struct timespec){ arg->mtime, arg->mtimensec };

          err = ((fi == NULL) ?
                 f->fs->op.utimens(path,tv) :
                 f->fs->op.futimens(fi,tv));
        }
      else
#endif
        if(!err && ((arg->valid & (FATTR_ATIME|FATTR_MTIME)) == (FATTR_ATIME|FATTR_MTIME)))
          {
            struct timespec tv[2];
            tv[0].tv_sec  = arg->atime;
            tv[0].tv_nsec = arg->atimensec;
            tv[1].tv_sec  = arg->mtime;
            tv[1].tv_nsec = arg->mtimensec;
            err = ((fi == NULL) ?
                   f->fs->op.utimens(path,tv) :
                   f->fs->op.futimens(fi,tv));
          }

      if(!err)
        err = ((fi == NULL) ?
               f->fs->op.getattr(path,&stbuf,&timeout) :
               f->fs->op.fgetattr(fi,&stbuf,&timeout));

      free_path(f,hdr_->nodeid,path);
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      update_stat(get_node(f,hdr_->nodeid),&stbuf);
      pthread_mutex_unlock(&f->lock);
      set_stat(f,hdr_->nodeid,&stbuf);
      fuse_reply_attr(req,&stbuf,timeout.attr);
    }
  else
    {
      fuse_reply_err(req,err);
    }
}

static
void
fuse_lib_access(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  struct fuse_access_in *arg;

  arg = fuse_hdr_arg(hdr_);

  f = req_fuse_prepare(req);

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.access(path,arg->mask);
      free_path(f,hdr_->nodeid,path);
    }

  fuse_reply_err(req,err);
}

static
void
fuse_lib_readlink(fuse_req_t             req,
                  struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  char linkname[PATH_MAX + 1];

  f = req_fuse_prepare(req);

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.readlink(path,linkname,sizeof(linkname));
      free_path(f,hdr_->nodeid,path);
    }

  if(!err)
    {
      linkname[PATH_MAX] = '\0';
      fuse_reply_readlink(req,linkname);
    }
  else
    {
      fuse_reply_err(req,err);
    }
}

static
void
fuse_lib_mknod(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  const char* name;
  struct fuse_entry_param e;
  struct fuse_mknod_in *arg;

  arg  = fuse_hdr_arg(hdr_);
  name = PARAM(arg);
  if(req->f->conn.proto_minor >= 12)
    req->ctx.umask = arg->umask;
  else
    name = (char*)arg + FUSE_COMPAT_MKNOD_IN_SIZE;

  f = req_fuse_prepare(req);

  err = get_path_name(f,hdr_->nodeid,name,&path);
  if(!err)
    {
      err = -ENOSYS;
      if(S_ISREG(arg->mode))
        {
          fuse_file_info_t fi;

          memset(&fi,0,sizeof(fi));
          fi.flags = O_CREAT | O_EXCL | O_WRONLY;
          err = f->fs->op.create(path,arg->mode,&fi);
          if(!err)
            {
              err = lookup_path(f,hdr_->nodeid,name,path,&e,&fi);
              f->fs->op.release(&fi);
            }
        }

      if(err == -ENOSYS)
        {
          err = f->fs->op.mknod(path,arg->mode,arg->rdev);
          if(!err)
            err = lookup_path(f,hdr_->nodeid,name,path,&e,NULL);
        }

      free_path(f,hdr_->nodeid,path);
    }

  reply_entry(req,&e,err);
}

static
void
fuse_lib_mkdir(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  const char *name;
  struct fuse_entry_param e;
  struct fuse_mkdir_in *arg;

  arg  = fuse_hdr_arg(hdr_);
  name = PARAM(arg);
  if(req->f->conn.proto_minor >= 12)
    req->ctx.umask = arg->umask;

  f = req_fuse_prepare(req);

  err = get_path_name(f,hdr_->nodeid,name,&path);
  if(!err)
    {
      err = f->fs->op.mkdir(path,arg->mode);
      if(!err)
        err = lookup_path(f,hdr_->nodeid,name,path,&e,NULL);
      free_path(f,hdr_->nodeid,path);
    }

  reply_entry(req,&e,err);
}

static
void
fuse_lib_unlink(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  const char *name;
  node_t *wnode;

  name = PARAM(hdr_);

  f = req_fuse_prepare(req);
  err = get_path_wrlock(f,hdr_->nodeid,name,&path,&wnode);

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      if(node_open(wnode))
        err = f->fs->op.prepare_hide(path,&wnode->hidden_fh);
      pthread_mutex_unlock(&f->lock);

      err = f->fs->op.unlink(path);
      if(!err)
        remove_node(f,hdr_->nodeid,name);

      free_path_wrlock(f,hdr_->nodeid,wnode,path);
    }

  fuse_reply_err(req,err);
}

static
void
fuse_lib_rmdir(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  const char *name;
  node_t *wnode;

  name = PARAM(hdr_);

  f = req_fuse_prepare(req);

  err = get_path_wrlock(f,hdr_->nodeid,name,&path,&wnode);
  if(!err)
    {
      err = f->fs->op.rmdir(path);
      if(!err)
        remove_node(f,hdr_->nodeid,name);
      free_path_wrlock(f,hdr_->nodeid,wnode,path);
    }

  fuse_reply_err(req,err);
}

static
void
fuse_lib_symlink(fuse_req_t             req_,
                 struct fuse_in_header *hdr_)
{
  int rv;
  char *path;
  struct fuse *f;
  const char *name;
  const char *linkname;
  struct fuse_entry_param e = {0};

  name     = fuse_hdr_arg(hdr_);
  linkname = (name + strlen(name) + 1);

  f = req_fuse_prepare(req_);

  rv = get_path_name(f,hdr_->nodeid,name,&path);
  if(rv == 0)
    {
      rv = f->fs->op.symlink(linkname,path,&e.attr,&e.timeout);
      if(rv == 0)
        rv = set_path_info(f,hdr_->nodeid,name,&e);
      free_path(f,hdr_->nodeid,path);
    }

  reply_entry(req_,&e,rv);
}

static
void
fuse_lib_rename(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f;
  char *oldpath;
  char *newpath;
  const char *oldname;
  const char *newname;
  node_t *wnode1;
  node_t *wnode2;
  struct fuse_rename_in *arg;

  arg     = fuse_hdr_arg(hdr_);
  oldname = PARAM(arg);
  newname = (oldname + strlen(oldname) + 1);

  f = req_fuse_prepare(req);
  err = get_path2(f,hdr_->nodeid,oldname,arg->newdir,newname,
                  &oldpath,&newpath,&wnode1,&wnode2);

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      if(node_open(wnode2))
        err = f->fs->op.prepare_hide(newpath,&wnode2->hidden_fh);
      pthread_mutex_unlock(&f->lock);

      err = f->fs->op.rename(oldpath,newpath);
      if(!err)
        err = rename_node(f,hdr_->nodeid,oldname,arg->newdir,newname);

      free_path2(f,hdr_->nodeid,arg->newdir,wnode1,wnode2,oldpath,newpath);
    }

  fuse_reply_err(req,err);
}

static
void
fuse_lib_link(fuse_req_t             req,
              struct fuse_in_header *hdr_)
{
  int rv;
  char *oldpath;
  char *newpath;
  struct fuse *f;
  const char *newname;
  struct fuse_link_in *arg;
  struct fuse_entry_param e = {0};

  arg = fuse_hdr_arg(hdr_);
  newname = PARAM(arg);

  f = req_fuse_prepare(req);

  rv = get_path2(f,
                 arg->oldnodeid,NULL,
                 hdr_->nodeid,newname,
                 &oldpath,&newpath,NULL,NULL);
  if(!rv)
    {
      rv = f->fs->op.link(oldpath,newpath,&e.attr,&e.timeout);
      if(rv == 0)
        rv = set_path_info(f,hdr_->nodeid,newname,&e);
      free_path2(f,arg->oldnodeid,hdr_->nodeid,NULL,NULL,oldpath,newpath);
    }

  reply_entry(req,&e,rv);
}

static
void
fuse_do_release(struct fuse      *f,
                uint64_t          ino,
                fuse_file_info_t *fi)
{
  uint64_t fh;
  node_t *node;

  fh = 0;

  f->fs->op.release(fi);

  pthread_mutex_lock(&f->lock);
  {
    node = get_node(f,ino);
    assert(node->open_count > 0);
    node->open_count--;

    if(node->hidden_fh && (node->open_count == 0))
      {
        fh = node->hidden_fh;
        node->hidden_fh = 0;
      }
  }
  pthread_mutex_unlock(&f->lock);

  if(fh)
    f->fs->op.free_hide(fh);
}

static
void
fuse_lib_create(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  const char *name;
  fuse_file_info_t ffi = {0};
  struct fuse_entry_param e;
  struct fuse_create_in *arg;

  arg  = fuse_hdr_arg(hdr_);
  name = PARAM(arg);

  ffi.flags = arg->flags;

  if(req->f->conn.proto_minor >= 12)
    req->ctx.umask = arg->umask;
  else
    name = (char*)arg + sizeof(struct fuse_open_in);

  f = req_fuse_prepare(req);

  err = get_path_name(f,hdr_->nodeid,name,&path);
  if(!err)
    {
      err = f->fs->op.create(path,arg->mode,&ffi);
      if(!err)
        {
          err = lookup_path(f,hdr_->nodeid,name,path,&e,&ffi);
          if(err)
            {
              f->fs->op.release(&ffi);
            }
          else if(!S_ISREG(e.attr.st_mode))
            {
              err = -EIO;
              f->fs->op.release(&ffi);
              forget_node(f,e.ino,1);
            }
        }
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      get_node(f,e.ino)->open_count++;
      pthread_mutex_unlock(&f->lock);

      if(fuse_reply_create(req,&e,&ffi) == -ENOENT)
        {
          /* The open syscall was interrupted,so it
             must be cancelled */
          fuse_do_release(f,e.ino,&ffi);
          forget_node(f,e.ino,1);
        }
    }
  else
    {
      fuse_reply_err(req,err);
    }

  free_path(f,hdr_->nodeid,path);
}

static
void
open_auto_cache(struct fuse      *f,
                uint64_t          ino,
                const char       *path,
                fuse_file_info_t *fi)
{
  node_t *node;
  fuse_timeouts_t timeout;

  pthread_mutex_lock(&f->lock);

  node = get_node(f,ino);
  if(node->is_stat_cache_valid)
    {
      int err;
      struct stat stbuf;

      pthread_mutex_unlock(&f->lock);
      err = f->fs->op.fgetattr(fi,&stbuf,&timeout);
      pthread_mutex_lock(&f->lock);

      if(!err)
        update_stat(node,&stbuf);
      else
        node->is_stat_cache_valid = 0;
    }

  if(node->is_stat_cache_valid)
    fi->keep_cache = 1;

  node->is_stat_cache_valid = 1;

  pthread_mutex_unlock(&f->lock);
}

static
void
fuse_lib_open(fuse_req_t             req,
              struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  fuse_file_info_t ffi = {0};
  struct fuse_open_in *arg;

  arg = fuse_hdr_arg(hdr_);

  ffi.flags = arg->flags;

  f = req_fuse_prepare(req);

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.open(path,&ffi);
      if(!err)
        {
          if(ffi.auto_cache)
            open_auto_cache(f,hdr_->nodeid,path,&ffi);
        }
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      get_node(f,hdr_->nodeid)->open_count++;
      pthread_mutex_unlock(&f->lock);
      /* The open syscall was interrupted,so it must be cancelled */
      if(fuse_reply_open(req,&ffi) == -ENOENT)
        fuse_do_release(f,hdr_->nodeid,&ffi);
    }
  else
    {
      fuse_reply_err(req,err);
    }

  free_path(f,hdr_->nodeid,path);
}

static
void
fuse_lib_read(fuse_req_t             req,
              struct fuse_in_header *hdr_)
{
  int res;
  struct fuse *f;
  fuse_file_info_t ffi = {0};
  struct fuse_read_in *arg;
  fuse_msgbuf_t *msgbuf;

  arg = fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;
  if(req->f->conn.proto_minor >= 9)
    {
      ffi.flags      = arg->flags;
      ffi.lock_owner = arg->lock_owner;
    }

  f = req_fuse_prepare(req);

  msgbuf = msgbuf_alloc_page_aligned();

  res = f->fs->op.read(&ffi,msgbuf->mem,arg->size,arg->offset);

  if(res >= 0)
    fuse_reply_data(req,msgbuf->mem,res);
  else
    fuse_reply_err(req,res);

  msgbuf_free(msgbuf);
}

static
void
fuse_lib_write(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  int res;
  char *data;
  struct fuse *f;
  fuse_file_info_t ffi = {0};
  struct fuse_write_in *arg;

  arg = fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;
  ffi.writepage = !!(arg->write_flags & 1);
  if(req->f->conn.proto_minor < 9)
    {
      data = ((char*)arg) + FUSE_COMPAT_WRITE_IN_SIZE;
    }
  else
    {
      ffi.flags = arg->flags;
      ffi.lock_owner = arg->lock_owner;
      data = PARAM(arg);
    }

  f = req_fuse_prepare(req);

  res = f->fs->op.write(&ffi,data,arg->size,arg->offset);
  free_path(f,hdr_->nodeid,NULL);

  if(res >= 0)
    fuse_reply_write(req,res);
  else
    fuse_reply_err(req,res);
}

static
void
fuse_lib_fsync(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f;
  struct fuse_fsync_in *arg;
  fuse_file_info_t ffi = {0};

  arg = fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;

  f = req_fuse_prepare(req);

  err = f->fs->op.fsync(&ffi,
                        !!(arg->fsync_flags & 1));

  fuse_reply_err(req,err);
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
fuse_lib_opendir(fuse_req_t             req,
                 struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse_dh *dh;
  fuse_file_info_t llffi = {0};
  fuse_file_info_t ffi = {0};
  struct fuse *f;
  struct fuse_open_in *arg;

  arg = fuse_hdr_arg(hdr_);
  llffi.flags = arg->flags;

  f = req_fuse_prepare(req);

  dh = (struct fuse_dh *)calloc(1,sizeof(struct fuse_dh));
  if(dh == NULL)
    {
      fuse_reply_err(req,ENOMEM);
      return;
    }

  fuse_dirents_init(&dh->d);
  fuse_mutex_init(&dh->lock);

  llffi.fh = (uintptr_t)dh;
  ffi.flags = llffi.flags;

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.opendir(path,&ffi);
      dh->fh = ffi.fh;
      llffi.keep_cache    = ffi.keep_cache;
      llffi.cache_readdir = ffi.cache_readdir;
    }

  if(!err)
    {
      if(fuse_reply_open(req,&llffi) == -ENOENT)
        {
          /* The opendir syscall was interrupted,so it
             must be cancelled */
          f->fs->op.releasedir(&ffi);
          pthread_mutex_destroy(&dh->lock);
          free(dh);
        }
    }
  else
    {
      fuse_reply_err(req,err);
      pthread_mutex_destroy(&dh->lock);
      free(dh);
    }

  free_path(f,hdr_->nodeid,path);
}

static
size_t
readdir_buf_size(fuse_dirents_t *d_,
                 size_t          size_,
                 off_t           off_)
{
  if(off_ >= kv_size(d_->offs))
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
fuse_lib_readdir(fuse_req_t             req_,
                 struct fuse_in_header *hdr_)
{
  int rv;
  size_t size;
  struct fuse *f;
  fuse_dirents_t *d;
  struct fuse_dh *dh;
  fuse_file_info_t ffi = {0};
  fuse_file_info_t llffi = {0};
  struct fuse_read_in *arg;

  arg = fuse_hdr_arg(hdr_);
  size = arg->size;
  llffi.fh = arg->fh;

  f  = req_fuse_prepare(req_);
  dh = get_dirhandle(&llffi,&ffi);
  d  = &dh->d;

  pthread_mutex_lock(&dh->lock);

  rv = 0;
  if((arg->offset == 0) || (kv_size(d->data) == 0))
    rv = f->fs->op.readdir(&ffi,d);

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
  pthread_mutex_unlock(&dh->lock);
}

static
void
fuse_lib_readdir_plus(fuse_req_t             req_,
                      struct fuse_in_header *hdr_)
{
  int rv;
  size_t size;
  struct fuse *f;
  fuse_dirents_t *d;
  struct fuse_dh *dh;
  fuse_file_info_t ffi = {0};
  fuse_file_info_t llffi = {0};
  struct fuse_read_in *arg;

  arg = fuse_hdr_arg(hdr_);
  size = arg->size;
  llffi.fh = arg->fh;

  f  = req_fuse_prepare(req_);
  dh = get_dirhandle(&llffi,&ffi);
  d  = &dh->d;

  pthread_mutex_lock(&dh->lock);

  rv = 0;
  if((arg->offset == 0) || (kv_size(d->data) == 0))
    rv = f->fs->op.readdir_plus(&ffi,d);

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
  pthread_mutex_unlock(&dh->lock);
}

static
void
fuse_lib_releasedir(fuse_req_t             req_,
                    struct fuse_in_header *hdr_)
{
  struct fuse *f;
  struct fuse_dh *dh;
  fuse_file_info_t ffi;
  fuse_file_info_t llffi = {0};
  struct fuse_release_in *arg;

  arg = fuse_hdr_arg(hdr_);
  llffi.fh    = arg->fh;
  llffi.flags = arg->flags;

  f  = req_fuse_prepare(req_);
  dh = get_dirhandle(&llffi,&ffi);

  f->fs->op.releasedir(&ffi);

  /* Done to keep race condition between last readdir reply and the unlock */
  pthread_mutex_lock(&dh->lock);
  pthread_mutex_unlock(&dh->lock);
  pthread_mutex_destroy(&dh->lock);
  fuse_dirents_free(&dh->d);
  free(dh);
  fuse_reply_err(req_,0);
}

static
void
fuse_lib_fsyncdir(fuse_req_t             req,
                  struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f;
  fuse_file_info_t ffi;
  fuse_file_info_t llffi = {0};
  struct fuse_fsync_in *arg;

  arg = fuse_hdr_arg(hdr_);
  llffi.fh = arg->fh;

  f = req_fuse_prepare(req);

  get_dirhandle(&llffi,&ffi);

  err = f->fs->op.fsyncdir(&ffi,
                           !!(arg->fsync_flags & FUSE_FSYNC_FDATASYNC));

  fuse_reply_err(req,err);
}

static
void
fuse_lib_statfs(fuse_req_t             req,
                struct fuse_in_header *hdr_)
{
  int err = 0;
  char *path = NULL;
  struct fuse *f;
  struct statvfs buf = {0};

  f = req_fuse_prepare(req);

  if(hdr_->nodeid)
    err = get_path(f,hdr_->nodeid,&path);

  if(!err)
    {
      err = f->fs->op.statfs(path ? path : "/",&buf);
      free_path(f,hdr_->nodeid,path);
    }

  if(!err)
    fuse_reply_statfs(req,&buf);
  else
    fuse_reply_err(req,err);
}

static
void
fuse_lib_setxattr(fuse_req_t             req,
                  struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  const char *name;
  const char *value;
  struct fuse *f;
  struct fuse_setxattr_in *arg;

  arg   = fuse_hdr_arg(hdr_);
  if((req->f->conn.capable & FUSE_SETXATTR_EXT) && (req->f->conn.want & FUSE_SETXATTR_EXT))
    name = PARAM(arg);
  else
    name = (((char*)arg) + FUSE_COMPAT_SETXATTR_IN_SIZE);

  value = (name + strlen(name) + 1);

  f = req_fuse_prepare(req);

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.setxattr(path,name,value,arg->size,arg->flags);
      free_path(f,hdr_->nodeid,path);
    }

  fuse_reply_err(req,err);
}

static
int
common_getxattr(struct fuse *f,
                fuse_req_t   req,
                uint64_t     ino,
                const char  *name,
                char        *value,
                size_t       size)
{
  int err;
  char *path;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = f->fs->op.getxattr(path,name,value,size);

      free_path(f,ino,path);
    }

  return err;
}

static
void
fuse_lib_getxattr(fuse_req_t             req,
                  struct fuse_in_header *hdr_)
{
  int res;
  struct fuse *f;
  const char* name;
  struct fuse_getxattr_in *arg;

  arg  = fuse_hdr_arg(hdr_);
  name = PARAM(arg);

  f = req_fuse_prepare(req);

  if(arg->size)
    {
      char *value = (char*)malloc(arg->size);
      if(value == NULL)
        {
          fuse_reply_err(req,ENOMEM);
          return;
        }

      res = common_getxattr(f,req,hdr_->nodeid,name,value,arg->size);
      if(res > 0)
        fuse_reply_buf(req,value,res);
      else
        fuse_reply_err(req,res);
      free(value);
    }
  else
    {
      res = common_getxattr(f,req,hdr_->nodeid,name,NULL,0);
      if(res >= 0)
        fuse_reply_xattr(req,res);
      else
        fuse_reply_err(req,res);
    }
}

static
int
common_listxattr(struct fuse *f,
                 fuse_req_t   req,
                 uint64_t     ino,
                 char        *list,
                 size_t       size)
{
  char *path;
  int err;

  err = get_path(f,ino,&path);
  if(!err)
    {
      err = f->fs->op.listxattr(path,list,size);
      free_path(f,ino,path);
    }

  return err;
}

static
void
fuse_lib_listxattr(fuse_req_t             req,
                   struct fuse_in_header *hdr_)
{
  int res;
  struct fuse *f;
  struct fuse_getxattr_in *arg;

  arg = fuse_hdr_arg(hdr_);

  f = req_fuse_prepare(req);

  if(arg->size)
    {
      char *list = (char*)malloc(arg->size);
      if(list == NULL)
        {
          fuse_reply_err(req,ENOMEM);
          return;
        }

      res = common_listxattr(f,req,hdr_->nodeid,list,arg->size);
      if(res > 0)
        fuse_reply_buf(req,list,res);
      else
        fuse_reply_err(req,res);
      free(list);
    }
  else
    {
      res = common_listxattr(f,req,hdr_->nodeid,NULL,0);
      if(res >= 0)
        fuse_reply_xattr(req,res);
      else
        fuse_reply_err(req,res);
    }
}

static
void
fuse_lib_removexattr(fuse_req_t                   req,
                     const struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  const char *name;
  struct fuse *f;

  name = fuse_hdr_arg(hdr_);

  f = req_fuse_prepare(req);

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.removexattr(path,name);
      free_path(f,hdr_->nodeid,path);
    }

  fuse_reply_err(req,err);
}

static
void
fuse_lib_copy_file_range(fuse_req_t                   req_,
                         const struct fuse_in_header *hdr_)
{
  ssize_t rv;
  struct fuse *f;
  fuse_file_info_t ffi_in = {0};
  fuse_file_info_t ffi_out = {0};
  const struct fuse_copy_file_range_in *arg;

  arg = fuse_hdr_arg(hdr_);
  ffi_in.fh  = arg->fh_in;
  ffi_out.fh = arg->fh_out;

  f = req_fuse_prepare(req_);

  rv = f->fs->op.copy_file_range(&ffi_in,
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
fuse_lib_setupmapping(fuse_req_t                   req_,
                      const struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

static
void
fuse_lib_removemapping(fuse_req_t                   req_,
                       const struct fuse_in_header *hdr_)
{
  fuse_reply_err(req_,ENOSYS);
}

static
void
fuse_lib_syncfs(fuse_req_t                   req_,
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
fuse_lib_tmpfile(fuse_req_t                   req_,
                 const struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  const char *name;
  fuse_file_info_t ffi = {0};
  struct fuse_entry_param e;
  struct fuse_create_in *arg;

  arg  = fuse_hdr_arg(hdr_);
  name = PARAM(arg);

  ffi.flags = arg->flags;

  if(req_->f->conn.proto_minor >= 12)
    req_->ctx.umask = arg->umask;
  else
    name = (char*)arg + sizeof(struct fuse_open_in);

  f = req_fuse_prepare(req_);

  err = get_path_name(f,hdr_->nodeid,name,&path);
  if(!err)
    {
      err = f->fs->op.tmpfile(path,arg->mode,&ffi);
      if(!err)
        {
          err = lookup_path(f,hdr_->nodeid,name,path,&e,&ffi);
          if(err)
            {
              f->fs->op.release(&ffi);
            }
          else if(!S_ISREG(e.attr.st_mode))
            {
              err = -EIO;
              f->fs->op.release(&ffi);
              forget_node(f,e.ino,1);
            }
        }
    }

  if(!err)
    {
      pthread_mutex_lock(&f->lock);
      get_node(f,e.ino)->open_count++;
      pthread_mutex_unlock(&f->lock);

      if(fuse_reply_create(req_,&e,&ffi) == -ENOENT)
        {
          /* The open syscall was interrupted,so it
             must be cancelled */
          fuse_do_release(f,e.ino,&ffi);
          forget_node(f,e.ino,1);
        }
    }
  else
    {
      fuse_reply_err(req_,err);
    }

  free_path(f,hdr_->nodeid,path);
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
      newl1 = malloc(sizeof(lock_t));
      newl2 = malloc(sizeof(lock_t));

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
fuse_flush_common(struct fuse      *f,
                  fuse_req_t        req,
                  uint64_t          ino,
                  fuse_file_info_t *fi)
{
  struct flock lock;
  lock_t l;
  int err;
  int errlock;

  memset(&lock,0,sizeof(lock));
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  err = f->fs->op.flush(fi);
  errlock = f->fs->op.lock(fi,F_SETLK,&lock);

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
fuse_lib_release(fuse_req_t             req,
                 struct fuse_in_header *hdr_)
{
  int err = 0;
  struct fuse *f;
  fuse_file_info_t ffi = {0};
  struct fuse_release_in *arg;

  arg = fuse_hdr_arg(hdr_);
  ffi.fh    = arg->fh;
  ffi.flags = arg->flags;
  if(req->f->conn.proto_minor >= 8)
    {
      ffi.flush      = !!(arg->release_flags & FUSE_RELEASE_FLUSH);
      ffi.lock_owner = arg->lock_owner;
    }
  else
    {
      ffi.flock_release = 1;
      ffi.lock_owner    = arg->lock_owner;
    }

  f = req_fuse_prepare(req);

  if(ffi.flush)
    {
      err = fuse_flush_common(f,req,hdr_->nodeid,&ffi);
      if(err == -ENOSYS)
        err = 0;
    }

  fuse_do_release(f,hdr_->nodeid,&ffi);

  fuse_reply_err(req,err);
}

static
void
fuse_lib_flush(fuse_req_t             req,
               struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f;
  fuse_file_info_t ffi = {0};
  struct fuse_flush_in *arg;

  arg = fuse_hdr_arg(hdr_);

  ffi.fh = arg->fh;
  ffi.flush = 1;
  if(req->f->conn.proto_minor >= 7)
    ffi.lock_owner = arg->lock_owner;

  f = req_fuse_prepare(req);

  err = fuse_flush_common(f,req,hdr_->nodeid,&ffi);

  fuse_reply_err(req,err);
}

static
int
fuse_lock_common(fuse_req_t        req,
                 uint64_t          ino,
                 fuse_file_info_t *fi,
                 struct flock     *lock,
                 int               cmd)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = f->fs->op.lock(fi,cmd,lock);

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
fuse_lib_getlk(fuse_req_t                   req,
               const struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f;
  lock_t lk;
  struct flock flk;
  lock_t *conflict;
  fuse_file_info_t ffi = {0};
  const struct fuse_lk_in *arg;

  arg = fuse_hdr_arg(hdr_);
  ffi.fh         = arg->fh;
  ffi.lock_owner = arg->owner;

  convert_fuse_file_lock(&arg->lk,&flk);

  f = req_fuse(req);

  flock_to_lock(&flk,&lk);
  lk.owner = ffi.lock_owner;
  pthread_mutex_lock(&f->lock);
  conflict = locks_conflict(get_node(f,hdr_->nodeid),&lk);
  if(conflict)
    lock_to_flock(conflict,&flk);
  pthread_mutex_unlock(&f->lock);
  if(!conflict)
    err = fuse_lock_common(req,hdr_->nodeid,&ffi,&flk,F_GETLK);
  else
    err = 0;

  if(!err)
    fuse_reply_lock(req,&flk);
  else
    fuse_reply_err(req,err);
}

static
void
fuse_lib_setlk(fuse_req_t        req,
               uint64_t          ino,
               fuse_file_info_t *fi,
               struct flock     *lock,
               int               sleep)
{
  int err = fuse_lock_common(req,ino,fi,lock,
                             sleep ? F_SETLKW : F_SETLK);
  if(!err)
    {
      struct fuse *f = req_fuse(req);
      lock_t l;
      flock_to_lock(lock,&l);
      l.owner = fi->lock_owner;
      pthread_mutex_lock(&f->lock);
      locks_insert(get_node(f,ino),&l);
      pthread_mutex_unlock(&f->lock);
    }

  fuse_reply_err(req,err);
}

static
void
fuse_lib_flock(fuse_req_t        req,
               uint64_t          ino,
               fuse_file_info_t *fi,
               int               op)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);

  err = f->fs->op.flock(fi,op);

  fuse_reply_err(req,err);
}

static
void
fuse_lib_bmap(fuse_req_t                   req,
              const struct fuse_in_header *hdr_)
{
  int err;
  char *path;
  struct fuse *f;
  uint64_t block;
  const struct fuse_bmap_in *arg;

  arg = fuse_hdr_arg(hdr_);
  block = arg->block;

  f = req_fuse_prepare(req);

  err = get_path(f,hdr_->nodeid,&path);
  if(!err)
    {
      err = f->fs->op.bmap(path,arg->blocksize,&block);
      free_path(f,hdr_->nodeid,path);
    }

  if(!err)
    fuse_reply_bmap(req,block);
  else
    fuse_reply_err(req,err);
}

static
void
fuse_lib_ioctl(fuse_req_t                   req,
               const struct fuse_in_header *hdr_)
{
  int err;
  char *out_buf = NULL;
  struct fuse *f = req_fuse_prepare(req);
  fuse_file_info_t ffi;
  fuse_file_info_t llffi = {0};
  const void *in_buf;
  uint32_t out_size;
  const struct fuse_ioctl_in *arg;

  arg = fuse_hdr_arg(hdr_);
  if((arg->flags & FUSE_IOCTL_DIR) && !(req->f->conn.want & FUSE_CAP_IOCTL_DIR))
    {
      fuse_reply_err(req,ENOTTY);
      return;
    }

  if((sizeof(void*) == 4)             &&
     (req->f->conn.proto_minor >= 16) &&
     !(arg->flags & FUSE_IOCTL_32BIT))
    {
      req->ioctl_64bit = 1;
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
      out_buf = malloc(out_size);
      if(!out_buf)
        goto err;
    }

  assert(!arg->in_size || !out_size || arg->in_size == out_size);
  if(out_buf)
    memcpy(out_buf,in_buf,arg->in_size);

  err = f->fs->op.ioctl(&ffi,
                        arg->cmd,
                        (void*)(uintptr_t)arg->arg,
                        arg->flags,
                        out_buf ?: (void *)in_buf,
                        &out_size);
  if(err < 0)
    goto err;

  fuse_reply_ioctl(req,err,out_buf,out_size);
  goto out;
 err:
  fuse_reply_err(req,err);
 out:
  free(out_buf);
}

static
void
fuse_lib_poll(fuse_req_t                   req,
              const struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f = req_fuse_prepare(req);
  unsigned revents = 0;
  fuse_file_info_t ffi = {0};
  fuse_pollhandle_t *ph = NULL;
  const struct fuse_poll_in *arg;

  arg = fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;

  if(arg->flags & FUSE_POLL_SCHEDULE_NOTIFY)
    {
      ph = (fuse_pollhandle_t*)malloc(sizeof(fuse_pollhandle_t));
      if(ph == NULL)
        {
          fuse_reply_err(req,ENOMEM);
          return;
        }

      ph->kh = arg->kh;
      ph->ch = req->ch;
      ph->f  = req->f;
    }

  err = f->fs->op.poll(&ffi,ph,&revents);

  if(!err)
    fuse_reply_poll(req,revents);
  else
    fuse_reply_err(req,err);
}

static
void
fuse_lib_fallocate(fuse_req_t                   req,
                   const struct fuse_in_header *hdr_)
{
  int err;
  struct fuse *f;
  fuse_file_info_t ffi = {0};
  const struct fuse_fallocate_in *arg;

  arg = fuse_hdr_arg(hdr_);
  ffi.fh = arg->fh;

  f = req_fuse_prepare(req);

  err = f->fs->op.fallocate(&ffi,
                            arg->mode,
                            arg->offset,
                            arg->length);

  fuse_reply_err(req,err);
}

static
int
remembered_node_cmp(const void *a_,
                    const void *b_)
{
  const remembered_node_t *a = a_;
  const remembered_node_t *b = b_;

  return (a->time - b->time);
}

static
void
remembered_nodes_sort(struct fuse *f_)
{
  pthread_mutex_lock(&f_->lock);
  qsort(&kv_first(f_->remembered_nodes),
        kv_size(f_->remembered_nodes),
        sizeof(remembered_node_t),
        remembered_node_cmp);
  pthread_mutex_unlock(&f_->lock);
}

#define MAX_PRUNE 100
#define MAX_CHECK 1000

int
fuse_prune_some_remembered_nodes(struct fuse *f_,
                                 int         *offset_)
{
  time_t now;
  int pruned;
  int checked;

  pthread_mutex_lock(&f_->lock);

  pruned = 0;
  checked = 0;
  now = current_time();
  while(*offset_ < kv_size(f_->remembered_nodes))
    {
      time_t age;
      remembered_node_t *fn = &kv_A(f_->remembered_nodes,*offset_);

      if(pruned >= MAX_PRUNE)
        break;
      if(checked >= MAX_CHECK)
        break;

      checked++;
      age = (now - fn->time);
      if(f_->conf.remember > age)
        break;

      assert(fn->node->nlookup == 1);

      /* Don't forget active directories */
      if(fn->node->refctr > 1)
        {
          (*offset_)++;
          continue;
        }

      fn->node->nlookup = 0;
      unref_node(f_,fn->node);
      kv_delete(f_->remembered_nodes,*offset_);
      pruned++;
    }

  pthread_mutex_unlock(&f_->lock);

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
fuse_prune_remembered_nodes(struct fuse *f_)
{
  int offset;
  int pruned;

  offset = 0;
  pruned = 0;
  for(;;)
    {
      pruned += fuse_prune_some_remembered_nodes(f_,&offset);
      if(offset >= 0)
        {
          sleep_100ms();
          continue;
        }

      break;
    }

  if(pruned > 0)
    remembered_nodes_sort(f_);
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
   .retrieve_reply  = NULL,
   .rmdir           = fuse_lib_rmdir,
   .setattr         = fuse_lib_setattr,
   .setlk           = fuse_lib_setlk,
   .setupmapping    = fuse_lib_setupmapping,
   .setxattr        = fuse_lib_setxattr,
   .statfs          = fuse_lib_statfs,
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
fuse_exited(struct fuse *f)
{
  return fuse_session_exited(f->se);
}

struct fuse_session*
fuse_get_session(struct fuse *f)
{
  return f->se;
}

void
fuse_exit(struct fuse *f)
{
  f->se->exited = 1;
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
   FUSE_OPT_KEY("debug",	      FUSE_OPT_KEY_KEEP),
   FUSE_OPT_KEY("-d",		      FUSE_OPT_KEY_KEEP),
   FUSE_LIB_OPT("debug",	      debug,1),
   FUSE_LIB_OPT("-d",		      debug,1),
   FUSE_LIB_OPT("nogc",               nogc,1),
   FUSE_LIB_OPT("umask=",	      set_mode,1),
   FUSE_LIB_OPT("umask=%o",	      umask,0),
   FUSE_LIB_OPT("uid=",	              set_uid,1),
   FUSE_LIB_OPT("uid=%d",	      uid,0),
   FUSE_LIB_OPT("gid=",	              set_gid,1),
   FUSE_LIB_OPT("gid=%d",	      gid,0),
   FUSE_LIB_OPT("noforget",           remember,-1),
   FUSE_LIB_OPT("remember=%u",        remember,0),
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
            size_t                        op_size)
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

  if(op)
    memcpy(&fs->op,op,op_size);

  return fs;
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
struct fuse*
fuse_get_fuse_obj()
{
  static struct fuse f = {0};

  return &f;
}

static
void
metrics_log_nodes_info(struct fuse *f_,
                       FILE        *file_)
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
           "sizeof(node): %"PRIu64"\n"
           "node id_table size: %"PRIu64"\n"
           "node id_table usage: %"PRIu64"\n"
           "node id_table total allocated memory: %"PRIu64"\n"
           "node name_table size: %"PRIu64"\n"
           "node name_table usage: %"PRIu64"\n"
           "node name_table total allocated memory: %"PRIu64"\n"
           "node memory pool slab count: %"PRIu64"\n"
           "node memory pool usage ratio: %f\n"
           "node memory pool avail objs: %"PRIu64"\n"
           "node memory pool total allocated memory: %"PRIu64"\n"
           "msgbuf bufsize: %"PRIu64"\n"
           "msgbuf allocation count: %"PRIu64"\n"
           "msgbuf available count: %"PRIu64"\n"
           "msgbuf total allocated memory: %"PRIu64"\n"
           "\n"
           ,
           time_str,
           sizeof_node,
           (uint64_t)f_->id_table.size,
           (uint64_t)f_->id_table.use,
           (uint64_t)(f_->id_table.size * sizeof(node_t*)),
           (uint64_t)f_->name_table.size,
           (uint64_t)f_->name_table.use,
           (uint64_t)(f_->name_table.size * sizeof(node_t*)),
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

  metrics_log_nodes_info(f_,file);

  fclose(file);
}

static
void
fuse_malloc_trim(void)
{
#ifdef HAVE_MALLOC_TRIM
  malloc_trim(1024 * 1024);
#endif
}

void
fuse_invalidate_all_nodes()
{
  struct fuse *f = fuse_get_fuse_obj();

  syslog(LOG_INFO,"invalidating file entries");

  pthread_mutex_lock(&f->lock);
  for(int i = 0; i < f->id_table.size; i++)
    {
      node_t *node;

      for(node = f->id_table.array[i]; node != NULL; node = node->id_next)
        {
          if(node->nodeid == FUSE_ROOT_ID)
            continue;
          if(node->parent->nodeid != FUSE_ROOT_ID)
            continue;

          fuse_lowlevel_notify_inval_entry(f->se->ch,
                                           node->parent->nodeid,
                                           node->name,
                                           strlen(node->name));
        }
    }
  pthread_mutex_unlock(&f->lock);
}

void
fuse_gc()
{
  syslog(LOG_INFO,"running thorough garbage collection");
  node_gc();
  msgbuf_gc();
  fuse_malloc_trim();
}

void
fuse_gc1()
{
  syslog(LOG_INFO,"running basic garbage collection");
  node_gc1();
  msgbuf_gc_10percent();
  fuse_malloc_trim();
}

static
void*
fuse_maintenance_loop(void *fuse_)
{
  int loops;
  int sleep_time;
  struct fuse *f = (struct fuse*)fuse_;

  pthread_setname_np(pthread_self(),"fuse.maint");

  loops = 0;
  sleep_time = 60;
  while(1)
    {
      if(remember_nodes(f))
        fuse_prune_remembered_nodes(f);

      if((loops % 15) == 0)
        fuse_gc1();

      if(g_LOG_METRICS)
        metrics_log_nodes_info_to_tmp_dir(f);

      loops++;
      sleep(sleep_time);
    }

  return NULL;
}

int
fuse_start_maintenance_thread(struct fuse *f_)
{
  return fuse_start_thread(&f_->maintenance_thread,fuse_maintenance_loop,f_);
}

void
fuse_stop_maintenance_thread(struct fuse *f_)
{
  pthread_mutex_lock(&f_->lock);
  pthread_cancel(f_->maintenance_thread);
  pthread_mutex_unlock(&f_->lock);
  pthread_join(f_->maintenance_thread,NULL);
}

struct fuse*
fuse_new_common(struct fuse_chan             *ch,
                struct fuse_args             *args,
                const struct fuse_operations *op,
                size_t                        op_size)
{
  struct fuse *f;
  node_t *root;
  struct fuse_fs *fs;
  struct fuse_lowlevel_ops llop = fuse_path_ops;

  if(fuse_create_context_key() == -1)
    goto out;

  f = fuse_get_fuse_obj();
  if(f == NULL)
    {
      fprintf(stderr,"fuse: failed to allocate fuse object\n");
      goto out_delete_context_key;
    }

  fs = fuse_fs_new(op,op_size);
  if(!fs)
    goto out_free;

  f->fs = fs;

  /* Oh f**k,this is ugly! */
  if(!fs->op.lock)
    {
      llop.getlk = NULL;
      llop.setlk = NULL;
    }

  if(fuse_opt_parse(args,&f->conf,fuse_lib_opts,fuse_lib_opt_proc) == -1)
    goto out_free_fs;

  g_LOG_METRICS = f->conf.debug;

  f->se = fuse_lowlevel_new_common(args,&llop,sizeof(llop),f);
  if(f->se == NULL)
    goto out_free_fs;

  fuse_session_add_chan(f->se,ch);

  /* Trace topmost layer by default */
  srand(time(NULL));
  f->nodeid_gen.nodeid = FUSE_ROOT_ID;
  f->nodeid_gen.generation = rand64();
  if(node_table_init(&f->name_table) == -1)
    goto out_free_session;

  if(node_table_init(&f->id_table) == -1)
    goto out_free_name_table;

  fuse_mutex_init(&f->lock);

  kv_init(f->remembered_nodes);

  root = node_alloc();
  if(root == NULL)
    {
      fprintf(stderr,"fuse: memory allocation failed\n");
      goto out_free_id_table;
    }

  root->name = filename_strdup(f,"/");

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
  free(f->fs);
 out_free:
  // free(f);
 out_delete_context_key:
  fuse_delete_context_key();
 out:
  return NULL;
}

struct fuse*
fuse_new(struct fuse_chan             *ch,
         struct fuse_args             *args,
         const struct fuse_operations *op,
         size_t                        op_size)
{
  return fuse_new_common(ch,args,op,op_size);
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
          node_t *node;

          for(node = f->id_table.array[i]; node != NULL; node = node->id_next)
            {
              if(!node->hidden_fh)
                continue;

              f->fs->op.free_hide(node->hidden_fh);
              node->hidden_fh = 0;
            }
        }
    }

  for(i = 0; i < f->id_table.size; i++)
    {
      node_t *node;
      node_t *next;

      for(node = f->id_table.array[i]; node != NULL; node = next)
        {
          next = node->id_next;
          free_node(f,node);
          f->id_table.use--;
        }
    }

  free(f->id_table.array);
  free(f->name_table.array);
  pthread_mutex_destroy(&f->lock);
  fuse_session_destroy(f->se);
  kv_destroy(f->remembered_nodes);
  fuse_delete_context_key();
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

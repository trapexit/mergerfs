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
#include "lfmp.h"

#include "config.h"
#include "fuse_dirents.h"
#include "fuse_i.h"
#include "fuse_kernel.h"
#include "fuse_lowlevel.h"
#include "fuse_misc.h"
#include "fuse_opt.h"

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

#ifdef HAVE_MALLOC_TRIM
#include <malloc.h>
#endif

#define FUSE_UNKNOWN_INO UINT64_MAX
#define OFFSET_MAX 0x7fffffffffffffffLL

#define NODE_TABLE_MIN_SIZE 8192

static int g_LOG_METRICS = 0;

struct fuse_config
{
  unsigned int uid;
  unsigned int gid;
  unsigned int umask;
  int remember;
  int debug;
  int nogc;
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
};

struct lock_queue_element
{
  struct lock_queue_element *next;
  pthread_cond_t cond;
  uint64_t   nodeid1;
  const char *name1;
  char **path1;
  struct node **wnode1;
  uint64_t   nodeid2;
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

typedef struct remembered_node_t remembered_node_t;
struct remembered_node_t
{
  struct node *node;
  time_t       time;
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
  lfmp_t node_fmp;
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
  uint8_t is_hidden:1;
  uint8_t is_stat_cache_valid:1;
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
struct node*
alloc_node(struct fuse *f)
{
  return lfmp_calloc(&f->node_fmp);
}

static
void
free_node_mem(struct fuse *f,
              struct node *node)
{
  return lfmp_free(&f->node_fmp,node);
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
struct node*
get_node_nocheck(struct fuse *f,
                 uint64_t     nodeid)
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
         const uint64_t    nodeid)
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

static
void
remove_remembered_node(struct fuse *f_,
                       struct node *node_)
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
          struct node *node_)
{
  filename_free(f_,node_->name);

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
          uint64_t     parentid,
          const char  *name)
{
  size_t hash = name_hash(f,parentid,name);
  struct node *parent = get_node(f,parentid);
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
            struct node *node)
{
  assert(node->treelock == 0);
  unhash_name(f,node);
  if(remember_nodes(f))
    remove_remembered_node(f,node);
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
struct node*
lookup_node(struct fuse *f,
            uint64_t     parent,
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
          uint64_t     parent,
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
             uint64_t      nodeid,
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
                uint64_t      nodeid,
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
                struct node **wnode)
{
  return get_path_common(f,nodeid,name,path,wnode);
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
          uint64_t      nodeid1,
          const char   *name1,
          uint64_t      nodeid2,
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

      err = wait_path(f,&qe);
    }
  pthread_mutex_unlock(&f->lock);

  return err;
}

static
void
free_path_wrlock(struct fuse *f,
                 uint64_t     nodeid,
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
            const uint64_t    nodeid,
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
            struct node *node)
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
            uint64_t     olddir,
            const char  *oldname,
            uint64_t     newdir,
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
         uint64_t     nodeid,
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
  return fs->op.getattr(path,buf,timeout);
}

int
fuse_fs_fgetattr(struct fuse_fs   *fs,
                 struct stat      *buf,
                 fuse_file_info_t *fi,
                 fuse_timeouts_t  *timeout)
{
  return fs->op.fgetattr(fi,buf,timeout);
}

int
fuse_fs_rename(struct fuse_fs *fs,
               const char     *oldpath,
               const char     *newpath)
{
  return fs->op.rename(oldpath,newpath);
}

int
fuse_fs_prepare_hide(struct fuse_fs *fs_,
                     const char     *path_,
                     uint64_t       *fh_)
{
  return fs_->op.prepare_hide(path_,fh_);
}

int
fuse_fs_free_hide(struct fuse_fs *fs_,
                  uint64_t        fh_)
{
  return fs_->op.free_hide(fh_);
}

int
fuse_fs_unlink(struct fuse_fs *fs,
               const char     *path)
{
  return fs->op.unlink(path);
}

int
fuse_fs_rmdir(struct fuse_fs *fs,
              const char     *path)
{
  return fs->op.rmdir(path);
}

int
fuse_fs_symlink(struct fuse_fs  *fs_,
                const char      *linkname_,
                const char      *path_,
                struct stat     *st_,
                fuse_timeouts_t *timeouts_)
{
  return fs_->op.symlink(linkname_,path_,st_,timeouts_);
}

int
fuse_fs_link(struct fuse_fs  *fs,
             const char      *oldpath,
             const char      *newpath,
             struct stat     *st_,
             fuse_timeouts_t *timeouts_)
{
  return fs->op.link(oldpath,newpath,st_,timeouts_);
}

int
fuse_fs_release(struct fuse_fs   *fs,
                fuse_file_info_t *fi)
{
  return fs->op.release(fi);
}

int
fuse_fs_opendir(struct fuse_fs   *fs,
                const char       *path,
                fuse_file_info_t *fi)
{
  return fs->op.opendir(path,fi);
}

int
fuse_fs_open(struct fuse_fs   *fs,
             const char       *path,
             fuse_file_info_t *fi)
{
  return fs->op.open(path,fi);
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
  int res;

  res = fs->op.read_buf(fi,bufp,size,off);
  if(res < 0)
    return res;

  return 0;
}

int
fuse_fs_write_buf(struct fuse_fs     *fs,
                  struct fuse_bufvec *buf,
                  off_t               off,
                  fuse_file_info_t   *fi)
{
  return fs->op.write_buf(fi,buf,off);
}

int
fuse_fs_fsync(struct fuse_fs   *fs,
              int               datasync,
              fuse_file_info_t *fi)
{
  return fs->op.fsync(fi,datasync);
}

int
fuse_fs_fsyncdir(struct fuse_fs   *fs,
                 int               datasync,
                 fuse_file_info_t *fi)
{
  return fs->op.fsyncdir(fi,datasync);
}

int
fuse_fs_flush(struct fuse_fs   *fs,
              fuse_file_info_t *fi)
{
  return fs->op.flush(fi);
}

int
fuse_fs_statfs(struct fuse_fs *fs,
               const char     *path,
               struct statvfs *buf)
{
  return fs->op.statfs(path,buf);
}

int
fuse_fs_releasedir(struct fuse_fs   *fs,
                   fuse_file_info_t *fi)
{
  return fs->op.releasedir(fi);
}

int
fuse_fs_readdir(struct fuse_fs   *fs,
                fuse_file_info_t *fi,
                fuse_dirents_t   *buf)
{
  return fs->op.readdir(fi,buf);
}

int
fuse_fs_readdir_plus(struct fuse_fs   *fs_,
                     fuse_file_info_t *ffi_,
                     fuse_dirents_t   *buf_)
{
  return fs_->op.readdir_plus(ffi_,buf_);
}

int
fuse_fs_create(struct fuse_fs   *fs,
               const char       *path,
               mode_t            mode,
               fuse_file_info_t *fi)
{
  return fs->op.create(path,mode,fi);
}

int
fuse_fs_lock(struct fuse_fs   *fs,
             fuse_file_info_t *fi,
             int               cmd,
             struct flock     *lock)
{
  return fs->op.lock(fi,cmd,lock);
}

int
fuse_fs_flock(struct fuse_fs   *fs,
              fuse_file_info_t *fi,
              int               op)
{
  return fs->op.flock(fi,op);
}

int
fuse_fs_chown(struct fuse_fs *fs,
              const char     *path,
              uid_t           uid,
              gid_t           gid)
{
  return fs->op.chown(path,uid,gid);
}

int
fuse_fs_fchown(struct fuse_fs         *fs_,
               const fuse_file_info_t *ffi_,
               const uid_t             uid_,
               const gid_t             gid_)
{
  return fs_->op.fchown(ffi_,uid_,gid_);
}

int
fuse_fs_truncate(struct fuse_fs *fs,
                 const char     *path,
                 off_t           size)
{
  return fs->op.truncate(path,size);
}

int
fuse_fs_ftruncate(struct fuse_fs   *fs,
                  off_t             size,
                  fuse_file_info_t *fi)
{
  return fs->op.ftruncate(fi,size);
}

int
fuse_fs_utimens(struct fuse_fs        *fs,
                const char            *path,
                const struct timespec  tv[2])
{
  return fs->op.utimens(path,tv);
}

int
fuse_fs_futimens(struct fuse_fs         *fs_,
                 const fuse_file_info_t *ffi_,
                 const struct timespec   tv_[2])
{
  return fs_->op.futimens(ffi_,tv_);
}

int
fuse_fs_access(struct fuse_fs *fs,
               const char     *path,
               int             mask)
{
  return fs->op.access(path,mask);
}

int
fuse_fs_readlink(struct fuse_fs *fs,
                 const char     *path,
                 char           *buf,
                 size_t          len)
{
  return fs->op.readlink(path,buf,len);
}

int
fuse_fs_mknod(struct fuse_fs *fs,
              const char     *path,
              mode_t          mode,
              dev_t           rdev)
{
  return fs->op.mknod(path,mode,rdev);
}

int
fuse_fs_mkdir(struct fuse_fs *fs,
              const char     *path,
              mode_t          mode)
{
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
  return fs->op.setxattr(path,name,value,size,flags);
}

int
fuse_fs_getxattr(struct fuse_fs *fs,
                 const char     *path,
                 const char     *name,
                 char           *value,
                 size_t          size)
{
  return fs->op.getxattr(path,name,value,size);
}

int
fuse_fs_listxattr(struct fuse_fs *fs,
                  const char     *path,
                  char           *list,
                  size_t          size)
{
  return fs->op.listxattr(path,list,size);
}

int
fuse_fs_bmap(struct fuse_fs *fs,
             const char     *path,
             size_t          blocksize,
             uint64_t       *idx)
{
  return fs->op.bmap(path,blocksize,idx);
}

int
fuse_fs_removexattr(struct fuse_fs *fs,
                    const char     *path,
                    const char     *name)
{
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
  return fs->op.ioctl(fi,cmd,arg,flags,data,out_size);
}

int
fuse_fs_poll(struct fuse_fs    *fs,
             fuse_file_info_t  *fi,
             fuse_pollhandle_t *ph,
             unsigned          *reventsp)
{
  return fs->op.poll(fi,ph,reventsp);
}

int
fuse_fs_fallocate(struct fuse_fs   *fs,
                  int               mode,
                  off_t             offset,
                  off_t             length,
                  fuse_file_info_t *fi)
{
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

static
void
update_stat(struct node       *node_,
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
  struct node *node;

  node = find_node(f,nodeid,name);
  if(node == NULL)
    return -ENOMEM;

  e->ino        = node->nodeid;
  e->generation = f->nodeid_gen.generation;

  pthread_mutex_lock(&f->lock);
  update_stat(node,&e->attr);
  pthread_mutex_unlock(&f->lock);

  set_stat(f,e->ino,&e->attr);

  return 0;
}

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
  fs->op.init(conn);
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
  if(fs->op.destroy)
    fs->op.destroy();
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
                uint64_t    parent,
                const char *name)
{
  int err;
  char *path;
  struct node *dot = NULL;
  struct fuse *f = req_fuse_prepare(req);
  struct fuse_entry_param e = {0};

  if(name[0] == '.')
    {
      if(name[1] == '\0')
        {
          name = NULL;
          pthread_mutex_lock(&f->lock);
          dot = get_node_nocheck(f,parent);
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
          if(parent == 1)
            {
              reply_entry(req,&e,-ENOENT);
              return;
            }

          name = NULL;
          pthread_mutex_lock(&f->lock);
          parent = get_node(f,parent)->parent->nodeid;
          pthread_mutex_unlock(&f->lock);
        }
    }

  err = get_path_name(f,parent,name,&path);
  if(!err)
    {
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
          const uint64_t    ino,
          const uint64_t    nlookup)
{
  forget_node(f,ino,nlookup);
}

static
void
fuse_lib_forget(fuse_req_t       req,
                const uint64_t   ino,
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
                 uint64_t          ino,
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
  if(fi == NULL)
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
  return fs->op.chmod(path,mode);
}

int
fuse_fs_fchmod(struct fuse_fs         *fs_,
               const fuse_file_info_t *ffi_,
               const mode_t            mode_)
{
  return fs_->op.fchmod(ffi_,mode_);
}

static
void
fuse_lib_setattr(fuse_req_t        req,
                 uint64_t          ino,
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
                uint64_t   ino,
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
                  uint64_t   ino)
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
               uint64_t    parent,
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
               uint64_t    parent,
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
                uint64_t    parent,
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
               uint64_t    parent,
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
                 uint64_t    parent_,
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
                uint64_t    olddir,
                const char *oldname,
                uint64_t    newdir,
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
              uint64_t    ino,
              uint64_t    newparent,
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
                uint64_t          ino,
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
                uint64_t          parent,
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
void
open_auto_cache(struct fuse      *f,
                uint64_t          ino,
                const char       *path,
                fuse_file_info_t *fi)
{
  struct node *node;
  fuse_timeouts_t timeout;

  pthread_mutex_lock(&f->lock);

  node = get_node(f,ino);
  if(node->is_stat_cache_valid)
    {
      int err;
      struct stat stbuf;

      pthread_mutex_unlock(&f->lock);
      err = fuse_fs_fgetattr(f->fs,&stbuf,fi,&timeout);
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
fuse_lib_open(fuse_req_t        req,
              uint64_t          ino,
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
              uint64_t          ino,
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
                   uint64_t            ino,
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
               uint64_t          ino,
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
                 uint64_t          ino,
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
fuse_lib_readdir(fuse_req_t        req_,
                 uint64_t          ino_,
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
  if((off_ == 0) || (kv_size(d->data) == 0))
    rv = fuse_fs_readdir(f->fs,&fi,d);

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
                      uint64_t          ino_,
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
  if((off_ == 0) || (kv_size(d->data) == 0))
    rv = fuse_fs_readdir_plus(f->fs,&fi,d);

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
                    uint64_t          ino_,
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
                  uint64_t          ino,
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
                uint64_t   ino)
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
                  uint64_t    ino,
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
      err = fuse_fs_getxattr(f->fs,path,name,value,size);

      free_path(f,ino,path);
    }

  return err;
}

static
void
fuse_lib_getxattr(fuse_req_t  req,
                  uint64_t    ino,
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
                 uint64_t     ino,
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
                   uint64_t   ino,
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
                     uint64_t    ino,
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
                         uint64_t          nodeid_in_,
                         off_t             off_in_,
                         fuse_file_info_t *ffi_in_,
                         uint64_t          nodeid_out_,
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
                  uint64_t          ino,
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
                 uint64_t          ino,
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
               uint64_t          ino,
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
                 uint64_t          ino,
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
               uint64_t          ino,
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
               uint64_t          ino,
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
              uint64_t   ino,
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
               uint64_t          ino,
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
              uint64_t           ino,
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
                   uint64_t          ino,
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
   .removexattr     = fuse_lib_removexattr,
   .rename          = fuse_lib_rename,
   .retrieve_reply  = NULL,
   .rmdir           = fuse_lib_rmdir,
   .setattr         = fuse_lib_setattr,
   .setlk           = fuse_lib_setlk,
   .setxattr        = fuse_lib_setxattr,
   .statfs          = fuse_lib_statfs,
   .symlink         = fuse_lib_symlink,
   .unlink          = fuse_lib_unlink,
   .write_buf       = fuse_lib_write_buf,
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
  struct fuse_chan *ch = f->se->ch;
  size_t bufsize = fuse_chan_bufsize(ch);
  struct fuse_cmd *cmd = fuse_alloc_cmd(bufsize);

  if(cmd != NULL)
    {
      int res = fuse_chan_recv(ch,cmd->buf,bufsize);
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
   FUSE_LIB_OPT("uid=",	      set_uid,1),
   FUSE_LIB_OPT("uid=%d",	      uid,0),
   FUSE_LIB_OPT("gid=",	      set_gid,1),
   FUSE_LIB_OPT("gid=%d",	      gid,0),
   FUSE_LIB_OPT("noforget",          remember,-1),
   FUSE_LIB_OPT("remember=%u",       remember,0),
   FUSE_LIB_OPT("threads=%d",        threads,0),
   FUSE_LIB_OPT("use_ino",           use_ino,1),
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
void
metrics_log_nodes_info(struct fuse *f_,
                       FILE        *file_)
{
  char buf[1024];

  lfmp_lock(&f_->node_fmp);
  snprintf(buf,sizeof(buf),
           "time: %zu\n"
           "sizeof(node): %zu\n"
           "node id_table size: %zu\n"
           "node id_table usage: %zu\n"
           "node id_table total allocated memory: %zu\n"
           "node name_table size: %zu\n"
           "node name_table usage: %zu\n"
           "node name_table total allocated memory: %zu\n"
           "node memory pool slab count: %zu\n"
           "node memory pool usage ratio: %f\n"
           "node memory pool avail objs: %zu\n"
           "node memory pool total allocated memory: %zu\n"
           "\n"
           ,
           time(NULL),
           sizeof(struct node),
           f_->id_table.size,
           f_->id_table.use,
           (f_->id_table.size * sizeof(struct node*)),
           f_->name_table.size,
           f_->name_table.use,
           (f_->name_table.size * sizeof(struct node*)),
           fmp_slab_count(&f_->node_fmp.fmp),
           fmp_slab_usage_ratio(&f_->node_fmp.fmp),
           fmp_avail_objs(&f_->node_fmp.fmp),
           fmp_total_allocated_memory(&f_->node_fmp.fmp)
           );
  lfmp_unlock(&f_->node_fmp);

  fputs(buf,file_);
}

static
void
metrics_log_nodes_info_to_tmp_dir(struct fuse *f_)
{
  FILE *file;
  char filepath[256];

  sprintf(filepath,"/tmp/mergerfs.%d.info",getpid());

  file = fopen(filepath,"w");
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

static
void*
fuse_maintenance_loop(void *fuse_)
{
  int gc;
  int loops;
  int sleep_time;
  struct fuse *f = (struct fuse*)fuse_;

  gc = 0;
  loops = 0;
  sleep_time = 60;
  while(1)
    {
      if(remember_nodes(f))
        fuse_prune_remembered_nodes(f);

      if((loops % 15) == 0)
        {
          fuse_malloc_trim();
          gc = 1;
        }

      // Trigger a followup gc if this gc succeeds
      if(!f->conf.nogc && gc)
        gc = lfmp_gc(&f->node_fmp);

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

  lfmp_init(&f->node_fmp,sizeof(struct node),256);
  kv_init(f->remembered_nodes);

  root = alloc_node(f);
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

  free(f->id_table.array);
  free(f->name_table.array);
  pthread_mutex_destroy(&f->lock);
  fuse_session_destroy(f->se);
  lfmp_destroy(&f->node_fmp);
  kv_destroy(f->remembered_nodes);
  free(f);
  fuse_delete_context_key();
}

int
fuse_config_num_threads(const struct fuse *fuse_)
{
  return fuse_->conf.threads;
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

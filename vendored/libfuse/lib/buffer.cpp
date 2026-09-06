/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2010  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "fuse_common.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>


size_t
fuse_buf_size(const struct fuse_bufvec *bufv_)
{
  size_t i;
  size_t size = 0;

  for(i = 0; i < bufv_->count; i++)
    {
      if(bufv_->buf[i].size == SIZE_MAX)
        size = SIZE_MAX;
      else
        size += bufv_->buf[i].size;
    }

  return size;
}

static
ssize_t
fuse_buf_write(const struct fuse_buf *dst,
               size_t                 dst_off_,
               const struct fuse_buf *src,
               size_t                 src_off_,
               size_t                 len_)
{
  ssize_t res = 0;
  size_t copied = 0;

  while(len_)
    {
      if(dst->flags & FUSE_BUF_FD_SEEK)
        res = pwrite(dst->fd,(char*)src->mem + src_off_,len_,dst->pos + dst_off_);
      else
        res = write(dst->fd,(char*)src->mem + src_off_,len_);
      if(res == -1)
        {
          if(!copied)
            return -errno;
          break;
        }
      if(res == 0)
        break;

      copied += res;
      if(!(dst->flags & FUSE_BUF_FD_RETRY))
        break;

      src_off_ += res;
      dst_off_ += res;
      len_     -= res;
    }

  return copied;
}

static
ssize_t
fuse_buf_read(const struct fuse_buf *dst,
              size_t                 dst_off_,
              const struct fuse_buf *src,
              size_t                 src_off_,
              size_t                 len_)
{
  ssize_t res = 0;
  size_t copied = 0;

  while(len_)
    {
      if(src->flags & FUSE_BUF_FD_SEEK)
        res = pread(src->fd,(char*)dst->mem + dst_off_,len_,src->pos + src_off_);
      else
        res = read(src->fd,(char*)dst->mem + dst_off_,len_);
      if(res == -1)
        {
          if(!copied)
            return -errno;
          break;
        }
      if(res == 0)
        break;

      copied += res;
      if(!(src->flags & FUSE_BUF_FD_RETRY))
        break;

      dst_off_ += res;
      src_off_ += res;
      len_     -= res;
    }

  return copied;
}

static
/* NOTE on partial-failure byte loss: when src is a one-shot,
   unrereadable pipe (e.g. mergerfs's write-splice fallback), a
   fuse_buf_write() failure on any chunk after the first (copied > 0
   below) silently drops that chunk's already-consumed
   fuse_buf_read() bytes - they were removed from the pipe but never
   land at dst, and the returned "copied" count has no way to signal
   the loss. This mirrors upstream libfuse's bounce-copy semantics and
   is not changed here. The dominant real-world trigger in mergerfs
   (an O_APPEND destination fd, which deterministically sends every
   splice(2) call through this function for the whole payload) is
   avoided upstream of this call: src/fuse_write_buf.cpp's
   FUSE::write_buf detects O_APPEND before ever attempting splice and
   routes straight to its own lossless heap-buffer fallback instead. */
ssize_t
fuse_buf_fd_to_fd(const struct fuse_buf *dst,
                  size_t                 dst_off_,
                  const struct fuse_buf *src,
                  size_t                 src_off_,
                  size_t                 len_)
{
  std::array<char,4096> buf;
  struct fuse_buf tmp =
    {
      .size  = buf.size(),
      .flags = (enum fuse_buf_flags)0,
      .mem   = buf.data(),
      .fd    = -1,
      .pos   = 0,
    };
  ssize_t res;
  size_t copied = 0;

  while(len_)
    {
      size_t this_len = std::min(tmp.size,len_);
      size_t read_len;

      res = fuse_buf_read(&tmp,0,src,src_off_,this_len);
      if(res < 0)
        {
          if(!copied)
            return res;
          break;
        }
      if(res == 0)
        break;

      read_len = res;
      res = fuse_buf_write(dst,dst_off_,&tmp,0,read_len);
      if(res < 0)
        {
          if(!copied)
            return res;
          break;
        }
      if(res == 0)
        break;

      copied += res;

      if((size_t)res < this_len)
        break;

      dst_off_ += res;
      src_off_ += res;
      len_     -= res;
    }

  return copied;
}

#if defined(__linux__)
static
ssize_t
fuse_buf_splice(const struct fuse_buf   *dst,
                size_t                   dst_off_,
                const struct fuse_buf   *src,
                size_t                   src_off_,
                size_t                   len_,
                enum fuse_buf_copy_flags flags_)
{
  int splice_flags = 0;
  off_t *srcpos = nullptr;
  off_t *dstpos = nullptr;
  off_t srcpos_val;
  off_t dstpos_val;
  ssize_t res;
  size_t copied = 0;

  if(flags_ & FUSE_BUF_SPLICE_MOVE)
    splice_flags |= SPLICE_F_MOVE;
  if(flags_ & FUSE_BUF_SPLICE_NONBLOCK)
    splice_flags |= SPLICE_F_NONBLOCK;

  if(src->flags & FUSE_BUF_FD_SEEK)
    {
      srcpos_val = src->pos + src_off_;
      srcpos = &srcpos_val;
    }
  if(dst->flags & FUSE_BUF_FD_SEEK)
    {
      dstpos_val = dst->pos + dst_off_;
      dstpos = &dstpos_val;
    }

  while(len_)
    {
      res = splice(src->fd,srcpos,dst->fd,dstpos,len_,splice_flags);
      if(res == -1)
        {
          if(copied)
            break;

          if(errno != EINVAL || (flags_ & FUSE_BUF_FORCE_SPLICE))
            return -errno;

          /* Maybe splice is not supported for this combination */
          return fuse_buf_fd_to_fd(dst,dst_off_,src,src_off_,len_);
        }
      if(res == 0)
        break;

      copied += res;
      if(!(src->flags & FUSE_BUF_FD_RETRY) &&
         !(dst->flags & FUSE_BUF_FD_RETRY))
        {
          break;
        }

      /* The kernel advances off_in and off_out in place on every
         splice() call; do NOT advance srcpos_val/dstpos_val here
         (upstream semantics - a manual advance double-steps and
         silently skips source data across loop iterations). */
      len_ -= res;
    }

  return copied;
}
#endif /* defined(__linux__) */

static
ssize_t
fuse_buf_copy_one(const struct fuse_buf   *dst,
                  size_t                   dst_off_,
                  const struct fuse_buf   *src,
                  size_t                   src_off_,
                  size_t                   len_,
                  enum fuse_buf_copy_flags flags_)
{
  bool src_is_fd = (src->flags & FUSE_BUF_IS_FD) != 0;
  bool dst_is_fd = (dst->flags & FUSE_BUF_IS_FD) != 0;

  if(!src_is_fd && !dst_is_fd)
    {
      char *dstmem = (char*)dst->mem + dst_off_;
      char *srcmem = (char*)src->mem + src_off_;

      if(dstmem != srcmem)
        {
          if(dstmem + len_ <= srcmem || srcmem + len_ <= dstmem)
            memcpy(dstmem,srcmem,len_);
          else
            memmove(dstmem,srcmem,len_);
        }

      return len_;
    }
  else if(!src_is_fd)
    {
      return fuse_buf_write(dst,dst_off_,src,src_off_,len_);
    }
  else if(!dst_is_fd)
    {
      return fuse_buf_read(dst,dst_off_,src,src_off_,len_);
    }
  else if(flags_ & FUSE_BUF_NO_SPLICE)
    {
      return fuse_buf_fd_to_fd(dst,dst_off_,src,src_off_,len_);
    }
  else
    {
#if defined(__linux__)
      return fuse_buf_splice(dst,dst_off_,src,src_off_,len_,flags_);
#else
      /* splice(2)/SPLICE_F_MOVE/SPLICE_F_NONBLOCK are Linux-only;
         other platforms always take the portable bounce-copy path
         (same behavior fuse_buf_splice itself falls back to on
         Linux when splice(2) returns EINVAL). */
      return fuse_buf_fd_to_fd(dst,dst_off_,src,src_off_,len_);
#endif
    }
}

static
const struct fuse_buf*
fuse_bufvec_current(struct fuse_bufvec *bufv_)
{
  if(bufv_->idx < bufv_->count)
    return &bufv_->buf[bufv_->idx];
  else
    return nullptr;
}

static
bool
fuse_bufvec_advance(struct fuse_bufvec *bufv_,
                    size_t              len_)
{
  const struct fuse_buf *buf = fuse_bufvec_current(bufv_);

  bufv_->off += len_;
  assert(bufv_->off <= buf->size);
  if(bufv_->off == buf->size)
    {
      assert(bufv_->idx < bufv_->count);
      bufv_->idx++;
      if(bufv_->idx == bufv_->count)
        return false;
      bufv_->off = 0;
    }
  return true;
}

ssize_t
fuse_buf_copy(struct fuse_bufvec  *dstv_,
              struct fuse_bufvec  *srcv_,
              enum fuse_buf_copy_flags flags_)
{
  size_t copied = 0;

  if(dstv_ == srcv_)
    return fuse_buf_size(dstv_);

  for(;;)
    {
      const struct fuse_buf *src = fuse_bufvec_current(srcv_);
      const struct fuse_buf *dst = fuse_bufvec_current(dstv_);
      size_t src_len;
      size_t dst_len;
      size_t len;
      ssize_t res;

      if(src == nullptr || dst == nullptr)
        break;

      src_len = src->size - srcv_->off;
      dst_len = dst->size - dstv_->off;
      len = std::min(src_len,dst_len);

      res = fuse_buf_copy_one(dst,dstv_->off,src,srcv_->off,len,flags_);
      if(res < 0)
        {
          if(!copied)
            return res;
          break;
        }
      copied += res;

      if(!fuse_bufvec_advance(srcv_,res) ||
         !fuse_bufvec_advance(dstv_,res))
        break;

      if((size_t)res < len)
        break;
    }

  return copied;
}

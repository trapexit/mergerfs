/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "ef.hpp"
#include "errno.hpp"
#include "fmt/core.h"
#include "fs_inode.hpp"
#include "wyhash.h"

#include <cstdint>
#include <string>

#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

typedef uint64_t (*inodefunc_t)(const std::string&,const char*,const uint64_t,const mode_t,const dev_t,const ino_t);

static uint64_t hybrid_hash(const std::string&,const char*,const uint64_t,const mode_t,const dev_t,const ino_t);

static inodefunc_t g_func = hybrid_hash;


static
uint32_t
h64_to_h32(uint64_t h_)
{
  return (h_ - (h_ >> 32));
}

static
uint64_t
passthrough(const std::string &basepath_,
            const char     *fusepath_,
            const uint64_t  fusepath_len_,
            const mode_t    mode_,
            const dev_t     dev_,
            const ino_t     ino_)
{
  return ino_;
}

static
uint64_t
path_hash(const std::string &basepath_,
          const char     *fusepath_,
          const uint64_t  fusepath_len_,
          const mode_t    mode_,
          const dev_t     dev_,
          const ino_t     ino_)
{
  return wyhash(fusepath_,
                fusepath_len_,
                fs::inode::MAGIC,
                _wyp);
}

static
uint64_t
path_hash32(const std::string &basepath_,
            const char     *fusepath_,
            const uint64_t  fusepath_len_,
            const mode_t    mode_,
            const dev_t     dev_,
            const ino_t     ino_)
{
  uint64_t h;

  h = path_hash(basepath_,
                fusepath_,
                fusepath_len_,
                mode_,
                dev_,
                ino_);

  return h64_to_h32(h);
}

static
uint64_t
devino_hash(const std::string &basepath_,
            const char     *fusepath_,
            const uint64_t  fusepath_len_,
            const mode_t    mode_,
            const dev_t     dev_,
            const ino_t     ino_)
{
  uint64_t buf[2];

  buf[0] = dev_;
  buf[1] = ino_;

  return wyhash((void*)&buf[0],
                sizeof(buf),
                fs::inode::MAGIC,
                _wyp);
}

static
uint64_t
devino_hash32(const std::string &basepath_,
              const char     *fusepath_,
              const uint64_t  fusepath_len_,
              const mode_t    mode_,
              const dev_t     dev_,
              const ino_t     ino_)
{
  uint64_t h;

  h = devino_hash(basepath_,
                  fusepath_,
                  fusepath_len_,
                  mode_,
                  dev_,
                  ino_);

  return h64_to_h32(h);
}

static
uint64_t
basepath_hash(const std::string &basepath_,
            const char     *fusepath_,
            const uint64_t  fusepath_len_,
            const mode_t    mode_,
            const dev_t     dev_,
            const ino_t     ino_)
{

  std::string buf = fmt::format("{}{}",ino_,basepath_);

  return wyhash(buf.c_str(),
              buf.length(),
              fs::inode::MAGIC,
              _wyp);
}

static
uint64_t
basepath_hash32(const std::string &basepath_,
              const char     *fusepath_,
              const uint64_t  fusepath_len_,
              const mode_t    mode_,
              const dev_t     dev_,
              const ino_t     ino_)
{
  uint64_t h;

  h = basepath_hash(basepath_,
                  fusepath_,
                  fusepath_len_,
                  mode_,
                  dev_,
                  ino_);

  return h64_to_h32(h);
}

static
uint64_t
hybrid_hash(const std::string &basepath_,
            const char     *fusepath_,
            const uint64_t  fusepath_len_,
            const mode_t    mode_,
            const dev_t     dev_,
            const ino_t     ino_)
{
  return (S_ISDIR(mode_) ?
          path_hash(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_) :
          devino_hash(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_));
}

static
uint64_t
hybrid_hash32(const std::string &basepath_,
              const char     *fusepath_,
              const uint64_t  fusepath_len_,
              const mode_t    mode_,
              const dev_t     dev_,
              const ino_t     ino_)
{
  return (S_ISDIR(mode_) ?
          path_hash32(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_) :
          devino_hash32(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_));
}

static
uint64_t
basehybrid_hash(const std::string &basepath_,
            const char     *fusepath_,
            const uint64_t  fusepath_len_,
            const mode_t    mode_,
            const dev_t     dev_,
            const ino_t     ino_)
{
  return (S_ISDIR(mode_) ?
          path_hash(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_) :
          basepath_hash(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_));
}

static
uint64_t
basehybrid_hash32(const std::string &basepath_,
              const char     *fusepath_,
              const uint64_t  fusepath_len_,
              const mode_t    mode_,
              const dev_t     dev_,
              const ino_t     ino_)
{
  return (S_ISDIR(mode_) ?
          path_hash32(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_) :
          basepath_hash32(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_));
}

namespace fs
{
  namespace inode
  {
    int
    set_algo(const std::string &algo_)
    {
      if(algo_ == "passthrough")
        g_func = passthrough;
      ef(algo_ == "path-hash")
        g_func = path_hash;
      ef(algo_ == "path-hash32")
        g_func = path_hash32;
      ef(algo_ == "devino-hash")
        g_func = devino_hash;
      ef(algo_ == "devino-hash32")
        g_func = devino_hash32;
      ef(algo_ == "hybrid-hash")
        g_func = hybrid_hash;
      ef(algo_ == "hybrid-hash32")
        g_func = hybrid_hash32;
      ef(algo_ == "basepath-hash")
        g_func = basepath_hash;
      ef(algo_ == "basepath-hash32")
        g_func = basepath_hash32;
      ef(algo_ == "basehybrid-hash")
        g_func = basehybrid_hash;
      ef(algo_ == "basehybrid-hash32")
        g_func = basehybrid_hash32;
      else
        return -EINVAL;

      return 0;
    }

    std::string
    get_algo(void)
    {
      if(g_func == passthrough)
        return "passthrough";
      if(g_func == path_hash)
        return "path-hash";
      if(g_func == path_hash32)
        return "path-hash32";
      if(g_func == devino_hash)
        return "devino-hash";
      if(g_func == devino_hash32)
        return "devino-hash32";
      if(g_func == hybrid_hash)
        return "hybrid-hash";
      if(g_func == hybrid_hash32)
        return "hybrid-hash32";
      if(g_func == basepath_hash)
        return "basepath-hash";
      if(g_func == basepath_hash)
        return "basepath-hash32";
      if(g_func == basehybrid_hash)
        return "basehybrid-hash";
      if(g_func == basehybrid_hash)
        return "basehybrid-hash32";

      return std::string();
    }

    uint64_t
    calc(const std::string &basepath_,
         const char     *fusepath_,
         const uint64_t  fusepath_len_,
         const mode_t    mode_,
         const dev_t     dev_,
         const ino_t     ino_)
    {
      return g_func(basepath_,fusepath_,fusepath_len_,mode_,dev_,ino_);
    }

    uint64_t
    calc(const std::string &basepath_,
         std::string const &fusepath_,
         const mode_t       mode_,
         const dev_t        dev_,
         const ino_t        ino_)
    {
      return calc(basepath_,
                  fusepath_.c_str(),
                  fusepath_.size(),
                  mode_,
                  dev_,
                  ino_);
    }

    void
    calc(const std::string &basepath_,
         const char     *fusepath_,
         const uint64_t  fusepath_len_,
         struct stat    *st_)
    {
      st_->st_ino = calc(basepath_,
                         fusepath_,
                         fusepath_len_,
                         st_->st_mode,
                         st_->st_dev,
                         st_->st_ino);
    }

    void
    calc(const std::string &basepath_,
         const char  *fusepath_,
         struct stat *st_)
    {
      calc(basepath_,fusepath_,strlen(fusepath_),st_);
    }

    void
    calc(const std::string &basepath_,
         const std::string &fusepath_,
         struct stat       *st_)
    {
      calc(basepath_,fusepath_.c_str(),fusepath_.size(),st_);
    }
  }
}

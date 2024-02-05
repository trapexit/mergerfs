#include "func_create_ff.hpp"

#include "fileinfo.hpp"
#include "fs_info.hpp"
#include "fs_open.hpp"
#include "ugid.hpp"
#include "func_open_create_utils.hpp"

#include <errno.h>


Func::CreateFF::CreateFF()
{

}

Func::CreateFF::~CreateFF()
{

}

namespace l
{
  static
  int
  create(Branches2        &branches_,
         char const       *fusepath_,
         mode_t const      mode_,
         mode_t const      umask_,
         fuse_file_info_t *ffi_)
  {
    int rv;
    fs::info_t info;
    Config::Read cfg;

    for(auto &tier : branches_)
      {
        for(auto &branch : tier)
          {
            if(branch.mode != +Branch2::Mode::RW)
              continue;
            rv = fs::info(branch.path,&info);
            if(rv == -1)
              continue;
            if(info.readonly)
              continue;
            if(info.spaceavail < branch.min_free_space)
              continue;

            ghc::filesystem::path fullpath;

            fullpath = branch.path / fusepath_;

            rv = fs::open(fullpath,ffi_->flags,mode_);
            if((rv == -1) && (errno == EROFS))
              {
                branch.mode = Branch2::Mode::RO;
                continue;
              }

            FileInfo *fi = new FileInfo(rv,fusepath_,ffi_->direct_io);
            
            ffi_->fh = reinterpret_cast<uint64_t>(fi);

            return 0;
          }
      }

    return rv;
  }
}

int
Func::CreateFF::operator()(Branches2        &branches_,
                           char const       *fusepath_,
                           mode_t const      mode_,
                           fuse_file_info_t *ffi_)
{
  int rv;
  Config::Read cfg;
  fuse_context const *fc = fuse_get_context();
  ugid::Set const ugid(fc->uid,fc->gid);

  utils::cfg_to_ffi_flags(cfg,fc->pid,ffi_);
  
  rv = l::create(branches_,fusepath_,mode_,fc->umask,ffi_);

  return rv;
}

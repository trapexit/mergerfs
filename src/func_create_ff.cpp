#include "func_create_ff.hpp"

#include "fileinfo.hpp"
#include "fs_acl.hpp"
#include "fs_info.hpp"
#include "fs_open.hpp"
#include "func_open_create_utils.hpp"
#include "ugid.hpp"

#include <errno.h>


Func2::CreateFF::CreateFF()
{

}

Func2::CreateFF::~CreateFF()
{

}

namespace l
{
  static
  int
  create(Branches2        &branches_,
         fs::Path const   &fusepath_,
         mode_t const      mode_,
         mode_t const      umask_,
         fuse_file_info_t *ffi_)
  {
    int rv;
    mode_t mode;
    FileInfo *fi;
    fs::info_t info;
    fs::Path fullpath;

    mode = mode_;
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

            fullpath = branch.path / fusepath_;

            if(!fs::acl::dir_has_defaults(fullpath))
              mode &= ~umask_;

            rv = fs::open(fullpath,ffi_->flags,mode);
            if((rv == -1) && (errno == ENOENT))
              {
                branches_.clonepath(branch.path,fusepath_);
                rv = fs::open(fullpath,ffi_->flags,mode);
              }
            if((rv == -1) && (errno == EROFS))
              {
                branch.mode = Branch2::Mode::RO;
                continue;
              }

            fi = new FileInfo(rv,fusepath_.string().c_str(),ffi_->direct_io);

            ffi_->fh = reinterpret_cast<uint64_t>(fi);

            return 0;
          }
      }

    return rv;
  }
}

int
Func2::CreateFF::operator()(Branches2                   &branches_,
                            ghc::filesystem::path const &fusepath_,
                            mode_t const                 mode_,
                            fuse_file_info_t            *ffi_)
{
  int rv;
  Config::Read cfg;
  fuse_context const *fc = fuse_get_context();
  ugid::Set const ugid(fc->uid,fc->gid);

  utils::cfg_to_ffi_flags(cfg,fc->pid,ffi_);
  if(cfg->writeback_cache)
    utils::tweak_flags_writeback_cache(&ffi_->flags);
  ffi_->noflush = !utils::calculate_flush(cfg->flushonclose,ffi_->flags);

  rv = l::create(branches_,fusepath_,mode_,fc->umask,ffi_);

  return rv;
}

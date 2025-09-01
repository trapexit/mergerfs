#include "mfm_options.hpp"

#include "str.hpp"
#include "fs_clonepath.hpp"
#include "fs_copyfile.hpp"

#include "fmt/core.h"

#include <filesystem>


namespace stdfs = std::filesystem;

namespace mfm
{
  void dup(const Opts::Dup &opts);
}

void
mfm::dup(const Opts::Dup &opts_)
{
  std::vector<std::filesystem::path> srcpaths;

  // TODO: expand if mergerfs mount
  srcpaths = opts_.paths;

  for(const auto &srcpath : srcpaths)
    {
      if(stdfs::is_regular_file(srcpath))
        {
        }
      else if(stdfs::is_directory(srcpath))
        {
          auto dir_opts = (stdfs::directory_options::follow_directory_symlink |
                           stdfs::directory_options::skip_permission_denied);

          for(const stdfs::directory_entry &de :
                stdfs::recursive_directory_iterator(srcpath,dir_opts))
            {
              if(!str::startswith(de.path().filename().string(),".dup_"))
                continue;

              for(const auto &de :
                    stdfs::directory_iterator(de.path().parent_path()))
                {
                  auto relpath = stdfs::relative(de.path(),srcpath);

                  for(const auto &dstpath : srcpaths)
                    {
                      if(dstpath == srcpath)
                        continue;

                      fmt::println("copy {} to {} : {} : {}",
                                   srcpath.string(),
                                   dstpath.string(),
                                   relpath.parent_path().string(),
                                   relpath.filename().string());

                      fs::clonepath(srcpath,
                                    dstpath,
                                    relpath.parent_path());
                      fs::copyfile(srcpath / relpath,
                                   dstpath / relpath,
                                   {.cleanup_failure=false});
                    }
                }
            }
        }
      else
        {
          //throw std::runtime_error("invalid file type");
          fmt::println("invalid file type: {}",srcpath.string());
        }
    }
}

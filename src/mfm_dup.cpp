#include "mfm_options.hpp"

#include "str.hpp"

#include "fmt/core.h"

#include <filesystem>



namespace fs = std::filesystem;

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
      if(fs::is_regular_file(srcpath))
        {
        }
      else if(fs::is_directory(srcpath))
        {
          auto dir_opts = (fs::directory_options::follow_directory_symlink |
                           fs::directory_options::skip_permission_denied);

          for(const fs::directory_entry &de :
                fs::recursive_directory_iterator(srcpath,dir_opts))
            {
              if(!str::startswith(de.path().filename().string(),".dup_"))
                continue;

              for(const auto &de :
                    fs::directory_iterator(de.path().parent_path()))
                {
                  auto relpath = fs::relative(de.path(),srcpath);
                  fmt::println("{} {}",
                               relpath.string(),
                               de.path().filename().string());

                  for(const auto &dstpath : srcpaths)
                    {
                      if(dstpath == srcpath)
                        continue;

                      fmt::println("copy {} to {} : {}",
                                   srcpath.string(),
                                   dstpath.string(),
                                   relpath.string());
                    }
                }
            }
        }
      else
        {
          //throw std::runtime_error("invalid file type");
          fmt::println("invalid file type: {}",path.string());
        }
    }
}

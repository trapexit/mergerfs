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

  for(const auto &path : srcpaths)
    {
      if(fs::is_regular_file(path))
        {
        }
      else if(fs::is_directory(path))
        {
          auto dir_opts = (fs::directory_options::follow_directory_symlink |
                           fs::directory_options::skip_permission_denied);

          for(const fs::directory_entry &de :
                fs::recursive_directory_iterator(path,dir_opts))
            {
              if(!str::startswith(de.path().filename().string(),".dup_"))
                continue;

              for(const auto &de :
                    fs::directory_iterator(de.path().parent_path()))
                {
                  auto relpath = fs::relative(de.path(),path);
                  fmt::println("{} {}",
                               relpath.string(),
                               de.path().filename().string());

                  for(const auto &dstpath : paths)
                    {
                      if(dstpath == path)
                        continue;

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

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
  std::vector<std::filesystem::path> paths;

  paths = opts_.paths;

  for(const auto &path : paths)
    {
      if(fs::is_regular_file(opts_.path))
        {
        }
      else if(fs::is_directory(opts_.path))
        {
          auto dir_opts = (fs::directory_options::follow_directory_symlink |
                           fs::directory_options::skip_permission_denied);

          for(const fs::directory_entry &de :
                fs::recursive_directory_iterator(opts_.path,dir_opts))
            {
              if(!str::startswith(de.path().filename().string(),".dup_"))
                continue;

              for(const auto &de :
                    fs::directory_iterator(de.path().parent_path()))
                {
                  fmt::println("{}",de.path().filename().string());
                }
            }
        }
      else
        {
          throw std::runtime_error("invalid file type");
        }
    }

  return 0;
}

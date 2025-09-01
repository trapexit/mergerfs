#include "mfm_options.hpp"

#include <filesystem>

#include "fmt/core.h"

namespace fs = std::filesystem;

namespace mfm
{
  void dup(const Opts::Dup &opts);
}

void
mfm::dup(const Opts::Dup &opts_)
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
          if(!de.is_directory())
            continue;

          std::error_code ec;
          auto filepath = de.path() / ".dup_2";
          if(!fs::exists(filepath,ec))
            continue;

          fmt::println("{}:",filepath.string());
          for(const fs::directory_entry &de :
                fs::directory_iterator(de.path()))
            {
              fmt::println("{}",de.path().string());
            }
        }
    }
  else
    {
      throw std::runtime_error("invalid file type");
    }
}

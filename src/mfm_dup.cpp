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
          if(!str::startswith(de.path().string(),".dup_"))
            continue;

          fmt::println("{} {}",
                       opts_.path.string(),
                       de.path().string());
          continue;
        }
    }
  else
    {
      throw std::runtime_error("invalid file type");
    }
}

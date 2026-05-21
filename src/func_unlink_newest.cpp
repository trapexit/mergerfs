#include "func_unlink_newest.hpp"
#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_unlink.hpp"
#include "timespec_utils.hpp"

std::string_view Func2::UnlinkNewest::name() const { return "newest"; }

int
Func2::UnlinkNewest::operator()(const Branches &branches_,
                                const fs::path &fusepath_)
{
  const Branch *chosen = nullptr;
  timespec newest{};
  struct stat st;
  for(const auto &branch : branches_)
    {
      if(branch.ro()) continue;
      if(!fs::exists(branch.path,fusepath_,&st))
        continue;
      if(chosen && (st.st_mtim < newest))
        continue;
      newest = st.st_mtim;
      chosen = &branch;
    }
  if(chosen == nullptr)
    return -ENOENT;
  return fs::unlink(chosen->path / fusepath_);
}

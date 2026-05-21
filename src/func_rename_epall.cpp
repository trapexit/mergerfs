#include "func_rename_epall.hpp"

#include "errno.hpp"
#include "fs_clonepath.hpp"
#include "fs_exists.hpp"
#include "fs_remove.hpp"
#include "fs_rename.hpp"


std::string_view
Func2::RenameEPAll::name() const
{
  return "epall";
}

int
Func2::RenameEPAll::operator()(const Branches &branches_,
                               const fs::path &oldfusepath_,
                               const fs::path &newfusepath_)
{
  // Snapshot the Branches::Impl once; iterating the live Branches each
  // time would release the shared_ptr at iterator destruction and could
  // leave Branch* pointers dangling if cfg.branches is mutated
  // concurrently.
  Branches::Ptr branches = branches_;
  int err;
  bool found;
  StrVec toremove;
  fs::path oldfullpath;
  fs::path newfullpath;
  const fs::path newfusedirpath = newfusepath_.parent_path();
  const Branch *clone_src = nullptr;

  err   = 0;
  found = false;
  for(auto &branch : *branches)
    {
      if(branch.ro())
        continue;

      oldfullpath = branch.path / oldfusepath_;
      newfullpath = branch.path / newfusepath_;

      if(!fs::exists(branch.path,oldfusepath_))
        {
          toremove.push_back(newfullpath);
          continue;
        }

      int rv = fs::rename(oldfullpath,newfullpath);
      if(rv == -ENOENT)
        {
          if(clone_src == nullptr)
            {
              for(auto &b : *branches)
                {
                  if(fs::exists(b.path,newfusedirpath))
                    {
                      clone_src = &b;
                      break;
                    }
                }
            }

          if(clone_src != nullptr)
            {
              rv = fs::clonepath(clone_src->path,branch.path,newfusedirpath);
              if(rv >= 0)
                rv = fs::rename(oldfullpath,newfullpath);
            }
        }

      found = true;
      err   = rv;
    }

  if(!found)
    return -ENOENT;

  if(err == 0)
    {
      for(const auto &path : toremove)
        fs::remove(path);
    }

  return err;
}

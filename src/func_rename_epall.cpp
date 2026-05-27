#include "func_rename_epall.hpp"

#include "clone_source.hpp"
#include "config.hpp"
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
            clone_src = CloneSource::find(*branches,
                                          newfusedirpath,
                                          cfg.getattr.to_string());

          if(clone_src != nullptr)
            {
              rv = fs::clonepath(clone_src->path,branch.path,newfusedirpath);
              if(rv >= 0)
                rv = fs::rename(oldfullpath,newfullpath);
            }
        }

      // If rename failed on this branch but the source still exists, queue
      // the stale source for removal on overall success so the file is not
      // left at both old and new paths.
      if(rv < 0)
        toremove.push_back(oldfullpath);

      if(!found)
        { err = rv; found = true; continue; }
      if(rv == 0)
        { err = 0; continue; }
      if(err == 0)
        continue;
      err = rv;
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

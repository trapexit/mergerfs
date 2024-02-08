#include "branches2.hpp"

#include "fs_clonepath.hpp"

#include "fmt/core.h"

#include <iostream>

#include <errno.h>

Branches2::Branches2()
{

}

Branches2::Branches2(toml::value const &v_)
{
  auto const &tiers = toml::find(v_,"tier");

  for(auto const &tier : tiers.as_array())
    {
      _branches.emplace_back(tier);
    }
}

void
Branches2::copy_enabled_rw(Branches2 &b_) const
{
  if(b_._branches.size() < _branches.size())
    b_._branches.resize(_branches.size());
  
  for(auto &bt : )
    {
      BranchTier newbt;

      bt.copy_enabled_rw(newbt);
    }
}

int
Branches2::clonepath(ghc::filesystem::path basepath_,
                     ghc::filesystem::path relpath_)
{
  int rv;

  for(auto const &tier : _branches)
    {
      for(auto const &branch : tier)
        {
          if(branch.path == basepath_)
            continue;
          rv = fs::clonepath(branch.path,basepath_,relpath_);
          if(rv < 0)
            continue;
          return 0;
        }
    }

  errno = ENOENT;
  return -1;
}

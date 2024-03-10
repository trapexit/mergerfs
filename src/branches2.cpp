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

  min_free_space = toml::find_or(v_,"min-free-space",0);
  for(auto const &tier : tiers.as_array())
    {
      bool enabled;

      enabled = toml::find_or(tier,"enabled",false);
      if(!enabled)
        continue;
      
      _branches.emplace_back(tier,min_free_space);
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

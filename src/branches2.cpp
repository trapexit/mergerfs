#include "branches2.hpp"

#include "fmt/core.h"

#include <iostream>

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
          if(rv == 0)
            return 0;
        }
    }

  return 0;
}

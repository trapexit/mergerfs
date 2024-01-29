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
      auto const &branches = toml::find(tier,"branch");
      
      for(auto const &branch : branches.as_array())
        {
          auto const &branch_table = branch.as_table();

          fmt::print("{}\n",
                     branch_table.at("type").as_string().str());
        }
    }
}

#include "branches2.hpp"

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

          if(branch_table["type"].as_string() == "literal")
        }
    }
}

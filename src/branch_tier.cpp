#include "branch_tier.hpp"

#include "fmt/core.h"

BranchTier::BranchTier()
{

}

BranchTier::BranchTier(toml::value const &v_)
{
  auto const &branches = toml::find(v_,"branch").as_array();

  for(auto const &branch : branches)
    {
      auto const &table = branch.as_table();
      std::string type;

      type = table.at("type").as_string();
      if(type == "literal")
        ;
      else if(type == "glob")
        ;
      else if(type == "scan")
        ;

      fmt::print("{}\n",type);
    }
}

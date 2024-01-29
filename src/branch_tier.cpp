#include "branch_tier.hpp"

#include "fmt/core.h"

namespace l
{
  static
  void
  load_literal_branch(toml::table const &v_)
  {
    
  }
}

BranchTier::BranchTier()
{

}

BranchTier::BranchTier(toml::value const &v_)
{
  auto const &branches = toml::find(v_,"branch").as_array();

  for(auto const &branch : branches)
    {
      std::string type;      
      auto const &table = branch.as_table();

      type = table.at("type").as_string();
      if(type == "literal")
        l::load_literal_branch(table);
      else if(type == "glob")
        ;
      else if(type == "scan")
        ;

      fmt::print("{}\n",type);
    }
}

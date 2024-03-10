#include "branch_tier.hpp"

#include "fmt/core.h"

#include "fs_glob.hpp"

#include <cassert>


namespace l
{
  static
  void
  load_branch_literal(toml::table const    &v_,
                      std::vector<Branch2> &branches_)
  {
    branches_.emplace_back(v_);
  }

  static
  void
  load_branch_glob(toml::table const    &v_,
                   std::vector<Branch2> &branches_)
  {
    toml::table table;
    std::string pattern;
    std::vector<std::string> paths;

    pattern = v_.at("path").as_string();
    fs::glob(pattern,&paths);

    table = v_;
    branches_.resize(branches_.size() + paths.size());
    for(auto &path : paths)
      {
        table["path"] = path;
        branches_.emplace_back(table);
      }
  }

  static
  void
  load_branch_scan(toml::table const    &v_,
                   std::vector<Branch2> &branches_)
  {
    assert(("not currently supported",0));
  }
}

BranchTier::BranchTier()
{

}

BranchTier::BranchTier(toml::value const &v_
                       uint64_t const     min_free_space_)
{
  uint64_t min_free_space;
  auto const &branches = toml::find(v_,"branch").as_array();

  min_free_space = toml::find_or(v_,"min-free-space",min_free_space_);
  for(auto const &branch : branches)
    {
      bool enabled;
      std::string type;
      auto const &table = branch.as_table();

      enabled = toml::find_or(branch,"enabled",false);
      if(!enabled)
        continue;

      type = table.at("type").as_string();
      if(type == "literal")
        l::load_branch_literal(table,min_free_space_,_branches);
      else if(type == "glob")
        l::load_branch_glob(table,min_free_space_,_branches);
      else if(type == "scan")
        l::load_branch_scan(table,min_free_space_,_branches);
    }
}

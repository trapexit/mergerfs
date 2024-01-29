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

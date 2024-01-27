#include "branches2.hpp"

#include <iostream>

Branches2::Branches2()
{

}

Branches2::Branches2(toml::value const &v_)
{
  auto const &tiers = toml::find(v_,"tier");

  for(auto const &branch : branches.as_array())
    {
    }
}

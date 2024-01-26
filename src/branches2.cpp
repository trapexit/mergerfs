#include "branches2.hpp"

#include <iostream>

Branches2::Branches2()
{

}

Branches2::Branches2(toml::value const &v_)
{
  auto const &branches = toml::find(v_,"branch");

  for(auto const &branch : branches.as_array())
    {
    }
}

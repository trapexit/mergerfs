#include "branch2.hpp"


Branch2::Branch2(toml::value const &v_)
{
  enabled = v_.at("enabled").as_boolean();
}

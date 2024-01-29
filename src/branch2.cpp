#include "branch2.hpp"


Branch2::Branch2()
  : mode(Mode::RW)
{

}

Branch2::Branch2(toml::value const &v_)
  : mode(Mode::RW)
{
  enabled = v_.at("enabled").as_boolean();
  mode    = Mode::RO;
  //  mode    = v_.at("mode").as_string();
}

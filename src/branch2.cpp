#include "branch2.hpp"


Branch2::Branch2()
  : mode(Mode::RW)
{

}

Branch2::Branch2(toml::value const &v_)
  : mode(Mode::RW)
{
  enabled        = v_.at("enabled").as_boolean();
  mode           = Mode::_from_string(toml::find<std::string>(v_,"mode").c_str());
  min_free_space = 0;
  path           = toml::find<std::string>(v_,"path");
  printf("%s\n",path.string().c_str());
}

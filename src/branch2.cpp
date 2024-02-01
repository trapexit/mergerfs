#include "branch2.hpp"

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


Branch2::Branch2()
  : mode(Mode::RW)
{

}

Branch2::Branch2(toml::value const &v_)
  : mode(Mode::RW)
{
  std::cout << v_ << '\n';

  enabled        = v_.at("enabled").as_boolean();
  mode           = Mode::_from_string(toml::find<std::string>(v_,"mode").c_str());
  min_free_space = 0;
  path           = toml::find<std::string>(v_,"path");

  int const flags = O_DIRECTORY | O_PATH | O_NOATIME;
  fd = openat(AT_FDCWD,path.string().c_str(),flags);
}

Branch2::~Branch2()
{
  if(fd >= 0)
    close(fd);
}

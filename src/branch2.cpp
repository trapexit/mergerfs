#include "branch2.hpp"

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


Branch2::Branch2()
  : enabled(false),
    mode(Mode::RW),
    min_free_space(0),
    fd(-1),
    path()
{

}

// Branch2::Branch2(Branch2 &v_)
//   : enabled(v_.enabled),
//     mode(v_.mode),
//     min_free_space(v_.min_free_space),
//     fd(v_.fd),
//     path(v_.path)
// {

// }

Branch2::Branch2(Branch2 &&v_)
  : enabled(v_.enabled),
    mode(v_.mode),
    min_free_space(v_.min_free_space),
    fd(v_.fd),
    path(std::move(v_.path))
{
  v_.fd = -1;
}

Branch2::Branch2(toml::value const &v_)
  : mode(Mode::RW)
{
  enabled        = v_.at("enabled").as_boolean();
  mode           = Mode::_from_string(toml::find<std::string>(v_,"mode").c_str());
  min_free_space = toml::find_or<uint64_t>(v_,"min-free-space",0);
  path           = toml::find<std::string>(v_,"path");

  int const flags = O_DIRECTORY | O_PATH | O_NOATIME;
  fd = openat(AT_FDCWD,path.string().c_str(),flags);
  if(fd < 0)
    enabled = false;
}

Branch2::~Branch2()
{
  if(fd >= 0)
    close(fd);
}

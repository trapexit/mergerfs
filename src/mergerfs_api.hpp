#pragma once

#include <string>
#include <vector>


namespace mergerfs
{
  namespace api
  {
    int
    basepath(const std::string &path,
             std::string       &basepath);
    int
    relpath(const std::string &path,
            std::string       &relpath);
    int
    fullpath(const std::string &path,
             std::string       &fullpath);
    int
    allpaths(const std::string        &path,
             std::vector<std::string> &paths);
  }
}

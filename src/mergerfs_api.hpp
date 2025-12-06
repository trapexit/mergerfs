#pragma once

#include "fs_path.hpp"

#include <map>
#include <string>
#include <vector>


namespace mergerfs
{
  namespace api
  {
    bool
    is_mergerfs(const fs::path &path);

    int
    get_kvs(const fs::path                    &mountpoint,
            std::map<std::string,std::string> *kvs);

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

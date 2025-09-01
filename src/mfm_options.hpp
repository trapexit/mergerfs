#pragma once

#include "int_types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace mfm
{
  struct Opts
  {
    struct Dup
    {
      std::vector<std::filesystem::path> paths;
    } dup;

    struct Watch
    {
      std::filesystem::path path;
    } watch;

    struct Migrate
    {
      std::vector<std::filesystem::path> src;
      std::vector<std::filesystem::path> dst;
    } migrate;

    struct Rebalance
    {
      std::vector<std::filesystem::path> dirpaths;
    } rebalance;
  };
};

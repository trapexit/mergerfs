#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include <filesystem>


class State
{
public:
  boost::concurrent_flat_map<std::filesystem::path,int> passthrough;
};

extern State state;

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include <filesystem>
#include <atomic>

#include "int_types.h"

struct PassthroughDetails
{
  std::atomic<u64>
};

class State
{
public:
  boost::concurrent_flat_map<std::filesystem::path,int> passthrough;
};

extern State state;

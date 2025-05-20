#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include <filesystem>
#include <atomic>

#include "int_types.h"

struct PassthroughDetails
{
  u64 ref_count;
  int backing_id;
};

class State
{
public:
  using PassthroughDetailMap = boost::concurrent_flat_map<std::filesystem::path,PassthroughDetails>;
  using BranchUsageMap = boost::concurrent_flat_map<std::filesystem::path,u64>;


public:
  PassthroughDetailMap passthrough;
};

extern State state;

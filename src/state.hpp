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
  using PassthroughDetailMap = boost::concurrent_flat_map<std::filesystem::path,
                                                          PassthroughDetails>;

public:
  PassthroughDetailMap passthrough;
  BranchUsageMap branch_usage;
};

extern State state;

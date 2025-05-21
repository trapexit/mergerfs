#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include <filesystem>
#include <mutex>

#include "int_types.h"

struct PassthroughDetails
{
  u64 ref_count;
  int backing_id;
  std::unique_ptr<std::mutex> mutex;
};

class State
{
public:
  // fusepath -> PassthroughDetails
  using PassthroughDetailMap = boost::concurrent_flat_map<std::filesystem::path,
                                                          PassthroughDetails>;

public:
  PassthroughDetailMap passthrough;
};

extern State state;

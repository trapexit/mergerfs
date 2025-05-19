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
  boost::concurrent_flat_map<std::filesystem::path,PassthroughDetails> passthrough;
};

extern State state;

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "branch.hpp"

#include <filesystem>
#include <mutex>

#include "int_types.h"

struct PassthroughDetails
{
  PassthroughDetails()
  {
  }

  PassthroughDetails(const u64 ref_count_,
                     const int backing_id_)
    : ref_count(ref_count_),
      backing_id(backing_id_)
  {
  }

  u64 ref_count;
  int fd;
  int backing_id;
  Branch branch;
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

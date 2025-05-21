#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include <filesystem>
#include <mutex>

#include "int_types.h"

struct PassthroughDetails
{
  PassthroughDetails()
    : ref_count(0),
      backing_id(-1),
      mutex()
  {
  }

  PassthroughDetails(const int ref_count_,
                     const int backing_id_)
    : ref_count(ref_count_),
      backing_id(backing_id_),
      mutex(std::make_unique<std::mutex>())
  {
  }

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

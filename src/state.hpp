#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "branch.hpp"

#include <filesystem>
#include <mutex>

#include "int_types.h"

struct PassthroughDetails
{
  PassthroughDetails()
    : ref_count(0),
      fd(-1),
      backing_id(-1)
  {
  }

  FileInfo *fi;
  int ref_count;
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

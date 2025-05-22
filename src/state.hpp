#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "branch.hpp"
#include "fileinfo.hpp"

#include <filesystem>
#include <mutex>

#include "int_types.h"

struct PassthroughDetails
{
  PassthroughDetails()
    : ref_count(0),
      backing_id(-1),
      fi(nullptr)
  {
  }

  int ref_count;
  int backing_id;
  FileInfo *fi;
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

#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "fileinfo.hpp"

#include <filesystem>
#include <mutex>

constexpr int INVALID_BACKING_ID = -1;

struct PassthroughDetails
{
  PassthroughDetails()
    : ref_count(0),
      backing_id(INVALID_BACKING_ID),
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

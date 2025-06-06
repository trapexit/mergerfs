#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "fileinfo.hpp"


constexpr int INVALID_BACKING_ID = -1;

class State
{
public:
  struct OpenFile
  {
    OpenFile()
      : ref_count(0),
        backing_id(INVALID_BACKING_ID),
        fi(nullptr)
    {
    }

    int ref_count;
    int backing_id;
    FileInfo *fi;
  };


public:
  using OpenFileMap = boost::concurrent_flat_map<u64,OpenFile>;

public:
  OpenFileMap open_files;
};

extern State state;

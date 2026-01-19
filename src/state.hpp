#pragma once

#include "boost/unordered/concurrent_flat_map.hpp"

#include "fileinfo.hpp"

#include "fuse.h"

#include <functional>
#include <map>
#include <string>


class State
{
public:
  State();

public:
  struct OpenFile
  {
    OpenFile()
      : ref_count(0),
        backing_id(INVALID_BACKING_ID),
        fi(nullptr)
    {
    }

    OpenFile(const int        backing_id_,
             FileInfo * const fi_)
      : ref_count(1),
        backing_id(backing_id_),
        fi(fi_)
    {
    }

    int ref_count;
    int backing_id;
    FileInfo *fi;
  };

public:
  struct GetSet
  {
    std::function<std::string()> get;
    std::function<int(const std::string_view)> set;
    std::function<int(const std::string_view)> valid;
  };

  void set_getset(const std::string   &name,
                  const State::GetSet &gs);

  int get(const std::string &key,
          std::string       &val);
  int set(const std::string      &key,
          const std::string_view  val);
  int valid(const std::string      &key,
            const std::string_view  val);

public:
  using OpenFileMap = boost::concurrent_flat_map<u64,OpenFile>;

private:
  std::map<std::string,GetSet> _getset;

public:
  OpenFileMap open_files;
};

extern State state;

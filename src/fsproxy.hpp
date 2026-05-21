#pragma once

#include <unordered_map>

#include <fcntl.h>
#include <unistd.h>

class FSProxy
{
public:
  struct UGID
  {
    bool
    operator==(const UGID &other_) const
    {
      return ((uid == other_.uid) &&
              (gid == other_.gid));
    }

    uid_t uid;
    gid_t gid;
  };

  struct UGIDHash
  {
    size_t
    operator()(const UGID &key_) const
    {
      size_t h0 = std::hash<uid_t>{}(key_.uid);
      size_t h1 = std::hash<uid_t>{}(key_.gid);

      return (h0 ^ (h1 << 1));
    }
  };

  struct Process
  {
    int pid;
    int req_pipe[2];
    int res_pipe[2];
  };

public:
  int create(uid_t uid,
             gid_t gid,
             const char *pathname,
             mode_t mode);

private:
  int _spawn_proxy_if_needed(uid_t,gid_t);

private:
  std::unordered_map<UGID,Process,UGIDHash> _proxies;
};

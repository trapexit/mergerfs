#include "fs_mount.hpp"

#include "subprocess.hpp"


int
fs::mount(const std::string &tgt_)
{
  return subprocess::call({"mount",tgt_});
}

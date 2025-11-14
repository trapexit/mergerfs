#include "func_readlink_base.hpp"

namespace Func2
{
  class ReadlinkFF : public ReadlinkBase
  {
  public:
    ReadlinkFF() {}
    ~ReadlinkFF() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   char           *buf,
                   const size_t    bufsize,
                   const time_t    symlinkify_timeout);
  };
}

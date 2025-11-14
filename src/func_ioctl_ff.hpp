#include "func_ioctl_base.hpp"

namespace Func2
{
  class IoctlFF : public IoctlBase
  {
  public:
    IoctlFF() {}
    ~IoctlFF() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath);
  };
}

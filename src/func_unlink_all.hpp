#include "func_unlink_base.hpp"

namespace Func2
{
  class UnlinkAll : public UnlinkBase
  {
  public:
    UnlinkAll() {}
    ~UnlinkAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath);
  };
}

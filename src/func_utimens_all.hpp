#include "func_utimens_base.hpp"

namespace Func2
{
  class UtimensAll : public UtimensBase
  {
  public:
    UtimensAll() {}
    ~UtimensAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const timespec  times_[2]);
  };
}

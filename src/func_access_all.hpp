#include "func_access_base.hpp"

namespace Func2
{
  class AccessAll : public AccessBase
  {
  public:
    AccessAll() {}
    ~AccessAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const int       mode);
  };
}

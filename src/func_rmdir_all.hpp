#include "func_rmdir_base.hpp"

namespace Func2
{
  class RmdirAll : public RmdirBase
  {
  public:
    RmdirAll() {}
    ~RmdirAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath);
  };
}

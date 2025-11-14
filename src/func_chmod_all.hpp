#include "func_chmod_base.hpp"

namespace Func2
{
  class ChmodAll : public ChmodBase
  {
  public:
    ChmodAll() {}
    ~ChmodAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const mode_t    mode);
  };
}

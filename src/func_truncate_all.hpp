#include "func_truncate_base.hpp"

namespace Func2
{
  class TruncateAll : public TruncateBase
  {
  public:
    TruncateAll() {}
    ~TruncateAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const off_t     size_);
  };
}

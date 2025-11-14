#include "func_removexattr_base.hpp"

namespace Func2
{
  class RemovexattrAll : public RemovexattrBase
  {
  public:
    RemovexattrAll() {}
    ~RemovexattrAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const char     *attrname);
  };
}

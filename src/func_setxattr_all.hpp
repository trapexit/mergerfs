#include "func_setxattr_base.hpp"

namespace Func2
{
  class SetxattrAll : public SetxattrBase
  {
  public:
    SetxattrAll() {}
    ~SetxattrAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const char     *attrname_,
                   const char     *attrval_,
                   size_t          attrvalsize_,
                   int             flags_);
  };
}

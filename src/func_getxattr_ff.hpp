#include "func_getxattr_base.hpp"

namespace Func2
{
  class GetxattrFF : public GetxattrBase
  {
  public:
    GetxattrFF() {}
    ~GetxattrFF() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const char     *attrname,
                   char           *attrval,
                   const size_t    attrvalsize);
  };
}

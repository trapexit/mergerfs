#include "func_chown_base.hpp"

namespace Func2
{
  class ChownAll : public ChownBase
  {
  public:
    ChownAll() {}
    ~ChownAll() {}

  public:
    std::string_view name() const;

  public:
    int operator()(const Branches &branches,
                   const fs::path &fusepath,
                   const uid_t     uid,
                   const gid_t     gid);
  };
}

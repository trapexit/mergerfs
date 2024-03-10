#include "branches2.hpp"
#include "func_create.hpp"

#include <memory>

class State
{
  typedef ghc::filesystem::path Path;
  
public:
  State();

public:
  Branches2 branches;

public:
  int
  create(Path const       &fusepath_,
         mode_t const      mode_,
         fuse_file_info_t *ffi_)
  {
    return _create(branches,fusepath_,mode_,ffi_);
  }
  
private:
  Func2::Create _create;
};

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
  int create(Path const       &fusepath,
             mode_t const      mode,
             fuse_file_info_t *ffi);
  
public:
  Func2::Create create;
};

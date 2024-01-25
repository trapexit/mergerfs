#include "ghc/filesystem.hpp"

#include <stdint.h>

class Branch2
{
public:
  enum class Mode
    {
      RO,
      RW,
      NC
    };
  
public:
  bool enabled;
  Mode mode;
  uint64_t min_free_space;
  int fd;
  ghc::filesystem::path path;
};

#include "func_open_ff.hpp"

#include "error.hpp"
#include "fs_open.hpp"


std::string_view
Func2::OpenFF::name() const
{
  return "ff";
}

int
Func2::OpenFF::operator()(const Branches &branches_,
                          const fs::path &fusepath_,
                          const mode_t    mode_)
{
  Err err;
  fs::path fullpath;

  for(const auto &branch : branches_)
    {

    }

  return err;
}

#pragma once

#include "fmt/core.h"

[[noreturn]]
static
inline
void
die(std::string_view msg_)
{
  fmt::println(stderr,"FATAL: {}",msg_);
  std::abort();
}

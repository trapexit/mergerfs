#pragma once

#include "fmt/core.h"
#include <cstdlib>

#define DIE(...)                                                        \
  do {                                                                  \
    fmt::print(stderr, fmt::emphasis::bold | fg(fmt::color::red),       \
               "{}:{} in {}: ", __FILE__, __LINE__, __func__);          \
    fmt::print(stderr, __VA_ARGS__);                                    \
    fmt::print(stderr, "\n");                                           \
    std::abort();                                                       \
  } while(0)

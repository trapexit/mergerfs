#pragma once

#include <filesystem>

namespace fs
{
  int copyfile(const std::filesystem::path &src,
               const std::filesystem::path &dst);
}

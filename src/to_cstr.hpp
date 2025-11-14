#pragma once

#include <filesystem>
#include <string>
#include <type_traits>


template<typename PathType>
const char*
to_cstr(const PathType &p_)
{
  if constexpr (std::is_same_v<std::decay_t<PathType>, std::filesystem::path>)
    return p_.c_str();
  if constexpr (std::is_same_v<std::decay_t<PathType>, std::string>)
    return p_.c_str();
  if constexpr (std::is_same_v<std::decay_t<PathType>, const char*> ||
                std::is_same_v<std::decay_t<PathType>, char*>)
    return p_;

  abort();
  return NULL;
}

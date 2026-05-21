#include "func_listxattr_all.hpp"

#include "errno.hpp"
#include "fs_llistxattr.hpp"

#include <cstring>
#include <set>
#include <string>


std::string_view
Func2::ListxattrAll::name() const
{
  return "all";
}

// listxattr=all: aggregate attribute names across every branch that
// has the file, deduplicating. Names are collected in a std::set so
// the result is sorted and stable across calls. Two passes per
// branch: size query then read into buffer.
ssize_t
Func2::ListxattrAll::operator()(const Branches &branches_,
                                const fs::path &fusepath_,
                                char           *list_,
                                const size_t    size_)
{
  std::set<std::string> names;
  fs::path fullpath;
  bool any = false;
  ssize_t branch_err = -ENOENT;

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      ssize_t need = fs::llistxattr(fullpath,NULL,0);
      if(need == -ENOENT)
        continue;
      if(need < 0)
        {
          branch_err = need;
          continue;
        }
      any = true;
      if(need == 0)
        continue;

      std::string buf(need,'\0');
      const ssize_t got = fs::llistxattr(fullpath,buf.data(),need);
      if(got < 0)
        continue;

      const char *p = buf.data();
      const char *end = p + got;
      while(p < end)
        {
          const size_t len = ::strnlen(p,end - p);
          names.emplace(p,len);
          p += len + 1;
        }
    }

  if(!any)
    return branch_err;

  size_t total = 0;
  for(const auto &n : names)
    total += n.size() + 1;

  if(size_ == 0)
    return (ssize_t)total;

  if(total > size_)
    return -ERANGE;

  char *out = list_;
  for(const auto &n : names)
    {
      std::memcpy(out,n.data(),n.size());
      out[n.size()] = '\0';
      out += n.size() + 1;
    }

  return (ssize_t)total;
}

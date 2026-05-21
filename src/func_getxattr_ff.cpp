#include "func_getxattr_ff.hpp"

#include "config.hpp"
#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_findallfiles.hpp"
#include "fs_lgetxattr.hpp"
#include "str.hpp"

#include <cstring>


static
int
_from_string(char              *destbuf_,
             const size_t       destbufsize_,
             const std::string &src_)
{
  const size_t srcbufsize = src_.size();

  if(destbufsize_ == 0)
    return srcbufsize;

  if(srcbufsize > destbufsize_)
    return -ERANGE;

  memcpy(destbuf_,src_.data(),srcbufsize);

  return srcbufsize;
}

static
int
_user_mergerfs_allpaths(const Branches  &branches_,
                        const fs::path  &fusepath_,
                        char            *buf_,
                        const size_t     count_)
{
  std::string concated;
  StrVec paths;
  StrVec branchpaths;

  for(const auto &branch : branches_)
    branchpaths.push_back(branch.path);

  fs::findallfiles(branchpaths,fusepath_,&paths);

  concated = str::join(paths,'\0');

  return ::_from_string(buf_,count_,concated);
}

static
int
_user_mergerfs(const fs::path &basepath_,
               const fs::path &fusepath_,
               const fs::path &fullpath_,
               const Branches &branches_,
               const char     *attrname_,
               char           *attrval_,
               const size_t    attrvalsize_)
{
  const std::string_view key = Config::prune_ctrl_xattr(attrname_);

  if(key == "basepath")
    return ::_from_string(attrval_,attrvalsize_,basepath_);
  if(key == "relpath")
    return ::_from_string(attrval_,attrvalsize_,fusepath_);
  if(key == "fullpath")
    return ::_from_string(attrval_,attrvalsize_,fullpath_);
  if(key == "allpaths")
    return ::_user_mergerfs_allpaths(branches_,fusepath_,attrval_,attrvalsize_);

  return -ENOATTR;
}


std::string_view
Func2::GetxattrFF::name() const
{
  return "ff";
}

int
Func2::GetxattrFF::operator()(const Branches &branches_,
                              const fs::path &fusepath_,
                              const char     *attrname_,
                              char           *attrval_,
                              const size_t    attrvalsize_)
{
  fs::path fullpath;

  if(Config::is_mergerfs_xattr(attrname_))
    {
      for(const auto &branch : branches_)
        {
          if(!fs::exists(branch.path,fusepath_))
            continue;

          fullpath = branch.path / fusepath_;

          return ::_user_mergerfs(branch.path,
                                  fusepath_,
                                  fullpath,
                                  branches_,
                                  attrname_,
                                  attrval_,
                                  attrvalsize_);
        }

      return -ENOENT;
    }

  for(const auto &branch : branches_)
    {
      fullpath = branch.path / fusepath_;

      const int rv = fs::lgetxattr(fullpath,attrname_,attrval_,attrvalsize_);
      if(rv == -ENOENT)
        continue;

      return rv;
    }

  return -ENOENT;
}

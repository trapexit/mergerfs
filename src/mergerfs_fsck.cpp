#include "mergerfs_fsck.hpp"

#include "fs_close.hpp"
#include "fs_copyfile.hpp"
#include "fs_lchmod.hpp"
#include "fs_lchown.hpp"
#include "fs_lgetxattr.hpp"
#include "fs_lstat.hpp"
#include "fs_open.hpp"
#include "mergerfs_api.hpp"
#include "str.hpp"

#include "fmt/core.h"
#include "fmt/chrono.h"
#include "CLI11.hpp"

#include "base_types.h"

#include <filesystem>

namespace FS = std::filesystem;

struct PathStat
{
  PathStat(const std::string &path_)
    : path(path_),
      st{}
  {
  }

  std::string path;
  struct stat st;
};

using PathStatVec = std::vector<PathStat>;
using FixFunc = std::function<void(const PathStatVec&,const bool)>;

static
bool
_files_differ(const PathStatVec &paths_,
              const bool         check_size_)
{
  if(paths_.size() <= 1)
    return false;

  const struct stat &st0 = paths_[0].st;
  for(u64 i = 1; i < paths_.size(); i++)
    {
      const struct stat &st = paths_[i].st;

      if(st.st_mode != st0.st_mode)
        return true;
      if(st.st_uid != st0.st_uid)
        return true;
      if(st.st_gid != st0.st_gid)
        return true;
      if(check_size_ &&
         S_ISREG(st0.st_mode) &&
         (st.st_size != st0.st_size))
        return true;
    }

  return false;
}

static
bool
_files_same_type(const PathStatVec &paths_)
{
  mode_t type;

  if(paths_.size() <= 1)
    return true;

  type = (paths_[0].st.st_mode & S_IFMT);
  for(u64 i = 1; i < paths_.size(); i++)
    {
      if((paths_[i].st.st_mode & S_IFMT) != type)
        return false;
    }

  return true;
}

static
const char*
_file_type(const mode_t mode_)
{
  switch(mode_ & S_IFMT)
    {
    case S_IFBLK:
      return "block dev";
    case S_IFCHR:
      return "char dev";
    case S_IFDIR:
      return "directory";
    case S_IFIFO:
      return "fifo";
    case S_IFLNK:
      return "symlink";
    case S_IFSOCK:
      return "socket";
    case S_IFREG:
      return "regular";
    }

  return "unknown";
}

static
void
_compare_files(const std::string &mergerfs_path_,
               PathStatVec       &pathstats_,
               const FixFunc      fix_func_,
               const bool         check_size_,
               const bool         copy_file_)
{
  int rv;

  if(pathstats_.size() <= 1)
    return;

  for(auto &pathstat : pathstats_)
    {
      rv = fs::lstat(pathstat.path,&pathstat.st);
      if(rv < 0)
        pathstat.st.st_size = rv;
    }

  if(!::_files_differ(pathstats_,check_size_))
    return;

  fmt::println("* {}",mergerfs_path_);
  for(u64 i = 0; i < pathstats_.size(); i++)
    {
      const auto &path = pathstats_[i].path;
      const auto &st   = pathstats_[i].st;

      fmt::println("  {}: {}",i,path);
      if(st.st_size < 0)
        fmt::println("    - err: {}",strerror(-st.st_size));

      time_t   mtime    = st.st_mtime;
      std::tm *mtime_tm = std::localtime(&mtime);
      fmt::println("    -"
                   " uid: {};"
                   " gid: {};"
                   " mode: {:05o};"
                   " type: {};"
                   " size: {};"
                   " mtime: {:%Y-%m-%dT%H:%M:%S%z}",
                   st.st_uid,
                   st.st_gid,
                   st.st_mode,
                   ::_file_type(st.st_mode),
                   st.st_size,
                   *mtime_tm);
    }

  if(!fix_func_)
    return;

  if(::_files_same_type(pathstats_))
    fix_func_(pathstats_,copy_file_);
  else
    fmt::println("  X: WARNING - files are of different types."
                 " Requires manual intervention.");
}

static
int
_get_allpaths(const std::string &mergerfs_path_,
              PathStatVec       &pathstats_)
{
  int rv;
  std::vector<std::string> allpaths;

  rv = mergerfs::api::allpaths(mergerfs_path_,allpaths);
  if(rv < 0)
    return rv;

  for(const auto &path : allpaths)
    pathstats_.emplace_back(path);

  return 0;
}

static
u64
_read_u64(const u64 min_,
          const u64 max_)
{
  u64 rv;
  while(true)
    {
      fmt::print("Select correct file: ");
      std::cin >> rv;
      if((rv >= min_) && (rv <= max_))
        return rv;
      std::cin.clear();
      std::cin.ignore(100, '\n');
    }
}

static
void
_setattr(const struct stat &st_,
         const PathStatVec &pathstats_)
{
  int rv;

  for(const auto &path : pathstats_)
    {
      rv  = fs::lchmod(path.path,st_.st_mode);
      rv |= fs::lchown(path.path,st_.st_uid,st_.st_gid);
      if(rv)
        fmt::println("ERROR: problem setting mode or owner - {}",
                     strerror(errno));
    }
}

static
void
_copy_files(const std::string &src_,
            const PathStatVec &dsts_)
{
  for(const auto &dst : dsts_)
    {
      int rv;

      // TODO: Additional protection to confirm not the literal same
      // file. fs::is_same_file(src,dst);
      if(src_ == dst.path)
        continue;

      rv = fs::copyfile(src_,dst.path,{.cleanup_failure=false});
      if(rv < 0)
        fmt::print(stderr,
                   "ERROR: failed copying to {} - {}",
                   dst.path,
                   strerror(errno));
    }
}

static
bool
_mtime_cmp(const struct stat &a_,
           const struct stat &b_)
{
  if(a_.st_mtim.tv_sec == b_.st_mtim.tv_sec)
    return (a_.st_mtim.tv_nsec < b_.st_mtim.tv_nsec);
  return (a_.st_mtim.tv_sec < b_.st_mtim.tv_sec);
}

static
bool
_pathstat_cmp(const PathStatVec::value_type &a_,
              const PathStatVec::value_type &b_)
{
  return ::_mtime_cmp(a_.st,b_.st);
}

static
const
PathStatVec::value_type*
_find_latest_mtime(const PathStatVec &pathstats_)
{
  PathStatVec::const_iterator i;

  i = std::max_element(pathstats_.begin(),
                       pathstats_.end(),
                       ::_pathstat_cmp);

  return &(*i);
}

static
const
PathStatVec::value_type*
_find_largest(const PathStatVec &pathstats_)
{
  PathStatVec::const_iterator i;

  i = std::max_element(pathstats_.begin(),
                       pathstats_.end(),
                       [](const PathStatVec::value_type &a_,
                          const PathStatVec::value_type &b_)
                       {
                         return (a_.st.st_size < b_.st.st_size);
                       });

  return &(*i);
}

static
void
_fix_func_manual(const PathStatVec &pathstats_,
                 const bool         copy_file_)
{
  u64 idx;

  idx = ::_read_u64(0ULL,pathstats_.size()-1);

  if(copy_file_ && S_ISREG(pathstats_[idx].st.st_mode))
    ::_copy_files(pathstats_[idx].path,pathstats_);
  else
    ::_setattr(pathstats_[idx].st,pathstats_);
}

static
void
_fix_func_newest(const PathStatVec &pathstats_,
                 const bool         copy_file_)
{
  const PathStatVec::value_type *pathst;

  pathst = _find_latest_mtime(pathstats_);
  if(!pathst)
    return;

  if(copy_file_ && S_ISREG(pathst->st.st_mode))
    ::_copy_files(pathst->path,pathstats_);
  else
    ::_setattr(pathst->st,pathstats_);
};

static
void
_fix_func_largest(const PathStatVec &pathstats_,
                  const bool         copy_file_)
{
  const PathStatVec::value_type *pathst;

  pathst = ::_find_largest(pathstats_);
  if(!pathst)
    return;

  if(copy_file_ && S_ISREG(pathst->st.st_mode))
    ::_copy_files(pathst->path,pathstats_);
  else
    ::_setattr(pathst->st,pathstats_);
}

static
FixFunc
_select_fix_func(const std::string &fix_)
{
  if(fix_ == "none")
    return {};
  else if(fix_ == "manual")
    return ::_fix_func_manual;
  else if(fix_ == "newest")
    return ::_fix_func_newest;
  else if(fix_ == "largest")
    return ::_fix_func_largest;
  else
    abort();

  return {};
}

static
void
_fsck(const FS::path &path_,
      const FixFunc   fix_func_,
      const bool      check_size_,
      const bool      copy_file_)
{
  int rv;
  PathStatVec paths;

  ::_get_allpaths(path_,paths);
  ::_compare_files(path_,paths,fix_func_,check_size_,copy_file_);

  auto opts = FS::directory_options::skip_permission_denied;
  auto rdi = FS::recursive_directory_iterator(path_,opts);
  for(const auto &de : rdi)
    {
      paths.clear();
      rv = ::_get_allpaths(de.path(),paths);
      if(rv < 0)
        continue;

      ::_compare_files(de.path(),
                       paths,
                       fix_func_,
                       check_size_,
                       copy_file_);
    }
}

int
mergerfs::fsck::main(int    argc_,
                     char **argv_)
{
  CLI::App app;
  FS::path path;
  std::string fix;
  bool check_size;
  bool copy_file;
  FixFunc fix_func;

  app.description("fsck.mergerfs:"
                  " A tool to help diagnose and solve mergerfs pool issues");
  app.name("USAGE: fsck.mergerfs");

  app.add_option("path",path)
    ->description("mergerfs path")
    ->check(CLI::ExistingDirectory)
    ->required();
  app.add_option("--fix",fix)
    ->description("Will attempt to 'fix' the problem by chown+chmod or copying files based on a selected file.\n"
                  " * none: Do nothing. Just print details.\n"
                  " * manual: User selects source file.\n"
                  " * newest: Use file with most recent mtime.\n"
                  " * largest: Use file with largest size.")
    ->check(CLI::IsMember({"none","manual","newest","largest"}))
    ->default_val("none");
  app.add_option("--check-size",check_size)
    ->description("Considers file size in calculating differences")
    ->default_val(false)
    ->default_str("false");
  app.add_option("--copy-file",copy_file)
    ->description("Copy file rather than chown/chmod to fix")
    ->default_val(false)
    ->default_str("false");

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e)
    {
      return app.exit(e);
    }

  if((geteuid() != 0) && (fix != "none"))
    fmt::println(stderr,"WARNING: fsck.mergerfs should be run as root to apply fixes");

  fix_func = ::_select_fix_func(fix);

  ::_fsck(path,
          fix_func,
          check_size,
          copy_file);

  return 0;
}

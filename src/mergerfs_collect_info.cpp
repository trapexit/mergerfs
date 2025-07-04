#include "mergerfs_collect_info.hpp"

#include "mergerfs_api.hpp"
#include "fs_mounts.hpp"
#include "fs_unlink.hpp"

#include "CLI11.hpp"
#include "fmt/core.h"
#include "scope_guard.hpp"
#include "subprocess.hpp"

#include <stdio.h>


static
void
_write_str(const std::string &output_,
           const std::string &str_)
{
  FILE *f;

  f = ::fopen(output_.c_str(),"a");
  if(f == NULL)
    return;
  DEFER{ ::fclose(f); };

  ::fwrite(str_.c_str(),1,str_.size(),f);
}

static
void
_lsblk(const std::string &output_)
{
  auto args =
    {
      "lsblk",
      "--json",
      "-o","NAME,FSTYPE,FSSIZE,SIZE,MOUNTPOINTS,RM,RO,ROTA"
    };

  subprocess::call(args,
                   subprocess::output{output_.c_str()});
}

static
void
_mounts(const std::string &output_)
{
  auto args =
    {
      "cat",
      "/proc/mounts"
    };

  subprocess::call(args,
                   subprocess::output{output_.c_str()});
}

static
void
_mount_point_stats(const std::string &output_)
{
  fs::MountVec mounts;

  fs::mounts(mounts);

  for(const auto &mount : mounts)
    {
      std::vector<std::string> allpaths;

      mergerfs::api::allpaths(mount.dir.string(),allpaths);
      for(const auto &path : allpaths)
        {
          auto args = {"stat",path.c_str()};
          subprocess::call(args,
                           subprocess::output{output_.c_str()});
        }
    }
}

static
void
_mergerfs_version(const std::string &output_)
{
  auto args =
    {
      "mergerfs",
      "--version"
    };

  try
    {
      subprocess::call(args,
                       subprocess::output{output_.c_str()});
    }
  catch(...)
    {
    }
}

static
void
_uname(const std::string &output_)
{
  auto args =
    {
      "uname",
      "-a"
    };

  try
    {
      subprocess::call(args,
                       subprocess::output{output_.c_str()});
    }
  catch(...)
    {
    }
}

static
void
_lsb_release(const std::string &output_)
{
  auto args =
    {
      "lsb_release",
      "-a"
    };

  try
    {
      subprocess::call(args,
                       subprocess::output{output_.c_str()});
    }
  catch(...)
    {
    }
}

static
void
_df(const std::string &output_)
{
  auto args =
    {
      "df",
      "-h"
    };

  try
    {
      subprocess::call(args,
                       subprocess::output{output_.c_str()});
    }
  catch(...)
    {
    }
}

static
void
_fstab(const std::string &output_)
{
  auto args =
    {
      "cat",
      "/etc/fstab"
    };

  try
    {
      subprocess::call(args,
                       subprocess::output{output_.c_str()});
    }
  catch(...)
    {
    }
}


int
mergerfs::collect_info::main(int    argc_,
                             char **argv_)
{
  CLI::App app;
  const char *output_filepath = "/tmp/mergerfs.info.txt";

  app.description("mergerfs.collect-info:"
                  " Collect info for support requests");
  app.name("USAGE: mergerfs.collect-info");

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e)
    {
      return app.exit(e);
    }

  fmt::print("* Please have mergerfs mounted before running this tool.\n");

  fs::unlink(output_filepath);
  ::_write_str(output_filepath,"::mergerfs --version::\n");
  ::_mergerfs_version(output_filepath);
  ::_write_str(output_filepath,"\n::uname -a::\n");
  ::_uname(output_filepath);
  ::_write_str(output_filepath,"\n::lsb_release -a::\n");
  ::_lsb_release(output_filepath);
  ::_write_str(output_filepath,"\n::df -h::\n");
  ::_df(output_filepath);
  ::_write_str(output_filepath,"\n::lsblk::\n");
  ::_lsblk(output_filepath);
  ::_write_str(output_filepath,"\n::cat /proc/mounts::\n");
  ::_mounts(output_filepath);
  ::_write_str(output_filepath,"\n::mount point stats::\n");
  ::_mount_point_stats(output_filepath);
  ::_write_str(output_filepath,"\n::cat /etc/fstab::\n");
  ::_fstab(output_filepath);

  fmt::print("* Upload the following file to your"
             " GitHub ticket or put on https://pastebin.com"
             " when requesting support.\n* {}\n",output_filepath);

  return 0;
}

#include "mergerfs_collect_info.hpp"

#include "mergerfs_api.hpp"
#include "fs_mounts.hpp"
#include "fs_unlink.hpp"

#include "CLI11.hpp"
#include "fmt/core.h"
#include "fmt/ranges.h"
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
_run(const std::initializer_list<const char*> &args_,
     const std::string                        &output_)
{
  std::string hdr;

  hdr = fmt::format("=== {}\n",fmt::join(args_," "));
  try
    {
      _write_str(output_,hdr);
      subprocess::call(args_,
                       subprocess::output{output_.c_str()});
    }
  catch(...)
    {
      ::_write_str(output_,"error: command failed to run\n");
    }

  _write_str(output_,"\n\n");
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

  ::_run(args,output_);
}

static
void
_mounts(const std::string &output_)
{
  ::_run({"cat","/proc/mounts"},output_);
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

      mergerfs::api::allpaths(mount.dir.string(),&allpaths);
      for(const auto &path : allpaths)
        {
          ::_run({"stat",path.c_str()},output_);
        }
    }
}

static
void
_mergerfs_version(const std::string &output_)
{
  ::_run({"mergerfs","--version"},output_);
}

static
void
_mergerfs_settings(const std::string &output_)
{
  fs::MountVec mounts;

  fs::mounts(mounts);

  for(const auto &mount : mounts)
    {
      int rv;
      std::map<std::string,std::string> kvs;

      rv = mergerfs::api::get_kvs(mount.dir.string(),&kvs);
      if(rv < 0)
        continue;

      std::string output_str;

      output_str = fmt::format("=== {}/.mergerfs\n",mount.dir.string());
      for(const auto &[k,v] : kvs)
        output_str += fmt::format("{}={}\n",k,v);
      output_str += "\n\n";

      _write_str(output_,output_str);
    }
}

static
void
_uname(const std::string &output_)
{
  ::_run({"uname","-a"},output_);
}

static
void
_lsb_release(const std::string &output_)
{
  ::_run({"lsb_release","-a"},output_);
}

static
void
_df(const std::string &output_)
{
  ::_run({"df","-h"},output_);
}

static
void
_fstab(const std::string &output_)
{
  ::_run({"cat","/etc/fstab"},output_);
}

static
void
_software_versions(const std::string &output_)
{
  ::_run({"docker","--version"},output_);
  ::_run({"docker","compose","--version"},output_);
  ::_run({"podman","--version"},output_);
  ::_run({"podman","compose","--version"},output_);
  ::_run({"smbd","--version"},output_);
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
  ::_mergerfs_version(output_filepath);
  ::_uname(output_filepath);
  ::_lsb_release(output_filepath);
  ::_df(output_filepath);
  ::_lsblk(output_filepath);
  ::_mounts(output_filepath);
  ::_mount_point_stats(output_filepath);
  ::_mergerfs_settings(output_filepath);
  ::_fstab(output_filepath);
  ::_software_versions(output_filepath);

  fmt::print("* Upload the following file to your"
             " GitHub ticket or put on https://pastebin.com"
             " when requesting support.\n* {}\n",output_filepath);

  return 0;
}

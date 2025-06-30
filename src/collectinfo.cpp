#include "collectinfo.hpp"

#include "fs_mounts.hpp"
#include "fs_unlink.hpp"

#include "subprocess.hpp"
#include "CLI11.hpp"

static
void
_lsblk(const char *output_)
{
  auto args =
    {
      "lsblk",
      "--json",
      "-o","NAME,FSTYPE,FSSIZE,SIZE,MOUNTPOINTS,RM,RO,ROTA"
    };

  subprocess::call(args,
                   subprocess::output{output_});
}

static
void
_mounts(const char *output_)
{
  auto args =
    {
      "cat",
      "/proc/mounts"
    };

  subprocess::call(args,
                   subprocess::output{output_});
}

static
void
_mergerfs_details(const char *output_)
{
  fs::MountVec mounts;

  fs::mounts(mounts);

  for(const auto &mount : mounts)
    {
      mount.dir;
    }
}

int
collectinfo::main(int    argc_,
                  char **argv_)
{
  CLI::App app;
  const char *output = "/tmp/mergerfs.info.txt";

  app.description("mergerfs.collectinfo:"
                  " Collect info for support requests");
  app.name("USAGE: mergerfs.collectinfo");

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e)
    {
      return app.exit(e);
    }

  fs::unlink(output);
  _lsblk(output);
  _mounts(output);
  _mergerfs_details(output);

  return 0;
}

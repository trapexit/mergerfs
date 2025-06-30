#include "collectinfo.hpp"

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

  _lsblk(output);

  return 0;
}

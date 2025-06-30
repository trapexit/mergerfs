#include "collectinfo.hpp"

#include "subprocess.hpp"
#include "CLI11.hpp"


int
collectinfo::main(int    argc_,
                  char **argv_)
{
  CLI::App app;

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

  return 0;
}

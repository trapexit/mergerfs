#include "mfm.hpp"

#include "mfm_options.hpp"

#include "CLI11.hpp"

static
void
_setup_argparser_dup(CLI::App       &app_,
                     mfm::Opts::Dup &opts_)
{
  CLI::App *subcmd;

  subcmd = app_.add_subcommand("dup",
                               "duplicate files");
  subcmd->add_option("path",opts_.paths)
    ->description("")
    ->type_name("PATH")
    ->check(CLI::ExistingPath)
    ->required();

  auto func = std::bind(mfm::dup,std::cref(opts_));

  subcmd->callback(func);
}

static
void
_setup_argparser_watch(CLI::App         &app_,
                       mfm::Opts::Watch &opts_)
{

}

static
void
_setup_argparser_migrate(CLI::App           &app_,
                         mfm::Opts::Migrate &opts_)
{

}

static
void
_setup_argparser_rebalance(CLI::App             &app_,
                           mfm::Opts::Rebalance &opts_)
{

}

static
void
_setup_argparser(CLI::App  &app_,
                 mfm::Opts &opts_)
{
  _setup_argparser_dup(app_,opts_.dup);
  _setup_argparser_migrate(app_,opts_.migrate);
  _setup_argparser_rebalance(app_,opts_.rebalance);
  _setup_argparser_watch(app_,opts_.watch);
}

int
mfm::main(int    argc_,
          char **argv_)
{
  CLI::App app;
  mfm::Opts opts;

  app.description("mfm: mergerfs filesystem manager");
  app.name("USAGE:: mfm");

  ::_setup_argparser(app,opts);

  auto a = std::filesystem::path("/a/b");
  auto b = std::filesystem::path("a/b");
  fmt::println("{}\n{}",
               a.parent_path().string(),
               b.parent_path().string());

  return 0;

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e_)
    {
      return app.exit(e_);
    }

  return 0;
}

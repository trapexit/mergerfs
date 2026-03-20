/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string>

static const char *progname;

static std::string shell_quote(const std::string &s)
{
  std::string r = "'";

  for (char c : s) {
    if (c == '\'')
      r += "'\\''";
    else
      r += c;
  }
  r += "'";

  return r;
}

static std::string add_option(const std::string &opt,
                              const std::string &options)
{
  if (options.empty())
    return opt;

  return options + "," + opt;
}

int main(int argc, char *argv[])
{
  std::string type;
  std::string source;
  std::string mountpoint;
  std::string basename;
  std::string options;
  std::string command;
  std::string setuid;
  int dev = 1;
  int suid = 1;

  progname = argv[0];
  {
    const char *slash = strrchr(argv[0], '/');
    basename = slash ? (slash + 1) : argv[0];
  }

  type = "mergerfs";
  if (strncmp(basename.c_str(), "mount.fuse.", 11) == 0)
    type = basename.substr(11);
  if (strncmp(basename.c_str(), "mount.fuseblk.", 14) == 0)
    type = basename.substr(14);

  if (type.empty())
    type = "";

  if (argc < 3) {
    fprintf(stderr,
            "usage: %s %s destination [-t type] [-o opt[,opts...]]\n",
            progname, type.empty() ? "type#[source]" : "source");
    exit(1);
  }

  source = argv[1];

  mountpoint = argv[2];

  for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0) {
      continue;
    } else if (strcmp(argv[i], "-t") == 0) {
      i++;

      if (i == argc) {
        fprintf(stderr,
                "%s: missing argument to option '-t'\n",
                progname);
        exit(1);
      }
      type = argv[i];
      if (strncmp(type.c_str(), "fuse.", 5) == 0)
        type = type.substr(5);
      else if (strncmp(type.c_str(), "fuseblk.", 8) == 0)
        type = type.substr(8);

      if (type.empty()) {
        fprintf(stderr,
                "%s: empty type given as argument to option '-t'\n",
                progname);
        exit(1);
      }
    } else if (strcmp(argv[i], "-o") == 0) {
      char *opts;
      char *opt;
      i++;
      if (i == argc)
        break;

      opts = strdup(argv[i]);
      opt = strtok(opts, ",");
      while (opt)
        {
          int j;
          int ignore = 0;
          const char *ignore_opts[] =
            {
              "",
              "nofail",
              "user",
              "nouser",
              "users",
              "auto",
              "noauto",
              "_netdev",
              NULL
            };
        if (strncmp(opt, "setuid=", 7) == 0) {
          setuid = opt + 7;
          ignore = 1;
        }
        for (j = 0; ignore_opts[j]; j++)
          if (strcmp(opt, ignore_opts[j]) == 0)
            ignore = 1;

        if (!ignore) {
          if (strcmp(opt, "nodev") == 0)
            dev = 0;
          else if (strcmp(opt, "nosuid") == 0)
            suid = 0;

          options = add_option(opt, options);
        }
        opt = strtok(NULL, ",");
      }
      free(opts);
    }
  }

  if (dev)
    options = add_option("dev", options);
  if (suid)
    options = add_option("suid", options);

  if (type.empty()) {
    if (!source.empty()) {
      size_t hash_pos = source.find('#');
      if (hash_pos != std::string::npos) {
        type = source.substr(0, hash_pos);
        source = source.substr(hash_pos + 1);
      } else {
        type = source;
        source.clear();
      }
      if (type.empty()) {
        fprintf(stderr, "%s: empty filesystem type\n",
                progname);
        exit(1);
      }
    } else {
      fprintf(stderr, "%s: empty source\n", progname);
      exit(1);
    }
  }

  command += shell_quote(type);
  if (!source.empty())
    command += " " + shell_quote(source);
  command += " " + shell_quote(mountpoint);
  if (!options.empty()) {
    command += " " + shell_quote("-o");
    command += " " + shell_quote(options);
  }

  if (!setuid.empty()) {
    std::string sucommand = command;
    command = "su";
    command += " -";
    command += " " + shell_quote(setuid);
    command += " -c";
    command += " " + shell_quote(sucommand);
  } else if (!getenv("HOME")) {
    setenv("HOME", "/root", 0);
  }

  execl("/bin/sh", "/bin/sh", "-c", command.c_str(), NULL);
  fprintf(stderr, "%s: failed to execute /bin/sh: %s\n", progname,
          strerror(errno));
  return 1;
}

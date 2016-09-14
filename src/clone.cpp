/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <unistd.h>
#include <string.h>

#include <iostream>

#include "errno.hpp"
#include "fs.hpp"
#include "fs_clonefile.hpp"
#include "fs_clonepath.hpp"

namespace clonetool
{
  static
  void
  print_usage_and__exit(void)
  {
    std::cerr << "usage: clone "
              << "[path <sourcedir> <destdir> <relativepath>]"
              << " | "
              << "[file <source> <dest>]"
              << std::endl;
    _exit(1);
  }

  int
  main(const int    argc,
       char * const argv[])
  {
    int rv = 0;

    if(argc == 4 && !strcmp(argv[1],"file"))
      rv = fs::clonefile(argv[2],argv[3]);
    else if(argc == 5 && !strcmp(argv[1],"path"))
      rv = fs::clonepath(argv[2],argv[3],argv[4]);
    else
      print_usage_and__exit();

    if(rv == -1)
      std::cerr << "error: "
                << strerror(errno)
                << std::endl;

    return 0;
  }
}

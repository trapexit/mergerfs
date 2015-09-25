/*
   The MIT License (MIT)

   Copyright (c) 2014 Antonio SJ Musumeci <trapexit@spawn.link>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <iostream>

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

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

#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <sys/stat.h>

#include <string>
#include <vector>

#include "policy.hpp"
#include "category.hpp"

namespace mergerfs
{
  namespace config
  {
    class Config
    {
    public:
      Config();

      std::string generateReadStr() const;
      void        updateReadStr();

    public:
      std::string              destmount;
      std::vector<std::string> srcmounts;

      const Policy *policies[Category::Enum::END];
      const Policy *&action;
      const Policy *&create;
      const Policy *&search;

      const std::string controlfile;
      struct stat       controlfilestat;
      std::string       readstr;

      bool testmode;
    };

    const Config &get(void);
    Config       &get_writable(void);
  }
}

#endif

/*
  ISC License

  Copyright (c) 2018, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "branch.hpp"
#include "fs.hpp"
#include "fs_glob.hpp"
#include "str.hpp"

#include <fnmatch.h>

#include <string>

using std::string;
using std::vector;

bool
Branch::ro(void) const
{
  return (mode == Branch::RO);
}

bool
Branch::nc(void) const
{
  return (mode == Branch::NC);
}

bool
Branch::ro_or_nc(void) const
{
  return ((mode == Branch::RO) ||
          (mode == Branch::NC));
}

string
Branches::to_string(const bool mode_) const
{
  string tmp;

  for(size_t i = 0; i < size(); i++)
    {
      const Branch &branch = (*this)[i];

      tmp += branch.path;

      if(mode_)
        {
          tmp += '=';
          switch(branch.mode)
            {
            default:
            case Branch::RW:
              tmp += "RW";
              break;
            case Branch::RO:
              tmp += "RO";
              break;
            case Branch::NC:
              tmp += "NC";
              break;
            }
        }

      tmp += ':';
    }

  if(*tmp.rbegin() == ':')
    tmp.erase(tmp.size() - 1);

  return tmp;
}

void
Branches::to_paths(vector<string> &vec_) const
{
  for(size_t i = 0; i < size(); i++)
    {
      const Branch &branch = (*this)[i];

      vec_.push_back(branch.path);
    }
}

static
void
parse(const string &str_,
      Branches     &branches_)
{
  string str;
  Branch branch;
  vector<string> globbed;

  str = str_;
  branch.mode = Branch::INVALID;
  if(str::ends_with(str,"=RO"))
    branch.mode = Branch::RO;
  else if(str::ends_with(str,"=RW"))
    branch.mode = Branch::RW;
  else if(str::ends_with(str,"=NC"))
    branch.mode = Branch::NC;

  if(branch.mode != Branch::INVALID)
    str.resize(str.size() - 3);
  else
    branch.mode = Branch::RW;

  fs::glob(str,globbed);
  fs::realpathize(globbed);
  for(size_t i = 0; i < globbed.size(); i++)
    {
      branch.path = globbed[i];
      branches_.push_back(branch);
    }
}

void
Branches::set(const std::string &str_)
{
  vector<string> paths;

  clear();

  str::split(paths,str_,':');

  for(size_t i = 0; i < paths.size(); i++)
    {
      Branches branches;

      parse(paths[i],branches);

      insert(end(),
             branches.begin(),
             branches.end());
    }
}

void
Branches::add_begin(const std::string &str_)
{
  vector<string> paths;

  str::split(paths,str_,':');

  for(size_t i = 0; i < paths.size(); i++)
    {
      Branches branches;

      parse(paths[i],branches);

      insert(begin(),
             branches.begin(),
             branches.end());
    }
}

void
Branches::add_end(const std::string &str_)
{
  vector<string> paths;

  str::split(paths,str_,':');

  for(size_t i = 0; i < paths.size(); i++)
    {
      Branches branches;

      parse(paths[i],branches);

      insert(end(),
             branches.begin(),
             branches.end());
    }
}

void
Branches::erase_begin(void)
{
  erase(begin());
}

void
Branches::erase_end(void)
{
  pop_back();
}

void
Branches::erase_fnmatch(const std::string &str_)
{
  vector<string> patterns;

  str::split(patterns,str_,':');

  for(iterator i = begin(); i != end();)
    {
      int match = FNM_NOMATCH;

      for(vector<string>::const_iterator pi = patterns.begin();
          pi != patterns.end() && match != 0;
          ++pi)
        {
          match = ::fnmatch(pi->c_str(),i->path.c_str(),0);
        }

      i = ((match == 0) ? erase(i) : (i+1));
    }
}

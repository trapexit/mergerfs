/*
  ISC License

  Copyright (c) 2019, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "func_category.hpp"
#include "str.hpp"

#include "buildvector.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
FuncCategory::from_string(const std::string &s_)
{
  const Policy *tmp;

  tmp = &Policy::find(s_);
  if(tmp == &Policy::invalid)
    return -EINVAL;

  for(uint64_t i = 0; i < _funcs.size(); i++)
    _funcs[i]->policy = tmp;

  return 0;
}

std::string
FuncCategory::to_string(void) const
{
  std::vector<std::string> rv;

  for(uint64_t i = 0; i < _funcs.size(); i++)
    rv.push_back(_funcs[i]->policy->to_string());

  std::sort(rv.begin(),rv.end());
  rv.erase(std::unique(rv.begin(),rv.end()),rv.end());

  return str::join(rv,',');
}

FuncCategoryCreate::FuncCategoryCreate(Funcs &funcs_)
{
  _funcs = buildvector<Func*>
    (&funcs_.create)
    (&funcs_.mkdir)
    (&funcs_.mknod)
    (&funcs_.symlink);
}

FuncCategoryAction::FuncCategoryAction(Funcs &funcs_)
{
  _funcs = buildvector<Func*>
    (&funcs_.chmod)
    (&funcs_.chown)
    (&funcs_.link)
    (&funcs_.removexattr)
    (&funcs_.rename)
    (&funcs_.rmdir)
    (&funcs_.setxattr)
    (&funcs_.truncate)
    (&funcs_.unlink)
    (&funcs_.utimens);
}

FuncCategorySearch::FuncCategorySearch(Funcs &funcs_)
{
  _funcs = buildvector<Func*>
    (&funcs_.access)
    (&funcs_.getattr)
    (&funcs_.getxattr)
    (&funcs_.listxattr)
    (&funcs_.open)
    (&funcs_.readlink);
}

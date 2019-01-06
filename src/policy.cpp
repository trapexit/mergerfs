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

#include <vector>

#include "buildvector.hpp"
#include "fs.hpp"
#include "policy.hpp"

#define POLICY(X,PP) (Policy(Policy::Enum::X,#X,Policy::Func::X,PP))
#define PRESERVES_PATH true
#define DOESNT_PRESERVE_PATH false

const std::vector<Policy> Policy::_policies_ =
  buildvector<Policy,true>
  (POLICY(invalid,DOESNT_PRESERVE_PATH))
  (POLICY(all,DOESNT_PRESERVE_PATH))
  (POLICY(epall,PRESERVES_PATH))
  (POLICY(epff,PRESERVES_PATH))
  (POLICY(eplfs,PRESERVES_PATH))
  (POLICY(eplus,PRESERVES_PATH))
  (POLICY(epmfs,PRESERVES_PATH))
  (POLICY(eprand,PRESERVES_PATH))
  (POLICY(erofs,DOESNT_PRESERVE_PATH))
  (POLICY(ff,DOESNT_PRESERVE_PATH))
  (POLICY(lfs,DOESNT_PRESERVE_PATH))
  (POLICY(lus,DOESNT_PRESERVE_PATH))
  (POLICY(mfs,DOESNT_PRESERVE_PATH))
  (POLICY(newest,DOESNT_PRESERVE_PATH))
  (POLICY(rand,DOESNT_PRESERVE_PATH));

const Policy * const Policy::policies = &_policies_[1];

#define CONST_POLICY(X) const Policy &Policy::X = Policy::policies[Policy::Enum::X]

CONST_POLICY(invalid);
CONST_POLICY(all);
CONST_POLICY(epall);
CONST_POLICY(epff);
CONST_POLICY(eplfs);
CONST_POLICY(eplus);
CONST_POLICY(epmfs);
CONST_POLICY(eprand);
CONST_POLICY(erofs);
CONST_POLICY(ff);
CONST_POLICY(lfs);
CONST_POLICY(lus);
CONST_POLICY(mfs);
CONST_POLICY(newest);
CONST_POLICY(rand);

const Policy&
Policy::find(const std::string &str)
{
for(int i = Enum::BEGIN; i != Enum::END; ++i)
  {
    if(policies[i] == str)
      return policies[i];
  }

 return invalid;
}

const Policy&
Policy::find(const Policy::Enum::Type i)
{
  if(i >= Policy::Enum::BEGIN &&
     i  < Policy::Enum::END)
    return policies[i];

  return invalid;
}


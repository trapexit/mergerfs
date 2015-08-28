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

#include <vector>

#include "policy.hpp"
#include "fs.hpp"
#include "buildvector.hpp"

#define POLICY(X) (Policy(Policy::Enum::X,#X,Policy::Func::X))

namespace mergerfs
{
  const std::vector<Policy> Policy::_policies_ =
    buildvector<Policy,true>
    (POLICY(invalid))
    (POLICY(all))
    (POLICY(einval))
    (POLICY(enosys))
    (POLICY(enotsup))
    (POLICY(epmfs))
    (POLICY(erofs))
    (POLICY(exdev))
    (POLICY(ff))
    (POLICY(ffwp))
    (POLICY(fwfs))
    (POLICY(lfs))
    (POLICY(mfs))
    (POLICY(newest))
    (POLICY(rand));

  const Policy * const Policy::policies = &_policies_[1];

#define CONST_POLICY(X) const Policy &Policy::X = Policy::policies[Policy::Enum::X]

  CONST_POLICY(invalid);
  CONST_POLICY(all);
  CONST_POLICY(einval);
  CONST_POLICY(enosys);
  CONST_POLICY(enotsup);
  CONST_POLICY(epmfs);
  CONST_POLICY(erofs);
  CONST_POLICY(exdev);
  CONST_POLICY(ff);
  CONST_POLICY(ffwp);
  CONST_POLICY(fwfs);
  CONST_POLICY(lfs);
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
}

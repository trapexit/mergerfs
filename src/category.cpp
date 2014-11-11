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

#include <string>
#include <vector>

#include "category.hpp"
#include "buildvector.hpp"

#define CATEGORY(X) Category(Category::Enum::X,#X)

namespace mergerfs
{
  const std::vector<Category> Category::_categories_ =
    buildvector<Category,true>
    (CATEGORY(invalid))
    (CATEGORY(action))
    (CATEGORY(create))
    (CATEGORY(search));

  const Category * const Category::categories = &_categories_[1];

  const Category &Category::invalid = Category::categories[Category::Enum::invalid];
  const Category &Category::action  = Category::categories[Category::Enum::action];
  const Category &Category::create  = Category::categories[Category::Enum::create];
  const Category &Category::search  = Category::categories[Category::Enum::search];

  const Category&
  Category::find(const std::string &str)
  {
    for(int i = Enum::BEGIN; i != Enum::END; ++i)
      {
        if(categories[i] == str)
          return categories[i];
      }

    return invalid;
  }

  const Category&
  Category::find(const Category::Enum::Type i)
  {
    if(i >= Category::Enum::BEGIN &&
       i  < Category::Enum::END)
      return categories[i];

    return invalid;
  }
}

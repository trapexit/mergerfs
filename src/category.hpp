/*
  ISC License

  Copyright (c) 2026, Antonio SJ Musumeci <trapexit@spawn.link>

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

#pragma once

#include "tofrom_string.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace Category
{
  class Base : public ToFromString
  {
  public:
    int from_string(const std::string_view s) final;
    std::string to_string() const final;

  protected:
    std::vector<ToFromString*> funcs;
    // The last value successfully applied via from_string. Categories
    // can map a single user input to different per-function default
    // classes, so the constituent name()s may disagree. Returning the
    // last-set value here keeps to_string output round-trippable, but
    // only if no per-function override has since diverged the children.
    std::string _last_set;
    mutable std::mutex _last_set_mutex;
  };

  class Action final : public Base
  {
  private:
    Action();

  public:
    Action(ToFromString &chmod_,
           ToFromString &chown_,
           ToFromString &link_,
           ToFromString &removexattr_,
           ToFromString &rename_,
           ToFromString &rmdir_,
           ToFromString &setxattr_,
           ToFromString &truncate_,
           ToFromString &unlink_,
           ToFromString &utimens_)
    {
      funcs.push_back(&chmod_);
      funcs.push_back(&chown_);
      funcs.push_back(&link_);
      funcs.push_back(&removexattr_);
      funcs.push_back(&rename_);
      funcs.push_back(&rmdir_);
      funcs.push_back(&setxattr_);
      funcs.push_back(&truncate_);
      funcs.push_back(&unlink_);
      funcs.push_back(&utimens_);
    }
  };

  class Create final : public Base
  {
  private:
    Create();

  public:
    Create(ToFromString &create_,
           ToFromString &mkdir_,
           ToFromString &mknod_,
           ToFromString &symlink_)
    {
      funcs.push_back(&create_);
      funcs.push_back(&mkdir_);
      funcs.push_back(&mknod_);
      funcs.push_back(&symlink_);
    }
  };

  class Search final : public Base
  {
  private:
    Search();

  public:
    Search(ToFromString &access_,
           ToFromString &getattr_,
           ToFromString &getxattr_,
           ToFromString &ioctl_,
           ToFromString &listxattr_,
           ToFromString &open_,
           ToFromString &readlink_)
    {
      funcs.push_back(&access_);
      funcs.push_back(&getattr_);
      funcs.push_back(&getxattr_);
      funcs.push_back(&ioctl_);
      funcs.push_back(&listxattr_);
      funcs.push_back(&open_);
      funcs.push_back(&readlink_);
    }
  };
}

class Categories final
{
private:
  Categories();

public:
  Categories(ToFromString &access_,
             ToFromString &chmod_,
             ToFromString &chown_,
              ToFromString &link_,
              ToFromString &getattr_,
              ToFromString &getxattr_,
              ToFromString &ioctl_,
              ToFromString &listxattr_,
              ToFromString &open_,
              ToFromString &readlink_,
             ToFromString &rename_,
             ToFromString &removexattr_,
             ToFromString &rmdir_,
             ToFromString &setxattr_,
             ToFromString &truncate_,
             ToFromString &unlink_,
             ToFromString &utimens_,
             ToFromString &create_,
             ToFromString &mkdir_,
             ToFromString &mknod_,
             ToFromString &symlink_)
    : action(chmod_,
             chown_,
             link_,
             removexattr_,
             rename_,
             rmdir_,
             setxattr_,
             truncate_,
             unlink_,
             utimens_),
      create(create_,mkdir_,mknod_,symlink_),
      search(access_,
             getattr_,
             getxattr_,
             ioctl_,
             listxattr_,
             open_,
             readlink_)
  {}

public:
  Category::Action action;
  Category::Create create;
  Category::Search search;
};

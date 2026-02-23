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

#include "state.hpp"

#include "errno.hpp"

State state;

// static
// void
// _register_getattr(State *state_)
// {
//   State::GetSet x;

//   x.get = [state_]() -> std::string
//   {
//     return state_->getattr.to_string();
//   };
//   x.set = [state_](const std::string_view s_) -> int
//   {
//     return state_->getattr.from_string(s_);
//   };

//   state_->set_getset("user.mergerfs.func.getattr",x);
// }

State::State()
{
  // _register_getattr(this);
}

void
State::set_getset(const std::string   &name_,
                  const State::GetSet &gs_)
{
  _getset[name_] = gs_;
}

int
State::get(const std::string &key_,
           std::string       &val_)
{
  std::map<std::string,State::GetSet>::iterator i;

  i = _getset.find(key_);
  if((i == _getset.end()) || (!i->second.get))
    return -ENOATTR;

  val_ = i->second.get();

  return 0;
}

int
State::set(const std::string      &key_,
           const std::string_view  val_)
{
  std::map<std::string,State::GetSet>::iterator i;

  i = _getset.find(key_);
  if((i == _getset.end()) || (!i->second.set))
    return -ENOATTR;

  return i->second.set(val_);
}

int
State::valid(const std::string      &key_,
             const std::string_view  val_)
{
  std::map<std::string,State::GetSet>::iterator i;

  i = _getset.find(key_);
  if((i == _getset.end()) || (!i->second.valid))
    return -ENOATTR;

  return i->second.valid(val_);
}

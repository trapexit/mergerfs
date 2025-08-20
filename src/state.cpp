#include "state.hpp"

#include "fmt/core.h"

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

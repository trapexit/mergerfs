#include "policy_cache.hpp"

#include "mutex.hpp"

#include <cstdlib>
#include <map>
#include <string>

#include <time.h>

using std::map;
using std::string;

static const uint64_t DEFAULT_TIMEOUT = 0;

namespace l
{
  static
  uint64_t
  get_time(void)
  {
    uint64_t rv;

    rv = ::time(NULL);

    return rv;
  }
}


PolicyCache::Value::Value()
  : time(0),
    paths()
{

}

PolicyCache::PolicyCache(void)
  : timeout(DEFAULT_TIMEOUT)
{
  mutex_init(&_lock);
}

void
PolicyCache::erase(const char *fusepath_)
{
  if(timeout == 0)
    return;

  mutex_lock(&_lock);

  _cache.erase(fusepath_);

  mutex_unlock(&_lock);
}

void
PolicyCache::cleanup(const int prob_)
{
  uint64_t now;
  map<string,Value>::iterator i;

  if(timeout == 0)
    return;

  if(rand() % prob_)
    return;

  now = l::get_time();

  mutex_lock(&_lock);

  i = _cache.begin();
  while(i != _cache.end())
    {
      if((now - i->second.time) >= timeout)
        _cache.erase(i++);
      else
        ++i;
    }

  mutex_unlock(&_lock);
}

void
PolicyCache::clear(void)
{
  mutex_lock(&_lock);

  _cache.clear();

  mutex_unlock(&_lock);
}

int
PolicyCache::operator()(const Policy::Search &policy_,
                        const Branches       &branches_,
                        const char           *fusepath_,
                        std::vector<Branch*> &paths_)
{
  int rv;
  Value *v;
  uint64_t now;

  if(timeout == 0)
    return policy_(branches_,fusepath_,paths_);

  now = l::get_time();

  mutex_lock(&_lock);
  v = &_cache[fusepath_];

  if((now - v->time) >= timeout)
    {
      mutex_unlock(&_lock);

      rv = policy_(branches_,fusepath_,paths_);
      if(rv == -1)
        return -1;

      mutex_lock(&_lock);
      v->time  = now;
      v->paths = paths_;
    }
  else
    {
      paths_ = v->paths;
    }

  mutex_unlock(&_lock);

  return 0;
}

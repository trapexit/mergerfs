#include "policy_cache.hpp"

#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include <time.h>

using std::map;
using std::string;
using std::vector;

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
    path()
{

}

PolicyCache::PolicyCache(void)
  : timeout(DEFAULT_TIMEOUT)
{
}

void
PolicyCache::erase(const char *fusepath_)
{
  pthread_mutex_lock(&_lock);

  _cache.erase(fusepath_);

  pthread_mutex_unlock(&_lock);
}

void
PolicyCache::cleanup(const int prob_)
{
  uint64_t now;
  map<string,Value>::iterator i;

  if(rand() % prob_)
    return;

  now = l::get_time();

  pthread_mutex_lock(&_lock);

  i = _cache.begin();
  while(i != _cache.end())
    {
      if((now - i->second.time) >= timeout)
        _cache.erase(i++);
      else
        ++i;
    }

  pthread_mutex_unlock(&_lock);
}

void
PolicyCache::clear(void)
{
  pthread_mutex_lock(&_lock);

  _cache.clear();

  pthread_mutex_unlock(&_lock);
}

int
PolicyCache::operator()(Policy::Func::Search &func_,
                        const Branches       &branches_,
                        const char           *fusepath_,
                        const uint64_t        minfreespace_,
                        std::string          *branch_)
{
  int rv;
  Value *v;
  uint64_t now;
  string branch;

  if(timeout == 0)
    return func_(branches_,fusepath_,minfreespace_,branch_);

  now = l::get_time();

  pthread_mutex_lock(&_lock);
  v = &_cache[fusepath_];

  if((now - v->time) >= timeout)
    {
      pthread_mutex_unlock(&_lock);
      rv = func_(branches_,fusepath_,minfreespace_,&branch);
      if(rv == -1)
        return -1;

      pthread_mutex_lock(&_lock);
      v->time = now;
      v->path = branch;
    }

  *branch_ = v->path;

  pthread_mutex_unlock(&_lock);

  return 0;
}

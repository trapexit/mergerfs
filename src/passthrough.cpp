#include "passthrough.hpp"

#include "boost/unordered/concurrent_flat_map.hpp"

#include <string>

typedef boost::unordered::concurrent_flat_map<std::string,PTInfo> PTMap;
static PTMap g_PT;

namespace PassthroughStuff
{
  std::mutex*
  get_mutex(const char *fusepath_)
  {
    std::mutex *m = nullptr;

    g_PT.visit(fusepath_,
             [&](const PTMap::value_type &x_)
             {
               m = &x_.second.mutex;
             });

    if(!m)
      {
        g_PT.insert(PTMap::value_type{fusepath_,PTInfo{}});
        g_PT.visit(fusepath_,
                   [&](const PTMap::value_type &x_)
                   {
                     m = &x_.second.mutex;
                   });
      }

    return m;
  }

  PTInfo*
  get_pti(const char *fusepath_)
  {
    PTInfo *pti = nullptr;

    g_PT.visit(fusepath_,
             [&](PTMap::value_type &x_)
             {
               pti = &x_.second;
             });

    if(!pti)
      {
        g_PT.insert(PTMap::value_type{fusepath_,PTInfo{}});
        g_PT.visit(fusepath_,
                   [&](PTMap::value_type &x_)
                   {
                     pti = &x_.second;                     
                   });
      }

    return pti;
  }  
};

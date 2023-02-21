#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cpu.hpp"
#include "ghc/filesystem.hpp"
#include "fmt/core.h"

#include <sched.h>

#include <fstream>


int
CPU::getaffinity(const pthread_t  thread_id_,
                 cpu_set_t       *cpuset_)
{
  CPU_ZERO(cpuset_);
  return sched_getaffinity(thread_id_,
                           sizeof(cpu_set_t),
                           cpuset_);
}

int
CPU::setaffinity(const pthread_t  thread_id_,
                 cpu_set_t       *cpuset_)
{
  return pthread_setaffinity_np(thread_id_,
                                sizeof(cpu_set_t),
                                cpuset_);
}

int
CPU::setaffinity(const pthread_t thread_id_,
                 const int       cpu_)
{
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(cpu_,&cpuset);

  return CPU::setaffinity(thread_id_,&cpuset);
}

int
CPU::setaffinity(const pthread_t     thread_id_,
                 const std::set<int> cpus_)
{
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  for(auto const cpu : cpus_)
    CPU_SET(cpu,&cpuset);

  return CPU::setaffinity(thread_id_,&cpuset);
}

static
ghc::filesystem::path
generate_cpu_core_id_path(const int cpu_id_)
{
  const ghc::filesystem::path basepath{"/sys/devices/system/cpu"};

  return basepath / fmt::format("cpu{}",cpu_id_) / "topology" / "core_id";
}

int
CPU::count()
{
  int rv;
  cpu_set_t cpuset;

  rv = CPU::getaffinity(0,&cpuset);
  if(rv < 0)
    return rv;

  return CPU_COUNT(&cpuset);
}

CPU::CPUVec
CPU::cpus()
{
  cpu_set_t cpuset;
  CPU::CPUVec cpuvec;

  CPU::getaffinity(0,&cpuset);

  for(int i = 0; i < CPU_SETSIZE; i++)
    {
      if(!CPU_ISSET(i,&cpuset))
        continue;

      cpuvec.push_back(i);
    }

  return cpuvec;
}

CPU::CPU2CoreMap
CPU::cpu2core()
{
  cpu_set_t cpuset;
  CPU::CPU2CoreMap c2c;

  CPU::getaffinity(0,&cpuset);

  for(int i = 0; i < CPU_SETSIZE; i++)
    {
      int core_id;
      std::ifstream ifs;
      ghc::filesystem::path path;

      if(!CPU_ISSET(i,&cpuset))
        continue;

      path = ::generate_cpu_core_id_path(i);

      ifs.open(path);
      if(!ifs)
        break;

      ifs >> core_id;

      c2c[i] = core_id;

      ifs.close();
    }

  return c2c;
}

CPU::Core2CPUsMap
CPU::core2cpus()
{
  cpu_set_t cpuset;
  CPU::Core2CPUsMap c2c;

  CPU::getaffinity(0,&cpuset);

  for(int i = 0; i < CPU_SETSIZE; i++)
    {
      int core_id;
      std::ifstream ifs;
      ghc::filesystem::path path;

      if(!CPU_ISSET(i,&cpuset))
        continue;

      path = ::generate_cpu_core_id_path(i);

      ifs.open(path);
      if(!ifs)
        break;

      ifs >> core_id;

      c2c[core_id].insert(i);

      ifs.close();
    }

  return c2c;
}

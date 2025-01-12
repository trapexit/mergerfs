#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>

#ifdef __FreeBSD__
# define sched_getaffinity(pid, size, set) 0
# define pthread_setaffinity_np(thread, size, set) 0
#else
# include <sched.h>
#endif

#include <set>
#include <unordered_map>
#include <vector>


class CPU
{
public:
  typedef std::vector<pthread_t> ThreadIdVec;
  typedef std::vector<int> CPUVec;
  typedef std::unordered_map<int,int> CPU2CoreMap;
  typedef std::unordered_map<int,std::set<int>> Core2CPUsMap;

public:
  static int count();
  static int getaffinity(const pthread_t thread_id, cpu_set_t *cpuset);
  static int setaffinity(const pthread_t thread_id, cpu_set_t *cpuset);
  static int setaffinity(const pthread_t thread_id, const int cpu);
  static int setaffinity(const pthread_t thread_id, const std::set<int> cpus);

  static CPU::CPUVec cpus();
  static CPU::CPU2CoreMap cpu2core();
  static CPU::Core2CPUsMap core2cpus();
};

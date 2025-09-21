#include "pin_threads.hpp"

#include "syslog.hpp"

void
PinThreads::R1L(const CPU::ThreadIdVec threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : threads_)
    CPU::setaffinity(thread_id,cpus.front());
}

void
PinThreads::R1P(const CPU::ThreadIdVec threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
}

void
PinThreads::RP1L(const CPU::ThreadIdVec read_threads_,
                 const CPU::ThreadIdVec process_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,cpus.front());
  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,cpus.front());
}

void
PinThreads::RP1P(const CPU::ThreadIdVec read_threads_,
                 const CPU::ThreadIdVec process_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
}


void
PinThreads::R1LP1L(const CPU::ThreadIdVec read_threads_,
                   const CPU::ThreadIdVec process_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,cpus.front());

  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,cpus.back());
}


void
PinThreads::R1PP1P(const CPU::ThreadIdVec read_threads_,
                   const CPU::ThreadIdVec process_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);

  if(core2cpus.size() > 1)
    core2cpus.erase(core2cpus.begin());

  for(auto const thread_id : process_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);
}


void
PinThreads::RPSL(const CPU::ThreadIdVec read_threads_,
                 const CPU::ThreadIdVec process_threads_)
{
  CPU::CPUVec cpus;

  cpus = CPU::cpus();
  if(cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    {
      if(cpus.empty())
        cpus = CPU::cpus();
      CPU::setaffinity(thread_id,cpus.back());
      cpus.pop_back();
    }

  for(auto const thread_id : process_threads_)
    {
      if(cpus.empty())
        cpus = CPU::cpus();
      CPU::setaffinity(thread_id,cpus.back());
      cpus.pop_back();
    }
}


void
PinThreads::RPSP(const CPU::ThreadIdVec read_threads_,
                 const CPU::ThreadIdVec process_threads_)
{
  CPU::Core2CPUsMap core2cpus;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    {
      if(core2cpus.empty())
        core2cpus = CPU::core2cpus();
      CPU::setaffinity(thread_id,core2cpus.begin()->second);
      core2cpus.erase(core2cpus.begin());
    }

  for(auto const thread_id : process_threads_)
    {
      if(core2cpus.empty())
        core2cpus = CPU::core2cpus();
      CPU::setaffinity(thread_id,core2cpus.begin()->second);
      core2cpus.erase(core2cpus.begin());
    }
}


void
PinThreads::R1PPSP(const CPU::ThreadIdVec read_threads_,
                   const CPU::ThreadIdVec process_threads_)
{
  CPU::Core2CPUsMap core2cpus;
  CPU::Core2CPUsMap leftover;

  core2cpus = CPU::core2cpus();
  if(core2cpus.empty())
    return;

  for(auto const thread_id : read_threads_)
    CPU::setaffinity(thread_id,core2cpus.begin()->second);

  core2cpus.erase(core2cpus.begin());
  if(core2cpus.empty())
    core2cpus = CPU::core2cpus();
  leftover = core2cpus;

  for(auto const thread_id : process_threads_)
    {
      if(core2cpus.empty())
        core2cpus = leftover;
      CPU::setaffinity(thread_id,core2cpus.begin()->second);
      core2cpus.erase(core2cpus.begin());
    }
}


void
PinThreads::pin(const CPU::ThreadIdVec read_threads_,
                const CPU::ThreadIdVec process_threads_,
                const std::string      type_)
{
  if(type_.empty() || (type_ == "false"))
    return;
  if(type_ == "R1L")
    return PinThreads::R1L(read_threads_);
  if(type_ == "R1P")
    return PinThreads::R1P(read_threads_);
  if(type_ == "RP1L")
    return PinThreads::RP1L(read_threads_,process_threads_);
  if(type_ == "RP1P")
    return PinThreads::RP1P(read_threads_,process_threads_);
  if(type_ == "R1LP1L")
    return PinThreads::R1LP1L(read_threads_,process_threads_);
  if(type_ == "R1PP1P")
    return PinThreads::R1PP1P(read_threads_,process_threads_);
  if(type_ == "RPSL")
    return PinThreads::RPSL(read_threads_,process_threads_);
  if(type_ == "RPSP")
    return PinThreads::RPSP(read_threads_,process_threads_);
  if(type_ == "R1PPSP")
    return PinThreads::R1PPSP(read_threads_,process_threads_);

  SysLog::warning("Invalid pin-threads type, ignoring: {}",
                  type_);
}

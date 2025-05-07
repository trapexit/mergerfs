#pragma once

#include "cpu.hpp"

#include <string>

namespace PinThreads
{
  void R1L(const CPU::ThreadIdVec threads);
  void R1P(const CPU::ThreadIdVec threads);
  void RP1L(const CPU::ThreadIdVec read_threads,
            const CPU::ThreadIdVec process_threads);
  void RP1P(const CPU::ThreadIdVec read_threads,
            const CPU::ThreadIdVec process_threads);
  void R1LP1L(const CPU::ThreadIdVec read_threads,
              const CPU::ThreadIdVec process_threads);
  void R1PP1P(const CPU::ThreadIdVec read_threads,
              const CPU::ThreadIdVec process_threads);
  void RPSL(const CPU::ThreadIdVec read_threads,
            const CPU::ThreadIdVec process_threads);
  void RPSP(const CPU::ThreadIdVec read_threads,
            const CPU::ThreadIdVec process_threads);
  void R1PPSP(const CPU::ThreadIdVec read_threads,
              const CPU::ThreadIdVec process_threads);
  void pin(const CPU::ThreadIdVec read_threads,
           const CPU::ThreadIdVec process_threads,
           const std::string      type);
}

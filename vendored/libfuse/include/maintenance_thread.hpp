#pragma once

#include <functional>
#include <vector>


class MaintenanceThread
{
public:
  static void setup();
  static void stop();
  static void push_job(const std::function<void(int)> &func);
};

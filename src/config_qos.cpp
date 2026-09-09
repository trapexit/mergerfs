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

#include "config_qos.hpp"

#include "from_string.hpp"
#include "qos.hpp"
#include "syslog.hpp"

#include "fmt/core.h"

#include <errno.h>
#include <stdlib.h>


QoS::QoS(const bool b_)
{
  qos::enable(b_);
}

std::string
QoS::to_string(void) const
{
  return (qos::enabled() ? "true" : "false");
}

int
QoS::from_string(const std::string_view s_)
{
  int rv;
  bool enable;

  rv = str::from(s_,&enable);
  if(rv)
    return rv;

  qos::enable(enable);

  return 0;
}

std::string
QoSRules::to_string(void) const
{
  return qos::rules_path();
}

int
QoSRules::from_string(const std::string_view s_)
{
  int rv;
  std::string err;
  const std::string path{s_};

  rv = qos::load_file(path,&err);
  if(rv < 0)
    {
      SysLog::error("qos.rules: {}: {}",path,err);
      return rv;
    }

  SysLog::info("qos.rules: loaded {}",path);

  return 0;
}

QoSRuleSet::QoSRuleSet()
{
  ro = true;
}

std::string
QoSRuleSet::to_string(void) const
{
  qos::RuleSet::Ptr rs = qos::ruleset();

  if(rs == nullptr)
    return {};

  return rs->to_string();
}

int
QoSRuleSet::from_string(const std::string_view)
{
  return -EINVAL;
}

std::string
QoSStats::to_string(void) const
{
  return qos::stats();
}

int
QoSStats::from_string(const std::string_view s_)
{
  if(s_ != "reset")
    return -EINVAL;

  qos::reset_stats();

  return 0;
}

std::string
QoSMaxSleepers::to_string(void) const
{
  return std::to_string(qos::max_sleepers.load(std::memory_order_relaxed));
}

int
QoSMaxSleepers::from_string(const std::string_view s_)
{
  int rv;
  int n;

  rv = str::from(s_,&n);
  if(rv)
    return rv;
  // -1 keeps the automatic sizing.
  if(n < -1)
    return -EINVAL;

  qos::max_sleepers.store(n,std::memory_order_relaxed);

  return 0;
}

std::string
QoSMaxSleepMS::to_string(void) const
{
  return std::to_string(qos::max_sleep_ns.load(std::memory_order_relaxed) /
                        (1000 * 1000));
}

int
QoSMaxSleepMS::from_string(const std::string_view s_)
{
  int rv;
  int n;

  rv = str::from(s_,&n);
  if(rv)
    return rv;
  if(n < 0)
    return -EINVAL;

  qos::max_sleep_ns.store(static_cast<u64>(n) * 1000 * 1000,
                          std::memory_order_relaxed);

  return 0;
}

std::string
QoSDistressMS::to_string(void) const
{
  return std::to_string(qos::distress_floor_ns.load(std::memory_order_relaxed) /
                        (1000 * 1000));
}

int
QoSDistressMS::from_string(const std::string_view s_)
{
  int rv;
  int n;

  rv = str::from(s_,&n);
  if(rv)
    return rv;
  if(n < 0)
    return -EINVAL;

  qos::distress_floor_ns.store(static_cast<u64>(n) * 1000 * 1000,
                               std::memory_order_relaxed);

  return 0;
}

std::string
QoSDistressFactor::to_string(void) const
{
  return fmt::format("{:.2f}",
                     qos::distress_factor.load(std::memory_order_relaxed));
}

int
QoSDistressFactor::from_string(const std::string_view s_)
{
  const std::string str{s_};
  char *end = nullptr;

  errno = 0;
  const double v = ::strtod(str.c_str(),&end);
  if(errno || (end == str.c_str()) || (*end != '\0') || (v < 1.0))
    return -EINVAL;

  qos::distress_factor.store(v,std::memory_order_relaxed);

  return 0;
}

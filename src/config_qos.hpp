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

#pragma once

#include "tofrom_string.hpp"


// qos=<bool>
class QoS : public ToFromString
{
public:
  QoS(const bool);

public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.rules=<filepath>
//
// Assigning a path loads it. Assigning the same path again is how a
// ruleset is reloaded at runtime. A file that fails to parse leaves
// the running ruleset in place.
class QoSRules : public ToFromString
{
public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.ruleset -- read only, the ruleset as parsed.
class QoSRuleSet : public ToFromString
{
public:
  QoSRuleSet();

public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.stats -- counters. Assigning "reset" zeroes them.
class QoSStats : public ToFromString
{
public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.max-sleepers=<int>
class QoSMaxSleepers : public ToFromString
{
public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.distress-ms=<int>
class QoSDistressMS : public ToFromString
{
public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.distress-factor=<float>
class QoSDistressFactor : public ToFromString
{
public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

// qos.max-sleep-ms=<int>
class QoSMaxSleepMS : public ToFromString
{
public:
  std::string to_string(void) const final;
  int from_string(const std::string_view) final;
};

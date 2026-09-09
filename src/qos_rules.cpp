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

#include "qos_rules.hpp"

#include "fmt/core.h"

#include <errno.h>
#include <fnmatch.h>

#include <cctype>
#include <cstdlib>
#include <sstream>

using qos::Class;
using qos::Field;
using qos::Op;
using qos::Rule;
using qos::RuleSet;


std::string
qos::ioprio::to_string(const int value_)
{
  if(value_ == qos::UNSET)
    return "unset";

  switch(qos::ioprio::to_class(value_))
    {
    case qos::ioprio::CLASS_NONE:
      return "none";
    case qos::ioprio::CLASS_RT:
      return fmt::format("rt:{}",qos::ioprio::to_data(value_));
    case qos::ioprio::CLASS_BE:
      return fmt::format("be:{}",qos::ioprio::to_data(value_));
    case qos::ioprio::CLASS_IDLE:
      return "idle";
    }

  return "unset";
}

int
qos::ioprio::from_string(const std::string_view s_,
                         int                   *value_)
{
  if(s_ == "idle")
    {
      // The idle class carries no priority data.
      *value_ = qos::ioprio::value(qos::ioprio::CLASS_IDLE,0);
      return 0;
    }

  if(s_ == "none")
    {
      *value_ = qos::ioprio::value(qos::ioprio::CLASS_NONE,0);
      return 0;
    }

  int klass;
  if(s_.rfind("rt:",0) == 0)
    klass = qos::ioprio::CLASS_RT;
  else if(s_.rfind("be:",0) == 0)
    klass = qos::ioprio::CLASS_BE;
  else
    return -EINVAL;

  const std::string_view data = s_.substr(3);
  if((data.size() != 1) || !std::isdigit(static_cast<unsigned char>(data[0])))
    return -EINVAL;

  const int n = (data[0] - '0');
  if(n > 7)
    return -EINVAL;

  *value_ = qos::ioprio::value(klass,n);

  return 0;
}

int
qos::parse_size(const std::string_view s_,
                u64                   *bytes_)
{
  if(s_.empty())
    return -EINVAL;

  std::size_t i = 0;
  while((i < s_.size()) &&
        (std::isdigit(static_cast<unsigned char>(s_[i])) || (s_[i] == '.')))
    i++;

  if(i == 0)
    return -EINVAL;

  const std::string num(s_.substr(0,i));

  errno = 0;
  char *end = nullptr;
  const double value = ::strtod(num.c_str(),&end);
  if(errno || (end == num.c_str()) || (*end != '\0') || (value < 0))
    return -EINVAL;

  std::string suffix(s_.substr(i));
  // Accept "MB" and "MiB" as aliases of "M". mergerfs treats all size
  // suffixes as binary multiples, matching minfreespace and friends.
  if((suffix.size() > 1) && (suffix.back() == 'B'))
    suffix.pop_back();
  if((suffix.size() > 1) && (suffix.back() == 'i'))
    suffix.pop_back();

  u64 mult;
  if(suffix.empty())
    mult = 1ULL;
  else if((suffix == "K") || (suffix == "k"))
    mult = 1024ULL;
  else if((suffix == "M") || (suffix == "m"))
    mult = 1024ULL * 1024;
  else if((suffix == "G") || (suffix == "g"))
    mult = 1024ULL * 1024 * 1024;
  else if((suffix == "T") || (suffix == "t"))
    mult = 1024ULL * 1024 * 1024 * 1024;
  else
    return -EINVAL;

  *bytes_ = static_cast<u64>(value * static_cast<double>(mult));

  return 0;
}

bool
Rule::matches(const std::string &cgroup_,
              const std::string &comm_,
              const u32          uid_,
              const u32          gid_) const
{
  switch(field)
    {
    case Field::UID:
      return (uid_ == number);
    case Field::GID:
      return (gid_ == number);
    case Field::CGROUP:
    case Field::COMM:
      break;
    }

  const std::string &subject = ((field == Field::CGROUP) ? cgroup_ : comm_);

  if(op == Op::EQ)
    return (subject == pattern);

  return (::fnmatch(pattern.c_str(),subject.c_str(),0) == 0);
}

const Class *
RuleSet::find_class(const std::string_view name_) const
{
  for(const auto &c : _classes)
    {
      if(c->name == name_)
        return c.get();
    }

  return nullptr;
}

const Class *
RuleSet::classify(const std::string &cgroup_,
                  const std::string &comm_,
                  const u32          uid_,
                  const u32          gid_) const
{
  for(const auto &rule : _rules)
    {
      if(rule.matches(cgroup_,comm_,uid_,gid_))
        return rule.cls;
    }

  return _default;
}

RuleSet::Ptr
RuleSet::make_default()
{
  auto rs = std::make_shared<RuleSet>();

  rs->_classes.push_back(std::make_unique<Class>("default"));
  rs->_default = rs->_classes.back().get();
  rs->_inert   = true;

  return rs;
}

namespace
{
  std::vector<std::string>
  tokenize(const std::string &line_)
  {
    std::vector<std::string> tokens;
    std::istringstream iss(line_);
    std::string tok;

    while(iss >> tok)
      tokens.push_back(tok);

    return tokens;
  }

  int
  parse_field(const std::string &s_,
              Field             *field_)
  {
    if(s_ == "cgroup")
      *field_ = Field::CGROUP;
    else if(s_ == "comm")
      *field_ = Field::COMM;
    else if(s_ == "uid")
      *field_ = Field::UID;
    else if(s_ == "gid")
      *field_ = Field::GID;
    else
      return -EINVAL;

    return 0;
  }
}

RuleSet::Ptr
RuleSet::parse(const std::string_view text_,
               std::string           *err_)
{
  auto rs = std::make_shared<RuleSet>();
  std::istringstream istrm{std::string(text_)};
  std::string line;
  std::string default_name;
  int lineno = 0;

  auto fail =
    [&](const std::string &msg_) -> RuleSet::Ptr
    {
      *err_ = fmt::format("line {}: {}",lineno,msg_);
      return nullptr;
    };

  while(std::getline(istrm,line,'\n'))
    {
      lineno++;

      const std::size_t hash = line.find('#');
      if(hash != std::string::npos)
        line.erase(hash);

      auto tokens = ::tokenize(line);
      if(tokens.empty())
        continue;

      if(tokens[0] == "class")
        {
          if(tokens.size() < 2)
            return fail("'class' needs a name");
          if(rs->find_class(tokens[1]))
            return fail(fmt::format("duplicate class '{}'",tokens[1]));

          auto cls = std::make_unique<Class>(tokens[1]);

          for(std::size_t i = 2; i < tokens.size(); i++)
            {
              const std::size_t eq = tokens[i].find('=');
              if(eq == std::string::npos)
                return fail(fmt::format("expected key=value, got '{}'",tokens[i]));

              const std::string key = tokens[i].substr(0,eq);
              const std::string val = tokens[i].substr(eq + 1);

              if(key == "ioprio")
                {
                  if(qos::ioprio::from_string(val,&cls->ioprio))
                    return fail(fmt::format("invalid ioprio '{}'",val));
                }
              else if(key == "nice")
                {
                  const int n = std::atoi(val.c_str());
                  if((n < -20) || (n > 19))
                    return fail(fmt::format("nice out of range: '{}'",val));
                  cls->nice = n;
                }
              else if(key == "rate")
                {
                  if(qos::parse_size(val,&cls->rate))
                    return fail(fmt::format("invalid rate '{}'",val));
                }
              else if(key == "burst")
                {
                  if(qos::parse_size(val,&cls->burst))
                    return fail(fmt::format("invalid burst '{}'",val));
                }
              else
                {
                  return fail(fmt::format("unknown class key '{}'",key));
                }
            }

          // A bucket with no explicit depth holds one second of
          // traffic, which is enough to absorb a readahead burst
          // without letting an idle class bank unlimited credit.
          if(cls->rate && (cls->burst == 0))
            cls->burst = cls->rate;
          if(cls->burst < cls->rate)
            cls->burst = cls->rate;

          cls->tokens = cls->burst;

          if((cls->ioprio != qos::UNSET) ||
             (cls->nice   != qos::UNSET) ||
             (cls->rate   != 0))
            rs->_inert = false;

          rs->_classes.push_back(std::move(cls));
        }
      else if(tokens[0] == "match")
        {
          if(tokens.size() != 6)
            return fail("expected: match <field> <=|~> <pattern> -> <class>");
          if(tokens[4] != "->")
            return fail(fmt::format("expected '->', got '{}'",tokens[4]));

          Rule rule{};

          if(::parse_field(tokens[1],&rule.field))
            return fail(fmt::format("unknown field '{}'",tokens[1]));

          if(tokens[2] == "=")
            rule.op = Op::EQ;
          else if(tokens[2] == "~")
            rule.op = Op::GLOB;
          else
            return fail(fmt::format("unknown operator '{}'",tokens[2]));

          if((rule.field == Field::UID) || (rule.field == Field::GID))
            {
              if(rule.op != Op::EQ)
                return fail("uid/gid only support the '=' operator");

              char *end = nullptr;
              const unsigned long n = ::strtoul(tokens[3].c_str(),&end,10);
              if((end == tokens[3].c_str()) || (*end != '\0'))
                return fail(fmt::format("invalid uid/gid '{}'",tokens[3]));
              rule.number = static_cast<u32>(n);
            }

          rule.pattern = tokens[3];

          rule.cls = rs->find_class(tokens[5]);
          if(rule.cls == nullptr)
            return fail(fmt::format("unknown class '{}' (classes must be "
                                    "defined before use)",tokens[5]));

          if(rule.field == Field::CGROUP)
            rs->_needs_cgroup = true;
          if(rule.field == Field::COMM)
            rs->_needs_comm = true;

          rs->_rules.push_back(std::move(rule));
        }
      else if(tokens[0] == "default")
        {
          if(tokens.size() != 2)
            return fail("expected: default <class>");
          default_name = tokens[1];
        }
      else
        {
          return fail(fmt::format("unknown directive '{}'",tokens[0]));
        }
    }

  if(default_name.empty())
    {
      // No explicit default: unmatched processes keep whatever
      // priority they already had.
      rs->_classes.push_back(std::make_unique<Class>("unclassified"));
      rs->_default = rs->_classes.back().get();
    }
  else
    {
      rs->_default = rs->find_class(default_name);
      if(rs->_default == nullptr)
        {
          *err_ = fmt::format("default names unknown class '{}'",default_name);
          return nullptr;
        }
    }

  if(rs->_rules.empty() && rs->_default->ioprio == qos::UNSET &&
     rs->_default->nice == qos::UNSET && rs->_default->rate == 0)
    rs->_inert = true;

  return rs;
}

std::string
RuleSet::to_string() const
{
  std::string s;

  for(const auto &c : _classes)
    {
      s += fmt::format("class {} ioprio={} nice={} rate={} burst={}\n",
                       c->name,
                       qos::ioprio::to_string(c->ioprio),
                       ((c->nice == qos::UNSET) ? "unset" : std::to_string(c->nice)),
                       c->rate,
                       c->burst);
    }

  for(const auto &r : _rules)
    {
      const char *field = "";
      switch(r.field)
        {
        case Field::CGROUP: field = "cgroup"; break;
        case Field::COMM:   field = "comm";   break;
        case Field::UID:    field = "uid";    break;
        case Field::GID:    field = "gid";    break;
        }

      s += fmt::format("match {} {} {} -> {}\n",
                       field,
                       ((r.op == Op::EQ) ? "=" : "~"),
                       r.pattern,
                       r.cls->name);
    }

  if(_default)
    s += fmt::format("default {}\n",_default->name);

  return s;
}

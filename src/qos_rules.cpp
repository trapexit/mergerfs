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
using qos::Condition;
using qos::Direction;
using qos::Field;
using qos::MatchOp;
using qos::Rule;
using qos::RuleSet;
using qos::Subject;


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
qos::Condition::matches(const Subject &s_) const
{
  const std::string *subject;

  switch(field)
    {
    case Field::UID:
      return (s_.uid == number);
    case Field::GID:
      return (s_.gid == number);
    case Field::OP:
      return (s_.dir == dir);
    case Field::CGROUP:
      subject = s_.cgroup;
      break;
    case Field::COMM:
      subject = s_.comm;
      break;
    case Field::CMDLINE:
      subject = s_.cmdline;
      break;
    case Field::PATH:
      subject = s_.path;
      break;
    default:
      return false;
    }

  if(subject == nullptr)
    return false;

  if(op == MatchOp::EQ)
    return (*subject == pattern);

  // FNM_PATHNAME is deliberately not set: a rule reading
  // "path ~ /TV/*" is expected to cover everything beneath /TV, which
  // is what an administrator writing a folder rule means.
  return (::fnmatch(pattern.c_str(),subject->c_str(),0) == 0);
}

bool
qos::Rule::matches(const Subject &s_) const
{
  for(const auto &cond : conditions)
    {
      if(!cond.matches(s_))
        return false;
    }

  return true;
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
RuleSet::classify(const Subject &s_) const
{
  for(const auto &rule : _rules)
    {
      if(rule.matches(s_))
        return rule.cls;
    }

  return _default;
}

u64
RuleSet::capacity(const std::string &resource_) const
{
  auto i = _capacity.find(resource_);

  if(i != _capacity.end())
    return i->second;

  return _capacity_default;
}

u64
RuleSet::floor_for(const Class       *cls_,
                   const std::string &resource_) const
{
  if(cls_->floor)
    return cls_->floor;

  if(cls_->floor_pct == 0)
    return 0;

  const u64 cap = capacity(resource_);
  if(cap == 0)
    return 0;

  return ((cap * cls_->floor_pct) / 100);
}

u64
RuleSet::rate_for(const Class       *cls_,
                  const std::string &resource_) const
{
  if(cls_->rate)
    return cls_->rate;

  if(cls_->pct == 0)
    return 0;

  const u64 cap = capacity(resource_);

  // No capacity known for this resource means a percentage cannot be
  // turned into a number. Running unlimited is the safe reading: a
  // guessed ceiling would throttle traffic against a figure nobody
  // supplied.
  if(cap == 0)
    return 0;

  return ((cap * cls_->pct) / 100);
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
  // Whitespace separated, except that a token may be wrapped in single
  // or double quotes to keep spaces in it. Necessary rather than
  // decorative: the process names this has to match include
  // "Plex Transcoder" and "Plex Media Scanner", and a command line is
  // nothing but spaces.
  std::vector<std::string>
  tokenize(const std::string &line_)
  {
    std::vector<std::string> tokens;
    std::string tok;
    bool in_tok = false;
    char quote = '\0';

    for(std::size_t i = 0; i < line_.size(); i++)
      {
        const char c = line_[i];

        if(quote)
          {
            if(c == quote)
              quote = '\0';
            else
              tok += c;
            continue;
          }

        if((c == '"') || (c == '\''))
          {
            // A quote starts a token even when empty, so that '' is a
            // deliberate empty pattern rather than nothing at all.
            quote  = c;
            in_tok = true;
            continue;
          }

        if(std::isspace(static_cast<unsigned char>(c)))
          {
            if(in_tok)
              {
                tokens.push_back(tok);
                tok.clear();
                in_tok = false;
              }
            continue;
          }

        tok += c;
        in_tok = true;
      }

    if(in_tok)
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
    else if(s_ == "cmdline")
      *field_ = Field::CMDLINE;
    else if(s_ == "path")
      *field_ = Field::PATH;
    else if(s_ == "op")
      *field_ = Field::OP;
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
              // `protect` is a bare flag rather than key=value.
              if(tokens[i] == "protect")
                {
                  cls->protect = true;
                  continue;
                }

              if(tokens[i] == "critical")
                {
                  cls->critical = true;
                  continue;
                }

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
                  if(!val.empty() && (val.back() == '%'))
                    {
                      const std::string num = val.substr(0,val.size() - 1);
                      char *end = nullptr;
                      const unsigned long pct = ::strtoul(num.c_str(),&end,10);
                      if((end == num.c_str()) || (*end != '\0') ||
                         (pct == 0) || (pct > 100))
                        return fail(fmt::format("percentage must be 1-100: '{}'",val));
                      cls->pct = static_cast<u32>(pct);
                    }
                  else if(qos::parse_size(val,&cls->rate))
                    {
                      return fail(fmt::format("invalid rate '{}'",val));
                    }
                }
              else if(key == "burst")
                {
                  if(qos::parse_size(val,&cls->burst))
                    return fail(fmt::format("invalid burst '{}'",val));
                }
              else if(key == "yield")
                {
                  char *end = nullptr;
                  const unsigned long y = ::strtoul(val.c_str(),&end,10);
                  if((end == val.c_str()) || (*end != '\0') || (y > 100))
                    return fail(fmt::format("yield must be 0-100: '{}'",val));
                  cls->yield = static_cast<u32>(y);
                }
              else if(key == "floor")
                {
                  if(!val.empty() && (val.back() == '%'))
                    {
                      const std::string num = val.substr(0,val.size() - 1);
                      char *end = nullptr;
                      const unsigned long pct = ::strtoul(num.c_str(),&end,10);
                      if((end == num.c_str()) || (*end != '\0') ||
                         (pct == 0) || (pct > 100))
                        return fail(fmt::format("floor percentage must be 1-100: '{}'",val));
                      cls->floor_pct = static_cast<u32>(pct);
                    }
                  else if(qos::parse_size(val,&cls->floor))
                    {
                      return fail(fmt::format("invalid floor '{}'",val));
                    }
                }
              else
                {
                  return fail(fmt::format("unknown class key '{}'",key));
                }
            }

          // A bucket with no explicit depth holds one second of
          // traffic, which is enough to absorb a readahead burst
          // without letting an idle class bank unlimited credit. For
          // a percentage rate the depth can only be resolved once the
          // serving resource is known, so it is left at zero here.
          if(cls->rate && (cls->burst < cls->rate))
            cls->burst = cls->rate;

          if(cls->protect)
            rs->_has_protected = true;

          if((cls->ioprio != qos::UNSET) ||
             (cls->nice   != qos::UNSET) ||
             cls->limited() ||
             cls->adaptive() ||
             cls->protect ||
             cls->critical)
            rs->_inert = false;

          rs->_classes.push_back(std::move(cls));
        }
      else if(tokens[0] == "capacity")
        {
          if(tokens.size() != 3)
            return fail("expected: capacity <branch-path|default> <rate>");

          u64 rate;
          if(qos::parse_size(tokens[2],&rate))
            return fail(fmt::format("invalid capacity '{}'",tokens[2]));

          if(tokens[1] == "default")
            rs->_capacity_default = rate;
          else
            rs->_capacity[tokens[1]] = rate;
        }
      else if(tokens[0] == "match")
        {
          Rule rule{};
          std::size_t i = 1;

          // Conditions are (field, operator, pattern) triples running
          // up to the arrow. Every one must match for the rule to
          // fire.
          while((i < tokens.size()) && (tokens[i] != "->"))
            {
              if((i + 2) >= tokens.size())
                return fail("expected: match <field> <=|~> <pattern> ... -> <class>");

              Condition cond{};

              if(::parse_field(tokens[i],&cond.field))
                return fail(fmt::format("unknown field '{}'",tokens[i]));

              if(tokens[i+1] == "=")
                cond.op = MatchOp::EQ;
              else if(tokens[i+1] == "~")
                cond.op = MatchOp::GLOB;
              else
                return fail(fmt::format("unknown operator '{}'",tokens[i+1]));

              cond.pattern = tokens[i+2];

              switch(cond.field)
                {
                case Field::UID:
                case Field::GID:
                  {
                    if(cond.op != MatchOp::EQ)
                      return fail("uid/gid only support the '=' operator");

                    char *end = nullptr;
                    const unsigned long n = ::strtoul(cond.pattern.c_str(),&end,10);
                    if((end == cond.pattern.c_str()) || (*end != '\0'))
                      return fail(fmt::format("invalid uid/gid '{}'",cond.pattern));
                    cond.number = static_cast<u32>(n);
                  }
                  break;

                case Field::OP:
                  if(cond.op != MatchOp::EQ)
                    return fail("op only supports the '=' operator");
                  if(cond.pattern == "read")
                    cond.dir = Direction::READ;
                  else if(cond.pattern == "write")
                    cond.dir = Direction::WRITE;
                  else
                    return fail(fmt::format("op must be read or write, got '{}'",
                                            cond.pattern));
                  break;

                case Field::CGROUP:
                  rs->_needs_cgroup = true;
                  break;

                case Field::COMM:
                  rs->_needs_comm = true;
                  break;

                case Field::CMDLINE:
                  rs->_needs_cmdline = true;
                  break;

                case Field::PATH:
                  rs->_needs_path = true;
                  break;
                }

              rule.conditions.push_back(std::move(cond));

              i += 3;
            }

          if(rule.conditions.empty())
            return fail("match needs at least one condition");
          if((i >= tokens.size()) || (tokens[i] != "->"))
            return fail("expected '->' after the last condition");
          if((i + 2) != tokens.size())
            return fail("expected exactly one class name after '->'");

          rule.cls = rs->find_class(tokens[i+1]);
          if(rule.cls == nullptr)
            return fail(fmt::format("unknown class '{}' (classes must be "
                                    "defined before use)",tokens[i+1]));

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

  if(rs->_rules.empty() &&
     (rs->_default->ioprio == qos::UNSET) &&
     (rs->_default->nice == qos::UNSET) &&
     !rs->_default->limited() &&
     !rs->_default->adaptive() &&
     !rs->_default->protect &&
     !rs->_default->critical)
    rs->_inert = true;

  return rs;
}

std::string
RuleSet::to_string() const
{
  std::string s;

  for(const auto &[resource,rate] : _capacity)
    s += fmt::format("capacity {} {}\n",resource,rate);
  if(_capacity_default)
    s += fmt::format("capacity default {}\n",_capacity_default);

  for(const auto &c : _classes)
    {
      s += fmt::format("class {}{}{} ioprio={} nice={} rate={} burst={} "
                       "yield={} floor={}\n",
                       c->name,
                       (c->protect ? " protect" : ""),
                       (c->critical ? " critical" : ""),
                       qos::ioprio::to_string(c->ioprio),
                       ((c->nice == qos::UNSET) ? "unset" : std::to_string(c->nice)),
                       (c->pct ? fmt::format("{}%",c->pct) : std::to_string(c->rate)),
                       c->burst,
                       c->yield,
                       (c->floor_pct ? fmt::format("{}%",c->floor_pct)
                                     : std::to_string(c->floor)));
    }

  for(const auto &r : _rules)
    {
      s += "match";

      for(const auto &cond : r.conditions)
        {
          const char *field = "";
          switch(cond.field)
            {
            case Field::CGROUP: field = "cgroup"; break;
            case Field::COMM:   field = "comm";   break;
            case Field::CMDLINE: field = "cmdline"; break;
            case Field::UID:    field = "uid";    break;
            case Field::GID:    field = "gid";    break;
            case Field::PATH:   field = "path";   break;
            case Field::OP:     field = "op";     break;
            }

          s += fmt::format(" {} {} {}",
                           field,
                           ((cond.op == MatchOp::EQ) ? "=" : "~"),
                           cond.pattern);
        }

      s += fmt::format(" -> {}\n",r.cls->name);
    }

  if(_default)
    s += fmt::format("default {}\n",_default->name);

  return s;
}

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

#include "qos_class.hpp"

#include <memory>
#include <string>
#include <vector>


namespace qos
{
  // Which attribute of the calling process a rule tests.
  enum class Field
    {
     CGROUP,   // contents of /proc/<pid>/cgroup
     COMM,     // contents of /proc/<pid>/comm
     UID,
     GID
    };

  enum class Op
    {
     EQ,     // '='  exact string / numeric equality
     GLOB    // '~'  fnmatch(3) pattern
    };

  struct Rule
  {
    Field       field;
    Op          op;
    std::string pattern;
    u32         number;      // parsed form of `pattern` for UID/GID
    const Class *cls;        // owned by the RuleSet holding this rule

    bool matches(const std::string &cgroup,
                 const std::string &comm,
                 const u32          uid,
                 const u32          gid) const;
  };

  // An immutable, atomically published set of classes and rules.
  //
  // Loading a rules file builds a whole new RuleSet and swaps it in,
  // so a reload can never expose a half-applied configuration to a
  // request in flight. Readers keep a shared_ptr for the duration of
  // a request, which is what keeps a Class alive after a swap.
  class RuleSet
  {
  public:
    typedef std::shared_ptr<const RuleSet> Ptr;

  public:
    // Builds the ruleset every mount starts with: a single
    // pass-through class and no rules, which leaves behaviour
    // identical to qos being off.
    static Ptr make_default();

    // Parses `text` and returns the new ruleset, or nullptr on
    // error, in which case `err` describes the first problem found
    // and the caller must keep using the ruleset it already has.
    static Ptr parse(const std::string_view text,
                     std::string           *err);

  public:
    const Class *classify(const std::string &cgroup,
                          const std::string &comm,
                          const u32          uid,
                          const u32          gid) const;

    const Class *find_class(const std::string_view name) const;

    // True when no rule and no class can change a thread's behaviour,
    // letting the hot path skip reading /proc entirely.
    bool inert() const { return _inert; }

    bool needs_cgroup() const { return _needs_cgroup; }
    bool needs_comm() const { return _needs_comm; }

    const std::vector<std::unique_ptr<Class>> &classes() const { return _classes; }

    std::string to_string() const;

  private:
    std::vector<std::unique_ptr<Class>> _classes;
    std::vector<Rule>                   _rules;
    const Class                        *_default = nullptr;
    bool                                _inert = true;
    bool                                _needs_cgroup = false;
    bool                                _needs_comm = false;
  };

  // Parses "10M", "1.5MiB", "512K", "1G" and bare byte counts into
  // bytes. Returns 0 on success, -EINVAL otherwise.
  int parse_size(const std::string_view, u64 *bytes);
}

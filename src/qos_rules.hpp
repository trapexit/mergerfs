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
#include <unordered_map>
#include <vector>


namespace qos
{
  // Which attribute of a request a condition tests.
  enum class Field
    {
     CGROUP,   // contents of /proc/<pid>/cgroup -- identifies the container
     COMM,     // contents of /proc/<pid>/comm   -- identifies the program
     UID,
     GID,
     CMDLINE,  // full /proc/<pid>/cmdline, arguments joined by spaces
     PATH,     // path within the pool, e.g. /Movies/Dune (2021)/Dune.mkv
     OP        // "read" or "write"
    };

  enum class MatchOp
    {
     EQ,     // '='  exact string / numeric equality
     GLOB    // '~'  fnmatch(3) pattern, '/' not treated specially
    };

  enum class Direction
    {
     READ,
     WRITE
    };

  // Everything a rule can be matched against. Assembled once per
  // request, and only for the fields some rule actually uses.
  struct Subject
  {
    const std::string *cgroup  = nullptr;
    const std::string *comm    = nullptr;
    const std::string *cmdline = nullptr;
    const std::string *path   = nullptr;
    u32                uid    = 0;
    u32                gid    = 0;
    Direction          dir    = Direction::READ;
  };

  struct Condition
  {
    Field       field;
    MatchOp     op;
    std::string pattern;
    u32         number;   // parsed form of `pattern` for UID/GID
    Direction   dir;      // parsed form of `pattern` for OP

    bool matches(const Subject &) const;
  };

  // A rule fires when *every* one of its conditions matches, which is
  // what makes "this program, but only on this folder, and only when
  // writing" expressible as one rule.
  struct Rule
  {
    std::vector<Condition> conditions;
    const Class           *cls;   // owned by the RuleSet holding this rule

    bool matches(const Subject &) const;
  };

  // An immutable, atomically published set of classes, rules and
  // resource capacities.
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
    const Class *classify(const Subject &) const;

    const Class *find_class(const std::string_view name) const;

    // Measured or declared throughput of `resource` in bytes/sec,
    // falling back to the `capacity default` line. Zero when nothing
    // is known, which makes percentage rates unenforceable and is
    // reported as such.
    u64 capacity(const std::string &resource) const;

    // Resolves a class's configured rate against the resource
    // serving the request. Returns 0 for "unlimited".
    u64 rate_for(const Class *, const std::string &resource) const;

    // True when no rule and no class can change a thread's behaviour,
    // letting the hot path skip reading /proc entirely.
    bool inert() const { return _inert; }

    // Which subject fields any rule actually inspects. Each one that
    // is unused is a file read or a string copy skipped per request.
    bool needs_cgroup() const { return _needs_cgroup; }
    bool needs_comm() const { return _needs_comm; }
    bool needs_cmdline() const { return _needs_cmdline; }
    bool needs_path() const { return _needs_path; }

    // True when some class is marked `protect`, which is what turns
    // the adaptive governor on.
    bool has_protected() const { return _has_protected; }

    // Resolves a class's floor against the resource.
    u64 floor_for(const Class *, const std::string &resource) const;

    const std::vector<std::unique_ptr<Class>> &classes() const { return _classes; }

    std::string to_string() const;

  private:
    std::vector<std::unique_ptr<Class>>    _classes;
    std::vector<Rule>                      _rules;
    std::unordered_map<std::string,u64>    _capacity;
    u64                                    _capacity_default = 0;
    const Class                           *_default = nullptr;
    bool                                   _inert = true;
    bool                                   _needs_cgroup = false;
    bool                                   _needs_comm = false;
    bool                                   _needs_cmdline = false;
    bool                                   _needs_path = false;
    bool                                   _has_protected = false;
  };

  // Parses "10M", "1.5MiB", "512K", "1G" and bare byte counts into
  // bytes. Returns 0 on success, -EINVAL otherwise.
  int parse_size(const std::string_view, u64 *bytes);
}

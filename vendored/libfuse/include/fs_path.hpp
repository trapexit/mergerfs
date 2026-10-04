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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>


namespace fs
{
  // relpath: a relative path (mergerfs's "fusepath") backed by a fixed
  // 8192-byte inline buffer, NUL-terminated, no heap.
  //
  // Layout: rel data is anchored at _rel_start, which defaults to
  // REL_OFFSET (4096) on construction, clear(), assign(), and
  // consolidate_to_rel(). The bytes before _rel_start form the
  // "prefix area" -- a long base/prefix can be written there in place
  // via set_prefix() without touching the rel. The remaining bytes
  // after the rel are available for appending (filenames in
  // readdir-style loops, etc.).
  //
  // Buffer is sized at 2 * PATH_MAX. The fast path -- a prefix that
  // fits in REL_OFFSET bytes -- writes only the prefix area; rel
  // stays anchored at REL_OFFSET. If a caller hands in a prefix
  // larger than REL_OFFSET (contract violation or future-larger
  // PATH_MAX), set_prefix shifts the rel rightward to make room and
  // updates _rel_start, so the class's effective capacity is the
  // full BUF_SIZE - 1 rather than artificially capping at REL_OFFSET.
  // set_prefix's cost is O(prefix size + shift size), independent of
  // BUF_SIZE -- only the actually-used bytes are touched -- so the
  // buffer size is a stack-footprint question, not a hot-path cost.
  // FUSE worker threads use the pthread default stack (~8 MB on
  // Linux); a 4-relpath frame in fuse_link/fuse_rename is 32 KB,
  // ~0.4% of the stack budget.
  //
  // Mergerfs has exactly one path-manipulation type: a relative
  // fusepath. Branch paths and other absolute paths are stored as
  // std::string -- they are spliced into the prefix area of an
  // fs::relpath at syscall time but are never themselves mutated as
  // path objects.
  //
  // Canonical mergerfs form -- prefix has no trailing '/', rel has
  // no leading '/'. set_prefix() inserts exactly one '/' between a
  // non-empty prefix and a non-empty rel. No sanitization; violating
  // the contract yields double slashes. Mergerfs satisfies the
  // contract by construction:
  //   - libfuse's try_get_path() walks leaf-to-root via prepend(),
  //     then calls consolidate_to_rel(strip=true) which drops the
  //     leading '/' that prepend produces.
  //   - branch paths are realpath-canonicalized at config load so
  //     they have no trailing '/'.
  class relpath final
  {
  public:
    static constexpr std::size_t BUF_SIZE   = 8192;
    static constexpr std::size_t REL_OFFSET = 4096;

  public:
    relpath() noexcept;
    relpath(const char *s);
    relpath(std::string_view s);
    relpath(const std::string &s);
    relpath(const relpath&);
    relpath(relpath&&) noexcept;
    ~relpath() = default;

  public:
    relpath& operator=(const relpath&);
    relpath& operator=(relpath&&) noexcept;
    relpath& operator=(const char *s);
    relpath& operator=(std::string_view s);
    relpath& operator=(const std::string &s);

  public:
    // Replace value entirely. The input is stored verbatim -- no
    // leading-'/' stripping. The caller is responsible for canonical
    // rel form (no leading '/') if the result is going to be re-
    // prefixed via set_prefix(); mergerfs's pipeline guarantees this
    // via libfuse's consolidate_to_rel(strip=true) step. Storing
    // verbatim means an fs::relpath can also hold a fully-built
    // absolute fullpath (`branch.path / fusepath`) without losing the
    // leading '/'.
    relpath& assign(std::string_view s);

    // The hot-path primitive: replace the prefix in place. The rel
    // portion is undisturbed; only [_start, _rel_start) is touched
    // when the prefix fits in front of the current rel anchor.
    //
    // Always inserts exactly one '/' between non-empty prefix and rel.
    // No sanitization: prefix must have no trailing '/' (canonical
    // mergerfs branch path form). Empty prefix drops the prefix and
    // restores bare-rel state.
    //
    // Each call is a fresh prefix assignment. If the prefix is
    // larger than the current prefix area, the rel data is shifted
    // rightward to enlarge the prefix area; this fallback engages
    // only when p_size + sep > _rel_start, which is unreachable
    // under Linux PATH_MAX (the fast path always fires).
    //
    // Capacity contract: prefix + sep + rel must fit in BUF_SIZE - 1.
    // BUF_SIZE = 2 * PATH_MAX, so any prefix and rel that came from
    // the kernel necessarily fit. Exceeding the capacity is a
    // programming error; the syscall layer (Linux PATH_MAX) returns
    // ENAMETOOLONG for any oversized path long before the path can
    // be assembled here.
    void set_prefix(std::string_view prefix);

    // Restore to just the rel portion (start = _rel_start, no prefix).
    void clear_prefix() noexcept
    {
      _start = _rel_start;
      _size  = _rel_size;
      _buf[_start + _size] = '\0';
    }

    // Prepend "/component" to the path, growing leftward into the
    // prefix area. Used for libfuse's path-building pattern: walk
    // leaf-to-root, prepending each ancestor's name. The component
    // must be a bare name (no leading '/'); the slash separator is
    // inserted automatically.
    //
    // Capacity contract: the cumulative prepend chain must fit in
    // REL_OFFSET bytes (PATH_MAX); the kernel's own PATH_MAX guarantees
    // this for any path that could be delivered to a FUSE op.
    void prepend(std::string_view component) noexcept
    {
      const std::size_t total = component.size() + 1; // '/' + component
      // Contract guard: the cumulative prepend chain must not underflow
      // _start (a uint16_t). Silent underflow would land somewhere in
      // the upper half of _buf and corrupt unrelated data.
      assert(_start >= total);
      _start -= total;
      _buf[_start] = '/';
      std::memcpy(_buf + _start + 1,component.data(),component.size());
      _size += total;
      // _rel_size unchanged: the prefix area grew; the rel anchor stays.
    }

    // Move current data to the REL anchor, optionally stripping a
    // single leading '/' (the one libfuse's prepend chain produces).
    // After this call:
    //   _start     == REL_OFFSET
    //   _rel_start == REL_OFFSET
    //   _rel_size  == _size
    // so set_prefix from a caller works as expected. This is the
    // bridge between the prepend-style backward build (data in the
    // prefix area) and the set_prefix-style forward use (rel anchored
    // at REL_OFFSET).
    //
    // When strip_leading_slash is true and the first byte is '/', it
    // is dropped. The libfuse path-build chain produces this slash
    // via prepend; the canonical mergerfs rel form does not include
    // it. If the first byte is not '/' (e.g. a zero-prepend root
    // case), nothing is stripped.
    void consolidate_to_rel(const bool strip_leading_slash = false) noexcept
    {
      if(strip_leading_slash && _size > 0 && _buf[_start] == '/')
        {
          _start += 1;
          _size  -= 1;
        }
      if((_start != REL_OFFSET) && (_size > 0))
        std::memmove(_buf + REL_OFFSET,_buf + _start,_size);
      _start     = REL_OFFSET;
      _rel_start = REL_OFFSET;
      _rel_size  = _size;
      _buf[REL_OFFSET + _size] = '\0';
    }

    // View into the prefix area (i.e. [_start, REL_OFFSET)).
    //
    // NOTE: prefix() includes the trailing '/' that set_prefix
    // inserts between prefix and rel. set_prefix("/foo") produces a
    // path whose prefix() is "/foo/" -- the input doesn't have a
    // trailing slash but the view does. This is asymmetric but
    // matches the in-buffer representation and avoids a copy.
    //
    // Lifetime caveat applies: see the LIFETIME block on the
    // structural queries below.
    std::string_view prefix() const noexcept;

    // View into the rel portion: [REL_OFFSET, REL_OFFSET + _rel_size).
    std::string_view rel() const noexcept;

  public:
    // Inserts a single '/' between non-empty base and rel (no
    // sanitization -- same contract as set_prefix: base no trailing
    // '/', rel no leading '/'). The result is a relpath, so it can
    // be re-prefixed in place by a downstream set_prefix().
    static relpath concat(std::string_view base, std::string_view rel);
    relpath&  assign_concat(std::string_view base, std::string_view rel);

    relpath   operator/(std::string_view rhs) const;
    relpath&  operator/=(std::string_view rhs);

  public:
    // Structural queries -- return string_view into _buf, zero
    // allocation.
    //
    // LIFETIME: all of native/filename/parent_path/extension/stem/
    // prefix/rel return views into this path's internal buffer. ANY
    // non-const operation on *this (set_prefix, clear_prefix, assign,
    // assign_concat, append, resize, remove_filename, clear, swap,
    // operator=, operator/=) may invalidate previously-returned views
    // -- using one after a mutation is undefined behavior. Same rule
    // as std::string -> std::string_view, but easier to hit here
    // because set_prefix is the natural per-iteration mutation in the
    // per-branch loop pattern.
    std::string_view native()      const noexcept { return {c_str(),_size}; }
    std::string_view filename()    const noexcept;
    std::string_view parent_path() const noexcept;
    std::string_view extension()   const noexcept;
    std::string_view stem()        const noexcept;

    // Truncate to the directory form: drops the final component but
    // retains the trailing '/'. For example, "foo/bar" -> "foo/",
    // "foo/" stays "foo/". Use parent_path() if you want the
    // dirname without the trailing slash.
    relpath& remove_filename();

    // Lexical relativization. Both *this and base must be in the
    // same form (both with or both without leading '/'). Result is a
    // relpath (in canonical form, no leading '/'); on shape mismatch
    // or an unresolvable ".." in base, returns an empty relpath. A
    // successful relativization never produces an empty result (it
    // is "." for equal paths), so callers can use .empty() to detect
    // failure.
    relpath  lexically_relative(std::string_view base) const;

  public:
    const char* c_str()    const noexcept { return _buf + _start; }
    const char* data()     const noexcept { return _buf + _start; }
    std::size_t size()     const noexcept { return _size; }
    // Practical capacity for a freshly-assigned bare-rel path: the
    // rel area is REL_OFFSET-wide and holds the trailing NUL, leaving
    // (BUF_SIZE - REL_OFFSET - 1) bytes (= 4095) for the rel content.
    // A path that has both a prefix and a rel can total up to
    // BUF_SIZE - 1, but assigning more than capacity() to a bare-rel
    // overflows.
    std::size_t capacity() const noexcept { return BUF_SIZE - REL_OFFSET - 1; }
    bool        empty()    const noexcept { return _size == 0; }

    void clear() noexcept;
    // Capacity is fixed at construction; reserve() exists for
    // std::string API compatibility only. Anything exceeding
    // capacity() is a programming error.
    void reserve(std::size_t /*n*/) noexcept {}
    void swap(relpath &o) noexcept;

    // std::string-style mutators. These work from the END of the
    // current data. Growing zero-fills the new bytes to match
    // std::string semantics.
    void resize(std::size_t n);
    void append(const char*,std::size_t);
    void append(std::string_view);

    std::string string() const { return std::string(c_str(),_size); }

  public:
    // Implicit conversions are intentional: they let an fs::relpath
    // flow into fs::* syscall wrappers that take const char* /
    // std::string_view without an explicit .c_str() / .native() at
    // every call site. The lifetime caveat from native() applies to
    // both -- the returned pointer / view becomes invalid on any
    // non-const op.
    //
    // FOOTGUN: `std::string s = some_relpath;` silently allocates --
    // it picks `string(const char*)` via the implicit conversion.
    // When you actually want a std::string copy use `.string()`
    // explicitly so the cost is visible at the call site.
    operator const char*()      const noexcept { return c_str(); }
    operator std::string_view() const noexcept { return native(); }

    bool operator==(const relpath &o) const noexcept
    { return native() == o.native(); }
    bool operator==(std::string_view s) const noexcept
    { return native() == s; }
    bool operator==(const char *s) const noexcept
    { return s ? native() == std::string_view(s) : _size == 0; }

    bool operator!=(const relpath &o)    const noexcept { return !(*this == o); }
    bool operator!=(std::string_view s)  const noexcept { return !(*this == s); }
    bool operator!=(const char *s)       const noexcept { return !(*this == s); }

    bool operator<(const relpath &o)     const noexcept
    { return native() < o.native(); }
    bool operator<(std::string_view s)   const noexcept
    { return native() < s; }

  private:
    void _init(std::string_view s);
    bool _aliases(std::string_view s) const noexcept;

  private:
    // Invariants:
    //   _buf[_start + _size] == '\0'
    //   data lives at [_start, _start + _size).
    //   _start is the offset where the (optionally prefixed) data
    //     begins; _start <= _rel_start.
    //   The rel portion is at [_rel_start, _rel_start + _rel_size).
    //   _size == (_rel_start - _start) + _rel_size.
    //   _rel_start >= REL_OFFSET; defaults to REL_OFFSET and only
    //     grows when set_prefix needs more prefix room than
    //     REL_OFFSET provides (oversized-prefix fallback).
    //
    // _rel_size is cached so set_prefix / clear_prefix / rel() can
    // read it directly instead of recomputing (_start + _size) -
    // _rel_start. It is maintained on every mutation (assign /
    // assign_concat / consolidate_to_rel set it; set_prefix and
    // clear_prefix leave it; resize/append adjust it).
    alignas(8) char _buf[BUF_SIZE];
    std::uint16_t   _start;
    std::uint16_t   _size;
    std::uint16_t   _rel_start;
    std::uint16_t   _rel_size;
  };

  inline
  relpath
  operator/(std::string_view a_,
            std::string_view b_)
  {
    return relpath::concat(a_,b_);
  }
}

namespace std
{
  template<>
  struct hash<fs::relpath>
  {
    std::size_t
    operator()(const fs::relpath &p_) const noexcept
    {
      return std::hash<std::string_view>{}(p_.native());
    }
  };
}

#include <ostream>
namespace fs
{
  inline
  std::ostream&
  operator<<(std::ostream      &os_,
             const relpath     &p_)
  {
    return os_ << p_.native();
  }
}

// Note: there is no fmt::formatter specialization here. The previous
// fs::path version included one because it was used directly in
// SysLog::warning("... {}", path) calls. After the path/string split,
// every formatted "path" is std::string (branches, mountpoint, etc.)
// or a structural-query string_view -- both fmt formats natively.
// Keeping fmt out of this header avoids pulling fmt/format.h into
// every translation unit that includes it.

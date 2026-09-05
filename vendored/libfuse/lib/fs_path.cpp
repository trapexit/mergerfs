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

#include "fs_path.hpp"

#include <cassert>
#include <cstring>


fs::relpath::relpath() noexcept
  : _start(REL_OFFSET),
    _size(0),
    _rel_start(REL_OFFSET),
    _rel_size(0)
{
  _buf[REL_OFFSET] = '\0';
}

fs::relpath::relpath(const char *s_)
  : fs::relpath()
{
  if(s_ != nullptr)
    assign(std::string_view(s_));
}

fs::relpath::relpath(std::string_view s_)
  : fs::relpath()
{
  assign(s_);
}

fs::relpath::relpath(const std::string &s_)
  : fs::relpath()
{
  assign(std::string_view(s_));
}

fs::relpath::relpath(const fs::relpath &o_)
  : _start(o_._start),
    _size(o_._size),
    _rel_start(o_._rel_start),
    _rel_size(o_._rel_size)
{
  std::memcpy(_buf + _start,o_._buf + _start,_size + 1);
}

fs::relpath::relpath(fs::relpath &&o_) noexcept
  : _start(o_._start),
    _size(o_._size),
    _rel_start(o_._rel_start),
    _rel_size(o_._rel_size)
{
  std::memcpy(_buf + _start,o_._buf + _start,_size + 1);
}

fs::relpath&
fs::relpath::operator=(const fs::relpath &o_)
{
  if(this == &o_)
    return *this;
  _start     = o_._start;
  _size      = o_._size;
  _rel_start = o_._rel_start;
  _rel_size  = o_._rel_size;
  std::memcpy(_buf + _start,o_._buf + _start,_size + 1);
  return *this;
}

fs::relpath&
fs::relpath::operator=(fs::relpath &&o_) noexcept
{
  if(this == &o_)
    return *this;
  _start     = o_._start;
  _size      = o_._size;
  _rel_start = o_._rel_start;
  _rel_size  = o_._rel_size;
  std::memcpy(_buf + _start,o_._buf + _start,_size + 1);
  return *this;
}

fs::relpath&
fs::relpath::operator=(const char *s_)
{
  return assign(s_ ? std::string_view(s_) : std::string_view());
}

fs::relpath&
fs::relpath::operator=(std::string_view s_)
{
  return assign(s_);
}

fs::relpath&
fs::relpath::operator=(const std::string &s_)
{
  return assign(std::string_view(s_));
}

void
fs::relpath::_init(std::string_view s_)
{
  // No transformation: store the input verbatim. The caller is
  // responsible for canonical rel form (no leading '/'). Mergerfs's
  // pipeline guarantees this:
  //   - libfuse's try_get_path() ends with consolidate_to_rel(strip=
  //     true), which drops the leading '/' produced by prepend().
  //   - branch.path / fusepath via the free operator/ flows through
  //     concat() which does no stripping; the rel side comes in
  //     canonical and stays canonical.
  //
  // Storing inputs verbatim means an fs::relpath can also hold a
  // fully-built absolute fullpath (e.g. the result of `branch.path /
  // fusepath_`) without losing the leading '/'. Such an object is
  // not a candidate for set_prefix() rebuilding -- doing so would
  // produce double slashes -- but that combination is explicitly
  // outside the documented contract.

  const std::size_t n = s_.size();
  if(n > 0)
    std::memmove(_buf + REL_OFFSET,s_.data(),n);
  _start              = REL_OFFSET;
  _size               = static_cast<std::uint16_t>(n);
  _rel_start          = REL_OFFSET;
  _rel_size           = static_cast<std::uint16_t>(n);
  _buf[REL_OFFSET+n]  = '\0';
}

fs::relpath&
fs::relpath::assign(std::string_view s_)
{
  _init(s_);
  return *this;
}

void
fs::relpath::clear() noexcept
{
  // Reset to the canonical default-constructed state: anchored at
  // REL_OFFSET, empty. This makes prepend() safe immediately after
  // clear() so the path can be reused.
  _size        = 0;
  _rel_start   = REL_OFFSET;
  _rel_size    = 0;
  _start       = REL_OFFSET;
  _buf[_start] = '\0';
}

void
fs::relpath::set_prefix(std::string_view p_)
{
  // Contract: paths are in canonical mergerfs form.
  //   - prefix has no trailing '/'
  //   - rel    has no leading  '/'
  // No sanitization happens here; if you violate the contract you
  // get double slashes. Mergerfs's fusepaths come from libfuse with
  // the leading '/' already stripped (consolidate_to_rel(strip=true)
  // in try_get_path) and branch paths are realpath-canonicalized at
  // config load, so the contract holds.
  //
  // Fast path: the prefix (plus '/' separator) fits in front of the
  // current rel anchor _rel_start. Rel data is not touched, only
  // [_start, _rel_start) is rewritten.
  //
  // Slow path: prefix is larger than the current prefix area. The
  // rel data is shifted rightward, _rel_start advances to make room,
  // then the prefix is written linearly. Under Linux PATH_MAX this
  // branch is unreachable; it engages only under contract violation
  // or future-larger PATH_MAX environments.

  const std::size_t rel_size = _rel_size; // cached
  const std::size_t p_size   = p_.size();

  if(p_size == 0)
    {
      // Drop the prefix. Rel stays at its current anchor.
      _start = _rel_start;
      _size  = static_cast<std::uint16_t>(rel_size);
      _buf[_start + _size] = '\0';
      return;
    }

  const std::size_t sep    = (rel_size > 0) ? 1 : 0;
  const std::size_t needed = p_size + sep; // bytes of prefix-area required

  // Contract guard: prefix + sep + rel + NUL must fit in BUF_SIZE.
  // Violations should be caught at the source rather than silently
  // corrupting the buffer via uint16 overflow.
  assert(needed + rel_size < BUF_SIZE);

  if(needed > _rel_start)
    {
      // Slow path: shift rel rightward so the prefix area can grow.
      const std::size_t new_rel_start = needed;
      if(rel_size > 0)
        std::memmove(_buf + new_rel_start,_buf + _rel_start,rel_size);
      _rel_start = static_cast<std::uint16_t>(new_rel_start);
    }

  const std::size_t new_start = _rel_start - needed;
  std::memmove(_buf + new_start,p_.data(),p_size);
  if(sep)
    _buf[new_start + p_size] = '/';
  _start = static_cast<std::uint16_t>(new_start);
  _size  = static_cast<std::uint16_t>(needed + rel_size);
  _buf[_start + _size] = '\0';
  // _rel_size unchanged: set_prefix only rewrites the prefix area
  // (and, in the slow path, relocates the rel without changing its
  // length).
}

bool
fs::relpath::_aliases(std::string_view s_) const noexcept
{
  // Returns true when the view's data pointer lies inside _buf.
  // Used to detect self-aliasing in assign_concat so we can stash the
  // source before clobbering it.
  std::uintptr_t p = reinterpret_cast<std::uintptr_t>(s_.data());
  std::uintptr_t b = reinterpret_cast<std::uintptr_t>(_buf);
  return (p >= b) && (p < b + BUF_SIZE);
}

fs::relpath&
fs::relpath::assign_concat(std::string_view base_,
                           std::string_view rel_)
{
  // The common case is that neither input aliases _buf (e.g.
  // `branch.path / fusepath`, where branch.path is std::string and
  // fusepath is a stack-owned fs::relpath belonging to a different
  // object). Skip the BUF_SIZE stack reservation in that case and only
  // allocate the temp when at least one input does alias.
  //
  // When aliasing is detected, stash the aliasing input(s) to a
  // stack-local temp so the subsequent writes don't corrupt our own
  // source data (e.g. `p /= "x"` where base_ is p.native()). The temp
  // is BUF_SIZE so it can fit any well-formed base+rel.
  const bool base_aliases = _aliases(base_);
  const bool rel_aliases  = _aliases(rel_);
  if(base_aliases || rel_aliases)
    {
      alignas(8) char tmp[BUF_SIZE];
      std::size_t tmp_pos = 0;
      if(base_aliases)
        {
          std::memcpy(tmp + tmp_pos,base_.data(),base_.size());
          base_ = std::string_view(tmp + tmp_pos,base_.size());
          tmp_pos += base_.size();
        }
      if(rel_aliases)
        {
          std::memcpy(tmp + tmp_pos,rel_.data(),rel_.size());
          rel_ = std::string_view(tmp + tmp_pos,rel_.size());
        }
      // base_ and rel_ now point into `tmp`. The rel write below and
      // set_prefix both complete before this scope ends, so tmp's
      // lifetime covers all reads.
      _init(rel_);
      set_prefix(base_);
      return *this;
    }

  // Common path: no aliasing -- write rel at REL_OFFSET via _init,
  // then prepend base via set_prefix.
  _init(rel_);
  set_prefix(base_);
  return *this;
}

fs::relpath
fs::relpath::concat(std::string_view base_,
                    std::string_view rel_)
{
  fs::relpath p;
  p.assign_concat(base_,rel_);
  return p;
}

fs::relpath
fs::relpath::operator/(std::string_view rhs_) const
{
  return fs::relpath::concat(native(),rhs_);
}

fs::relpath&
fs::relpath::operator/=(std::string_view rhs_)
{
  return assign_concat(native(),rhs_);
}

std::string_view
fs::relpath::filename() const noexcept
{
  if(_size == 0)
    return {};
  std::string_view sv = native();
  auto pos = sv.rfind('/');
  if(pos == std::string_view::npos)
    return sv;
  return sv.substr(pos + 1);
}

std::string_view
fs::relpath::parent_path() const noexcept
{
  if(_size <= 1)
    return {}; // "" or "/" -> ""
  std::string_view sv = native();
  auto pos = sv.rfind('/');
  if(pos == std::string_view::npos)
    return {};
  if(pos == 0)
    return sv.substr(0,1); // "/foo" -> "/"
  return sv.substr(0,pos);
}

std::string_view
fs::relpath::extension() const noexcept
{
  std::string_view fn = filename();
  if(fn.empty() || fn == "." || fn == "..")
    return {};
  auto pos = fn.rfind('.');
  if(pos == std::string_view::npos || pos == 0)
    return {};
  return fn.substr(pos);
}

std::string_view
fs::relpath::stem() const noexcept
{
  std::string_view fn = filename();
  if(fn == "." || fn == "..")
    return fn;
  std::string_view ext = extension();
  return fn.substr(0,fn.size() - ext.size());
}

fs::relpath&
fs::relpath::remove_filename()
{
  if(_size == 0)
    return *this;
  std::string_view sv = native();
  auto pos = sv.rfind('/');
  if(pos == std::string_view::npos)
    {
      clear();
      return *this;
    }
  // Keep the trailing slash so the result is the directory form.
  // Use parent_path() if you want the dirname without the trailing '/'.
  const std::size_t new_size   = pos + 1;
  const std::size_t prefix_len = _rel_start - _start;
  _size     = static_cast<std::uint16_t>(new_size);
  // Truncating into the prefix area drops the rel portion entirely;
  // clamp to 0 so the uint16 can't underflow.
  _rel_size = (new_size > prefix_len)
              ? static_cast<std::uint16_t>(new_size - prefix_len)
              : 0;
  _buf[_start + _size] = '\0';
  return *this;
}

void
fs::relpath::resize(std::size_t n_)
{
  if(n_ > _size)
    // Zero-fill new bytes to match std::string::resize semantics.
    std::memset(_buf + _start + _size,'\0',n_ - _size);
  const std::size_t prefix_len = _rel_start - _start;
  _size     = static_cast<std::uint16_t>(n_);
  // Shrinking below the prefix area drops the rel portion entirely;
  // clamp to 0 so the uint16 can't underflow.
  _rel_size = (n_ > prefix_len)
              ? static_cast<std::uint16_t>(n_ - prefix_len)
              : 0;
  _buf[_start + _size] = '\0';
}

void
fs::relpath::swap(fs::relpath &o_) noexcept
{
  // Self-swap is a no-op; bail before touching memcpy with identical
  // src/dst pointers (UB per the C standard) and avoid the round-trip
  // through the stack temp.
  if(this == &o_)
    return;

  // Copy this's used portion to a stack temp, slot in o's data, then
  // restore the saved data into o. Touches only the actually-used
  // bytes plus the metadata quadruplet.
  alignas(8) char tmp[BUF_SIZE];
  const std::uint16_t my_start     = _start;
  const std::uint16_t my_size      = _size;
  const std::uint16_t my_rel_start = _rel_start;
  const std::uint16_t my_rel_size  = _rel_size;

  std::memcpy(tmp + my_start,_buf + my_start,my_size + 1);

  _start     = o_._start;
  _size      = o_._size;
  _rel_start = o_._rel_start;
  _rel_size  = o_._rel_size;
  std::memcpy(_buf + _start,o_._buf + _start,_size + 1);

  o_._start     = my_start;
  o_._size      = my_size;
  o_._rel_start = my_rel_start;
  o_._rel_size  = my_rel_size;
  std::memcpy(o_._buf + my_start,tmp + my_start,my_size + 1);
}

std::string_view
fs::relpath::prefix() const noexcept
{
  const std::size_t plen = _rel_start - _start;
  return {_buf + _start,plen};
}

std::string_view
fs::relpath::rel() const noexcept
{
  return {_buf + _rel_start,_rel_size};
}

void
fs::relpath::append(const char  *s_,
                    std::size_t  n_)
{
  if(n_ == 0)
    return;
  std::memcpy(_buf + _start + _size,s_,n_);
  _size     = static_cast<std::uint16_t>(_size + n_);
  _rel_size = static_cast<std::uint16_t>(_rel_size + n_);
  _buf[_start + _size] = '\0';
}

void
fs::relpath::append(std::string_view s_)
{
  append(s_.data(),s_.size());
}

namespace
{
  // Extracts the next path component from sv (which has leading '/'
  // already consumed), advancing sv past it. Returns empty when no
  // more components.
  inline
  std::string_view
  _next_component(std::string_view &sv_)
  {
    while(!sv_.empty() && sv_.front() == '/')
      sv_.remove_prefix(1);
    if(sv_.empty())
      return {};
    auto pos = sv_.find('/');
    std::string_view c =
      (pos == std::string_view::npos) ? sv_ : sv_.substr(0,pos);
    sv_.remove_prefix(c.size());
    return c;
  }
}

fs::relpath
fs::relpath::lexically_relative(std::string_view base_) const
{
  std::string_view a = native();
  std::string_view b = base_;

  // Both sides must have the same shape. fs::relpath always starts in
  // canonical (no-leading-slash) form, but legacy callers and the
  // libfuse symlink-rewrite path may pass leading-slash literals
  // through native(); accept either as long as both match. Shape
  // mismatch returns an empty result; a successful relativization
  // produces "." (for equal paths) so empty is unambiguous failure.
  const bool a_abs = (!a.empty() && a.front() == '/');
  const bool b_abs = (!b.empty() && b.front() == '/');
  if(a_abs != b_abs)
    return fs::relpath();
  if(a_abs)
    {
      a.remove_prefix(1);
      b.remove_prefix(1);
    }

  for(;;)
    {
      auto sa = a;
      auto sb = b;
      auto ca = _next_component(sa);
      auto cb = _next_component(sb);
      if(ca.empty() && cb.empty())
        break;
      if(ca != cb)
        break;
      a = sa;
      b = sb;
    }

  std::size_t up = 0;
  {
    std::string_view tmp = b;
    for(;;)
      {
        auto c = _next_component(tmp);
        if(c.empty())
          break;
        if(c == "..")
          // ".." in base means the relativization can't resolve
          // without filesystem access; return empty.
          return fs::relpath();
        if(c != ".")
          ++up;
      }
  }

  fs::relpath result;
  for(std::size_t i = 0; i < up; ++i)
    {
      if(i > 0)
        result.append("/",1);
      result.append("..",2);
    }

  for(;;)
    {
      auto c = _next_component(a);
      if(c.empty())
        break;
      if(!result.empty())
        result.append("/",1);
      result.append(c);
    }

  if(result.empty())
    result.assign(".");

  return result;
}

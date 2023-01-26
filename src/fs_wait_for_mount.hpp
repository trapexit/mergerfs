/*
  ISC License

  Copyright (c) 2023, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "ghc/filesystem.hpp"

#include "fs_pathvector.hpp"
#include "fs_stat.hpp"

#include <chrono>
#include <vector>


namespace fs
{
  typedef std::vector<ghc::filesystem::path> PathVector;

  bool
  wait_for_mount(const struct stat               &st,
                 const ghc::filesystem::path     &tgtpath,
                 const std::chrono::milliseconds &timeout);

  void
  wait_for_mount(const struct stat               &st,
                 const fs::PathVector            &tgtpaths,
                 const std::chrono::milliseconds &timeout,
                 fs::PathVector                  &failed_paths);

  void
  wait_for_mount(const ghc::filesystem::path     &srcpath,
                 const fs::PathVector            &tgtpaths,
                 const std::chrono::milliseconds &timeout,
                 fs::PathVector                  &failed_paths);
}

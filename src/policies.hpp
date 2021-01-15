/*
  ISC License

  Copyright (c) 2020, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "policy_all.hpp"
#include "policy_epall.hpp"
#include "policy_epff.hpp"
#include "policy_eplfs.hpp"
#include "policy_eplus.hpp"
#include "policy_epmfs.hpp"
#include "policy_eppfrd.hpp"
#include "policy_eprand.hpp"
#include "policy_erofs.hpp"
#include "policy_ff.hpp"
#include "policy_lfs.hpp"
#include "policy_lus.hpp"
#include "policy_mfs.hpp"
#include "policy_msplfs.hpp"
#include "policy_msplus.hpp"
#include "policy_mspmfs.hpp"
#include "policy_msppfrd.hpp"
#include "policy_newest.hpp"
#include "policy_pfrd.hpp"
#include "policy_rand.hpp"

struct Policies
{
  struct Action
  {
    static Policy::ActionImpl *find(const std::string &name);
    
    static Policy::All::Action     all;
    static Policy::EPAll::Action   epall;
    static Policy::EPFF::Action    epff;
    static Policy::EPLFS::Action   eplfs;
    static Policy::EPLUS::Action   eplus;
    static Policy::EPMFS::Action   epmfs;
    static Policy::EPPFRD::Action  eppfrd;
    static Policy::EPRand::Action  eprand;
    static Policy::ERoFS::Action   erofs;
    static Policy::FF::Action      ff;
    static Policy::LFS::Action     lfs;
    static Policy::LUS::Action     lus;
    static Policy::MFS::Action     mfs;
    static Policy::MSPLFS::Action  msplfs;
    static Policy::MSPLUS::Action  msplus;
    static Policy::MSPMFS::Action  mspmfs;
    static Policy::MSPPFRD::Action msppfrd;
    static Policy::Newest::Action  newest;
    static Policy::PFRD::Action    pfrd;
    static Policy::Rand::Action    rand;
  };

  struct Create
  {
    static Policy::CreateImpl *find(const std::string &name);
    
    static Policy::All::Create     all;
    static Policy::EPAll::Create   epall;
    static Policy::EPFF::Create    epff;
    static Policy::EPLFS::Create   eplfs;
    static Policy::EPLUS::Create   eplus;
    static Policy::EPMFS::Create   epmfs;
    static Policy::EPPFRD::Create  eppfrd;
    static Policy::EPRand::Create  eprand;
    static Policy::ERoFS::Create   erofs;
    static Policy::FF::Create      ff;
    static Policy::LFS::Create     lfs;
    static Policy::LUS::Create     lus;
    static Policy::MFS::Create     mfs;
    static Policy::MSPLFS::Create  msplfs;
    static Policy::MSPLUS::Create  msplus;
    static Policy::MSPMFS::Create  mspmfs;
    static Policy::MSPPFRD::Create msppfrd;
    static Policy::Newest::Create  newest;
    static Policy::PFRD::Create    pfrd;
    static Policy::Rand::Create    rand;
  };

  struct Search
  {
    static Policy::SearchImpl *find(const std::string &name);

    static Policy::All::Search     all;
    static Policy::EPAll::Search   epall;
    static Policy::EPFF::Search    epff;
    static Policy::EPLFS::Search   eplfs;
    static Policy::EPLUS::Search   eplus;
    static Policy::EPMFS::Search   epmfs;
    static Policy::EPPFRD::Search  eppfrd;
    static Policy::EPRand::Search  eprand;
    static Policy::ERoFS::Search   erofs;
    static Policy::FF::Search      ff;
    static Policy::LFS::Search     lfs;
    static Policy::LUS::Search     lus;
    static Policy::MFS::Search     mfs;
    static Policy::MSPLFS::Search  msplfs;
    static Policy::MSPLUS::Search  msplus;
    static Policy::MSPMFS::Search  mspmfs;
    static Policy::MSPPFRD::Search msppfrd;
    static Policy::Newest::Search  newest;
    static Policy::PFRD::Search    pfrd;
    static Policy::Rand::Search    rand;
  };
};

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

#include "policies.hpp"

#define IFERTA(X) if(name_ == #X) return &Policies::Action::X;
#define IFERTC(X) if(name_ == #X) return &Policies::Create::X;
#define IFERTS(X) if(name_ == #X) return &Policies::Search::X;

#define IFERT(FUNC)                             \
  FUNC(all)                                     \
  FUNC(epall)                                   \
  FUNC(epff)                                    \
  FUNC(eplfs)                                   \
  FUNC(eplus)                                   \
  FUNC(epmfs)                                   \
  FUNC(eppfrd)                                  \
  FUNC(eprand)                                  \
  FUNC(erofs)                                   \
  FUNC(ff)                                      \
  FUNC(lfs)                                     \
  FUNC(lus)                                     \
  FUNC(mfs)                                     \
  FUNC(msplfs)                                  \
  FUNC(msplus)                                  \
  FUNC(mspmfs)                                  \
  FUNC(msppfrd)                                 \
  FUNC(newest)                                  \
  FUNC(pfrd)                                    \
  FUNC(rand)

Policy::ActionImpl*
Policies::Action::find(const std::string &name_)
{
  IFERT(IFERTA);

  return NULL;
}

Policy::CreateImpl*
Policies::Create::find(const std::string &name_)
{
  IFERT(IFERTC);

  return NULL;
}

Policy::SearchImpl*
Policies::Search::find(const std::string &name_)
{
  IFERT(IFERTS);

  return NULL;
}

Policy::All::Action     Policies::Action::all;
Policy::EPAll::Action   Policies::Action::epall;
Policy::EPFF::Action    Policies::Action::epff;
Policy::EPLFS::Action   Policies::Action::eplfs;
Policy::EPLUS::Action   Policies::Action::eplus;
Policy::EPMFS::Action   Policies::Action::epmfs;
Policy::EPPFRD::Action  Policies::Action::eppfrd;
Policy::EPRand::Action  Policies::Action::eprand;
Policy::ERoFS::Action   Policies::Action::erofs;
Policy::FF::Action      Policies::Action::ff;
Policy::LFS::Action     Policies::Action::lfs;
Policy::LUS::Action     Policies::Action::lus;
Policy::MFS::Action     Policies::Action::mfs;
Policy::MSPLFS::Action  Policies::Action::msplfs;
Policy::MSPLUS::Action  Policies::Action::msplus;
Policy::MSPMFS::Action  Policies::Action::mspmfs;
Policy::MSPPFRD::Action Policies::Action::msppfrd;
Policy::Newest::Action  Policies::Action::newest;
Policy::PFRD::Action    Policies::Action::pfrd;
Policy::Rand::Action    Policies::Action::rand;

Policy::All::Create     Policies::Create::all;
Policy::EPAll::Create   Policies::Create::epall;
Policy::EPFF::Create    Policies::Create::epff;
Policy::EPLFS::Create   Policies::Create::eplfs;
Policy::EPLUS::Create   Policies::Create::eplus;
Policy::EPMFS::Create   Policies::Create::epmfs;
Policy::EPPFRD::Create  Policies::Create::eppfrd;
Policy::EPRand::Create  Policies::Create::eprand;
Policy::ERoFS::Create   Policies::Create::erofs;
Policy::FF::Create      Policies::Create::ff;
Policy::LFS::Create     Policies::Create::lfs;
Policy::LUS::Create     Policies::Create::lus;
Policy::MFS::Create     Policies::Create::mfs;
Policy::MSPLFS::Create  Policies::Create::msplfs;
Policy::MSPLUS::Create  Policies::Create::msplus;
Policy::MSPMFS::Create  Policies::Create::mspmfs;
Policy::MSPPFRD::Create Policies::Create::msppfrd;
Policy::Newest::Create  Policies::Create::newest;
Policy::PFRD::Create    Policies::Create::pfrd;
Policy::Rand::Create    Policies::Create::rand;

Policy::All::Search     Policies::Search::all;
Policy::EPAll::Search   Policies::Search::epall;
Policy::EPFF::Search    Policies::Search::epff;
Policy::EPLFS::Search   Policies::Search::eplfs;
Policy::EPLUS::Search   Policies::Search::eplus;
Policy::EPMFS::Search   Policies::Search::epmfs;
Policy::EPPFRD::Search  Policies::Search::eppfrd;
Policy::EPRand::Search  Policies::Search::eprand;
Policy::ERoFS::Search   Policies::Search::erofs;
Policy::FF::Search      Policies::Search::ff;
Policy::LFS::Search     Policies::Search::lfs;
Policy::LUS::Search     Policies::Search::lus;
Policy::MFS::Search     Policies::Search::mfs;
Policy::MSPLFS::Search  Policies::Search::msplfs;
Policy::MSPLUS::Search  Policies::Search::msplus;
Policy::MSPMFS::Search  Policies::Search::mspmfs;
Policy::MSPPFRD::Search Policies::Search::msppfrd;
Policy::Newest::Search  Policies::Search::newest;
Policy::PFRD::Search    Policies::Search::pfrd;
Policy::Rand::Search    Policies::Search::rand;

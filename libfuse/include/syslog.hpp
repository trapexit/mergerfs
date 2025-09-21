/*
  ISC License

  Copyright (c) 2025, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "fmt/core.h"
#include "fmt/format.h"

#include <syslog.h>

namespace SysLog
{
  static
  inline
  void
  open()
  {
    const char *ident = "mergerfs";
    const int   option = (LOG_CONS|LOG_PID);
    const int   facility = LOG_USER;

    openlog(ident,option,facility);
  }

  static
  inline
  void
  close()
  {
    closelog();
  }
  
  template<typename... Args>
  void
  log(int priority_,
      fmt::format_string<Args...> format_,
      Args&&... args)
  {
    auto msg = fmt::format(format_,std::forward<Args>(args)...);
    ::syslog(priority_,"%s",msg.c_str());
  }

  template<typename... Args>
  void
  info(fmt::format_string<Args...> format_,
       Args&&... args)
  {
    SysLog::log(LOG_INFO,format_,std::forward<Args>(args)...);
  }

  template<typename... Args>
  void
  debug(fmt::format_string<Args...> format_,
        Args&&... args)
  {
    SysLog::log(LOG_DEBUG,format_,std::forward<Args>(args)...);
  }

  template<typename... Args>
  void
  notice(fmt::format_string<Args...> format_,
         Args&&... args)
  {
    SysLog::log(LOG_NOTICE,format_,std::forward<Args>(args)...);
  }

  template<typename... Args>
  void
  warning(fmt::format_string<Args...> format_,
          Args&&... args)
  {
    SysLog::log(LOG_WARNING,format_,std::forward<Args>(args)...);
  }

  template<typename... Args>
  void
  error(fmt::format_string<Args...> format_,
        Args&&... args)
  {
    SysLog::log(LOG_ERR,format_,std::forward<Args>(args)...);
  }

  template<typename... Args>
  void
  alert(fmt::format_string<Args...> format_,
        Args&&... args)
  {
    SysLog::log(LOG_ALERT,format_,std::forward<Args>(args)...);
  }

  template<typename... Args>
  void
  crit(fmt::format_string<Args...> format_,
       Args&&... args)
  {
    SysLog::log(LOG_CRIT,format_,std::forward<Args>(args)...);
  }
}

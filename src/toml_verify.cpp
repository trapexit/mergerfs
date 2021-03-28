/*
  ISC License

  Copyright (c) 2021, Antonio SJ Musumeci <trapexit@spawn.link>

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

#include "humanize.hpp"

#include "toml.hpp"

#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

typedef std::vector<std::string> strvec;

namespace toml
{
  enum class Status
    {
      OPTIONAL,
      REQUIRED
    };

  constexpr auto INODE_CALC_ENUMS =
    {
      "passthrough",
      "path-hash",
      "path-hash32",
      "devino-hash",
      "devino-hash32",
      "hybrid-hash",
      "hybrid-hash32"
    };

  constexpr auto XATTR_ENUMS =
    {
      "passthrough",
      "noattr",
      "nosys"
    };

  constexpr auto STATFS_ENUMS =
    {
      "base",
      "full"
    };

  constexpr auto STATFS_IGNORE_ENUMS =
    {
      "none",
      "ro",
      "nc"
    };

  constexpr auto NFS_OPEN_HACK_ENUMS =
    {
      "off",
      "git",
      "all"
    };

  constexpr auto FOLLOW_SYMLINKS_ENUMS =
    {
      "never",
      "directory",
      "regular",
      "all"
    };

  constexpr auto LINK_EXDEV_ENUMS =
    {
      "passthrough",
      "rel-symlink",
      "abs-base-symlink",
      "abs-pool-symlink"
    };

  constexpr auto RENAME_EXDEV_ENUMS =
    {
      "passthrough",
      "rel-symlink",
      "abs-symlink"
    };

  constexpr auto ACTION_POLICY_ENUMS =
    {
      "all",
      "epall",
      "epff",
      "eplfs",
      "eplus",
      "epmfs",
      "eppfrd",
      "eprand",
      "erofs",
      "ff",
      "lfs",
      "lus",
      "mfs",
      "msplfs",
      "msplus",
      "mspmfs",
      "msppfrd",
      "newest",
      "pfrd",
      "rand"
    };

  constexpr auto CREATE_POLICY_ENUMS =
    {
      "all",
      "epall",
      "epff",
      "eplfs",
      "eplus",
      "epmfs",
      "eppfrd",
      "eprand",
      "erofs",
      "ff",
      "lfs",
      "lus",
      "mfs",
      "msplfs",
      "msplus",
      "mspmfs",
      "msppfrd",
      "newest",
      "pfrd",
      "rand"
    };

  constexpr auto SEARCH_POLICY_ENUMS =
    {
      "all",
      "epall",
      "epff",
      "eplfs",
      "eplus",
      "epmfs",
      "eppfrd",
      "eprand",
      "erofs",
      "ff",
      "lfs",
      "lus",
      "mfs",
      "msplfs",
      "msplus",
      "mspmfs",
      "msppfrd",
      "newest",
      "pfrd",
      "rand"
    };

  constexpr auto CACHE_FILES_ENUMS =
    {
      "off",
      "partial",
      "full",
      "auto"
    };

  constexpr auto BRANCH_MODE_ENUMS =
    {
      "RW",
      "RO",
      "NC"
    };

  void
  required(const toml::value &val_,
           const toml::key   &key_,
           strvec            &errs_)
  {
    std::string str;

    str = "[error] required key: " + key_;
    str = toml::format_error(str,val_,"");

    errs_.push_back(str);
  }

  static
  void
  invalid_type(const toml::value   &val_,
               const toml::value_t  type_,
               strvec              &errs_)
  {
    std::string str;

    str = "[error] invalid type";
    str = toml::format_error(str,val_,"should be " + toml::stringize(type_));

    errs_.push_back(str);
  }

  template<class T>
  static
  void
  invalid_enum(const toml::value &val_,
               const T           &enums_,
               strvec            &errs_)
  {
    std::string str;
    strvec hints;

    for(const auto &e : enums_)
      hints.push_back(e);

    str = "[error] invalid enum";
    str = toml::format_error(str,val_,"",hints);

    errs_.push_back(str);
  }

  static
  void
  invalid_human_size(const toml::value &val_,
                     strvec            &errs_)
  {
    std::string str;
    const strvec hints = {"'1048576'","'1024K'","'512M'","'4G'","'1T'"};

    str = "[error] invalid 'humanize' integer";
    str = toml::format_error(str,val_,"",hints);

    errs_.push_back(str);
  }

  void
  verify_int(const toml::value &doc_,
             const toml::key   &key_,
             const int64_t      min_,
             const int64_t      max_,
             toml::Status       status_,
             strvec            &errs_)
  {
    const bool key_exists = doc_.contains(key_);

    if((status_ == Status::REQUIRED) && (key_exists == false))
      return toml::required(doc_,key_,errs_);
    if(key_exists == false)
      return;

    const auto &val = doc_.at(key_);
    if(val.is_integer() == false)
      {
        std::string str;

        str = toml::format_error("[error] wrong type: should be integer",
                                 val,
                                 "",
                                 {"min = " + std::to_string(min_),
                                  "max = " + std::to_string(max_)});
        errs_.push_back(str);

        return;
      }

    if(val.as_integer() < min_)
      {
        std::string str;

        str = toml::format_error("[error] value below valid minimum",
                                 val,
                                 "min = " + std::to_string(min_));
        errs_.push_back(str);

        return;
      }

    if(val.as_integer() > max_)
      {
        std::string str;

        str = toml::format_error("[error] value above valid maximum",
                                 val,
                                 "max = " + std::to_string(max_));
        errs_.push_back(str);

        return;
      }
  }

  void
  verify_uint(const toml::value &doc_,
              const toml::key   &key_,
              const int64_t      max_,
              toml::Status       status_,
              strvec            &errs_)
  {
    constexpr int64_t min = 0;

    toml::verify_int(doc_,key_,min,max_,status_,errs_);
  }

  void
  verify_uint(const toml::value &doc_,
              const toml::key   &key_,
              toml::Status       status_,
              strvec            &errs_)
  {
    constexpr int64_t max = std::numeric_limits<int64_t>::max();

    toml::verify_uint(doc_,key_,max,status_,errs_);
  }

  void
  verify_string(const toml::value &doc_,
                const toml::key   &key_,
                toml::Status       status_,
                strvec            &errs_)
  {
    if((status_ == Status::REQUIRED) && (doc_.contains(key_) == false))
      return toml::required(doc_,key_,errs_);
    if(doc_.contains(key_) == false)
      return;
    if(doc_.at(key_).is_string() == false)
      return toml::invalid_type(doc_.at(key_),toml::value_t::string,errs_);
  }

  void
  verify_human_size(const toml::value &doc_,
                    const toml::key   &key_,
                    toml::Status       status_,
                    strvec            &errs_)
  {
    if((status_ == Status::REQUIRED) && (doc_.contains(key_) == false))
      return toml::required(doc_,key_,errs_);
    if(doc_.contains(key_) == false)
      return;

    const auto &val = doc_.at(key_);
    if(val.is_integer() == true)
      return;

    if((val.is_string() == false) && (val.is_integer() == false))
      return toml::invalid_human_size(val,errs_);

    uint64_t u;
    if(humanize::from(val.as_string(),&u) != 0)
      return toml::invalid_human_size(val,errs_);
  }

  static
  void
  verify_boolean(const toml::value &doc_,
                 const toml::key   &key_,
                 toml::Status       status_,
                 strvec            &errs_)
  {
    const bool key_exists = doc_.contains(key_);

    if((status_ == Status::REQUIRED) && (key_exists == false))
      return toml::required(doc_,key_,errs_);
    if(key_exists == false)
      return;
    if(doc_.at(key_).is_boolean() == false)
      return toml::invalid_type(doc_.at(key_),toml::value_t::boolean,errs_);
  }

  template<class T>
  static
  void
  verify_enum(const toml::value &doc_,
              const toml::key   &key_,
              const T           &enums_,
              toml::Status       status_,
              strvec            &errs_)
  {
    const bool key_exists = doc_.contains(key_);

    if((status_ == Status::REQUIRED) && (key_exists == false))
      return toml::required(doc_,key_,errs_);
    if(key_exists == false)
      return;
    if(doc_.at(key_).is_string() == false)
      return toml::invalid_enum(doc_.at(key_),enums_,errs_);

    const std::string &val = doc_.at(key_).as_string();
    for(const auto &e : enums_)
      {
        if(val == e)
          return;
      }

    return toml::invalid_enum(doc_.at(key_),enums_,errs_);
  }

  static
  void
  verify_cache_table(const toml::value &doc_,
                     strvec            &errs_)
  {
    if(doc_.contains("cache") == false)
      return toml::required(doc_,"cache",errs_);

    const auto &cache_table = doc_.at("cache");

    toml::verify_enum(cache_table,"files",CACHE_FILES_ENUMS,Status::REQUIRED,errs_);
    toml::verify_uint(cache_table,"open",Status::OPTIONAL,errs_);
    toml::verify_uint(cache_table,"statfs",Status::OPTIONAL,errs_);
    toml::verify_uint(cache_table,"attr",Status::OPTIONAL,errs_);
    toml::verify_uint(cache_table,"entry",Status::OPTIONAL,errs_);
    toml::verify_uint(cache_table,"negative-entry",Status::OPTIONAL,errs_);
    toml::verify_boolean(cache_table,"writeback",Status::OPTIONAL,errs_);
    toml::verify_boolean(cache_table,"symlinks",Status::OPTIONAL,errs_);
    toml::verify_boolean(cache_table,"readdir",Status::OPTIONAL,errs_);
  }

  static
  void
  verify_policy_table(const toml::value &doc_,
                      strvec            &errs_)
  {
    if(doc_.contains("policy") == false)
      return toml::required(doc_,"policy",errs_);

    const auto &policy_table = doc_.at("policy");

    if(policy_table.is_table() == false)
      return toml::invalid_type(policy_table,toml::value_t::table,errs_);

    toml::verify_enum(policy_table,"chmod",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"chown",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"link",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"removexattr",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"rename",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"rmdir",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"setxattr",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"truncate",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"unlink",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"utimens",ACTION_POLICY_ENUMS,Status::REQUIRED,errs_);

    toml::verify_enum(policy_table,"create",CREATE_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"mkdir",CREATE_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"mknod",CREATE_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"symlink",CREATE_POLICY_ENUMS,Status::REQUIRED,errs_);

    toml::verify_enum(policy_table,"access",SEARCH_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"getattr",SEARCH_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"getxattr",SEARCH_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"listxattr",SEARCH_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"open",SEARCH_POLICY_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(policy_table,"readlink",SEARCH_POLICY_ENUMS,Status::REQUIRED,errs_);
  }

  static
  void
  verify_branch_table(const toml::value &branch_,
                strvec            &errs_)
  {
    toml::verify_string(branch_,"path",Status::REQUIRED,errs_);
    toml::verify_human_size(branch_,"min-free-space",Status::OPTIONAL,errs_);
    toml::verify_enum(branch_,"mode",BRANCH_MODE_ENUMS,Status::OPTIONAL,errs_);
    toml::verify_boolean(branch_,"glob",Status::OPTIONAL,errs_);
  }

  static
  void
  verify_branch_array(const toml::value &doc_,
                      strvec            &errs_)
  {
    if(doc_.contains("branch") == false)
      return toml::required(doc_,"branch",errs_);

    const auto &branches = doc_.at("branch");
    if(branches.is_array() == false)
      return toml::invalid_type(branches,toml::value_t::array,errs_);

    for(const auto &branch : branches.as_array())
      {
        toml::verify_branch_table(branch,errs_);
      }
  }

  static
  void
  verify_branches(const toml::value &doc_,
                  strvec            &errs_)
  {
    if(doc_.contains("branches") == false)
      return toml::required(doc_,"branches",errs_);

    const auto &branches = doc_.at("branches");
    if(branches.is_table() == false)
      return toml::invalid_type(branches,toml::value_t::table,errs_);

    toml::verify_human_size(branches,"min-free-space",Status::REQUIRED,errs_);
    toml::verify_branch_array(branches,errs_);
  }

  void
  verify(const toml::value &doc_,
         strvec            &errs_)
  {
    std::cout << doc_ << std::endl;
    
    toml::verify_branches(doc_,errs_);

    return;

    toml::verify_boolean(doc_,"link-cow",Status::OPTIONAL,errs_);
    toml::verify_cache_table(doc_,errs_);
    toml::verify_enum(doc_,"follow-symlinks",FOLLOW_SYMLINKS_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(doc_,"inode-calc",INODE_CALC_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(doc_,"move-on-enospc",CREATE_POLICY_ENUMS,Status::OPTIONAL,errs_);
    toml::verify_enum(doc_,"nfs-open-hack",NFS_OPEN_HACK_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(doc_,"statfs",STATFS_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(doc_,"statfs-ignore",STATFS_IGNORE_ENUMS,Status::REQUIRED,errs_);
    toml::verify_enum(doc_,"xattr",XATTR_ENUMS,Status::REQUIRED,errs_);
    toml::verify_human_size(doc_,"min-free-space",Status::OPTIONAL,errs_);
    toml::verify_policy_table(doc_,errs_);
    toml::verify_string(doc_,"filesystem-name",Status::REQUIRED,errs_);
  }
}

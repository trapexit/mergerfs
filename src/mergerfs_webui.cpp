#include "mergerfs_webui.hpp"

#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_mounts.hpp"
#include "mergerfs_api.hpp"
#include "str.hpp"
#include "strvec.hpp"

#include "CLI11.hpp"
#include "fmt/chrono.h"
#include "fmt/core.h"
#include "httplib.h"
#include "json.hpp"

#include <unistd.h>
#include <cstring>
#include <fstream>
#include <chrono>

#include "index_min_html_gz.h"
#include "favicon_ico.h"

using json = nlohmann::json;

static std::string g_password;
static std::string g_index_html;

static
std::string
_get_timestamp()
{
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::floor<std::chrono::milliseconds>(now);

  return fmt::format("{:%Y-%m-%dT%H:%M:%S}",now_ms);
 }

static
bool
_validate_password(const std::string &password_provided_)
{
  if(g_password.empty())
    return true;

  if(password_provided_.empty())
    return false;

  return (password_provided_ == g_password);
}

static
json
_generate_error(const fs::path    &mount_,
                const std::string &key_,
                const std::string &val_,
                const int          err_)
{
  json rv;

  rv = json::object();

  rv["mount"] = mount_.string();
  rv["key"]   = key_;
  rv["value"] = val_;

  switch(err_)
    {
    case -EROFS:
      rv["msg"] = fmt::format("'{}' is read only.",key_);
      break;
    case -EINVAL:
      rv["msg"] = fmt::format("value '{}' is invalid for '{}'",
                              val_,
                              key_);
      break;
    case -EACCES:
      rv["msg"] = fmt::format("mergerfs.webui (pid {}) is running as uid {}"
                              " which appears not to have access to modify the"
                              " mount's config.",
                              ::getpid(),
                              ::getuid());
      break;
    case -ENOTCONN:
      rv["msg"] = fmt::format("It appears the mergerfs mount '{}' is in a bad state."
                              " mergerfs may have crashed.",
                              mount_.string());
      break;
    default:
      rv["msg"] = strerror(-err_);
      break;
    }

  return rv;
}

static
void
_get_root(const httplib::Request &req_,
          httplib::Response      &res_)
{
  if(not g_index_html.empty() and fs::exists(g_index_html))
    {
      res_.set_file_content(g_index_html);
      return;
    }

  std::string accept_encoding;
  accept_encoding = req_.get_header_value("Accept-Encoding");
  if(accept_encoding.find("gzip") != std::string::npos)
    {
      res_.set_header("Content-Encoding","gzip");
      res_.set_header("Content-Type","text/html");
      res_.set_content((const char*)index_min_html_gz,
                       index_min_html_gz_len,
                       "text/html");
      return;
    }

  res_.set_content("browser needs to support gzip","text/plain");
}

static
void
_get_favicon(const httplib::Request &req_,
             httplib::Response      &res_)
{
  res_.set_content((const char*)favicon_ico,
                   favicon_ico_len,
                   "image/png");
}

// If Equal Return True
#define IERT(S) if(type_ == (S)) return true;
// If Equal Return False
#define IERF(S) if(type_ == (S)) return false;

static
bool
_valid_fs_type(const fs::path    &path_,
               const std::string &type_)
{
  if(not (str::startswith(path_,"/mnt/")   or
          str::startswith(path_,"/media/") or
          str::startswith(path_,"/opt/")   or
          str::startswith(path_,"/tmp/")   or
          str::startswith(path_,"/srv/")))
    return false;

  IERT("bcachefs");
  IERT("btrfs");
  IERT("exfat");
  IERT("ext2");
  IERT("ext3");
  IERT("ext4");
  IERT("f2fs");
  IERT("jfs");
  IERT("ntfs");
  IERT("reiserfs");
  IERT("vfat");
  IERT("xfs");
  IERT("zfs");

  IERF("fuse.gvfsd-fuse");
  IERF("fuse.kio-fuse");

  if(str::startswith(type_,"fuse."))
    return true;

  return false;
}

static
void
_get_mounts(const httplib::Request &req_,
            httplib::Response      &res_)
{
  json json_array;
  std::string type;
  fs::MountVec mounts;

  fs::mounts(mounts);

  json_array = json::array();
  for(const auto &mount : mounts)
    {
      if(not ::_valid_fs_type(mount.dir,mount.type))
        continue;

      json obj;

      obj["path"] = mount.dir;
      obj["type"] = mount.type;

      json_array.push_back(obj);
    }

  res_.set_content(json_array.dump(),
                   "application/json");
}

static
void
_get_mounts_mergerfs(const httplib::Request &req_,
                     httplib::Response      &res_)
{
  json json_array;
  std::string type;
  fs::MountVec mounts;

  fs::mounts(mounts);

  json_array = json::array();
  for(const auto &mount : mounts)
    {
      if(mount.type != "fuse.mergerfs")
        continue;

      json_array.push_back(mount.dir);
    }

  res_.set_content(json_array.dump(),
                   "application/json");
}

static
void
_get_kvs(const httplib::Request &req_,
         httplib::Response      &res_)
{
  if(not req_.has_param("mount"))
    {
      res_.status = 400;
      res_.set_content("mount param not set",
                       "text/plain");
      return;
    }

  json j;
  std::string mount;
  std::map<std::string,std::string> kvs;

  mount = req_.get_param_value("mount");

  mergerfs::api::get_kvs(mount,&kvs);

  j = kvs;

  res_.set_content(j.dump(),
                   "application/json");
}

static
void
_get_kvs_key(const httplib::Request &req_,
             httplib::Response      &res_)
{
  if(not req_.has_param("mount"))
    {
      res_.status = 400;
      res_.set_content("mount param not set",
                       "text/plain");
      return;
    }

  json j;
  fs::path mount;
  std::string key;
  std::string val;

  key   = req_.path_params.at("key");
  mount = req_.get_param_value("mount");

  mergerfs::api::get_kv(mount,key,&val);

  j = val;

  res_.set_content(j.dump(),
                   "application/json");
}

static
bool
_check_auth(const httplib::Request &req_)
{
  if(g_password.empty())
    return true;

  std::string auth_header = req_.get_header_value("Authorization");
  if(auth_header.empty())
    return false;

  if(auth_header.substr(0,7) == "Bearer ")
    {
      std::string token = auth_header.substr(7);
      return _validate_password(token);
    }

  return false;
}

static
void
_post_kvs_key(const httplib::Request &req_,
              httplib::Response      &res_)
{
  if(not req_.has_param("mount"))
    {
      res_.status = 400;
      res_.set_content("{\"error\":\"mount param not set\"}",
                       "application/json");
      return;
    }

  if(!g_password.empty() && !_check_auth(req_))
    {
      res_.status = 401;
      res_.set_content("{\"error\":\"authentication required\"}",
                       "application/json");
      return;
    }

  try
    {
      int rv;
      fs::path mount;
      std::string key;
      std::string val;

      key = req_.path_params.at("key");
      val = json::parse(req_.body);
      mount = req_.get_param_value("mount");

      rv = mergerfs::api::set_kv(mount,key,val);

      json j;
      if(rv >= 0)
        {
          j["result"] = "success";
          res_.set_content(j.dump(),
                           "application/json");
        }
      else
        {
          res_.status = 400;
          j["result"] = "error";
          j["error"] = ::_generate_error(mount,key,val,rv);
          res_.set_content(j.dump(),
                           "application/json");
        }
    }
  catch(const std::exception &e)
    {
      fmt::print("{}\n",e.what());
      res_.status = 400;
      res_.set_content("invalid json",
                       "text/plain");
    }
}

static
void
_get_auth(const httplib::Request &req_,
          httplib::Response      &res_)
{
  json j;

  j["password_required"] = !g_password.empty();

  res_.set_content(j.dump(), "application/json");
}

static
void
_post_auth_verify(const httplib::Request &req_,
                  httplib::Response      &res_)
{
  try
    {
      json j;
      json body = json::parse(req_.body);

      std::string password = body.value("password", "");

      if(_validate_password(password))
        {
          j["result"] = "success";
          j["valid"] = true;
        }
      else
        {
          res_.status = 401;
          j["result"] = "error";
          j["valid"] = false;
          j["error"] = "Invalid password";
        }

      res_.set_content(j.dump(), "application/json");
    }
  catch(const std::exception &e)
    {
      fmt::print("{}\n", e.what());
      res_.status = 400;
      res_.set_content("invalid json", "text/plain");
    }
}

static
void
_get_branches_info(const httplib::Request &req_,
                   httplib::Response      &res_)
{
  if(not req_.has_param("mount"))
    {
      res_.status = 400;
      res_.set_content("{\"error\":\"mount param not set\"}",
                       "application/json");
      return;
    }

  fs::path mount;
  std::string branches_str;
  std::vector<std::string> branches;

  mount = req_.get_param_value("mount");

  int rv = mergerfs::api::get_kv(mount,"branches",&branches_str);
  if(rv < 0)
    {
      res_.status = 404;
      res_.set_content("{\"error\":\"failed to get branches\"}",
                       "application/json");
      return;
    }

  str::split(branches_str,':',&branches);

  json json_array = json::array();

  for(const auto &branch_full : branches)
    {
      fs::info_t info;
      std::string path;
      std::string mode;
      std::string minfreespace;

      str::splitkv(branch_full,'=',&path,&mode);

      if(!mode.empty())
        {
          std::string::size_type pos = mode.find(',');
          if(pos != std::string::npos)
            {
              minfreespace = mode.substr(pos + 1);
              mode = mode.substr(0,pos);
            }
        }

      json branch_obj;
      branch_obj["path"] = path;
      branch_obj["mode"] = mode;
      branch_obj["minfreespace"] = minfreespace;

      rv = fs::info(path,&info);
      if(rv == 0)
        {
          uint64_t total = info.spaceavail + info.spaceused;

          branch_obj["total_space"] = total;
          branch_obj["used_space"] = info.spaceused;
          branch_obj["available_space"] = info.spaceavail;
          branch_obj["readonly"] = info.readonly;
        }
      else
        {
          branch_obj["total_space"] = 0;
          branch_obj["used_space"] = 0;
          branch_obj["available_space"] = 0;
          branch_obj["readonly"] = false;
          branch_obj["error"] = "Failed to get disk info";
        }

      json_array.push_back(branch_obj);
    }

  res_.set_content(json_array.dump(),
                   "application/json");
}

int
mergerfs::webui::main(const int   argc_,
                      char      **argv_)
{
  CLI::App app;
  bool log;
  int port;
  std::string host;

  app.description("mergerfs.webui:"
                  " A simple webui to configure mergerfs instances");
  app.name("USAGE: mergerfs.webui");

  app.add_option("--host",host)
    ->description("Interface to bind to")
    ->default_val("0.0.0.0");
  app.add_option("--port",port)
    ->description("TCP port to use")
    ->default_val(8080);
  app.add_option("--log",log)
    ->description("Enable logging")
    ->default_val(false)
    ->default_str("false");
  app.add_option("--password",g_password)
    ->description("Require a password for changes.");
  app.add_option("--index.html",g_index_html)
    ->description("Use provided file for serving. For development.");

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e)
    {
      return app.exit(e);
    }

  if(!g_password.empty())
    fmt::print("Password authentication enabled\n");
  else
    fmt::print("No password set. Authentication disabled.\n");

  // TODO: Warn if uid of server is not root but mergerfs is.

  httplib::Server http_server;

  if(log)
    {
      http_server.set_logger([](const httplib::Request  &req_,
                                const httplib::Response &res_)
      {
        fmt::println("{} {} {} -> {}",
                     ::_get_timestamp(),
                     req_.method,
                     req_.path,
                     res_.status);
      });
    }

  http_server.new_task_queue =
    []()
    {
      return new httplib::ThreadPool(2);
    };

  http_server.Get("/",::_get_root);
  http_server.Get("/favicon.ico",_get_favicon);
  http_server.Get("/auth",::_get_auth);
  http_server.Post("/auth/verify",::_post_auth_verify);
  http_server.Get("/mounts",::_get_mounts);
  http_server.Get("/mounts/mergerfs",::_get_mounts_mergerfs);
  http_server.Get("/kvs",::_get_kvs);
  http_server.Get("/kvs/:key",::_get_kvs_key);
  http_server.Post("/kvs/:key",::_post_kvs_key);
  http_server.Get("/branches-info",::_get_branches_info);

  fmt::print("host:port = http://{}:{}\n",
             host,
             port);

  http_server.listen(host,port);

  return 0;
}

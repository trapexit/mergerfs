#include "mergerfs_webui.hpp"

#include "fs_exists.hpp"
#include "fs_mounts.hpp"
#include "mergerfs_api.hpp"
#include "str.hpp"

#include "CLI11.hpp"
#include "fmt/core.h"
#include "httplib.h"
#include "json.hpp"

#include <unistd.h>

using json = nlohmann::json;
using namespace std::string_view_literals;


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
  std::string html;

  if(fs::exists("index.html"))
    {
      res_.set_file_content("index.html");
      return;
    }

  html = R"html(
<!DOCTYPE html>
<html>
<head><title>mergerfs ui</title>
<style>
.tab {
  overflow: hidden;
  border: 1px solid #ccc;
  background-color: #f1f1f1;
}
.tab button {
  background-color: inherit;
  float: left;
  border: none;
  outline: none;
  cursor: pointer;
  padding: 14px 16px;
  transition: 0.3s;
}
.tab button:hover {
  background-color: #ddd;
}
.tab button.active {
  background-color: #ccc;
}
.tabcontent {
  display: none;
  padding: 6px 12px;
  border: 1px solid #ccc;
  border-top: none;
}
</style>
</head>
<body>
<div class="tab">
  <button class="tablinks active" onclick="openTab(event, 'Advanced')">Advanced</button>
  <button class="tablinks" onclick="openTab(event, 'Mounts')">Mounts</button>
</div>
<div id="Advanced" class="tabcontent" style="display: block;">
  <div>
    <label for="mount-select-advanced">Select Mountpoint:</label>
    <select id="mount-select-advanced"></select>
  </div>
  <div id="kv-list"></div>
</div>
<div id="Mounts" class="tabcontent">
  <form>
    <label for="mount-select">Select Mountpoint:</label>
    <select id="mount-select"></select>
  </form>
</div>
<script>
function openTab(evt, tabName) {
  var i, tabcontent, tablinks;
  tabcontent = document.getElementsByClassName("tabcontent");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }
  tablinks = document.getElementsByClassName("tablinks");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }
  document.getElementById(tabName).style.display = "block";
  if (evt) evt.currentTarget.className += " active";
}
function loadKV(mount) {
    let url = '/kvs';
    if (mount) {
        url += '?mount=' + encodeURIComponent(mount);
    }
    fetch(url)
    .then(r => r.json())
    .then(data => {
        const div = document.getElementById('kv-list');
        div.innerHTML = '';
        for (const [k, v] of Object.entries(data)) {
            const input = document.createElement('input');
            input.type = 'text';
            input.value = v;
            input.onkeydown = function(e) {
                if (e.key === 'Enter') {
                    fetch('/kvs', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({k: input.value})
                    });
                }
            };
            const label = document.createElement('label');
            label.textContent = k + ': ';
            label.appendChild(input);
            div.appendChild(label);
            div.appendChild(document.createElement('br'));
        }
    });
}
function loadMounts() {
    fetch('/mounts')
    .then(r => r.json())
    .then(data => {
        const select = document.getElementById('mount-select');
        const selectAdvanced = document.getElementById('mount-select-advanced');
        [select, selectAdvanced].forEach(s => {
            s.innerHTML = '';
            data.forEach(m => {
                const opt = document.createElement('option');
                opt.value = m;
                opt.text = m;
                s.appendChild(opt);
            });
        });
        const onchangeFunc = function() {
            const mount = this.value;
            loadKV(mount);
        };
        select.onchange = onchangeFunc;
        selectAdvanced.onchange = onchangeFunc;
        if (data.length > 0) {
            select.value = data[0];
            selectAdvanced.value = data[0];
            loadKV(data[0]);
        }
    });
}
window.onload = () => { loadMounts(); };
</script>
</body>
</html>
)html";

  res_.set_content(html,
                   "text/html");
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

  IERT("ext2");
  IERT("ext3");
  IERT("ext4");
  IERT("xfs");
  IERT("jfs");
  IERT("btrfs");
  IERT("zfs");
  IERT("reiserfs");
  IERT("f2fs");
  IERT("ntfs");
  IERT("vfat");
  IERT("exfat");

  IERF("fuse.kio-fuse");
  IERF("fuse.gvfsd-fuse");

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
void
_execute_command(const httplib::Request &req_,
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
  std::string cmd;

  mount = req_.get_param_value("mount");
  cmd = req_.path_params.at("command");

  int rv = mergerfs::api::set_kv(mount,"cmd." + cmd,"");

  if(rv >= 0)
    {
      res_.set_content("{\"result\":\"success\",\"message\":\"Command executed\"}", "application/json");
    }
  else
    {
      res_.status = 400;
      res_.set_content("{\"result\":\"error\",\"message\":\"Failed to execute command\"}", "application/json");
    }
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

int
mergerfs::webui::main(const int   argc_,
                      char      **argv_)
{
  CLI::App app;
  std::string host;
  int port;
  std::string password;

  app.description("mergerfs.webui:"
                  " A simple webui to configure mergerfs instances");
  app.name("USAGE: mergerfs.webui");

  app.add_option("--host",host)
    ->description("Interface to bind to")
    ->default_val("0.0.0.0");
  app.add_option("--port",port)
    ->description("TCP port to use")
    ->default_val(8080);
  app.add_option("--password",password)
    ->description("");

  try
    {
      app.parse(argc_,argv_);
    }
  catch(const CLI::ParseError &e)
    {
      return app.exit(e);
    }

  // TODO: Warn if uid of server is not root but mergerfs is.

  httplib::Server http_server;

  http_server.Get("/",::_get_root);
  http_server.Get("/mounts",::_get_mounts);
  http_server.Get("/mounts/mergerfs",::_get_mounts_mergerfs);
  http_server.Get("/kvs",::_get_kvs);
  http_server.Get("/kvs/:key",::_get_kvs_key);
  http_server.Post("/kvs/:key",::_post_kvs_key);

  http_server.set_mount_point("/files","/mnt/tmp");

  fmt::print("host:port = http://{}:{}\n",
             host,
             port);

  http_server.listen(host,port);

  return 0;
}

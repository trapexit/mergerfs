#include "mergerfs_webui.hpp"

#include "mergerfs_api.hpp"
#include "fs_mounts.hpp"
#include "fs_exists.hpp"

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

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

static
void
_get_mounts(const httplib::Request &req_,
            httplib::Response      &res_)
{
  json j;
  fs::MountVec mounts;

  fs::mounts(mounts);

  j = json::array();
  for(const auto &mount : mounts)
    {
      if(mount.type != "fuse.mergerfs")
        continue;
      j.push_back(mount.dir);
    }

  res_.set_content(j.dump(),
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
      res_.set_content("mount param not set", "text/plain");
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
_get_kv(const httplib::Request &req_,
        httplib::Response      &res_)
{
  if(not req_.has_param("mount"))
    {
      res_.status = 400;
      res_.set_content("mount param not set", "text/plain");
      return;
    }

  json j;
  std::string key;
  std::string val;
  fs::path mount;

  key   = req_.path_params.at("key");
  mount = req_.get_param_value("mount");

  mergerfs::api::get_kv(mount,key,&val);

  if(key == "branches")
    {
      res_.set_content(val, "text/plain");
    }
  else
    {
      json j;

      j = val;

      res_.set_content(j.dump(),
                       "application/json");
    }
}

static
void
_post_kvs(const httplib::Request &req_,
          httplib::Response      &res_)
{
  if(not req_.has_param("mount"))
    {
      res_.status = 400;
      res_.set_content("mount param not set","text/plain");
      return;
    }

  try
    {
      json j;
      std::string mount;

      mount = req_.get_param_value("mount");
      j     = json::parse(req_.body);

      for(const auto &[key,val] : j.items())
        {
          std::cout << "mount=" << mount << " | " << key << ": " << val << std::endl;
        }

      res_.set_content("{}", "application/json");
    }
  catch (const std::exception& e)
    {
      res_.status = 400;
      res_.set_content("Invalid JSON", "text/plain");
    }
}

int
mergerfs::webui::main(const int   argc_,
                      char      **argv_)
{
  httplib::Server http_server;
  std::string host;
  int port;

  host = "0.0.0.0";
  port = 8000;

  http_server.Get("/",::_get_root);
  http_server.Get("/mounts",::_get_mounts);
  http_server.Get("/kvs",::_get_kvs);
  http_server.Get("/kvs/:key",::_get_kv);
  http_server.Post("/kvs",::_post_kvs);

  http_server.listen(host,port);

  return 0;
}

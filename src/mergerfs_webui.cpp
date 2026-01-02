#include "mergerfs_webui.hpp"

#include "mergerfs_api.hpp"
#include "fs_mounts.hpp"

#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;

static
void
_serve_root(const httplib::Request &req_,
            httplib::Response      &res_)
{
  std::string html = R"html(<html></html>)html";

  res_.set_content(html,
                   "text/html");
}

static
void
_serve_mounts(const httplib::Request &req_,
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
_serve_kvs(const httplib::Request &req_,
           httplib::Response      &res_)
{
  json j;
  std::map<std::string,std::string> kvs;

  mergerfs::api::get_kvs("/mnt/tmp/mergerfs",&kvs);

  j = kvs;

  res_.set_content(j.dump(),
                   "application/json");
}

static
void
_post_kvs(const httplib::Request &req_,
          httplib::Response      &res_)
{
  try
    {
      json j = json::parse(req_.body);

      for(const auto &[key,val] : j.items())
        {
          std::cout << key << ": " << val << std::endl;
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

  http_server.Get("/",::_serve_root);
  http_server.Get("/mounts",::_serve_mounts);
  http_server.Get("/kvs",::_serve_kvs);
  http_server.Post("/kvs",::_post_kvs);

  http_server.listen(host,port);

  return 0;
}

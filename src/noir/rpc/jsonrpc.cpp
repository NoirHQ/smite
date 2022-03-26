// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/rpc/jsonrpc.h>
#include <fc/io/json.hpp>

namespace noir::rpc {

using namespace appbase;
using namespace noir::jsonrpc;

class jsonrpc_impl : std::enable_shared_from_this<jsonrpc_impl> {
public:
  jsonrpc_impl(appbase::application& app): app(app) {}

  endpoint& get_or_create_endpoint(const std::string& url) {
    if (endpoints.find(url) != endpoints.end())
      return endpoints[url];

    endpoints.emplace(std::make_pair(url, endpoint{}));

    app.get_plugin<rpc>().add_api({
      {url,
        [&](std::string url, std::string body, url_response_callback cb) mutable {
          try {
            if (body.empty())
              body = "{}";
            auto result = endpoints[url].handle_request(body);
            cb(200, result);
          } catch (...) {
            rpc::handle_exception("jsonrpc", "jsonrpc", body, cb);
          }
        }},
    });
    return endpoints[url];
  }

  endpoint& get_or_create_ws_endpoint(const std::string& url) {
    if (ws_endpoints.find(url) != ws_endpoints.end())
      return ws_endpoints[url];

    ws_endpoints.emplace(std::make_pair(url, endpoint{}));

    app().get_plugin<rpc>().add_ws_api({
      {url,
        [&](std::string payload, message_sender sender) mutable {
          try {
            if (payload.empty())
              payload = "{}";
            auto result = ws_endpoints[url].handle_request(payload);
            sender(result);
          } catch (...) {
            sender("Internal Server Error");
          }
        }},
    });
    return ws_endpoints[url];
  }

private:
  appbase::application& app;
  std::map<std::string, endpoint> endpoints;
  std::map<std::string, endpoint> ws_endpoints;
};

jsonrpc::jsonrpc(appbase::application& app): plugin(app), my(new jsonrpc_impl(app)) {}

void jsonrpc::plugin_initialize(const CLI::App& config) {}

void jsonrpc::plugin_startup() {
  ilog("starting jsonrpc");
}

void jsonrpc::plugin_shutdown() {}

endpoint& jsonrpc::get_or_create_endpoint(const std::string& url) {
  return my->get_or_create_endpoint(url);
}

endpoint& jsonrpc::get_or_create_ws_endpoint(const std::string& url) {
  return my->get_or_create_ws_endpoint(url);
}

} // namespace noir::rpc

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
  endpoint& get_or_create_endpoint(const std::string& url) {
    if (endpoints.find(url) != endpoints.end())
      return *endpoints[url];

    endpoints[url] = std::make_unique<endpoint>();

    app().get_plugin<rpc>().add_api({
      {url,
        [&](std::string url, std::string body, url_response_callback cb) mutable {
          try {
            if (body.empty())
              body = "{}";
            auto result = endpoints[url]->handle_request(body);
            cb(200, result);
          } catch (...) {
            rpc::handle_exception("jsonrpc", "jsonrpc", body, cb);
          }
        }},
    });
    return *endpoints[url];
  }

private:
  std::map<std::string, std::unique_ptr<endpoint>> endpoints;
};

jsonrpc::jsonrpc() : my(new jsonrpc_impl()) {}

void jsonrpc::plugin_initialize(const CLI::App& cli, const CLI::App& config) {}

void jsonrpc::plugin_startup() {
  ilog("starting jsonrpc");
}

void jsonrpc::plugin_shutdown() {}

endpoint& jsonrpc::get_or_create_endpoint(const std::string& url) {
  return my->get_or_create_endpoint(url);
}

} // namespace noir::rpc

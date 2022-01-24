#include <eosio/jsonrpc_plugin/jsonrpc_plugin.hpp>
#include <fc/io/json.hpp>

namespace eosio {

static appbase::abstract_plugin& _jsonrpc_plugin = app().register_plugin<jsonrpc_plugin>();

class jsonrpc_plugin_impl {
public:
  jsonrpc::endpoint& get_or_create_endpoint(const std::string& url) {
    if( endpoints.find(url) != endpoints.end() )
      return *endpoints[url];

    endpoints[url] = std::make_unique<jsonrpc::endpoint>();

    app().get_plugin<http_plugin>().add_api({{
      url,
      [&](std::string url, std::string body, url_response_callback cb) mutable {
        try {
          if( body.empty() ) body = "{}";
          auto result = endpoints[url]->handle_request(body);
          cb(200, result);
        } catch( ... ) {
          http_plugin::handle_exception("jsonrpc", "jsonrpc", body, cb);
        }
      }
    }});
    return *endpoints[url];
  }

private:
  std::map<std::string,std::unique_ptr<jsonrpc::endpoint>> endpoints;
};

jsonrpc_plugin::jsonrpc_plugin(): my(new jsonrpc_plugin_impl()) {}
jsonrpc_plugin::~jsonrpc_plugin() {}

void jsonrpc_plugin::plugin_initialize(const variables_map& options) {}

void jsonrpc_plugin::plugin_startup() {
  ilog("starting jsonrpc_plugin");
}

void jsonrpc_plugin::plugin_shutdown() {}

jsonrpc::endpoint& jsonrpc_plugin::get_or_create_endpoint(const std::string& url) {
  return my->get_or_create_endpoint(url);
}

} /// namespace eosio


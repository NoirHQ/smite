#pragma once

#include <appbase/application.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <eosio/jsonrpc_plugin/jsonrpc.hpp>

namespace eosio {

/**
 * This plugin provides json-rpc 2.0 api endpoint over eosio::http_plugin.
 *
 * This was adapted from steem:
 * https://github.com/steemit/steem/blob/ff9b801/libraries/plugins/json_rpc/json_rpc_plugin.cpp
 */
class jsonrpc_plugin: public appbase::plugin<jsonrpc_plugin> {
public:
  jsonrpc_plugin();
  virtual ~jsonrpc_plugin();

  APPBASE_PLUGIN_REQUIRES( (http_plugin) )
  virtual void set_program_options(options_description&, options_description& cfg) override {}

  void plugin_initialize(const variables_map& options);
  void plugin_startup();
  void plugin_shutdown();

  jsonrpc::endpoint& get_or_create_endpoint(const std::string& url);

private:
  std::unique_ptr<class jsonrpc_plugin_impl> my;
};

} /// namespace eosio

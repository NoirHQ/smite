// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/rpc/jsonrpc/endpoint.h>
#include <noir/rpc/rpc.h>
#include <appbase/application.hpp>

namespace noir::rpc {

/**
 * This plugin provides json-rpc 2.0 api endpoint over noir::rpc.
 *
 * This was adapted from steem:
 * https://github.com/steemit/steem/blob/ff9b801/libraries/plugins/json_rpc/json_rpc_plugin.cpp
 */
class jsonrpc : public appbase::plugin<jsonrpc> {
public:
  jsonrpc();

  APPBASE_PLUGIN_REQUIRES((rpc))
  void set_program_options(CLI::App& cli, CLI::App& config) override {}

  void plugin_initialize(const CLI::App& cli, const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  noir::jsonrpc::endpoint& get_or_create_endpoint(const std::string& url);

private:
  std::shared_ptr<class jsonrpc_impl> my;
};

} // namespace noir::rpc

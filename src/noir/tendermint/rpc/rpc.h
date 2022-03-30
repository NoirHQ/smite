// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/rpc/jsonrpc.h>
#include <noir/tendermint/rpc/api.h>
#include <noir/tx_pool/tx_pool.h>
#include <appbase/application.hpp>

namespace noir::tendermint::rpc {

class rpc : public appbase::plugin<rpc> {
public:
  explicit rpc(appbase::application& app);

  APPBASE_PLUGIN_REQUIRES((noir::rpc::jsonrpc)(noir::tx_pool::tx_pool))
  void set_program_options(CLI::App& config) override;

  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

private:
  std::unique_ptr<api> api_;
};

} // namespace noir::tendermint::rpc

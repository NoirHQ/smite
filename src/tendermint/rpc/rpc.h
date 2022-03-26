// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/rpc/rpc.h>
#include <appbase/application.hpp>
#include <tendermint/rpc/mempool.h>

namespace tendermint::rpc {

class rpc : public appbase::plugin<rpc> {
public:
  rpc(appbase::application& app);

  APPBASE_PLUGIN_REQUIRES((noir::rpc::rpc)(noir::rpc::jsonrpc))
  void set_program_options(CLI::App& config) override;

  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

private:
  std::shared_ptr<mempool> mempool_;
};

} // namespace tendermint::rpc

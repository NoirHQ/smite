// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/rpc/jsonrpc.h>
#include <appbase/application.hpp>
#include <eth/common/types.h>

namespace eth::rpc {

class rpc : public appbase::plugin<rpc> {
public:
  APPBASE_PLUGIN_REQUIRES((noir::rpc::jsonrpc))
  void set_program_options(CLI::App& config) override;

  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  static void check_send_raw_tx(const fc::variant& req);
  std::string send_raw_tx(const std::string& rlp);

private:
  eth::uint256_t tx_fee_cap;
  bool allow_unprotected_txs;
};

} // namespace eth::rpc

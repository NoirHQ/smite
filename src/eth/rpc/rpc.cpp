// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/cli_helper.h>
#include <eth/rpc/api.h>
#include <eth/rpc/rpc.h>

namespace eth::rpc {

using namespace appbase;
using namespace noir;

rpc::rpc(): api(std::make_unique<api::api>()){};

void rpc::set_program_options(CLI::App& config) {
  auto eth_options = config.add_section("eth", "Ethereum Configuration");

  eth_options
    ->add_option("--rpc-tx-fee-cap", tx_fee_cap,
      "RPC tx fee cap is the global transaction fee(price * gaslimit) cap for send transaction variants")
    ->force_callback()
    ->default_val(0);
  eth_options
    ->add_option("--rpc-allow-unprotected-txs", allow_unprotected_txs,
      "Allow for unprotected transactions to be submitted via RPC")
    ->force_callback()
    ->default_val(false);
}

void rpc::plugin_initialize(const CLI::App& config) {
  ilog("initializing ethereum rpc");

  auto& endpoint = app().get_plugin<noir::rpc::jsonrpc>().get_or_create_endpoint("/eth");
  endpoint.add_handler("eth_sendRawTransaction", [this](auto& req) {
    api::api::check_send_raw_tx(req);
    auto params = req.get_array();
    auto rlp = params[0].get_string();
    auto tx_hash = api->send_raw_tx(rlp, tx_fee_cap, allow_unprotected_txs);
    return fc::variant(tx_hash);
  });
}

void rpc::plugin_startup() {
  ilog("starting ethereum rpc");
}

void rpc::plugin_shutdown() {}

} // namespace eth::rpc

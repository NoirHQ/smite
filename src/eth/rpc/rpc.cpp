// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/helper/cli.h>
#include <eth/rpc/api.h>
#include <eth/rpc/rpc.h>

namespace eth::rpc {

using namespace appbase;
using namespace noir;

rpc::rpc(appbase::application& app): plugin(app), api(std::make_unique<api::api>()){};

void rpc::set_program_options(CLI::App& config) {
  auto eth_options = config.add_section("eth", "Ethereum Configuration");

  eth_options
    ->add_option("--rpc-tx-fee-cap",
      "RPC tx fee cap is the global transaction fee(price * gaslimit) cap for send transaction variants")
    ->default_val(0);
  eth_options->add_option("--rpc-allow-unprotected-txs", "Allow for unprotected transactions to be submitted via RPC")
    ->default_val(false);
}

void rpc::plugin_initialize(const CLI::App& config) {
  ilog("initializing ethereum rpc");

  auto eth_options = config.get_subcommand("eth");
  auto tx_fee_cap = eth_options->get_option("--rpc-tx-fee-cap")->as<uint256_t>();
  auto allow_unprotected_txs = eth_options->get_option("--rpc-allow-unprotected-txs")->as<bool>();

  api->set_tx_fee_cap(tx_fee_cap);
  api->set_allow_unprotected_txs(allow_unprotected_txs);

  auto tx_poor_ptr = app.find_plugin<tx_pool::tx_pool>();
  api->set_tx_pool_ptr(tx_poor_ptr);

  auto& block_store_ptr = app.get_plugin<consensus::abci>().node_->block_store_;
  api->set_block_store(block_store_ptr);
}

void rpc::plugin_startup() {
  ilog("starting ethereum rpc");

  auto& endpoint = app.get_plugin<noir::rpc::jsonrpc>().get_or_create_endpoint("/eth");
  endpoint.add_handler("eth_sendRawTransaction", [&](auto& req) { return api->send_raw_tx(req); });
  endpoint.add_handler("eth_chainId", [&](auto& req) { return api->chain_id(req); });
  endpoint.add_handler("net_version", [&](auto& req) { return api->net_version(req); });
  endpoint.add_handler("net_listening", [&](auto& req) { return api->net_listening(req); });
  endpoint.add_handler("eth_getBalance", [&](auto& req) { return api->get_balance(req); });
  endpoint.add_handler("eth_getTransactionCount", [&](auto& req) { return api->get_tx_count(req); });
  endpoint.add_handler("eth_blockNumber", [&](auto& req) { return api->block_number(req); });
  endpoint.add_handler("eth_gasPrice", [&](auto& req) { return api->gas_price(req); });
  endpoint.add_handler("eth_estimateGas", [&](auto& req) { return api->estimate_gas(req); });
  endpoint.add_handler("eth_getTransactionByHash", [&](auto& req) { return api->get_tx_by_hash(req); });
  endpoint.add_handler("eth_getBlockByNumber", [&](auto& req) { return api->get_block_by_number(req); });
  endpoint.add_handler("eth_getBlockByHash", [&](auto& req) { return api->get_block_by_hash(req); });
  endpoint.add_handler("eth_getTransactionReceipt", [&](auto& req) { return api->get_tx_receipt(req); });
  endpoint.add_handler("eth_call", [&](auto& req) { return api->call(req); });
}

void rpc::plugin_shutdown() {}

} // namespace eth::rpc

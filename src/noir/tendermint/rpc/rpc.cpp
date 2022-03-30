// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/rpc/jsonrpc.h>
#include <noir/tendermint/rpc/rpc.h>

namespace noir::tendermint::rpc {
using namespace appbase;
using namespace fc;
using namespace noir;
using namespace noir::rpc;

rpc::rpc(appbase::application& app): plugin(app), api_(std::make_unique<api>()) {}

void rpc::set_program_options(CLI::App& config) {}

void rpc::plugin_initialize(const CLI::App& config) {
  ilog("initializing tendermint rpc");

  auto tx_poor_ptr = app.find_plugin<tx_pool::tx_pool>();
  api_->set_tx_pool_ptr(tx_poor_ptr);
}

void rpc::plugin_startup() {
  ilog("starting tendermint rpc");

  auto& endpoint = app.get_plugin<noir::rpc::jsonrpc>().get_or_create_endpoint("/tendermint");
  endpoint.add_handler("broadcast_tx_async", [&](auto& req) { return api_->broadcast_tx_async(req); });
  endpoint.add_handler("broadcast_tx_sync", [&](auto& req) { return api_->broadcast_tx_sync(req); });
  endpoint.add_handler("broadcast_tx_commit", [&](auto& req) { return api_->broadcast_tx_commit(req); });
  endpoint.add_handler("unconfirmed_txs", [&](auto& req) { return api_->unconfirmed_txs(req); });
  endpoint.add_handler("num_unconfirmed_txs", [&](auto& req) { return api_->num_unconfirmed_txs(req); });
  endpoint.add_handler("check_tx", [&](auto& req) { return api_->check_tx(req); });
}

void rpc::plugin_shutdown() {}

} // namespace noir::tendermint::rpc

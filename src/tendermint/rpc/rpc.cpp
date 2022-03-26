// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/rpc/jsonrpc.h>
#include <tendermint/rpc/rpc.h>

namespace tendermint::rpc {
using namespace appbase;
using namespace fc;
using namespace noir;
using namespace noir::rpc;

rpc::rpc(appbase::application& app): plugin(app), mempool_(std::make_shared<mempool>()){};

void rpc::set_program_options(CLI::App& config) {}

void rpc::plugin_initialize(const CLI::App& config) {
  ilog("initializing tendermint rpc");

  auto tx_poor_ptr = app.find_plugin<tx_pool::tx_pool>();
  mempool_->set_tx_pool_ptr(tx_poor_ptr);
}

void rpc::plugin_startup() {
  ilog("starting tendermint rpc");

  auto& endpoint = app.get_plugin<noir::rpc::jsonrpc>().get_or_create_endpoint("/tendermint");
  endpoint.add_handler("broadcast_tx_async", [&](auto& req) {
    auto tx = req.get_object()["tx"].as_string();
    auto d = base64_decode(tx);
    std::vector<char> raw_tx(d.begin(), d.end());
    auto result = mempool_->broadcast_tx_async(raw_tx);
    variant res;
    to_variant(result, res);
    return res;
  });
  endpoint.add_handler("broadcast_tx_sync", [&](auto& req) {
    auto enc_tx = req.get_object()["tx"].as_string();
    auto d = base64_decode(enc_tx);
    std::vector<char> raw_tx(d.begin(), d.end());
    auto result = mempool_->broadcast_tx_sync(raw_tx);
    variant res;
    to_variant(result, res);
    return res;
  });
  endpoint.add_handler("broadcast_tx_commit", [&](auto& req) {
    //      auto enc_tx = req.get_object()["tx"].as_string();
    //      auto d = fc::base64_decode(enc_tx);
    //      std::vector<char> raw_tx(d.begin(), d.end());
    //      auto result = mempool_->broadcast_tx_commit(raw_tx);
    //      variant res;
    //      to_variant(result, res);
    //      return res;
    check(false, "not implemented yet");
    return fc::variant(nullptr);
  });
  endpoint.add_handler("unconfirmed_txs", [&](const fc::variant& req) {
    auto limit = req.get_object()["limit"].as<uint32_t>();
    auto result = mempool_->unconfirmed_txs(limit);
    variant res;
    to_variant(result, res);
    return fc::variant(res);
  });
  endpoint.add_handler("num_unconfirmed_txs", [&](auto& req) {
    auto result = mempool_->num_unconfirmed_txs();
    variant res;
    to_variant(result, res);
    return res;
  });
  endpoint.add_handler("check_tx", [&](auto& req) {
    auto enc_tx = req.get_object()["tx"].as_string();
    auto d = base64_decode(enc_tx);
    std::vector<char> raw_tx(d.begin(), d.end());
    auto result = mempool_->check_tx(raw_tx);
    variant res;
    to_variant(result, res);
    return res;
  });
}

void rpc::plugin_shutdown() {}

} // namespace tendermint::rpc

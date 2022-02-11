// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/rlp.h>
#include <noir/common/check.h>
#include <noir/common/cli_helper.h>
#include <noir/common/hex.h>
#include <appbase/application.hpp>
#include <eth/rpc/rpc.h>
#include <eth/rpc/tx.h>

namespace eth::rpc {

using namespace appbase;
using namespace noir;
using namespace noir::codec;

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
    check_send_raw_tx(req);
    auto params = req.get_array();
    auto rlp = params[0].get_string();
    auto tx_hash = send_raw_tx(rlp);
    return fc::variant(tx_hash);
  });
}

void rpc::plugin_startup() {
  ilog("starting ethereum rpc");
}

void rpc::plugin_shutdown() {}

void rpc::check_send_raw_tx(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto& params = req.get_array();
  check(!params.empty(), "invalid parameters: not enough params to decode");
  check(params.size() == 1, "too many arguments, want at most 1");
  check(params[0].is_string(), "invalid parameters: json: cannot unmarshal");
}

std::string rpc::send_raw_tx(const std::string& rlp) {
  auto t = rlp::decode<tx>(from_hex(rlp));
  check(tx_fee_cap == 0 || t.fee() <= tx_fee_cap, "tx fee exceeds the configured cap");
  if (!allow_unprotected_txs && !(t.v != 0 && t.v != 1 && t.v != 27 && t.v != 28)) {
    throw std::runtime_error("only replay-protected transactions allowed over RPC");
  }

  // TODO: add tx pool and return tx_hash
  std::string tx_hash;
  return tx_hash;
}

} // namespace eth::rpc

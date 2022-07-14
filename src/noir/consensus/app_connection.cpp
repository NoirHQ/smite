// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/application/noop_app.h>
#include <noir/application/socket_app.h>
#include <noir/consensus/app_connection.h>

namespace noir::consensus {

app_connection::app_connection(const std::string& proxy_app) {
  if (proxy_app == "noop") {
    application = std::make_shared<application::noop_app>();
    return;
  } else if (proxy_app == "socket") {
    application = std::make_shared<application::socket_app>();
    is_socket = true;
    return;
  } else if (proxy_app.empty()) {
    application = std::make_shared<application::base_application>();
    return;
  }
  check(false, "failed to load application");
}

Result<void> app_connection::start() {
  // not much to do
  return success();
}

std::unique_ptr<tendermint::abci::ResponseBeginBlock> app_connection::begin_block_sync(
  const tendermint::abci::RequestBeginBlock& req) {
  std::scoped_lock g(mtx);
  return std::move(application->begin_block(req));
}
std::unique_ptr<tendermint::abci::ResponseEndBlock> app_connection::end_block_sync(
  const tendermint::abci::RequestEndBlock& req) {
  std::scoped_lock g(mtx);
  return std::move(application->end_block(req));
}
std::unique_ptr<tendermint::abci::ResponseDeliverTx> app_connection::deliver_tx_async(
  const tendermint::abci::RequestDeliverTx& req) {
  std::scoped_lock g(mtx);
  return std::move(application->deliver_tx_async(req));
}
std::unique_ptr<tendermint::abci::ResponseCommit> app_connection::commit_sync() {
  std::scoped_lock g(mtx);
  return std::move(application->commit());
}

std::unique_ptr<tendermint::abci::ResponseCheckTx> app_connection::check_tx_sync(request_check_tx req) {
  std::scoped_lock g(mtx);
  return {};
}

std::unique_ptr<tendermint::abci::ResponseCheckTx> app_connection::check_tx_async(request_check_tx req) {
  std::scoped_lock g(mtx);
  return {};
}

response_prepare_proposal& app_connection::prepare_proposal_sync(request_prepare_proposal req) {
  std::scoped_lock g(mtx);
  auto& res = application->prepare_proposal();
  return res;
}

void app_connection::flush_async() {
  std::scoped_lock g(mtx);
}

void app_connection::flush_sync() {
  std::scoped_lock g(mtx);
}

} // namespace noir::consensus

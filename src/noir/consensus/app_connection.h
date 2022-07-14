// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/application/app.h>

namespace noir::consensus {

struct app_connection {
  app_connection(const std::string& proxy_app = "");

  Result<void> start();

  std::unique_ptr<tendermint::abci::ResponseBeginBlock> begin_block_sync(
    const tendermint::abci::RequestBeginBlock& req);
  response_end_block end_block_sync(request_end_block req);
  req_res<response_deliver_tx> deliver_tx_async(request_deliver_tx req);
  response_commit commit_sync();

  response_prepare_proposal& prepare_proposal_sync(request_prepare_proposal req);
  response_check_tx& check_tx_sync(request_check_tx req);
  req_res<response_check_tx>& check_tx_async(request_check_tx req);
  void flush_async();
  void flush_sync();

  std::shared_ptr<application::base_application> application;
  bool is_socket{}; // FIXME: remove later; for now it's used for ease

private:
  std::mutex mtx;
};

} // namespace noir::consensus

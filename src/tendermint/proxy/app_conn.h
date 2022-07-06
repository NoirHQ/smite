// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/abci/client/client.h>
#include <tendermint/abci/types.pb.h>

namespace noir::proxy {

template<abci::Client ABCIClient>
class AppConnMempool {
public:
  AppConnMempool(const std::shared_ptr<ABCIClient>& app_conn): app_conn(app_conn) {}

  void set_response_callback(abci::Callback cb) {
    app_conn->set_response_callback(cb);
  }

  auto error() -> Result<void> {
    return app_conn->error();
  }

  auto check_tx_async(const abci::RequestCheckTx& req) -> Result<std::shared_ptr<abci::ReqRes>> {
    return app_conn->check_tx_async(req);
  }

  auto check_tx_sync(const abci::RequestCheckTx& req) -> Result<std::unique_ptr<abci::ResponseCheckTx>> {
    return app_conn->check_tx_sync(req);
  }

  auto flush_async() -> Result<std::shared_ptr<abci::ReqRes>> {
    return app_conn->flush_async();
  }

  auto flush_sync() -> Result<void> {
    return app_conn->flush_sync();
  }

private:
  std::shared_ptr<ABCIClient> app_conn;
};

} // namespace noir::proxy

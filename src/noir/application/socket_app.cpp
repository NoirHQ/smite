// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/application/socket_app.h>

#include <noir/net/tcp_conn.h>
#include <tendermint/abci/client/socket_client.h>
#include <tendermint/abci/types.pb.h>

namespace noir::application {

using namespace consensus;
using namespace tendermint::abci;

struct cli_impl {
  cli_impl() {
    conn = std::make_shared<abci::SocketClient<net::TcpConn>>("127.0.0.1:26658", true); // TODO : read from config
    conn->start();
  }
  std::shared_ptr<abci::SocketClient<net::TcpConn>> conn;
};

socket_app::socket_app() {
  my_cli = std::make_shared<cli_impl>();
}

std::unique_ptr<ResponseInfo> socket_app::info_sync(const RequestInfo& req) {
  auto res = my_cli->conn->info_sync(req);
  if (!res)
    return {};
  return std::move(res.value());
}

std::unique_ptr<ResponseInitChain> socket_app::init_chain(const RequestInitChain& req) {
  auto res = my_cli->conn->init_chain_sync(req);
  if (!res)
    return {};
  return std::move(res.value());
}

std::unique_ptr<ResponseBeginBlock> socket_app::begin_block(const RequestBeginBlock& req) {
  ilog("!!! BeginBlock !!!");
  auto res = my_cli->conn->begin_block_sync(req);
  if (!res)
    return {};
  return std::move(res.value());
}
std::unique_ptr<ResponseEndBlock> socket_app::end_block(const RequestEndBlock& req) {
  ilog("!!! EndBlock !!!");
  auto res = my_cli->conn->end_block_sync(req);
  if (!res)
    return {};
  return std::move(res.value());
}
std::unique_ptr<ResponseDeliverTx> socket_app::deliver_tx_async(const RequestDeliverTx& req) {
  ilog("!!! DeliverTx !!!");
  auto res = my_cli->conn->deliver_tx_sync(req);
  if (!res)
    return {};
  return std::move(res.value());
}

} // namespace noir::application

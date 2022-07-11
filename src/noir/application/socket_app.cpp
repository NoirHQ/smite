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
    conn = std::make_shared<abci::SocketClient<net::TcpConn>>("127.0.0.1:26658", true);
    conn->start();
  }
  std::shared_ptr<abci::SocketClient<net::TcpConn>> conn;
};

socket_app::socket_app() {
  my_cli = std::make_shared<cli_impl>();
}

consensus::response_init_chain& socket_app::init_chain() {
  RequestInitChain init_req;
  my_cli->conn->init_chain_sync(init_req); /// called by replay_block(), which is not implemented yet

  return response_init_chain_;
}

consensus::response_begin_block& socket_app::begin_block() {
  // ilog("!!! BeginBlock !!!");
  request_begin_block req;

  RequestBeginBlock b_req;
  b_req.set_hash({req.hash.begin(), req.hash.end()});
  b_req.set_allocated_header(block_header::to_proto(req.header_).release());
  auto last_commit_info = b_req.mutable_last_commit_info();
  last_commit_info->set_round(req.last_commit_info_.round);
  auto votes = last_commit_info->mutable_votes();
  for (auto& v : req.last_commit_info_.votes) {
    auto new_vote = votes->Add();
    new_vote->set_signed_last_block(v.signed_last_block);
    auto new_val = new_vote->mutable_validator();
    new_val->set_address({v.validator_.address.begin(), v.validator_.address.end()});
    new_val->set_power(v.validator_.voting_power);
  }
  Result<std::unique_ptr<ResponseBeginBlock>> res = my_cli->conn->begin_block_sync(b_req);

  return response_begin_block_;
}
consensus::req_res<consensus::response_deliver_tx>& socket_app::deliver_tx_async() {
  // ilog("!!! DeliverTx !!!");
  return req_res_deliver_tx_;
}
consensus::response_end_block& socket_app::end_block() {
  // ilog("!!! EndBlock !!!");
  return response_end_block_;
}

} // namespace noir::application

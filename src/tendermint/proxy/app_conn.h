// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/abci/client/client.h>
#include <tendermint/abci/types.pb.h>
#include <tendermint/common/common.h>

namespace tendermint::proxy {

template<typename ABCIClient>
class AppConnConsensus {
public:
  AppConnConsensus(const std::string& address, boost::asio::io_context& io_context)
    : app_conn(address, true, io_context) {}

  void set_response_callback(abci::Callback cb) {
    app_conn.set_response_callback(cb);
  }
  result<void> error() {
    return app_conn.error();
  }

  result<std::unique_ptr<abci::ResponseInitChain>> init_chain_sync(const abci::RequestInitChain& req) {
    return app_conn.init_chain_sync(req);
  }

  result<std::unique_ptr<abci::ResponseBeginBlock>> begin_block_sync(const abci::RequestBeginBlock& req) {
    return app_conn.begin_block_sync(req);
  }
  result<std::shared_ptr<abci::ReqRes>> deliver_tx_async(const abci::RequestDeliverTx& req) {
    return app_conn.deliver_tx_async(req);
  }
  result<std::unique_ptr<abci::ResponseEndBlock>> end_block_sync(const abci::RequestEndBlock& req) {
    return app_conn.end_block_sync(req);
  }
  result<std::unique_ptr<abci::ResponseCommit>> commit_sync(const abci::RequestCommit& req) {
    return app_conn.commit_sync(req);
  }

  ABCIClient app_conn;
};

template<typename ABCIClient>
class AppConnMempool {
public:
  AppConnMempool(const std::string& address, boost::asio::io_context& io_context)
    : app_conn(address, true, io_context) {}

  void set_response_callback(abci::Callback cb) {
    app_conn.set_response_callback(cb);
  }
  result<void> error() {
    return app_conn.error();
  }

  result<std::shared_ptr<abci::ReqRes>> check_tx_async(const abci::RequestCheckTx& req) {
    return app_conn.check_tx_async(req);
  }
  result<std::unique_ptr<abci::ResponseCheckTx>> check_tx_sync(const abci::RequestCheckTx& req) {
    return app_conn.check_tx_sync(req);
  }

  result<std::shared_ptr<abci::ReqRes>> flush_async() {
    return app_conn.flush_async();
  }
  result<void> flush_sync() {
    return app_conn.flush_sync();
  }

  ABCIClient app_conn;
};

template<typename ABCIClient>
class AppConnQuery {
public:
  AppConnQuery(const std::string& address, boost::asio::io_context& io_context)
    : app_conn(address, true, io_context) {}

  result<void> error() {
    return app_conn.error();
  }

  result<std::unique_ptr<abci::ResponseEcho>> echo_sync(const std::string& msg) {
    return app_conn.echo_sync(msg);
  }
  result<std::unique_ptr<abci::ResponseInfo>> info_sync(const abci::RequestInfo& req) {
    return app_conn.info_sync(req);
  }
  result<std::unique_ptr<abci::ResponseQuery>> query_sync(const abci::RequestQuery& req) {
    return app_conn.query_sync(req);
  }

  ABCIClient app_conn;
};

template<typename ABCIClient>
class AppConnSnapshot {
public:
  AppConnSnapshot(const std::string& address, boost::asio::io_context& io_context)
    : app_conn(address, true, io_context) {}

  result<void> error() {
    return app_conn.error();
  }

  result<std::unique_ptr<abci::ResponseListSnapshots>> list_snapshots_sync(const abci::RequestListSnapshots& req) {
    return app_conn.list_snapshots_sync(req);
  }
  result<std::unique_ptr<abci::ResponseOfferSnapshot>> offer_snapshot_sync(const abci::RequestOfferSnapshot& req) {
    return app_conn.offer_snapshot_sync(req);
  }
  result<std::unique_ptr<abci::ResponseLoadSnapshotChunk>> load_snapshot_chunk_sync(const abci::RequestLoadSnapshotChunk& req) {
    return app_conn.load_snapshot_sync(req);
  }
  result<std::unique_ptr<abci::ResponseApplySnapshotChunk>> apply_snapshot_chunk_sync(const abci::RequestApplySnapshotChunk& req) {
    return app_conn.apply_snapshot_chunk_sync(req);
  }

  ABCIClient app_conn;
};

} // namespace tendermint::proxy

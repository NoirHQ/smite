// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/abci/client/client.h>
#include <tendermint/common/common.h>
#include <boost/asio/io_context_strand.hpp>

namespace tendermint::abci {

static constexpr auto req_queue_size = 256;
static constexpr auto flush_throttle_ms = 20;

class SocketClient : public Client<SocketClient> {
public:
  /// \breif creates a new socket client
  ///
  /// SocketClient creates a new socket client, which connects to a given
  /// address. If must_connect is true, the client will throw an exception  upon start
  /// if it fails to connect.
  SocketClient(const std::string& address, bool must_connect, boost::asio::io_context& io_context);

  void on_set_response_callback(Callback cb) noexcept;
  result<void> on_error() noexcept;

  result<std::shared_ptr<ReqRes>> on_echo_async(const std::string& msg) noexcept;
  result<std::shared_ptr<ReqRes>> on_flush_async() noexcept;
  result<std::shared_ptr<ReqRes>> on_info_async(const RequestInfo& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_set_option_async(const RequestSetOption& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_deliver_tx_async(const RequestDeliverTx& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_check_tx_async(const RequestCheckTx& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_query_async(const RequestQuery& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_commit_async(const RequestCommit& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_init_chain_async(const RequestInitChain& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_begin_block_async(const RequestBeginBlock& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_end_block_async(const RequestEndBlock& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_list_snapshots_async(const RequestListSnapshots& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_offer_snapshot_async(const RequestOfferSnapshot& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_load_snapshot_chunk_async(const RequestLoadSnapshotChunk& req) noexcept;
  result<std::shared_ptr<ReqRes>> on_apply_snapshot_chunk_async(const RequestApplySnapshotChunk& req) noexcept;

  result<std::unique_ptr<ResponseEcho>> on_echo_sync(const std::string& msg) noexcept;
  result<void> on_flush_sync() noexcept;
  result<std::unique_ptr<ResponseInfo>> on_info_sync(const RequestInfo& req) noexcept;
  result<std::unique_ptr<ResponseSetOption>> on_set_option_sync(const RequestSetOption& req) noexcept;
  result<std::unique_ptr<ResponseDeliverTx>> on_deliver_tx_sync(const RequestDeliverTx& req) noexcept;
  result<std::unique_ptr<ResponseCheckTx>> on_check_tx_sync(const RequestCheckTx& req) noexcept;
  result<std::unique_ptr<ResponseQuery>> on_query_sync(const RequestQuery& req) noexcept;
  result<std::unique_ptr<ResponseCommit>> on_commit_sync(const RequestCommit& req) noexcept;
  result<std::unique_ptr<ResponseInitChain>> on_init_chain_sync(const RequestInitChain& req) noexcept;
  result<std::unique_ptr<ResponseBeginBlock>> on_begin_block_sync(const RequestBeginBlock& req) noexcept;
  result<std::unique_ptr<ResponseEndBlock>> on_end_block_sync(const RequestEndBlock& req) noexcept;
  result<std::unique_ptr<ResponseListSnapshots>> on_list_snapshots_sync(const RequestListSnapshots& req) noexcept;
  result<std::unique_ptr<ResponseOfferSnapshot>> on_offer_snapshot_sync(const RequestOfferSnapshot& req) noexcept;
  result<std::unique_ptr<ResponseLoadSnapshotChunk>> on_load_snapshot_chunk_sync(
    const RequestLoadSnapshotChunk& req) noexcept;
  result<std::unique_ptr<ResponseApplySnapshotChunk>> on_apply_snapshot_chunk_sync(
    const RequestApplySnapshotChunk& req) noexcept;

  result<void> on_start() noexcept;
  void on_stop() noexcept;
  result<void> on_reset() noexcept;

private:
  std::string addr;
  bool must_connect;
  // net::Conn conn;

  // flush_timer;

  std::mutex mtx;
  std::optional<tendermint::error> err;
  std::deque<std::shared_ptr<ReqRes>> req_sent;
  std::function<void(Request*, Response*)> res_cb;

  boost::asio::io_context::strand strand;

  std::shared_ptr<ReqRes> queue_request(std::unique_ptr<Request> req);
  void will_send_req(std::shared_ptr<ReqRes> reqres);
  result<void> did_recv_response(std::unique_ptr<Response> res);
  void flush_queue();
  void stop_for_error(const tendermint::error& err);
};

} // namespace tendermint::abci

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/abci/types.pb.h>
#include <tendermint/common/common.h>
#include <tendermint/service/service.h>
#include <appbase/application.hpp>
#include <atomic>
#include <functional>

namespace tendermint::abci {

static constexpr auto dial_retry_interval_seconds = 3;
static constexpr auto echo_retry_interval_seconds = 1;

using Callback = std::function<void(Request*, Response*)>;

struct ReqRes {
  std::unique_ptr<Request> request;
  std::unique_ptr<Response> response; // FIXME: atomic

  mutable std::mutex mtx;
  bool done{false};
  std::function<void(Response*)> cb;

  void set_callback(std::function<void(Response*)> cb);
  void invoke_callback() const;
  std::function<void(Response*)> get_callback() const;
  void set_done();
  void wait() const;
};

template<typename Derived>
class Client : public service::Service<Client<Derived>> {
public:
  void set_response_callback(Callback cb) noexcept {
    return static_cast<Derived*>(this)->on_set_response_callback(cb);
  }
  result<void> error() noexcept {
    return static_cast<Derived*>(this)->on_error();
  }

  result<std::shared_ptr<ReqRes>> echo_async(const std::string& msg) noexcept {
    return static_cast<Derived*>(this)->on_echo_async(msg);
  }
  result<std::shared_ptr<ReqRes>> flush_async() noexcept {
    return static_cast<Derived*>(this)->on_flush_async();
  }
  result<std::shared_ptr<ReqRes>> info_async(const RequestInfo& req) noexcept {
    return static_cast<Derived*>(this)->on_info_async(req);
  }
  result<std::shared_ptr<ReqRes>> set_option_async(const RequestSetOption& req) noexcept {
    return static_cast<Derived*>(this)->on_set_option_async(req);
  }
  result<std::shared_ptr<ReqRes>> deliver_tx_async(const RequestDeliverTx& req) noexcept {
    return static_cast<Derived*>(this)->on_deliver_tx_async(req);
  }
  result<std::shared_ptr<ReqRes>> check_tx_async(const RequestCheckTx& req) noexcept {
    return static_cast<Derived*>(this)->on_check_tx_async(req);
  }
  result<std::shared_ptr<ReqRes>> query_async(const RequestQuery& req) noexcept {
    return static_cast<Derived*>(this)->on_query_async(req);
  }
  result<std::shared_ptr<ReqRes>> commit_async(const RequestCommit& req) noexcept {
    return static_cast<Derived*>(this)->on_commit_async(req);
  }
  result<std::shared_ptr<ReqRes>> init_chain_async(const RequestInitChain& req) noexcept {
    return static_cast<Derived*>(this)->on_init_chain_async(req);
  }
  result<std::shared_ptr<ReqRes>> begin_block_async(const RequestBeginBlock& req) noexcept {
    return static_cast<Derived*>(this)->on_begin_block_async(req);
  }
  result<std::shared_ptr<ReqRes>> end_block_async(const RequestEndBlock& req) noexcept {
    return static_cast<Derived*>(this)->on_end_block_async(req);
  }
  result<std::shared_ptr<ReqRes>> list_snapshots_async(const RequestListSnapshots& req) noexcept {
    return static_cast<Derived*>(this)->on_list_snapshots_sync(req);
  }
  result<std::shared_ptr<ReqRes>> offer_snapshot_async(const RequestOfferSnapshot& req) noexcept {
    return static_cast<Derived*>(this)->on_offer_snapshot_async(req);
  }
  result<std::shared_ptr<ReqRes>> load_snapshot_chunk_async(const RequestLoadSnapshotChunk& req) noexcept {
    return static_cast<Derived*>(this)->on_load_snapshot_chunk_async(req);
  }
  result<std::shared_ptr<ReqRes>> apply_snapshot_chunk_async(const RequestApplySnapshotChunk& req) noexcept {
    return static_cast<Derived*>(this)->on_apply_snapshot_chunk_async(req);
  }

  result<std::unique_ptr<ResponseEcho>> echo_sync(const std::string& msg) noexcept {
    return static_cast<Derived*>(this)->on_echo_sync(msg);
  }
  result<void> flush_sync() noexcept {
    return static_cast<Derived*>(this)->on_flush_sync();
  }
  result<std::unique_ptr<ResponseInfo>> info_sync(const RequestInfo& req) noexcept {
    return static_cast<Derived*>(this)->on_info_sync(req);
  }
  result<std::unique_ptr<ResponseSetOption>> set_option_sync(const RequestSetOption& req) noexcept {
    return static_cast<Derived*>(this)->on_set_option_sync(req);
  }
  result<std::unique_ptr<ResponseDeliverTx>> deliver_tx_sync(const RequestDeliverTx& req) noexcept {
    return static_cast<Derived*>(this)->on_deliver_tx_sync(req);
  }
  result<std::unique_ptr<ResponseCheckTx>> check_tx_sync(const RequestCheckTx& req) noexcept {
    return static_cast<Derived*>(this)->on_check_tx_sync(req);
  }
  result<std::unique_ptr<ResponseQuery>> query_sync(const RequestQuery& req) noexcept {
    return static_cast<Derived*>(this)->on_query_sync(req);
  }
  result<std::unique_ptr<ResponseCommit>> commit_sync(const RequestCommit& req) noexcept {
    return static_cast<Derived*>(this)->on_commit_sync(req);
  }
  result<std::unique_ptr<ResponseInitChain>> init_chain_sync(const RequestInitChain& req) noexcept {
    return static_cast<Derived*>(this)->on_init_chain_sync(req);
  }
  result<std::unique_ptr<ResponseBeginBlock>> begin_block_sync(const RequestBeginBlock& req) noexcept {
    return static_cast<Derived*>(this)->on_begin_block_sync(req);
  }
  result<std::unique_ptr<ResponseEndBlock>> end_block_sync(const RequestEndBlock& req) noexcept {
    return static_cast<Derived*>(this)->on_end_block_sync(req);
  }
  result<std::unique_ptr<ResponseListSnapshots>> list_snapshots_sync(const RequestListSnapshots& req) noexcept {
    return static_cast<Derived*>(this)->on_list_snapshots_sync(req);
  }
  result<std::unique_ptr<ResponseOfferSnapshot>> offer_snapshot_sync(const RequestOfferSnapshot& req) noexcept {
    return static_cast<Derived*>(this)->on_offer_snapshot_sync(req);
  }
  result<std::unique_ptr<ResponseLoadSnapshotChunk>> load_snapshot_chunk_sync(
    const RequestLoadSnapshotChunk& req) noexcept {
    return static_cast<Derived*>(this)->on_load_snapshot_chunk_sync(req);
  }
  result<std::unique_ptr<ResponseApplySnapshotChunk>> apply_snapshot_chunk_sync(
    const RequestApplySnapshotChunk& req) noexcept {
    return static_cast<Derived*>(this)->on_apply_snapshot_chunk_sync(req);
  }

  // Concrete client implementation MUST have following methods, or infinite recursion occurs
  // {
  result<void> on_start() noexcept {
    return static_cast<Derived*>(this)->on_start();
  }
  void on_stop() noexcept {
    return static_cast<Derived*>(this)->on_stop();
  }
  result<void> on_reset() noexcept {
    return static_cast<Derived*>(this)->on_reset();
  }
  // }
};

} // namespace tendermint::abci

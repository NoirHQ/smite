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
  Request* request;
  Response* response; // FIXME: atomic

  std::mutex mtx;
  bool done;
  std::function<void(Response*)> cb;

  void set_callback(std::function<void(Response*)> cb);
  void invoke_callback(Response* r);
  std::function<void(Response*)> get_callback();
  void set_done();
};

template<typename Derived>
class Client : public service::Service<Client<Derived>> {
public:
  Client(appbase::application& app): app(app) {}

  void set_response_callback(Callback cb) noexcept {
    return static_cast<Derived*>(this)->on_set_response_callback(cb);
  }
  result<void> error() noexcept {
    return static_cast<Derived*>(this)->on_error();
  }

  result<ReqRes*> flush_async() noexcept {
    return static_cast<Derived*>(this)->on_flush_async();
  }
  result<ReqRes*> echo_async(const std::string& msg) noexcept {
    return static_cast<Derived*>(this)->on_echo_async(msg);
  }
  result<ReqRes*> info_async(RequestInfo* req) noexcept {
    return static_cast<Derived*>(this)->on_info_async(req);
  }
  result<ReqRes*> set_option_async(RequestSetOption* req) noexcept {
    return static_cast<Derived*>(this)->on_set_option_async(req);
  }
  result<ReqRes*> deliver_tx_async(RequestDeliverTx* req) noexcept {
    return static_cast<Derived*>(this)->on_deliver_tx_async(req);
  }
  result<ReqRes*> check_tx_async(RequestCheckTx* req) noexcept {
    return static_cast<Derived*>(this)->on_check_tx_async(req);
  }
  result<ReqRes*> query_async(RequestQuery* req) noexcept {
    return static_cast<Derived*>(this)->on_query_async(req);
  }
  result<ReqRes*> commit_async(RequestCommit* req) noexcept {
    return static_cast<Derived*>(this)->on_commit_async(req);
  }
  result<ReqRes*> init_chain_async(RequestInitChain* req) noexcept {
    return static_cast<Derived*>(this)->on_init_chain_async(req);
  }
  result<ReqRes*> begin_block_async(RequestBeginBlock* req) noexcept {
    return static_cast<Derived*>(this)->on_begin_block_async(req);
  }
  result<ReqRes*> end_block_async(RequestEndBlock* req) noexcept {
    return static_cast<Derived*>(this)->on_end_block_async(req);
  }
  result<ReqRes*> list_snapshots_async(RequestListSnapshots* req) noexcept {
    return static_cast<Derived*>(this)->on_list_snapshots_sync(req);
  }
  result<ReqRes*> offer_snapshot_async(RequestOfferSnapshot* req) noexcept {
    return static_cast<Derived*>(this)->on_offer_snapshot_async(req);
  }
  result<ReqRes*> load_snapshot_chunk_async(RequestLoadSnapshotChunk* req) noexcept {
    return static_cast<Derived*>(this)->on_load_snapshot_chunk_async(req);
  }
  result<ReqRes*> apply_snapshot_chunk_async(RequestApplySnapshotChunk* req) noexcept {
    return static_cast<Derived*>(this)->on_apply_snapshot_chunk_async(req);
  }

  result<void> flush_sync() noexcept {
    return static_cast<Derived*>(this)->on_flush_sync();
  }
  result<ResponseEcho*> echo_sync(const std::string& msg) noexcept {
    return static_cast<Derived*>(this)->on_echo_sync(msg);
  }
  result<ResponseInfo*> info_sync(RequestInfo* req) noexcept {
    return static_cast<Derived*>(this)->on_info_sync(req);
  }
  result<ResponseSetOption*> set_option_sync(RequestSetOption* req) noexcept {
    return static_cast<Derived*>(this)->on_set_option_sync(req);
  }
  result<ResponseDeliverTx*> deliver_tx_sync(RequestDeliverTx* req) noexcept {
    return static_cast<Derived*>(this)->on_deliver_tx_sync(req);
  }
  result<ResponseCheckTx*> check_tx_sync(RequestCheckTx* req) noexcept {
    return static_cast<Derived*>(this)->on_check_tx_sync(req);
  }
  result<ResponseQuery*> query_sync(RequestQuery* req) noexcept {
    return static_cast<Derived*>(this)->on_query_sync(req);
  }
  result<ResponseCommit*> commit_sync(RequestCommit* req) noexcept {
    return static_cast<Derived*>(this)->on_commit_sync(req);
  }
  result<ResponseInitChain*> init_chain_sync(RequestInitChain* req) noexcept {
    return static_cast<Derived*>(this)->on_init_chain_sync(req);
  }
  result<ResponseBeginBlock*> begin_block_sync(RequestBeginBlock* req) noexcept {
    return static_cast<Derived*>(this)->on_begin_block_sync(req);
  }
  result<ResponseEndBlock*> end_block_sync(RequestEndBlock* req) noexcept {
    return static_cast<Derived*>(this)->on_end_block_sync(req);
  }
  result<ResponseListSnapshots*> list_snapshots_sync(RequestListSnapshots* req) noexcept {
    return static_cast<Derived*>(this)->on_list_snapshots_sync(req);
  }
  result<ResponseOfferSnapshot*> offer_snapshot_sync(RequestOfferSnapshot* req) noexcept {
    return static_cast<Derived*>(this)->on_offer_snapshot_sync(req);
  }
  result<ResponseLoadSnapshotChunk*> load_snapshot_chunk_sync(RequestLoadSnapshotChunk* req) noexcept {
    return static_cast<Derived*>(this)->on_load_snapshot_chunk_sync(req);
  }
  result<ResponseApplySnapshotChunk*> apply_snapshot_chunk_sync(RequestApplySnapshotChunk* req) noexcept {
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

  protected:
    appbase::application& app;
};

} // namespace tendermint::abci

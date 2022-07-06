// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/abci/types.pb.h>
#include <eo/sync.h>
#include <concepts>

namespace noir::abci {

using namespace tendermint::abci;
using namespace eo;

constexpr auto dial_retry_interval_seconds = 3;

using Callback = std::function<void(Request*, Response*)>;

struct ReqRes {
public:
  std::unique_ptr<Request> request;
  eo::sync::WaitGroup wg;
  std::unique_ptr<Response> response; // FIXME: atomic

  mutable std::mutex mtx;

  bool callback_invoked;
  std::function<void(Response*)> cb;

public:
  ReqRes() {
    wg.add(1);
  }

  void set_callback(std::function<void(Response*)> cb);
  void invoke_callback();
  auto get_callback() -> std::function<void(Response*)>&;

  void add(int);
  void done();
  void wait();
};

template<typename T>
concept Client = /* Service<T> && */ requires(T v) {
  v.set_response_callback(Callback{});
  { v.error() } -> std::same_as<Result<void>>;

  { v.echo_async(std::string{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.flush_async() } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.info_async(RequestInfo{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.deliver_tx_async(RequestDeliverTx{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.check_tx_async(RequestCheckTx{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.query_async(RequestQuery{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.commit_async() } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.init_chain_async(RequestInitChain{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.begin_block_async(RequestBeginBlock{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.end_block_async(RequestEndBlock{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.list_snapshots_async(RequestListSnapshots{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.offer_snapshot_async(RequestOfferSnapshot{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.load_snapshot_chunk_async(RequestLoadSnapshotChunk{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;
  { v.apply_snapshot_chunk_async(RequestApplySnapshotChunk{}) } -> std::same_as<Result<std::shared_ptr<ReqRes>>>;

  { v.echo_sync(std::string{}) } -> std::same_as<Result<std::unique_ptr<ResponseEcho>>>;
  { v.flush_sync() } -> std::same_as<Result<void>>;
  { v.info_sync(RequestInfo{}) } -> std::same_as<Result<std::unique_ptr<ResponseInfo>>>;
  { v.deliver_tx_sync(RequestDeliverTx{}) } -> std::same_as<Result<std::unique_ptr<ResponseDeliverTx>>>;
  { v.check_tx_sync(RequestCheckTx{}) } -> std::same_as<Result<std::unique_ptr<ResponseCheckTx>>>;
  { v.query_sync(RequestQuery{}) } -> std::same_as<Result<std::unique_ptr<ResponseQuery>>>;
  { v.commit_sync() } -> std::same_as<Result<std::unique_ptr<ResponseCommit>>>;
  { v.init_chain_sync(RequestInitChain{}) } -> std::same_as<Result<std::unique_ptr<ResponseInitChain>>>;
  { v.begin_block_sync(RequestBeginBlock{}) } -> std::same_as<Result<std::unique_ptr<ResponseBeginBlock>>>;
  { v.end_block_sync(RequestEndBlock{}) } -> std::same_as<Result<std::unique_ptr<ResponseEndBlock>>>;
  { v.list_snapshots_sync(RequestListSnapshots{}) } -> std::same_as<Result<std::unique_ptr<ResponseListSnapshots>>>;
  { v.offer_snapshot_sync(RequestOfferSnapshot{}) } -> std::same_as<Result<std::unique_ptr<ResponseOfferSnapshot>>>;
  {
    v.load_snapshot_chunk_sync(RequestLoadSnapshotChunk{})
    } -> std::same_as<Result<std::unique_ptr<ResponseLoadSnapshotChunk>>>;
  {
    v.apply_snapshot_chunk_sync(RequestApplySnapshotChunk{})
    } -> std::same_as<Result<std::unique_ptr<ResponseApplySnapshotChunk>>>;
};

} // namespace noir::abci

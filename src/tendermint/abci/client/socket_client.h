// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/abci/client/client.h>
#include <tendermint/common/common.h>

namespace tendermint::abci {

class SocketClient : public Client<SocketClient> {
public:
  SocketClient(appbase::application& app);

  void on_set_response_callback(Callback cb) noexcept;
  result<void> on_error() noexcept;

  result<ReqRes*> on_flush_async() noexcept;
  result<ReqRes*> on_echo_async(const std::string& msg) noexcept;
  result<ReqRes*> on_info_async(RequestInfo* req) noexcept;
  result<ReqRes*> on_set_option_async(RequestSetOption* req) noexcept;
  result<ReqRes*> on_deliver_tx_async(RequestDeliverTx* req) noexcept;
  result<ReqRes*> on_check_tx_async(RequestCheckTx* req) noexcept;
  result<ReqRes*> on_query_async(RequestQuery* req) noexcept;
  result<ReqRes*> on_commit_async(RequestCommit* req) noexcept;
  result<ReqRes*> on_init_chain_async(RequestInitChain* req) noexcept;
  result<ReqRes*> on_begin_block_async(RequestBeginBlock* req) noexcept;
  result<ReqRes*> on_end_block_async(RequestEndBlock* req) noexcept;
  result<ReqRes*> on_list_snapshots_async(RequestListSnapshots* req) noexcept;
  result<ReqRes*> on_offer_snapshot_async(RequestOfferSnapshot* req) noexcept;
  result<ReqRes*> on_load_snapshot_chunk_async(RequestLoadSnapshotChunk* req) noexcept;
  result<ReqRes*> on_apply_snapshot_chunk_async(RequestApplySnapshotChunk* req) noexcept;

  result<void> on_flush_sync() noexcept;
  result<ResponseEcho*> on_echo_sync(const std::string& msg) noexcept;
  result<ResponseInfo*> on_info_sync(RequestInfo* req) noexcept;
  result<ResponseSetOption*> on_set_option_sync(RequestSetOption* req) noexcept;
  result<ResponseDeliverTx*> on_deliver_tx_sync(RequestDeliverTx* req) noexcept;
  result<ResponseCheckTx*> on_check_tx_sync(RequestCheckTx* req) noexcept;
  result<ResponseQuery*> on_query_sync(RequestQuery* req) noexcept;
  result<ResponseCommit*> on_commit_sync(RequestCommit* req) noexcept;
  result<ResponseInitChain*> on_init_chain_sync(RequestInitChain* req) noexcept;
  result<ResponseBeginBlock*> on_begin_block_sync(RequestBeginBlock* req) noexcept;
  result<ResponseEndBlock*> on_end_block_sync(RequestEndBlock* req) noexcept;
  result<ResponseListSnapshots*> on_list_snapshots_sync(RequestListSnapshots* req) noexcept;
  result<ResponseOfferSnapshot*> on_offer_snapshot_sync(RequestOfferSnapshot* req) noexcept;
  result<ResponseLoadSnapshotChunk*> on_load_snapshot_chunk_sync(RequestLoadSnapshotChunk* req) noexcept;
  result<ResponseApplySnapshotChunk*> on_apply_snapshot_chunk_sync(RequestApplySnapshotChunk* req) noexcept;

  result<void> on_start() noexcept;
  void on_stop() noexcept;
  result<void> on_reset() noexcept;
};

} // namespace tendermint::abci

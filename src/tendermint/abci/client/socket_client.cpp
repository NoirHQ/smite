// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/abci/client/socket_client.h>

namespace tendermint::abci {

SocketClient::SocketClient(appbase::application& app): Client(app) {}

void SocketClient::on_set_response_callback(Callback cb) noexcept {}

result<void> SocketClient::on_error() noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_flush_async() noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_echo_async(const std::string& msg) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_info_async(RequestInfo* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_set_option_async(RequestSetOption* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_deliver_tx_async(RequestDeliverTx* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_check_tx_async(RequestCheckTx* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_query_async(RequestQuery* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_commit_async(RequestCommit* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_init_chain_async(RequestInitChain* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_begin_block_async(RequestBeginBlock* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_end_block_async(RequestEndBlock* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_list_snapshots_async(RequestListSnapshots* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_offer_snapshot_async(RequestOfferSnapshot* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_load_snapshot_chunk_async(RequestLoadSnapshotChunk* req) noexcept {
  return {};
}

result<ReqRes*> SocketClient::on_apply_snapshot_chunk_async(RequestApplySnapshotChunk* req) noexcept {
  return {};
}

result<void> SocketClient::on_flush_sync() noexcept {
  return {};
}

result<ResponseEcho*> SocketClient::on_echo_sync(const std::string& msg) noexcept {
  return {};
}

result<ResponseInfo*> SocketClient::on_info_sync(RequestInfo* req) noexcept {
  return {};
}

result<ResponseSetOption*> SocketClient::on_set_option_sync(RequestSetOption* req) noexcept {
  return {};
}

result<ResponseDeliverTx*> SocketClient::on_deliver_tx_sync(RequestDeliverTx* req) noexcept {
  return {};
}

result<ResponseCheckTx*> SocketClient::on_check_tx_sync(RequestCheckTx* req) noexcept {
  return {};
}

result<ResponseQuery*> SocketClient::on_query_sync(RequestQuery* req) noexcept {
  return {};
}

result<ResponseCommit*> SocketClient::on_commit_sync(RequestCommit* req) noexcept {
  return {};
}

result<ResponseInitChain*> SocketClient::on_init_chain_sync(RequestInitChain* req) noexcept {
  return {};
}

result<ResponseBeginBlock*> SocketClient::on_begin_block_sync(RequestBeginBlock* req) noexcept {
  return {};
}

result<ResponseEndBlock*> SocketClient::on_end_block_sync(RequestEndBlock* req) noexcept {
  return {};
}

result<ResponseListSnapshots*> SocketClient::on_list_snapshots_sync(RequestListSnapshots* req) noexcept {
  return {};
}

result<ResponseOfferSnapshot*> SocketClient::on_offer_snapshot_sync(RequestOfferSnapshot* req) noexcept {
  return {};
}

result<ResponseLoadSnapshotChunk*> SocketClient::on_load_snapshot_chunk_sync(RequestLoadSnapshotChunk* req) noexcept {
  return {};
}

result<ResponseApplySnapshotChunk*> SocketClient::on_apply_snapshot_chunk_sync(
  RequestApplySnapshotChunk* req) noexcept {
  return {};
}

} // namespace tendermint::abci

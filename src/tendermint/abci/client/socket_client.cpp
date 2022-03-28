// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/abci/client/socket_client.h>
#include <tendermint/abci/types/messages.h>

namespace tendermint::abci {

SocketClient::SocketClient(appbase::application& app, const std::string& addr, bool must_connect): Client(app), addr(addr), must_connect(must_connect), thread_pool("socketClient", 4 /* make configurable */) {
  name = "socketClient";
}

void SocketClient::on_set_response_callback(Callback cb) noexcept {
  std::scoped_lock _(mtx);
  res_cb = cb;
}

result<void> SocketClient::on_error() noexcept {
  std::scoped_lock _(mtx);
  return make_unexpected(err);
}

result<std::unique_ptr<ReqRes>> SocketClient::on_echo_async(const std::string& msg) noexcept {
  return queue_request(to_request_echo(msg));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_flush_async() noexcept {
  return queue_request(to_request_flush());
}

result<std::unique_ptr<ReqRes>> SocketClient::on_info_async(const RequestInfo& req) noexcept {
  return queue_request(to_request_info(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_set_option_async(const RequestSetOption& req) noexcept {
  return queue_request(to_request_set_option(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_deliver_tx_async(const RequestDeliverTx& req) noexcept {
  return queue_request(to_request_deliver_tx(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_check_tx_async(const RequestCheckTx& req) noexcept {
  return queue_request(to_request_check_tx(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_query_async(const RequestQuery& req) noexcept {
  return queue_request(to_request_query(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_commit_async(const RequestCommit& req) noexcept {
  return queue_request(to_request_commit(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_init_chain_async(const RequestInitChain& req) noexcept {
  return queue_request(to_request_init_chain(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_begin_block_async(const RequestBeginBlock& req) noexcept {
  return queue_request(to_request_begin_block(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_end_block_async(const RequestEndBlock& req) noexcept {
  return queue_request(to_request_end_block(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_list_snapshots_async(const RequestListSnapshots& req) noexcept {
  return queue_request(to_request_list_snapshots(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_offer_snapshot_async(const RequestOfferSnapshot& req) noexcept {
  return queue_request(to_request_offer_snapshot(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_load_snapshot_chunk_async(const RequestLoadSnapshotChunk& req) noexcept {
  return queue_request(to_request_load_snapshot_chunk(req));
}

result<std::unique_ptr<ReqRes>> SocketClient::on_apply_snapshot_chunk_async(const RequestApplySnapshotChunk& req) noexcept {
  return queue_request(to_request_apply_snapshot_chunk(req));
}

result<std::unique_ptr<ResponseEcho>> SocketClient::on_echo_sync(const std::string& msg) noexcept {
  auto reqres = queue_request(to_request_echo(msg));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseEcho>(reqres->response->release_echo());
}

result<void> SocketClient::on_flush_sync() noexcept {
  auto reqres = queue_request(to_request_flush());
  if (auto err = error(); !err) {
    return err;
  }
  reqres->wait();
  return {};
}

result<std::unique_ptr<ResponseInfo>> SocketClient::on_info_sync(const RequestInfo& req) noexcept {
  auto reqres = queue_request(to_request_info(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseInfo>(reqres->response->release_info());
}

result<std::unique_ptr<ResponseSetOption>> SocketClient::on_set_option_sync(const RequestSetOption& req) noexcept {
  return {};
  auto reqres = queue_request(to_request_set_option(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseSetOption>(reqres->response->release_set_option());

}

result<std::unique_ptr<ResponseDeliverTx>> SocketClient::on_deliver_tx_sync(const RequestDeliverTx& req) noexcept {
  auto reqres = queue_request(to_request_deliver_tx(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseDeliverTx>(reqres->response->release_deliver_tx());
}

result<std::unique_ptr<ResponseCheckTx>> SocketClient::on_check_tx_sync(const RequestCheckTx& req) noexcept {
  auto reqres = queue_request(to_request_check_tx(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseCheckTx>(reqres->response->release_check_tx());
}

result<std::unique_ptr<ResponseQuery>> SocketClient::on_query_sync(const RequestQuery& req) noexcept {
  auto reqres = queue_request(to_request_query(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseQuery>(reqres->response->release_query());
}

result<std::unique_ptr<ResponseCommit>> SocketClient::on_commit_sync(const RequestCommit& req) noexcept {
  auto reqres = queue_request(to_request_commit(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseCommit>(reqres->response->release_commit());
}

result<std::unique_ptr<ResponseInitChain>> SocketClient::on_init_chain_sync(const RequestInitChain& req) noexcept {
  auto reqres = queue_request(to_request_init_chain(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseInitChain>(reqres->response->release_init_chain());

}

result<std::unique_ptr<ResponseBeginBlock>> SocketClient::on_begin_block_sync(const RequestBeginBlock& req) noexcept {
  auto reqres = queue_request(to_request_begin_block(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseBeginBlock>(reqres->response->release_begin_block());

}

result<std::unique_ptr<ResponseEndBlock>> SocketClient::on_end_block_sync(const RequestEndBlock& req) noexcept {
  auto reqres = queue_request(to_request_end_block(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseEndBlock>(reqres->response->release_end_block());
}

result<std::unique_ptr<ResponseListSnapshots>> SocketClient::on_list_snapshots_sync(const RequestListSnapshots& req) noexcept {
  auto reqres = queue_request(to_request_list_snapshots(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseListSnapshots>(reqres->response->release_list_snapshots());
}

result<std::unique_ptr<ResponseOfferSnapshot>> SocketClient::on_offer_snapshot_sync(const RequestOfferSnapshot& req) noexcept {
  auto reqres = queue_request(to_request_offer_snapshot(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseOfferSnapshot>(reqres->response->release_offer_snapshot());
}

result<std::unique_ptr<ResponseLoadSnapshotChunk>> SocketClient::on_load_snapshot_chunk_sync(const RequestLoadSnapshotChunk& req) noexcept {
  auto reqres = queue_request(to_request_load_snapshot_chunk(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseLoadSnapshotChunk>(reqres->response->release_load_snapshot_chunk());
}

result<std::unique_ptr<ResponseApplySnapshotChunk>> SocketClient::on_apply_snapshot_chunk_sync(const RequestApplySnapshotChunk& req) noexcept {
  auto reqres = queue_request(to_request_apply_snapshot_chunk(req));
  reqres->wait();
  if (auto err = error(); !err) {
    return make_unexpected(err.error());
  }
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseApplySnapshotChunk>(reqres->response->release_apply_snapshot_chunk());
}

std::unique_ptr<ReqRes> SocketClient::queue_request(std::unique_ptr<Request> req) {
  auto reqres = std::make_unique<ReqRes>();
  reqres->request = std::move(req);
  reqres->future = async_thread_pool(thread_pool.get_executor(), [&]() {
  });
  if (req->has_flush()) {
    // flush_timer.unset();
  } else {
    // flush_timer.set();
  }
  return reqres;
}

} // namespace tendermint::abci

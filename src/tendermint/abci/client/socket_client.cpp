// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/abci/client/socket_client.h>
#include <tendermint/abci/types/messages.h>
#include <tendermint/common/thread_utils.h>

namespace tendermint::abci {

SocketClient::SocketClient(const std::string& addr, bool must_connect, boost::asio::io_context& io_context)
  : addr(addr), must_connect(must_connect), strand(io_context) {
  name = "SocketClient";
}

result<void> SocketClient::on_start() noexcept {
  return {};
}

void SocketClient::on_stop() noexcept {}

result<void> SocketClient::on_reset() noexcept {
  return make_unexpected("not implemented");
}

void SocketClient::on_set_response_callback(Callback cb) noexcept {
  std::scoped_lock _(mtx);
  res_cb = cb;
}

result<void> SocketClient::on_error() noexcept {
  std::scoped_lock _(mtx);
  if (err) {
    return make_unexpected(*err);
  }
  return {};
}

result<std::shared_ptr<ReqRes>> SocketClient::on_echo_async(const std::string& msg) noexcept {
  return queue_request(to_request_echo(msg));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_flush_async() noexcept {
  return queue_request(to_request_flush());
}

result<std::shared_ptr<ReqRes>> SocketClient::on_info_async(const RequestInfo& req) noexcept {
  return queue_request(to_request_info(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_set_option_async(const RequestSetOption& req) noexcept {
  return queue_request(to_request_set_option(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_deliver_tx_async(const RequestDeliverTx& req) noexcept {
  return queue_request(to_request_deliver_tx(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_check_tx_async(const RequestCheckTx& req) noexcept {
  return queue_request(to_request_check_tx(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_query_async(const RequestQuery& req) noexcept {
  return queue_request(to_request_query(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_commit_async(const RequestCommit& req) noexcept {
  return queue_request(to_request_commit(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_init_chain_async(const RequestInitChain& req) noexcept {
  return queue_request(to_request_init_chain(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_begin_block_async(const RequestBeginBlock& req) noexcept {
  return queue_request(to_request_begin_block(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_end_block_async(const RequestEndBlock& req) noexcept {
  return queue_request(to_request_end_block(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_list_snapshots_async(const RequestListSnapshots& req) noexcept {
  return queue_request(to_request_list_snapshots(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_offer_snapshot_async(const RequestOfferSnapshot& req) noexcept {
  return queue_request(to_request_offer_snapshot(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_load_snapshot_chunk_async(
  const RequestLoadSnapshotChunk& req) noexcept {
  return queue_request(to_request_load_snapshot_chunk(req));
}

result<std::shared_ptr<ReqRes>> SocketClient::on_apply_snapshot_chunk_async(
  const RequestApplySnapshotChunk& req) noexcept {
  return queue_request(to_request_apply_snapshot_chunk(req));
}

result<std::unique_ptr<ResponseEcho>> SocketClient::on_echo_sync(const std::string& msg) noexcept {
  auto reqres = queue_request(to_request_echo(msg));
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
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseInfo>(reqres->response->release_info());
}

result<std::unique_ptr<ResponseSetOption>> SocketClient::on_set_option_sync(const RequestSetOption& req) noexcept {
  auto reqres = queue_request(to_request_set_option(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseSetOption>(reqres->response->release_set_option());
}

result<std::unique_ptr<ResponseDeliverTx>> SocketClient::on_deliver_tx_sync(const RequestDeliverTx& req) noexcept {
  auto reqres = queue_request(to_request_deliver_tx(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseDeliverTx>(reqres->response->release_deliver_tx());
}

result<std::unique_ptr<ResponseCheckTx>> SocketClient::on_check_tx_sync(const RequestCheckTx& req) noexcept {
  auto reqres = queue_request(to_request_check_tx(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseCheckTx>(reqres->response->release_check_tx());
}

result<std::unique_ptr<ResponseQuery>> SocketClient::on_query_sync(const RequestQuery& req) noexcept {
  auto reqres = queue_request(to_request_query(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseQuery>(reqres->response->release_query());
}

result<std::unique_ptr<ResponseCommit>> SocketClient::on_commit_sync(const RequestCommit& req) noexcept {
  auto reqres = queue_request(to_request_commit(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseCommit>(reqres->response->release_commit());
}

result<std::unique_ptr<ResponseInitChain>> SocketClient::on_init_chain_sync(const RequestInitChain& req) noexcept {
  auto reqres = queue_request(to_request_init_chain(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseInitChain>(reqres->response->release_init_chain());
}

result<std::unique_ptr<ResponseBeginBlock>> SocketClient::on_begin_block_sync(const RequestBeginBlock& req) noexcept {
  auto reqres = queue_request(to_request_begin_block(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseBeginBlock>(reqres->response->release_begin_block());
}

result<std::unique_ptr<ResponseEndBlock>> SocketClient::on_end_block_sync(const RequestEndBlock& req) noexcept {
  auto reqres = queue_request(to_request_end_block(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseEndBlock>(reqres->response->release_end_block());
}

result<std::unique_ptr<ResponseListSnapshots>> SocketClient::on_list_snapshots_sync(
  const RequestListSnapshots& req) noexcept {
  auto reqres = queue_request(to_request_list_snapshots(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseListSnapshots>(reqres->response->release_list_snapshots());
}

result<std::unique_ptr<ResponseOfferSnapshot>> SocketClient::on_offer_snapshot_sync(
  const RequestOfferSnapshot& req) noexcept {
  auto reqres = queue_request(to_request_offer_snapshot(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseOfferSnapshot>(reqres->response->release_offer_snapshot());
}

result<std::unique_ptr<ResponseLoadSnapshotChunk>> SocketClient::on_load_snapshot_chunk_sync(
  const RequestLoadSnapshotChunk& req) noexcept {
  auto reqres = queue_request(to_request_load_snapshot_chunk(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseLoadSnapshotChunk>(reqres->response->release_load_snapshot_chunk());
}

result<std::unique_ptr<ResponseApplySnapshotChunk>> SocketClient::on_apply_snapshot_chunk_sync(
  const RequestApplySnapshotChunk& req) noexcept {
  auto reqres = queue_request(to_request_apply_snapshot_chunk(req));
  if (auto err = flush_sync(); !err) {
    return make_unexpected(err.error());
  }
  return std::unique_ptr<ResponseApplySnapshotChunk>(reqres->response->release_apply_snapshot_chunk());
}

std::shared_ptr<ReqRes> SocketClient::queue_request(std::unique_ptr<Request> req) {
  auto reqres = std::make_unique<ReqRes>();
  reqres->request = std::move(req);
  reqres->future = async_strand(strand, [&]() {});
  if (req->has_flush()) {
    // flush_timer.unset();
  } else {
    // flush_timer.set();
  }
  return reqres;
}

void SocketClient::flush_queue() {
  std::scoped_lock _(mtx);

  // TODO: mark all in-flight messages as resolved (they will get error())
  // TODO: mark all queued messages as resolved
}

void SocketClient::will_send_req(std::shared_ptr<ReqRes> reqres) {
  std::scoped_lock _(mtx);
  req_sent.push_back(reqres);
}

bool res_matches_req(Request* req, Response* res) {
  auto reqv = req->value_case();
  auto resv = res->value_case();
  if (!reqv || !resv) {
    return false;
  }
  return reqv + 1 == resv;
}

result<void> SocketClient::did_recv_response(std::unique_ptr<Response> res) {
  std::scoped_lock _(mtx);

  if (!req_sent.empty()) {
    return errorf("unexpected {} when nothing expected", res->GetTypeName());
  }

  auto reqres = req_sent.front();
  if (!res_matches_req(reqres->request.get(), res.get())) {
    return errorf("unexpected {} when response to {} expected", res->GetTypeName(), reqres->request->GetTypeName());
  }

  reqres->response = std::move(res);
  // FIXME: need to end async task
  reqres->set_done();
  req_sent.pop_front();

  if (res_cb) {
    res_cb(reqres->request.get(), res.get());
  }

  reqres->invoke_callback();
  return {};
}

void SocketClient::stop_for_error(const tendermint::error& err) {
  if (!is_running()) {
    return;
  }
  {
    std::scoped_lock _(mtx);
    if (!this->err) {
      this->err = err;
    }
  }
  tm_elog(logger.get(), "Stopping SocketClient for error: {}", err);
  if (auto err = stop(); !err) {
    tm_elog(logger.get(), "Error stopping SocketClient: {}", err.error());
  }
}

} // namespace tendermint::abci

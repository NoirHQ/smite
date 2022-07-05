// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/net/conn.h>
#include <tendermint/abci/client/client.h>
#include <tendermint/abci/types/messages.h>
#include <tendermint/service/service.h>
#include <eo/sync.h>
#include <eo/time.h>

namespace noir::abci {

using namespace eo;

static constexpr auto req_queue_size = 256;

template<typename Conn>
class SocketClient : public service::BaseService<SocketClient<Conn>> {
private:
  using service_type = service::BaseService<SocketClient<Conn>>;

  std::string addr;
  bool must_connect;
  std::shared_ptr<Conn> conn;

  chan<std::shared_ptr<ReqRes>> req_queue = make_chan<std::shared_ptr<ReqRes>>(req_queue_size);

  std::mutex mtx;
  Result<void> err{std::in_place_type<void>};
  std::deque<std::shared_ptr<ReqRes>> req_sent;
  std::function<void(Request*, Response*)> res_cb;

  boost::asio::strand<boost::asio::any_io_executor> strand;

public:
  /// \breif creates a new socket client
  ///
  /// SocketClient creates a new socket client, which connects to a given
  /// address. If must_connect is true, the client will throw an exception  upon start
  /// if it fails to connect.
  template<typename Executor>
  SocketClient(Executor&& ex, std::string_view address, bool must_connect)
    : addr(address), must_connect(must_connect), conn(Conn::create(ex, address)), strand(ex) {
    service_type::name = "SocketClient";
  }

  SocketClient(std::string_view address, bool must_connect)
    : SocketClient(runtime::execution_context.get_executor(), address, must_connect) {}

  Result<void> on_start() {
    return invoke([&]() -> func<Result<void>> {
      for (;;) {
        if (auto ok = co_await conn->connect(); !ok) {
          if (must_connect) {
            co_return ok.error();
          }
          noir_elog(service_type::logger.get(),
            fmt::format("abci::SocketClient failed to connect to {}.  Retrying after {}s...", addr,
              dial_retry_interval_seconds) /* , "err", ok.error() */);
          co_await time::sleep(std::chrono::seconds(dial_retry_interval_seconds));
          continue;
        }

        go(send_requests_routine());
        go(recv_response_routine());

        co_return success();
      }
    });
  }

  void on_stop() {
    if (conn->is_open()) {
      conn->close();
    }
    drain_queue();
  }

  Result<void> on_reset() {
    return Error("not implemented");
  }

  Result<void> error() {
    return err;
  }

  void set_response_callback(Callback cb) {
    std::unique_lock _{mtx};
    res_cb = cb;
  }

  func<Result<std::shared_ptr<ReqRes>>> echo_async(const std::string& msg) {
    co_return queue_request_async(to_request_echo(msg));
  }

  func<Result<std::shared_ptr<ReqRes>>> flush_async() {
    co_return queue_request_async(to_request_flush());
  }

  func<Result<std::shared_ptr<ReqRes>>> info_async(const RequestInfo& req) {
    co_return queue_request_async(to_request_info(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> deliver_tx_async(const RequestDeliverTx& req) {
    co_return queue_request_async(to_request_deliver_tx(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> check_tx_async(const RequestCheckTx& req) {
    co_return queue_request_async(to_request_check_tx(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> query_async(const RequestQuery& req) {
    co_return queue_request_async(to_request_query(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> commit_async() {
    co_return queue_request_async(to_request_commit());
  }

  func<Result<std::shared_ptr<ReqRes>>> init_chain_async(const RequestInitChain& req) {
    co_return queue_request_async(to_request_init_chain(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> begin_block_async(const RequestBeginBlock& req) {
    co_return queue_request_async(to_request_begin_block(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> end_block_async(const RequestEndBlock& req) {
    co_return queue_request_async(to_request_end_block(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> list_snapshots_async(const RequestListSnapshots& req) {
    return queue_request_async(to_request_list_snapshots(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> offer_snapshot_async(const RequestOfferSnapshot& req) {
    co_return queue_request_async(to_request_offer_snapshot(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> load_snapshot_chunk_async(const RequestLoadSnapshotChunk& req) {
    co_return queue_request_async(to_request_load_snapshot_chunk(req));
  }

  func<Result<std::shared_ptr<ReqRes>>> apply_snapshot_chunk_async(const RequestApplySnapshotChunk& req) {
    co_return queue_request_async(to_request_apply_snapshot_chunk(req));
  }

  Result<std::unique_ptr<ResponseEcho>> echo_sync(const std::string& msg) {
    auto ok = queue_request_and_flush_sync(to_request_echo(msg));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseEcho>(ok.value()->response->release_echo());
  }

  Result<void> flush_sync() {
    auto ok = queue_request(to_request_flush());
    if (!ok) {
      return ok.error();
    }
    if (auto ok = error(); !ok) {
      return ok.error();
    }
    auto& reqres = ok.value();

    auto got_resp = make_chan();
    go([&]() {
      reqres->wait();
      got_resp.close();
    });

    return invoke([&]() -> func<Result<void>> {
      auto select = Select{*got_resp};
      switch (co_await select.index()) {
      case 0:
        co_await select.process<0>();
        co_return error();
        /*
        case 1:
        */
      }
      co_return success();
    });
  }

  Result<std::unique_ptr<ResponseInfo>> info_sync(const RequestInfo& req) {
    auto ok = queue_request_and_flush_sync(to_request_info(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseInfo>(ok.value()->response->release_info());
  }

  Result<std::unique_ptr<ResponseDeliverTx>> deliver_tx_sync(const RequestDeliverTx& req) {
    auto ok = queue_request_and_flush_sync(to_request_deliver_tx(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseDeliverTx>(ok.value()->response->release_deliver_tx());
  }

  Result<std::unique_ptr<ResponseCheckTx>> check_tx_sync(const RequestCheckTx& req) {
    auto ok = queue_request_and_flush_sync(to_request_check_tx(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseCheckTx>(ok.value()->response->release_check_tx());
  }

  Result<std::unique_ptr<ResponseQuery>> query_sync(const RequestQuery& req) {
    auto ok = queue_request_and_flush_sync(to_request_query(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseQuery>(ok.value()->response->release_query());
  }

  Result<std::unique_ptr<ResponseCommit>> commit_sync() {
    auto ok = queue_request_and_flush_sync(to_request_commit());
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseCommit>(ok.value()->response->release_commit());
  }

  Result<std::unique_ptr<ResponseInitChain>> init_chain_sync(const RequestInitChain& req) {
    auto ok = queue_request_and_flush_sync(to_request_init_chain(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseInitChain>(ok.value()->response->release_init_chain());
  }

  Result<std::unique_ptr<ResponseBeginBlock>> begin_block_sync(const RequestBeginBlock& req) {
    auto ok = queue_request_and_flush_sync(to_request_begin_block(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseBeginBlock>(ok.value()->response->release_begin_block());
  }

  Result<std::unique_ptr<ResponseEndBlock>> end_block_sync(const RequestEndBlock& req) {
    auto ok = queue_request_and_flush_sync(to_request_end_block(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseEndBlock>(ok.value()->response->release_end_block());
  }

  Result<std::unique_ptr<ResponseListSnapshots>> list_snapshots_sync(const RequestListSnapshots& req) {
    auto ok = queue_request_and_flush_sync(to_request_list_snapshots(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseListSnapshots>(ok.value()->response->release_list_snapshots());
  }

  Result<std::unique_ptr<ResponseOfferSnapshot>> offer_snapshot_sync(const RequestOfferSnapshot& req) {
    auto ok = queue_request_and_flush_sync(to_request_offer_snapshot(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseOfferSnapshot>(ok.value()->response->release_offer_snapshot());
  }

  Result<std::unique_ptr<ResponseLoadSnapshotChunk>> load_snapshot_chunk_sync(const RequestLoadSnapshotChunk& req) {
    auto ok = queue_request_and_flush_sync(to_request_load_snapshot_chunk(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseLoadSnapshotChunk>(ok.value()->response->release_load_snapshot_chunk());
  }

  Result<std::unique_ptr<ResponseApplySnapshotChunk>> apply_snapshot_chunk_sync(const RequestApplySnapshotChunk& req) {
    auto ok = queue_request_and_flush_sync(to_request_apply_snapshot_chunk(req));
    if (!ok) {
      return ok.error();
    }
    return std::unique_ptr<ResponseApplySnapshotChunk>(ok.value()->response->release_apply_snapshot_chunk());
  }

private:
  Result<std::shared_ptr<ReqRes>> queue_request(std::unique_ptr<Request> req) {
    auto reqres = std::make_shared<ReqRes>();
    std::swap(reqres->request, req);

    invoke([&]() -> func<> {
      auto select = Select{(req_queue << reqres)};
      switch (co_await select.index()) {
      case 0:
        co_await select.template process<0>();
      case 1:
        co_return;
      }
    });

    return reqres;
  }

  Result<std::shared_ptr<ReqRes>> queue_request_async(std::unique_ptr<Request> req) {
    auto ok = queue_request(std::move(req));
    if (!ok) {
      return ok.error();
    }
    if (auto ok = error(); !ok) {
      return ok.error();
    }
    return std::forward<std::shared_ptr<ReqRes>>(ok.value());
  }

  Result<std::shared_ptr<ReqRes>> queue_request_and_flush_sync(std::unique_ptr<Request> req) {
    auto reqres = queue_request(std::move(req));
    if (!reqres) {
      return reqres.error();
    }
    if (auto ok = flush_sync(); !ok) {
      return ok.error();
    }
    if (auto ok = error(); !ok) {
      return ok.error();
    }
    return reqres;
  }

  void drain_queue() {
    std::scoped_lock _{mtx};

    for (auto& reqres : req_sent) {
      reqres->done();
    }

    invoke([&]() -> func<> {
      auto select = Select{*req_queue, CaseDefault()};
      for (;;) {
        switch (co_await select.index()) {
        case 0: {
          auto reqres = co_await select.template process<0>();
          reqres->done();
          break;
        }
        default:
          co_return;
        }
      }
    });
  }

  void will_send_req(const std::shared_ptr<ReqRes>& reqres) {
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

  Result<void> did_recv_response(std::unique_ptr<Response> res) {
    std::scoped_lock _(mtx);

    if (req_sent.empty()) {
      return Error::format("unexpected {} when nothing expected", res->value_case());
    }

    auto reqres = req_sent.front();
    if (!res_matches_req(reqres->request.get(), res.get())) {
      return Error::format(
        "unexpected {} when response to {} expected", res->value_case(), reqres->request->value_case() + 1);
    }

    reqres->response = std::move(res);
    // FIXME: need to end async task
    reqres->done();
    req_sent.pop_front();

    if (res_cb) {
      res_cb(reqres->request.get(), reqres->response.get());
    }

    reqres->invoke_callback();
    return success();
  }

  void stop_for_error(const Error& err) {
    if (!service_type::is_running()) {
      return;
    }
    {
      std::scoped_lock _(mtx);
      if (!this->err) {
        this->err = err;
      }
    }
    noir_ilog(service_type::logger.get(), "Stopping SocketClient for error: {}", err);
    if (auto ok = service_type::stop(); !ok) {
      noir_elog(service_type::logger.get(), "Error stopping SocketClient: {}", ok.error());
    }
  }

  func<> send_requests_routine() {
    auto select = Select{*req_queue, *service_type::quit()};
    for (;;) {
      switch (co_await select.index()) {
      case 0: {
        auto reqres = co_await select.template process<0>();
        // TODO: reqres context is done
        will_send_req(reqres);

        if (auto ok = co_await write_message(*reqres->request, *conn); !ok) {
          stop_for_error(ok.error());
          co_return;
        }
        // conn.flush(); ?
        break;
      }
      case 1:
        co_await select.template process<1>();
        co_return;
      }
    }
  }

  func<> recv_response_routine() {
    for (;;) {
      auto res = std::make_unique<Response>();
      if (auto ok = co_await read_message(*conn, *res); !ok) {
        stop_for_error(ok.error());
        co_return;
      }

      switch (res->value_case()) {
      case Response::ValueCase::kException:
        stop_for_error(Error(res->exception().error()));
        co_return;
      default: {
        auto ok = did_recv_response(std::move(res));
        if (!ok) {
          stop_for_error(ok.error());
          co_return;
        }
      }
      }
    }
  }
};

} // namespace noir::abci

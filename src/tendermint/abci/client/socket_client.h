// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/common/types/varint.h>
#include <tendermint/abci/client/client.h>
#include <tendermint/abci/types/messages.h>
#include <tendermint/common/common.h>
#include <tendermint/net/conn.h>
#include <tendermint/net/details/log.h>
#include <boost/asio/steady_timer.hpp>

namespace tendermint::abci {

static constexpr auto req_queue_size = 256;
static constexpr auto flush_throttle_ms = 20;

template<typename Conn>
class SocketClient : public Client<SocketClient<Conn>> {
private:
  using client_type = Client<SocketClient<Conn>>;
  using service_type = service::Service<client_type>;

public:
  /// \breif creates a new socket client
  ///
  /// SocketClient creates a new socket client, which connects to a given
  /// address. If must_connect is true, the client will throw an exception  upon start
  /// if it fails to connect.
  SocketClient(const std::string& address, bool must_connect, boost::asio::io_context& io_context)
    : addr(address),
      must_connect(must_connect),
      conn(new Conn(address, io_context)),
      strand(io_context),
      flush_timer(io_context) {
    // set service name
    service_type::name = "SocketClient";
  }

  void on_set_response_callback(Callback cb) noexcept {
    std::scoped_lock _(mtx);
    res_cb = cb;
  }
  result<void> on_error() noexcept {
    std::scoped_lock _(mtx);
    if (err) {
      return make_unexpected(*err);
    }
    return {};
  }

  result<std::shared_ptr<ReqRes>> on_echo_async(const std::string& msg) noexcept {
    return queue_request(to_request_echo(msg));
  }

  result<std::shared_ptr<ReqRes>> on_flush_async() noexcept {
    return queue_request(to_request_flush());
  }

  result<std::shared_ptr<ReqRes>> on_info_async(const RequestInfo& req) noexcept {
    return queue_request(to_request_info(req));
  }

  result<std::shared_ptr<ReqRes>> on_set_option_async(const RequestSetOption& req) noexcept {
    return queue_request(to_request_set_option(req));
  }

  result<std::shared_ptr<ReqRes>> on_deliver_tx_async(const RequestDeliverTx& req) noexcept {
    return queue_request(to_request_deliver_tx(req));
  }

  result<std::shared_ptr<ReqRes>> on_check_tx_async(const RequestCheckTx& req) noexcept {
    return queue_request(to_request_check_tx(req));
  }

  result<std::shared_ptr<ReqRes>> on_query_async(const RequestQuery& req) noexcept {
    return queue_request(to_request_query(req));
  }

  result<std::shared_ptr<ReqRes>> on_commit_async(const RequestCommit& req) noexcept {
    return queue_request(to_request_commit(req));
  }

  result<std::shared_ptr<ReqRes>> on_init_chain_async(const RequestInitChain& req) noexcept {
    return queue_request(to_request_init_chain(req));
  }

  result<std::shared_ptr<ReqRes>> on_begin_block_async(const RequestBeginBlock& req) noexcept {
    return queue_request(to_request_begin_block(req));
  }

  result<std::shared_ptr<ReqRes>> on_end_block_async(const RequestEndBlock& req) noexcept {
    return queue_request(to_request_end_block(req));
  }

  result<std::shared_ptr<ReqRes>> on_list_snapshots_async(const RequestListSnapshots& req) noexcept {
    return queue_request(to_request_list_snapshots(req));
  }

  result<std::shared_ptr<ReqRes>> on_offer_snapshot_async(const RequestOfferSnapshot& req) noexcept {
    return queue_request(to_request_offer_snapshot(req));
  }

  result<std::shared_ptr<ReqRes>> on_load_snapshot_chunk_async(const RequestLoadSnapshotChunk& req) noexcept {
    return queue_request(to_request_load_snapshot_chunk(req));
  }

  result<std::shared_ptr<ReqRes>> on_apply_snapshot_chunk_async(const RequestApplySnapshotChunk& req) noexcept {
    return queue_request(to_request_apply_snapshot_chunk(req));
  }

  result<std::unique_ptr<ResponseEcho>> on_echo_sync(const std::string& msg) noexcept {
    auto reqres = queue_request(to_request_echo(msg));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseEcho>(reqres->response->release_echo());
  }

  result<void> on_flush_sync() noexcept {
    auto reqres = queue_request(to_request_flush());
    if (auto err = client_type::error(); !err) {
      return err;
    }
    reqres->wait();
    return {};
  }

  result<std::unique_ptr<ResponseInfo>> on_info_sync(const RequestInfo& req) noexcept {
    auto reqres = queue_request(to_request_info(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseInfo>(reqres->response->release_info());
  }

  result<std::unique_ptr<ResponseSetOption>> on_set_option_sync(const RequestSetOption& req) noexcept {
    auto reqres = queue_request(to_request_set_option(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseSetOption>(reqres->response->release_set_option());
  }

  result<std::unique_ptr<ResponseDeliverTx>> on_deliver_tx_sync(const RequestDeliverTx& req) noexcept {
    auto reqres = queue_request(to_request_deliver_tx(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseDeliverTx>(reqres->response->release_deliver_tx());
  }

  result<std::unique_ptr<ResponseCheckTx>> on_check_tx_sync(const RequestCheckTx& req) noexcept {
    auto reqres = queue_request(to_request_check_tx(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseCheckTx>(reqres->response->release_check_tx());
  }

  result<std::unique_ptr<ResponseQuery>> on_query_sync(const RequestQuery& req) noexcept {
    auto reqres = queue_request(to_request_query(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseQuery>(reqres->response->release_query());
  }

  result<std::unique_ptr<ResponseCommit>> on_commit_sync(const RequestCommit& req) noexcept {
    auto reqres = queue_request(to_request_commit(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseCommit>(reqres->response->release_commit());
  }

  result<std::unique_ptr<ResponseInitChain>> on_init_chain_sync(const RequestInitChain& req) noexcept {
    auto reqres = queue_request(to_request_init_chain(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseInitChain>(reqres->response->release_init_chain());
  }

  result<std::unique_ptr<ResponseBeginBlock>> on_begin_block_sync(const RequestBeginBlock& req) noexcept {
    auto reqres = queue_request(to_request_begin_block(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseBeginBlock>(reqres->response->release_begin_block());
  }

  result<std::unique_ptr<ResponseEndBlock>> on_end_block_sync(const RequestEndBlock& req) noexcept {
    auto reqres = queue_request(to_request_end_block(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseEndBlock>(reqres->response->release_end_block());
  }

  result<std::unique_ptr<ResponseListSnapshots>> on_list_snapshots_sync(const RequestListSnapshots& req) noexcept {
    auto reqres = queue_request(to_request_list_snapshots(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseListSnapshots>(reqres->response->release_list_snapshots());
  }

  result<std::unique_ptr<ResponseOfferSnapshot>> on_offer_snapshot_sync(const RequestOfferSnapshot& req) noexcept {
    auto reqres = queue_request(to_request_offer_snapshot(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseOfferSnapshot>(reqres->response->release_offer_snapshot());
  }

  result<std::unique_ptr<ResponseLoadSnapshotChunk>> on_load_snapshot_chunk_sync(
    const RequestLoadSnapshotChunk& req) noexcept {
    auto reqres = queue_request(to_request_load_snapshot_chunk(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseLoadSnapshotChunk>(reqres->response->release_load_snapshot_chunk());
  }

  result<std::unique_ptr<ResponseApplySnapshotChunk>> on_apply_snapshot_chunk_sync(
    const RequestApplySnapshotChunk& req) noexcept {
    auto reqres = queue_request(to_request_apply_snapshot_chunk(req));
    if (auto err = client_type::flush_sync(); !err) {
      return make_unexpected(err.error());
    }
    return std::unique_ptr<ResponseApplySnapshotChunk>(reqres->response->release_apply_snapshot_chunk());
  }

  result<void> on_start() noexcept {
    boost::asio::post(strand, [this]() {
      conn->connect();
      recv_response_routine();
    });
    /*
    conn->async_connect([this](const auto& ec) {
      send_request_routine();
      recv_response_routine();
    });
    */
    return {};
  }
  void on_stop() noexcept {
    // conn->close();
    // flush_queue();
    flush_timer.cancel();
  }
  result<void> on_reset() noexcept {
    return make_unexpected("not implemented");
  }

private:
  std::string addr;
  bool must_connect;
  std::shared_ptr<Conn> conn;

  boost::asio::steady_timer flush_timer;
  std::atomic_bool flush_timer_is_set;

  std::mutex mtx;
  std::optional<tendermint::error> err;
  std::deque<std::shared_ptr<ReqRes>> req_sent;
  std::function<void(Request*, Response*)> res_cb;

  boost::asio::io_context::strand strand;

  std::shared_ptr<ReqRes> queue_request(std::unique_ptr<Request> req) {
    auto reqres = std::make_shared<ReqRes>();
    reqres->request = std::move(req);
    boost::asio::post(strand, [this, reqres]() {
      will_send_req(reqres);

      varint64 message_length = reqres->request->ByteSizeLong();
      codec::datastream<size_t> ss(0);
      auto message_header_size = write_zigzag(ss, message_length);
      auto total_bytes = message_length + message_header_size;
      std::vector<char> buf(total_bytes);
      codec::datastream<char> ds(buf);
      write_zigzag(ds, message_length);
      reqres->request->SerializeToArray(ds.ptr(), message_length);

      boost::asio::async_write(*conn->socket, boost::asio::buffer(buf), boost::asio::transfer_all(),
        boost::asio::bind_executor(
          conn->strand, [](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (ec) {
              elog(ec.message());
              return;
            }
          }));
      /*
      if (reqres->request->value_case() == Request::kFlush) {
      }
      */
    });
    if (reqres->request->value_case() == Request::kFlush) {
      flush_timer.cancel();
    } else {
      if (!flush_timer_is_set.load()) {
        flush_timer_is_set.store(true);
        boost::asio::post(strand, [this]() {
          flush_timer.expires_after(std::chrono::milliseconds(flush_throttle_ms));
          flush_timer.async_wait(boost::asio::bind_executor(strand, [this](const boost::system::error_code& ec) {
            client_type::flush_async();
            flush_timer_is_set.store(false);
          }));
        });
      }
    }
    return reqres;
  }
  void will_send_req(std::shared_ptr<ReqRes> reqres) {
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
  result<void> did_recv_response(std::unique_ptr<Response> res) {
    std::scoped_lock _(mtx);

    if (req_sent.empty()) {
      return errorf("unexpected {} when nothing expected", res->value_case());
    }

    auto reqres = req_sent.front();
    if (!res_matches_req(reqres->request.get(), res.get())) {
      return errorf("unexpected {} when response to {} expected", res->value_case(), reqres->request->value_case() + 1);
    }

    reqres->response = std::move(res);
    // FIXME: need to end async task
    reqres->set_done();
    req_sent.pop_front();

    if (res_cb) {
      res_cb(reqres->request.get(), reqres->response.get());
    }

    reqres->invoke_callback();
    return {};
  }
  void flush_queue() {}
  void stop_for_error(const tendermint::error& err) {
    if (!service_type::is_running()) {
      return;
    }
    {
      std::scoped_lock _(mtx);
      if (!this->err) {
        this->err = err;
      }
    }
    noir_elog(service_type::logger.get(), "Stopping SocketClient for error: {}", err);
    if (auto err = service_type::stop(); !err) {
      noir_elog(service_type::logger.get(), "Error stopping SocketClient: {}", err.error());
    }
  }

  void send_request_routine() {}

  void recv_response_routine() {
    std::size_t minimum_read =
      std::atomic_exchange<decltype(conn->outstanding_read_bytes.load())>(&conn->outstanding_read_bytes, 0);
    minimum_read = minimum_read != 0 ? minimum_read : 1;

    auto completion_handler = [conn = conn, minimum_read](
                                const boost::system::error_code& ec, std::size_t bytes_transferred) -> std::size_t {
      if (ec || bytes_transferred >= minimum_read) {
        return 0;
      } else {
        return minimum_read - bytes_transferred;
      }
    };

    boost::asio::async_read(*conn->socket, conn->pending_message_buffer.get_buffer_sequence_for_boost_async_read(),
      completion_handler,
      boost::asio::bind_executor(
        conn->strand, [this, conn = conn](const boost::system::error_code& ec, std::size_t bytes_transferred) {
          if (ec) {
            peer_elog(conn, ec.message());
            return;
          }
          try {
            conn->pending_message_buffer.advance_write_ptr(bytes_transferred);
            while (conn->pending_message_buffer.bytes_to_read() > 0) {
              uint32_t bytes_in_buffer = conn->pending_message_buffer.bytes_to_read();
              try {
                varint64 message_length = 0;
                net::details::mb_peek_datastream ds(conn->pending_message_buffer);
                auto message_header_size = read_zigzag(ds, message_length);
                // TODO: message size check
                auto total_message_bytes = message_length + message_header_size;
                if (bytes_in_buffer >= total_message_bytes) {
                  conn->pending_message_buffer.advance_read_ptr(message_header_size);
                  // FIXME
                  auto res = std::make_unique<Response>();
                  res->ParseFromArray(conn->pending_message_buffer.read_ptr(), message_length);
                  conn->pending_message_buffer.advance_read_ptr(message_length);
                  if (res->value_case() == Response::kException) {
                    elog(res->exception().error());
                    break;
                  }
                  if (auto err = did_recv_response(std::move(res)); !err) {
                    elog(err.error());
                    break;
                  }
                } else {
                  auto outstanding_message_bytes = total_message_bytes - bytes_in_buffer;
                  auto available_buffer_bytes = conn->pending_message_buffer.bytes_to_write();
                  if (outstanding_message_bytes > available_buffer_bytes) {
                    conn->pending_message_buffer.add_space(outstanding_message_bytes - available_buffer_bytes);
                  }
                  conn->outstanding_read_bytes = outstanding_message_bytes;
                  break;
                }
              } catch (std::out_of_range& e) {
                conn->outstanding_read_bytes = 1;
                break;
              }
            }
            // FIXME
            recv_response_routine();
          } catch (...) {
            elog("CRITICAL ERROR!");
          }
        }));
  }
};

} // namespace tendermint::abci

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/p2p/conn/secret_connection.h>
#include <tendermint/p2p/transport_mconn.h>

namespace tendermint::p2p {

using namespace noir;
using namespace noir::net;
using namespace boost::asio;

auto MConnConnection::handshake(Chan<Done>& done, NodeInfo& node_info, Bytes32& priv_key)
  -> awaitable<Result<std::tuple<NodeInfo, std::shared_ptr<Bytes>>>> {
  // TODO: handshake
  co_return std::make_tuple(NodeInfo{}, std::make_shared<Bytes>());
}

auto MConnConnection::send_message(Chan<Done>& done, ChannelId ch_id, std::shared_ptr<Bytes>& msg)
  -> awaitable<Result<void>> {
  if (ch_id > std::numeric_limits<uint8_t>::max()) {
    co_return Error::format("MConnection only supports 1-byte channel IDs (got {})", ch_id);
  }
  auto res =
    co_await (error_ch.async_receive(use_awaitable) || done.async_receive(use_awaitable) || mconn->send(ch_id, msg));
  if (res.index() == 0) {
    co_return std::get<0>(res);
  } else if (res.index() == 1) {
    co_return err_eof;
  } else if (res.index() == 2) {
    auto send_res = std::get<2>(res);
    if (send_res.has_error()) {
      co_return Error("sending message timed out");
    } else {
      co_return success();
    }
  } else {
    co_return err_unreachable;
  }
}

auto MConnConnection::receive_message(Chan<Done>& done)
  -> awaitable<Result<std::tuple<ChannelId, std::shared_ptr<Bytes>>>> {
  auto res = co_await (error_ch.async_receive(use_awaitable) || done_ch.async_receive(use_awaitable) ||
    done.async_receive(use_awaitable) || receive_ch.async_receive(use_awaitable));

  if (res.index() == 0) {
    co_return std::get<0>(res);
  } else if (res.index() == 1 || res.index() == 2) {
    co_return err_eof;
  } else if (res.index() == 3) {
    auto msg = std::get<3>(res);
    co_return std::make_tuple(msg.channel_id, msg.payload);
  } else {
    co_return err_unreachable;
  }
}

auto MConnConnection::remote_endpoint() -> std::string {
  return conn->address;
}

auto MConnConnection::close() -> Result<void> {
  done_ch.close();
  if (mconn) {
    mconn->stop();
  } else {
    auto res = conn->close();
    if (res.has_error()) {
      return res.error();
    }
  }
  return success();
}

auto MConnTransport::listen(const std::string& endpoint) -> Result<void> {
  if (listener) {
    return Error("transport is already listening");
  }
  auto endpoint_res = validate_endpoint(endpoint);
  if (endpoint_res.has_error()) {
    return endpoint_res.error();
  }
  listener = TcpListener::create(io_context);

  return success();
}

auto MConnTransport::accept(Chan<Done>& done) -> awaitable<Result<MConnConnection>> {
  if (!listener) {
    co_return Error("transport is not listening");
  }

  auto con_ch = Chan<std::shared_ptr<Conn<TcpConn>>>{io_context};
  auto err_ch = Chan<Error>{io_context};

  co_spawn(
    io_context,
    [&]() -> awaitable<void> {
      auto res = co_await listener->accept();
      if (res.has_error()) {
        co_await (done.async_receive(use_awaitable) ||
          err_ch.async_send(boost::system::error_code{}, res.error(), use_awaitable));
      }
      co_await (done.async_receive(use_awaitable) ||
        con_ch.async_send(boost::system::error_code{}, res.value(), use_awaitable));
    },
    detached);

  auto res = co_await (done.async_receive(use_awaitable) || done_ch.async_receive(use_awaitable) ||
    err_ch.async_receive(use_awaitable) || con_ch.async_receive(use_awaitable));

  if (res.index() == 0 || res.index() == 1) {
    listener->close();
    co_return err_eof;
  } else if (res.index() == 2) {
    co_return std::get<2>(res);
  } else if (res.index() == 3) {
    auto tcp_conn = std::get<3>(res);
    co_return MConnConnection(io_context, tcp_conn, mconn_config, channel_descs);
  } else {
    co_return err_unreachable;
  }
}

auto MConnTransport::dial(Chan<Done>& done, const std::string& endpoint) -> awaitable<Result<MConnConnection>> {
  auto endpoint_res = validate_endpoint(endpoint);
  if (endpoint_res.has_error()) {
    co_return endpoint_res.error();
  }
  std::shared_ptr<Conn<TcpConn>> tcp_conn = TcpConn::create(endpoint, io_context);
  auto res = co_await tcp_conn->connect();
  if (res.has_error()) {
    co_return res.error();
  }
  co_return MConnConnection(io_context, tcp_conn, mconn_config, channel_descs);
}

auto MConnTransport::close() -> Result<void> {
  done_ch.close();
  if (listener) {
    listener->close();
  }
  return success();
}

void MConnTransport::add_channel_descriptor(std::vector<conn::ChannelDescriptorPtr>& channel_desc) {
  channel_descs.insert(channel_descs.end(), channel_desc.begin(), channel_desc.end());
}

auto MConnTransport::validate_endpoint(const std::string& endpoint) -> Result<void> {
  if (endpoint.empty()) {
    return Error("endpoint has no protocol");
  }
  auto host = endpoint.substr(0, endpoint.find(':'));
  auto port = endpoint.substr(host.size() + 1, endpoint.size());

  if (host.empty()) {
    return Error("empty host");
  }
  if (port.empty()) {
    return Error("empty port");
  }
  return success();
}

} //namespace tendermint::p2p

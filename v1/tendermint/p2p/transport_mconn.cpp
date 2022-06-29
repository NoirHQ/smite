// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <noir/common/proto_readwrite.h>
#include <google/protobuf/wrappers.pb.h>
#include <tendermint/p2p/conn/secret_connection.h>
#include <tendermint/p2p/transport_mconn.h>
#include <tendermint/p2p/types.pb.h>
#include <tuple>

namespace tendermint::p2p {

using namespace noir;
using namespace noir::net;
using namespace boost::asio;
using namespace google::protobuf;

auto MConnConnection::handshake_inner(NodeInfo& node_info, Bytes& priv_key)
  -> asio::awaitable<Result<std::tuple<std::shared_ptr<conn::MConnection>, NodeInfo, Bytes>>> {
  if (mconn) {
    co_return Error("connection is already handshaked");
  }
  Chan<std::monostate> sent_ch{io_context, 1};
  Chan<std::monostate> rcvd_ch{io_context, 1};

  auto secret_conn = conn::SecretConnection::make_secret_connection(priv_key, conn);

  // share eph pub
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      BytesValue msg{};
      msg.set_value(std::string(secret_conn->loc_eph_pub.begin(), secret_conn->loc_eph_pub.end()));
      auto buf = ProtoReadWrite::write_msg<BytesValue, unsigned char>(msg);
      co_await conn->write(std::span<const unsigned char>(buf));
      co_await sent_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
    },
    detached);
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto msg = co_await ProtoReadWrite::read_msg_async<BytesValue, std::shared_ptr<Conn<TcpConn>>>(conn);
      Bytes32 rem_eph_pub(std::span(msg.value().data(), 32));
      secret_conn->shared_eph_pub_key(rem_eph_pub);
      co_await rcvd_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
    },
    detached);
  co_await (
    sent_ch.async_receive(as_result(asio::use_awaitable)) && rcvd_ch.async_receive(as_result(asio::use_awaitable)));

  // share sig
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      AuthSigMessage msg{};
      msg.mutable_pub_key()->set_ed25519(std::string(secret_conn->loc_pub_key.begin(), secret_conn->loc_pub_key.end()));
      msg.set_sig(std::string(secret_conn->loc_signature.begin(), secret_conn->loc_signature.end()));

      auto buf = ProtoReadWrite::write_msg<AuthSigMessage, unsigned char>(msg);
      co_await conn->write(std::span<const unsigned char>(buf));
      co_await sent_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
    },
    detached);
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto msg = co_await ProtoReadWrite::read_msg_async<AuthSigMessage, std::shared_ptr<Conn<TcpConn>>>(conn);
      conn::AuthSigMessage sig_msg = {.key = Bytes{msg.pub_key().ed25519().begin(), msg.pub_key().ed25519().end()},
        .sig = Bytes{msg.sig().begin(), msg.sig().end()}};
      secret_conn->shared_auth_sig(sig_msg);
      co_await rcvd_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
    },
    detached);
  co_await (
    sent_ch.async_receive(as_result(asio::use_awaitable)) && rcvd_ch.async_receive(as_result(asio::use_awaitable)));

  // share node info using secret_conn
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto loc_node = node_info.to_proto();
      auto buf = ProtoReadWrite::write_msg<DefaultNodeInfo, unsigned char>(loc_node);
      co_await secret_conn->write(std::span<const unsigned char>(buf));
      co_await sent_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
    },
    detached);

  NodeInfo peer_info;
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      std::vector<unsigned char> buf(10240);
      co_await secret_conn->read(std::span<unsigned char>(buf));
      codec::datastream<const unsigned char> ds(buf);
      auto dni = ProtoReadWrite::read_msg<DefaultNodeInfo, codec::datastream<const unsigned char>>(ds);
      peer_info = NodeInfo::from_proto(dni);
      co_await rcvd_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
    },
    detached);
  co_await (
    sent_ch.async_receive(as_result(asio::use_awaitable)) && rcvd_ch.async_receive(as_result(asio::use_awaitable)));

  auto on_receive = [&](ChannelId channel_id, std::shared_ptr<Bytes> payload) -> asio::awaitable<void> {
    auto msg = MConnMessage{.channel_id = channel_id, .payload = payload};
    co_await (receive_ch.async_send(boost::system::error_code{}, msg, asio::use_awaitable) ||
      close_ch.async_receive(as_result(asio::use_awaitable)));
  };

  auto on_error = [&](noir::Error err) -> asio::awaitable<void> {
    close();
    co_await (error_ch.async_send(boost::system::error_code{}, err, asio::use_awaitable) ||
      close_ch.async_receive(as_result(asio::use_awaitable)));
  };

  auto new_mconn = conn::MConnection::new_mconnection_with_config(io_context,
    secret_conn,
    channel_descs,
    std::move(on_receive),
    std::move(on_error),
    mconn_config);
  co_return std::make_tuple(new_mconn, peer_info, secret_conn->rem_pub_key);
}

auto MConnConnection::handshake(Chan<std::monostate>& done, NodeInfo& node_info, Bytes& priv_key)
  -> asio::awaitable<std::tuple<NodeInfo, std::shared_ptr<Bytes>, Result<void>>> {
  Chan<Result<void>> res_ch{io_context, 1};

  std::shared_ptr<conn::MConnection> mconn_ptr;
  NodeInfo peer_info;
  Bytes peer_key;
  co_spawn(
    io_context,
    [&, this]() -> asio::awaitable<void> {
      auto res = co_await handshake_inner(node_info, priv_key);
      if (!res.has_error()) {
        auto tup = res.value();
        mconn_ptr = std::get<0>(tup);
        peer_info = std::get<1>(tup);
        peer_key = std::get<2>(tup);
        co_await res_ch.async_send(boost::system::error_code{}, success(), asio::use_awaitable);
      } else {
        co_await res_ch.async_send(boost::system::error_code{}, res.error(), asio::use_awaitable);
      }
    },
    detached);

  auto res = co_await (done.async_receive(as_result(asio::use_awaitable)) || res_ch.async_receive(asio::use_awaitable));
  if (res.index() == 0) {
    close();
    co_return std::make_tuple(NodeInfo{}, nullptr, std::get<0>(res).error());
  } else if (res.index() == 1) {
    auto inner_res = std::get<1>(res);
    if (!inner_res.has_error()) {
      co_return std::make_tuple(peer_info, std::make_shared<Bytes>(peer_key), success());
    } else {
      co_return std::make_tuple(NodeInfo{}, nullptr, inner_res.error());
    }
  } else {
    co_return std::make_tuple(NodeInfo{}, nullptr, err_unreachable);
  }
}

auto MConnConnection::send_message(Chan<std::monostate>& done, ChannelId ch_id, std::shared_ptr<Bytes>& msg)
  -> asio::awaitable<Result<void>> {
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

auto MConnConnection::receive_message(Chan<std::monostate>& done)
  -> asio::awaitable<std::tuple<ChannelId, std::shared_ptr<noir::Bytes>, Result<void>>> {
  auto res = co_await (error_ch.async_receive(use_awaitable) || close_ch.async_receive(use_awaitable) ||
    done.async_receive(use_awaitable) || receive_ch.async_receive(use_awaitable));

  if (res.index() == 0) {
    co_return std::make_tuple(0, nullptr, std::get<0>(res));
  } else if (res.index() == 1 || res.index() == 2) {
    co_return std::make_tuple(0, nullptr, err_eof);
  } else if (res.index() == 3) {
    auto msg = std::get<3>(res);
    co_return std::make_tuple(msg.channel_id, msg.payload, success());
  } else {
    co_return std::make_tuple(0, nullptr, err_unreachable);
  }
}

auto MConnConnection::remote_endpoint() -> std::string {
  return conn->address;
}

auto MConnConnection::close() -> Result<void> {
  close_ch.close();
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

auto MConnTransport::listen(const std::string& endpoint) -> asio::awaitable<Result<void>> {
  if (listener) {
    co_return Error("transport is already listening");
  }
  auto endpoint_res = validate_endpoint(endpoint);
  if (endpoint_res.has_error()) {
    co_return endpoint_res.error();
  }
  listener = TcpListener::create(io_context);
  co_await listener->listen(endpoint);

  co_return success();
}

auto MConnTransport::accept(Chan<std::monostate>& done) -> asio::awaitable<Result<std::shared_ptr<MConnConnection>>> {
  if (!listener) {
    co_return Error("transport is not listening");
  }

  auto con_ch = Chan<std::shared_ptr<Conn<TcpConn>>>{io_context};
  auto err_ch = Chan<Error>{io_context};

  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto res = co_await listener->accept();
      if (res.has_error()) {
        co_await (done.async_receive(use_awaitable) ||
          err_ch.async_send(boost::system::error_code{}, res.error(), use_awaitable));
      } else {
        co_await (done.async_receive(use_awaitable) ||
          con_ch.async_send(boost::system::error_code{}, res.value(), use_awaitable));
      }
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
    co_return MConnConnection::new_mconn_connection(io_context, tcp_conn, mconn_config, channel_descs);
  } else {
    co_return err_unreachable;
  }
}

auto MConnTransport::dial(const std::string& endpoint) -> asio::awaitable<Result<std::shared_ptr<MConnConnection>>> {
  auto endpoint_res = validate_endpoint(endpoint);
  if (endpoint_res.has_error()) {
    co_return endpoint_res.error();
  }
  std::shared_ptr<Conn<TcpConn>> tcp_conn = TcpConn::create(endpoint, io_context);
  auto res = co_await tcp_conn->connect();
  if (res.has_error()) {
    co_return res.error();
  }
  co_return MConnConnection::new_mconn_connection(io_context, tcp_conn, mconn_config, channel_descs);
}

auto MConnTransport::close() -> Result<void> {
  done_ch.close();
  if (listener) {
    listener->close();
  }
  return success();
}

void MConnTransport::add_channel_descriptor(std::vector<std::shared_ptr<conn::ChannelDescriptor>>&& channel_desc) {
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

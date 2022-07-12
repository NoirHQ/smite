// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <noir/common/proto_readwrite.h>
#include <tendermint/p2p/conn/secret_connection.h>
#include <tendermint/p2p/transport_mconn.h>
#include <tendermint/p2p/types.pb.h>
#include <google/protobuf/wrappers.pb.h>
#include <tuple>

namespace tendermint::p2p {

using namespace noir;
using namespace noir::net;
using namespace boost::asio;
using namespace google::protobuf;

auto MConnConnection::handshake_inner(tendermint::type::NodeInfo& node_info, std::vector<unsigned char>& priv_key)
  -> asio::awaitable<
    Result<std::tuple<std::shared_ptr<conn::MConnection>, tendermint::type::NodeInfo, std::vector<unsigned char>>>> {
  if (mconn) {
    co_return Error("connection is already handshaked");
  }
  auto sent_ch = eo::make_chan(1);
  auto rcvd_ch = eo::make_chan(1);

  auto secret_conn = conn::SecretConnection::make_secret_connection(priv_key, conn);

  // share eph pub
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      BytesValue msg{};
      msg.set_value(std::string(secret_conn->loc_eph_pub.begin(), secret_conn->loc_eph_pub.end()));
      auto buf = ProtoReadWrite::write_msg<BytesValue, unsigned char>(msg);
      co_await conn->write(std::span<const unsigned char>(buf));
      co_await *(sent_ch << std::monostate{});
    },
    detached);
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto msg = co_await ProtoReadWrite::read_msg_async<BytesValue, std::shared_ptr<Conn<TcpConn>>>(conn);
      Bytes32 rem_eph_pub(std::span(msg.value().data(), 32));
      secret_conn->shared_eph_pub_key(rem_eph_pub);
      co_await *(rcvd_ch << std::monostate{});
    },
    detached);
  co_await (sent_ch.async_receive(asio::use_awaitable) && rcvd_ch.async_receive(asio::use_awaitable));

  // share sig
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      AuthSigMessage msg{};
      msg.mutable_pub_key()->set_ed25519(std::string(secret_conn->loc_pub_key.begin(), secret_conn->loc_pub_key.end()));
      msg.set_sig(std::string(secret_conn->loc_signature.begin(), secret_conn->loc_signature.end()));

      auto buf = ProtoReadWrite::write_msg<AuthSigMessage, unsigned char>(msg);
      co_await conn->write(std::span<const unsigned char>(buf));
      co_await *(sent_ch << std::monostate{});
    },
    detached);
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto msg = co_await ProtoReadWrite::read_msg_async<AuthSigMessage, std::shared_ptr<Conn<TcpConn>>>(conn);
      conn::AuthSigMessage sig_msg = {
        .key = std::vector<unsigned char>(msg.pub_key().ed25519().begin(), msg.pub_key().ed25519().end()),
        .sig = std::vector<unsigned char>(msg.sig().begin(), msg.sig().end())};
      secret_conn->shared_auth_sig(sig_msg);
      co_await *(rcvd_ch << std::monostate{});
    },
    detached);
  co_await (sent_ch.async_receive(asio::use_awaitable) && rcvd_ch.async_receive(asio::use_awaitable));

  // share node info using secret_conn
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto loc_node = node_info.to_proto();
      auto buf = ProtoReadWrite::write_msg<tendermint::p2p::NodeInfo, unsigned char>(loc_node);
      co_await secret_conn->write(std::span<const unsigned char>(buf));
      co_await *(sent_ch << std::monostate{});
    },
    detached);

  tendermint::type::NodeInfo peer_info;
  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      std::vector<unsigned char> buf(10240);
      co_await secret_conn->read(std::span<unsigned char>(buf));
      codec::Datastream<const unsigned char> ds(buf);
      auto dni = ProtoReadWrite::read_msg<p2p::NodeInfo, codec::Datastream<const unsigned char>>(ds);
      peer_info = tendermint::type::NodeInfo::from_proto(dni);
      co_await *(rcvd_ch << std::monostate{});
    },
    detached);
  co_await (sent_ch.async_receive(asio::use_awaitable) && rcvd_ch.async_receive(asio::use_awaitable));

  auto on_receive = [&](ChannelId channel_id,
                      std::shared_ptr<std::vector<unsigned char>> payload) -> asio::awaitable<void> {
    auto select = eo::Select{receive_ch << MConnMessage{.channel_id = channel_id, .payload = payload}, *close_ch};
    switch (co_await select.index()) {
    case 0:
      co_await select.process<0>();
      break;
    case 1:
      co_await select.process<1>();
      break;
    }
  };

  auto on_error = [&](noir::Error err) -> asio::awaitable<void> {
    close();
    auto select = eo::Select{error_ch << err, *close_ch};
    switch (co_await select.index()) {
    case 0:
      co_await select.process<0>();
      break;
    case 1:
      co_await select.process<1>();
      break;
    }
  };

  auto new_mconn = conn::MConnection::new_mconnection_with_config(
    io_context, secret_conn, channel_descs, std::move(on_receive), std::move(on_error), mconn_config);
  co_return std::make_tuple(new_mconn, peer_info, secret_conn->rem_pub_key);
}

auto MConnConnection::handshake(
  eo::chan<std::monostate>& done, tendermint::type::NodeInfo& node_info, std::vector<unsigned char>& priv_key)
  -> asio::awaitable<Result<std::tuple<tendermint::type::NodeInfo, std::shared_ptr<std::vector<unsigned char>>>>> {
  eo::chan<std::shared_ptr<noir::Error>> err_ch{io_context, 1};

  std::shared_ptr<conn::MConnection> mconn_ptr;
  tendermint::type::NodeInfo peer_info;
  std::vector<unsigned char> peer_key;
  co_spawn(
    io_context,
    [&, this]() -> asio::awaitable<void> {
      auto res = co_await handshake_inner(node_info, priv_key);
      if (res) {
        auto tup = res.value();
        mconn_ptr = std::get<0>(tup);
        peer_info = std::get<1>(tup);
        peer_key = std::get<2>(tup);
        co_await *(err_ch << nullptr);
      } else {
        co_await *(err_ch << std::make_shared<noir::Error>(res.error()));
      }
    },
    detached);

  auto select = eo::Select{*done, *err_ch};
  switch (co_await select.index()) {
  case 0:
    co_await select.process<0>();
    close();
    co_return err_context_done;
  case 1:
    auto err = co_await select.process<1>();
    if (err) {
      co_return *err;
    }
    mconn = mconn_ptr;
    mconn->start(done);
    co_return std::make_tuple(peer_info, std::make_shared<std::vector<unsigned char>>(peer_key));
  }
  co_return err_unreachable;
}

auto MConnConnection::send_message(
  eo::chan<std::monostate>& done, const ChannelId& ch_id, std::shared_ptr<std::vector<unsigned char>>& msg)
  -> asio::awaitable<Result<void>> {
  if (ch_id > std::numeric_limits<uint8_t>::max()) {
    co_return Error::format("MConnection only supports 1-byte channel IDs (got {})", ch_id);
  }

  auto select = eo::Select{*error_ch, *done, eo::CaseDefault()};
  switch (co_await select.index()) {
  case 0:
    co_return co_await select.process<0>();
  case 1:
    co_await select.process<1>();
    co_return err_eof;
  default:
    auto res = co_await mconn->send(ch_id, msg);
    if (!res) {
      co_return Error("sending message timed out");
    }
    co_return success();
  }
}

auto MConnConnection::receive_message(eo::chan<std::monostate>& done)
  -> asio::awaitable<Result<std::tuple<ChannelId, std::shared_ptr<std::vector<unsigned char>>>>> {
  auto select = eo::Select{*error_ch, *close_ch, *done, *receive_ch};

  switch (co_await select.index()) {
  case 0:
    co_return co_await select.process<0>();
  case 1:
    co_await select.process<1>();
    co_return err_eof;
  case 2:
    co_await select.process<2>();
    co_return err_eof;
  case 3:
    auto msg = co_await select.process<3>();
    co_return std::make_tuple(msg.channel_id, msg.payload);
  }
  co_return err_unreachable;
}

auto MConnConnection::remote_endpoint() -> std::string {
  return conn->address;
}

auto MConnConnection::close() -> Result<void> {
  close_ch.close();
  if (mconn) {
    mconn->stop();
  } else {
    conn->close();
  }
  return success();
}

auto MConnTransport::listen(const std::string& endpoint) -> asio::awaitable<Result<void>> {
  if (listener) {
    co_return Error("transport is already listening");
  }
  auto endpoint_res = validate_endpoint(endpoint);
  if (!endpoint_res) {
    co_return endpoint_res.error();
  }
  listener = new_tcp_listener(io_context);
  co_await listener->listen(endpoint);

  co_return success();
}

auto MConnTransport::accept(eo::chan<std::monostate>& done)
  -> asio::awaitable<Result<std::shared_ptr<MConnConnection>>> {
  if (!listener) {
    co_return Error("transport is not listening");
  }

  auto con_ch = eo::make_chan<std::shared_ptr<Conn<TcpConn>>>();
  auto err_ch = eo::make_chan<Error>();

  co_spawn(
    io_context,
    [&]() -> asio::awaitable<void> {
      auto res = co_await listener->accept();
      if (!res) {
        auto select = eo::Select{err_ch << res.error(), *done};
        switch (co_await select.index()) {
        case 0:
          co_await select.process<0>();
          break;
        case 1:
          co_await select.process<1>();
          break;
        }
      }
      auto select = eo::Select{con_ch << res.value(), *done};
      switch (co_await select.index()) {
      case 0:
        co_await select.process<0>();
        break;
      case 1:
        co_await select.process<1>();
        break;
      }
    },
    detached);

  auto select = eo::Select{*done, *done_ch, *err_ch, *con_ch};
  switch (co_await select.index()) {
  case 0:
    co_await select.process<0>();
    listener->close();
    co_return err_eof;
  case 1:
    co_await select.process<1>();
    listener->close();
    co_return err_eof;
  case 2:
    co_return co_await select.process<2>();
  case 3:
    auto tcp_conn = co_await select.process<3>();
    co_return MConnConnection::new_mconn_connection(io_context, tcp_conn, mconn_config, channel_descs);
  }
  co_return err_unreachable;
}

auto MConnTransport::dial(const std::string& endpoint) -> asio::awaitable<Result<std::shared_ptr<MConnConnection>>> {
  auto endpoint_res = validate_endpoint(endpoint);
  if (!endpoint_res) {
    co_return endpoint_res.error();
  }
  std::shared_ptr<Conn<TcpConn>> tcp_conn = new_tcp_conn(io_context, endpoint);
  auto res = co_await reinterpret_cast<std::shared_ptr<TcpConn>&>(tcp_conn)->connect();
  if (!res) {
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

} // namespace tendermint::p2p

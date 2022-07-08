// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <noir/common/proto_readwrite.h>
#include <tendermint/p2p/conn/connection.h>

namespace tendermint::p2p::conn {

using namespace noir;
using namespace std::chrono;

namespace detail {
  auto Channel::send_bytes(std::shared_ptr<std::vector<unsigned char>> bytes) -> asio::awaitable<Result<bool>> {
    asio::steady_timer timer{io_context};
    timer.expires_after(default_send_timeout);
    boost::system::error_code ec{};

    auto res =
      co_await (send_queue.async_send(ec, bytes, asio::use_awaitable) || timer.async_wait(asio::use_awaitable));
    switch (res.index()) {
    case 0:
      if (ec) {
        co_return false;
      }
      co_return true;
    case 1:
      co_return false;
    }
    co_return err_unreachable;
  }

  auto Channel::is_send_pending() -> bool {
    if (!sending) {
      return send_queue.try_receive(
        [&](boost::system::error_code ec, const std::shared_ptr<std::vector<unsigned char>>& bytes) mutable {
          sending = bytes;
          sent_pos = 0;
        });
    }
    return true;
  }

  void Channel::set_next_packet_msg(PacketMsg* msg) {
    msg->set_channel_id(desc->id);
    auto remaining = sending->size() - sent_pos;
    auto packet_size = remaining < max_packet_msg_payload_size ? remaining : max_packet_msg_payload_size;
    msg->set_data({sending->begin() + sent_pos, sending->begin() + sent_pos + packet_size});
    if (remaining <= max_packet_msg_payload_size) {
      msg->set_eof(true);
      sending.reset();
    } else {
      msg->set_eof(false);
      sent_pos += packet_size;
    }
  }

  auto Channel::write_packet_msg_to(BufferedWriterUptr<net::Conn<net::TcpConn>>& writer)
    -> asio::awaitable<Result<std::size_t>> {
    Packet packet{};
    PacketMsg* msg = packet.mutable_packet_msg();
    set_next_packet_msg(msg);
    auto serialized = ProtoReadWrite::write_msg<Packet, unsigned char>(packet);

    auto res = co_await writer->write(serialized);
    if (res.has_value()) {
      recently_sent += res.value();
    }
    co_return res;
  }

  auto Channel::recv_packet_msg(const PacketMsg& packet) -> Result<std::vector<unsigned char>> {
    auto recv_cap = desc->recv_message_capacity;
    auto recv_received = recving.size() + packet.data().size();
    if (recv_cap < recv_received) {
      return Error::format("received message exceeds available capacity: {} < {}", recv_cap, recv_received);
    }
    recving.insert(recving.end(), packet.data().begin(), packet.data().end());
    if (packet.eof()) {
      auto msg_bytes = recving;
      recving.clear();
      return msg_bytes;
    }
    return std::vector<unsigned char>();
  }

  void Channel::update_stats() {
    recently_sent = (recently_sent * 8) / 10;
  }
} // namespace detail

void MConnection::start(eo::chan<std::monostate>& done) {
  if (!conn || !buf_conn_writer) {
    throw std::runtime_error("MUST set connection and buf_conn_writer");
  }

  ping_timer.start();
  ch_stats_timer.start();
  set_recv_last_msg_at(system_clock::to_time_t(system_clock::now()));

  asio::co_spawn(io_context, send_routine(done), asio::detached);
  asio::co_spawn(io_context, recv_routine(done), asio::detached);
}

void MConnection::set_recv_last_msg_at(const std::time_t& t) {
  std::scoped_lock _{last_msg_recv.mtx};
  last_msg_recv.at = t;
}

auto MConnection::get_last_message_at() -> std::time_t {
  std::scoped_lock _{last_msg_recv.mtx};
  return last_msg_recv.at;
}

auto MConnection::string() -> std::string {
  return fmt::format("MConn{{}}", conn->address);
}

auto MConnection::flush() -> boost::asio::awaitable<void> {
  co_await buf_conn_writer->flush();
}

auto MConnection::send(ChannelId ch_id, std::shared_ptr<std::vector<unsigned char>> msg_bytes)
  -> asio::awaitable<Result<bool>> {
  if (!channels_idx.contains(ch_id)) {
    co_return false;
  }
  auto res = co_await channels_idx[ch_id]->send_bytes(msg_bytes);
  if (!res.has_error()) {
    co_return res.error();
  }
  auto success = res.value();
  if (success) {
    boost::system::error_code ec{};
    co_await send_ch.async_send(ec, {}, asio::use_awaitable);
    if (ec) {
      co_return ec;
    }
  }
  co_return success;
}

auto MConnection::send_routine(eo::chan<std::monostate>& done) -> asio::awaitable<Result<void>> {
  Ticker pong_timeout{io_context, config.pong_timeout};
  pong_timeout.start();

  for (;;) {
    Error err{};

    auto res = co_await (flush_timer.event_ch.async_receive(boost::asio::use_awaitable) ||
      ch_stats_timer.time_ch.async_receive(asio::use_awaitable) ||
      ping_timer.time_ch.async_receive(asio::use_awaitable) || pong_ch.async_receive(asio::use_awaitable) ||
      done.async_receive(as_result(asio::use_awaitable)) ||
      quit_send_routine_ch.async_receive(as_result(asio::use_awaitable)) ||
      pong_timeout.time_ch.async_receive(asio::use_awaitable) || send_ch.async_receive(asio::use_awaitable));

    if (res.index() == 0) {
      co_await flush();
    } else if (res.index() == 1) {
      for (auto& channel : channels_idx) {
        channel.second->update_stats();
      }
    } else if (res.index() == 2) {
      auto ping_res = co_await send_ping();
      if (ping_res.has_error()) {
        err = ping_res.error();
      } else {
        co_await flush();
      }
    } else if (res.index() == 3) {
      auto pong_res = co_await send_pong();
      if (pong_res.has_error()) {
        err = pong_res.error();
      } else {
        co_await flush();
      }
    } else if (res.index() == 4 || res.index() == 5) {
      break;
    } else if (res.index() == 6) {
    } else if (res.index() == 7) {
      auto msg_res = co_await send_some_packet_msgs(done);
      if (!msg_res.has_error()) {
        auto eof = msg_res.value();
        if (!eof) {
          co_await send_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
        }
      }
    } else {
      co_return err_unreachable;
    }
    if (duration_cast<milliseconds>(system_clock::now() - system_clock::from_time_t(get_last_message_at())).count() >
      config.pong_timeout.count()) {
      err = Error("pong timeout");
    }

    if (err) {
      co_await stop_for_error(err);
      break;
    }
  }

  done_send_routine_ch.close();
  pong_timeout.stop();
  co_return success();
}

auto MConnection::send_ping() -> asio::awaitable<Result<void>> {
  Packet packet{};
  packet.mutable_packet_ping();
  auto serialized = ProtoReadWrite::write_msg<Packet, unsigned char>(packet);
  auto res = co_await buf_conn_writer->write(serialized);
  if (res.has_error()) {
    co_return res.error();
  } else {
    co_return success();
  }
}

auto MConnection::send_pong() -> asio::awaitable<Result<void>> {
  Packet packet{};
  packet.mutable_packet_pong();
  auto serialized = ProtoReadWrite::write_msg<Packet, unsigned char>(packet);
  auto res = co_await buf_conn_writer->write(serialized);
  if (res.has_error()) {
    co_return res.error();
  } else {
    co_return success();
  }
}

auto MConnection::send_some_packet_msgs(eo::chan<std::monostate>& done) -> asio::awaitable<Result<bool>> {
  for (auto i = 0; i < num_batch_packet_msgs; i++) {
    auto res = co_await send_packet_msg(done);
    if (!res.has_error() && res.value()) {
      co_return true;
    }
  }
  co_return false;
}

auto MConnection::send_packet_msg(eo::chan<std::monostate>& done) -> asio::awaitable<Result<bool>> {
  auto least_ratio = std::numeric_limits<float_t>::max();
  detail::ChannelUptr* least_channel{};
  for (auto& channel : channels_idx) {
    if (!channel.second->is_send_pending()) {
      continue;
    }
    auto ratio =
      static_cast<float_t>(channel.second->recently_sent) / static_cast<float_t>(channel.second->desc->priority);
    if (ratio < least_ratio) {
      least_ratio = ratio;
      least_channel = &channel.second;
    }
  }

  if (!least_channel) {
    co_return true;
  }
  auto res = co_await (*least_channel)->write_packet_msg_to(buf_conn_writer);
  if (res.has_error()) {
    co_await stop_for_error(res.error());
  }
  flush_timer.set();
  co_return false;
}

auto MConnection::stop_for_error(const Error& err) -> asio::awaitable<void> {
  stop();
  co_await on_error(err);
}

void MConnection::stop() {
  if (stop_services()) {
    return;
  }
  conn->close();
}

auto MConnection::stop_services() -> bool {
  std::scoped_lock _{stop_mtx};

  if (!quit_send_routine_ch.is_open() || !quit_recv_routine_ch.is_open()) {
    return true;
  }

  flush_timer.stop();
  ping_timer.stop();
  ch_stats_timer.stop();

  quit_recv_routine_ch.close();
  quit_send_routine_ch.close();
  return false;
}

auto MConnection::recv_routine(eo::chan<std::monostate>& done) -> asio::awaitable<Result<void>> {
  for (;;) {
    Packet packet{};
    auto res = co_await (done.async_receive(as_result(asio::use_awaitable)) ||
      done_send_routine_ch.async_receive(as_result(asio::use_awaitable)) ||
      quit_recv_routine_ch.async_receive(as_result(asio::use_awaitable)) || read_packet(packet));

    if (res.index() == 0 || res.index() == 1 || res.index() == 2) {
      co_return success();
    } else if (res.index() == 3) {
      auto read_res = std::get<3>(res);
      if (read_res.has_error()) {
        co_await stop_for_error(read_res.error());
        break;
      }

      set_recv_last_msg_at(system_clock::to_time_t(system_clock::now()));

      if (packet.sum_case() == Packet::kPacketPing) {
        boost::system::error_code ec{};
        co_await pong_ch.async_send(ec, {}, asio::use_awaitable);
      } else if (packet.sum_case() == Packet::kPacketPong) {
      } else if (packet.sum_case() == Packet::kPacketMsg) {
        auto channel_id = static_cast<uint16_t>(packet.packet_msg().channel_id());
        auto& channel = channels_idx[channel_id];
        if (packet.packet_msg().channel_id() < 0 ||
          packet.packet_msg().channel_id() > std::numeric_limits<uint8_t>::max() || channels_idx.contains(channel_id) ||
          channel == nullptr) {
          co_await stop_for_error(Error::format("unknown channel {}", packet.packet_msg().channel_id()));
          break;
        }
        auto msg_res = channel->recv_packet_msg(packet.packet_msg());
        if (msg_res.has_error()) {
          co_await stop_for_error(msg_res.error());
          break;
        }
        if (msg_res.has_value() && !msg_res.value().empty()) {
          auto msg_bytes = std::make_shared<std::vector<unsigned char>>(msg_res.value());
          co_await on_receive(channel_id, msg_bytes);
        }
      } else {
        co_await stop_for_error(Error::format("unknown message type {}", packet.sum_case()));
      }
    } else {
      co_return err_unreachable;
    }
  }
  co_return success();
}

auto MConnection::read_packet(Packet& packet) -> asio::awaitable<Result<void>> {
  Varint64 message_length = 0;
  auto length_res = co_await read_uleb128_async(*conn, message_length);
  if (length_res.has_error()) {
    co_return length_res.error();
  }
  std::vector<unsigned char> message_buffer(message_length);
  auto msg_res = co_await conn->read(message_buffer);
  if (msg_res.has_error()) {
    co_return msg_res.error();
  }
  packet.ParseFromArray(message_buffer.data(), message_buffer.size());
  co_return success();
}

} // namespace tendermint::p2p::conn

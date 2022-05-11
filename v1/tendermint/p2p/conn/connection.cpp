// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <tendermint/p2p/conn/connection.h>

namespace tendermint::p2p::conn {

using namespace noir;

namespace detail {
  auto packet_to_bytes(Packet& msg) -> Bytes {
    VarInt64 message_length = msg.ByteSizeLong();
    codec::datastream<size_t> ss(0);
    auto message_header_size = write_uleb128(ss, message_length).value();
    auto total_bytes = message_length + message_header_size;
    Bytes buf(total_bytes);
    codec::datastream<ByteType> ds(buf);
    write_uleb128(ds, message_length).value();
    msg.SerializeToArray(ds.ptr(), message_length);
    return buf;
  }

  auto Channel::send_bytes(BytesPtr bytes) -> asio::awaitable<Result<bool>> {
    asio::steady_timer timer{io_context};
    timer.expires_after(std::chrono::seconds{10});
    boost::system::error_code ec{};
    auto res =
      co_await (timer.async_wait(asio::use_awaitable) || send_queue.async_send(ec, bytes, asio::use_awaitable));
    switch (res.index()) {
    case 0:
      co_return false;
    case 1:
      // TODO: if ec is true, how to handle?
      send_queue_size++;
      co_return true;
    }
    co_return err_unreachable;
  }

  auto Channel::is_send_pending() -> bool {
    if (sending.empty()) {
      return send_queue.try_receive([&](boost::system::error_code ec, BytesPtr bytes) mutable {
        sending.insert(sending.end(), bytes->begin(), bytes->end());
      });
    }
    return true;
  }

  auto Channel::next_packet_msg() -> PacketMsg {
    PacketMsg packet{};
    packet.set_channel_id(desc->id);
    auto packet_size = sending.size() < max_packet_msg_payload_size ? sending.size() : max_packet_msg_payload_size;
    packet.set_data({sending.begin(), sending.begin() + packet_size});
    if (sending.size() <= max_packet_msg_payload_size) {
      packet.set_eof(true);
      sending.clear();
    } else {
      packet.set_eof(false);
      sending = Bytes{sending.begin() + packet_size, sending.end()};
    }
    return packet;
  }

  auto Channel::write_packet_msg_to(BufferedWriter<net::Conn<net::TcpConn>>& w)
    -> asio::awaitable<Result<std::size_t>> {
    PacketMsg msg = next_packet_msg();
    Packet packet{};
    packet.set_allocated_packet_msg(&msg);
    auto bytes = detail::packet_to_bytes(packet);

    auto res = co_await w.write(bytes);
    if (res.has_value()) {
      recently_sent += res.value();
    }
    co_return res;
  }

  auto Channel::recv_packet_msg(PacketMsg packet) -> Result<Bytes> {
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
    return Bytes{};
  }

  void Channel::update_stats() {
    recently_sent = (recently_sent * 8) / 10;
  }
} //namespace detail

void MConnection::start(Chan<Done>& done) {
  ping_timer.start();
  ch_stats_timer.start();
  set_recv_last_msg_at(std::chrono::system_clock::now());

  asio::co_spawn(io_context, send_routine(done), asio::detached);
  asio::co_spawn(io_context, recv_routine(done), asio::detached);
}

void MConnection::set_recv_last_msg_at(Time&& t) {
  std::scoped_lock _{last_msg_recv.mtx};
  last_msg_recv.at = t;
}

auto MConnection::get_last_message_at() -> Time {
  std::scoped_lock _{last_msg_recv.mtx};
  return last_msg_recv.at;
}

auto MConnection::string() -> std::string {
  return fmt::format("MConn{{}}", conn.address);
}

auto MConnection::flush() -> boost::asio::awaitable<void> {
  co_await buf_conn_writer.flush();
}

auto MConnection::send(ChannelId ch_id, BytesPtr msg_bytes) -> asio::awaitable<Result<bool>> {
  if (!channels_idx.contains(ch_id)) {
    co_return false;
  }

  auto success = (co_await channels_idx[ch_id]->send_bytes(msg_bytes)).value();
  if (success) {
    boost::system::error_code ec{};
    co_await send_ch.async_send(ec, {}, asio::use_awaitable);
    // TODO: how to handle ec?
  }
  co_return success;
}

auto MConnection::send_routine(Chan<Done>& done) -> asio::awaitable<void> {
  Ticker pong_timeout{io_context, config.pong_timeout};
  pong_timeout.start();

  for (;;) {
    auto res = co_await (flush_timer.event_ch.async_receive(boost::asio::use_awaitable) ||
      ch_stats_timer.time_ch.async_receive(asio::use_awaitable) ||
      ping_timer.time_ch.async_receive(asio::use_awaitable) || pong_ch.async_receive(asio::use_awaitable) ||
      done.async_receive(as_result(asio::use_awaitable)) || quit_send_routine_ch.async_receive(asio::use_awaitable) ||
      pong_timeout.time_ch.async_receive(asio::use_awaitable) || send_ch.async_receive(asio::use_awaitable));

    switch (res.index()) {
    case 0:
      co_await flush();
      break;
    case 1:
      for (auto& channel : channels_idx) {
        channel.second->update_stats();
      }
    case 2:
      if (!(co_await send_ping()).has_error()) {
        co_await flush();
      }
      break;
    case 3:
      if (!(co_await send_pong()).has_error()) {
        co_await flush();
      }
      break;
    case 4:
    case 5:
      co_return;
    case 6:
      break;
    case 7:
      auto send_packet_res = co_await send_some_packet_msgs(done);
      if (!send_packet_res.has_error()) {
        auto eof = send_packet_res.value();
        if (!eof) {
          co_await send_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
        }
      }
      break;
    };
  }
}

auto MConnection::send_ping() -> asio::awaitable<Result<void>> {
  Packet packet{};
  PacketPing ping{};
  packet.set_allocated_packet_ping(&ping);
  auto bytes = detail::packet_to_bytes(packet);
  auto res = co_await conn.write(boost::asio::buffer(bytes.data(), bytes.size()));
  if (res.has_error()) {
    co_return res.error();
  } else {
    co_return success();
  }
}

auto MConnection::send_pong() -> asio::awaitable<Result<void>> {
  Packet packet{};
  PacketPong pong{};
  packet.set_allocated_packet_pong(&pong);
  auto bytes = detail::packet_to_bytes(packet);
  auto res = co_await conn.write(boost::asio::buffer(bytes.data(), bytes.size()));
  if (res.has_error()) {
    co_return res.error();
  } else {
    co_return success();
  }
}

auto MConnection::send_some_packet_msgs(Chan<Done>& done) -> asio::awaitable<Result<bool>> {
  for (auto i = 0; i < num_batch_packet_msgs; i++) {
    auto res = co_await send_packet_msg(done);
    if (!res.has_error() && res.value()) {
      co_return true;
    }
  }
  co_return false;
}

auto MConnection::send_packet_msg(Chan<Done>& done) -> asio::awaitable<Result<bool>> {
  auto least_ratio = std::numeric_limits<float_t>::max();
  detail::ChannelUptr* least_channel;
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
    stop_for_error(done);
  }
  flush_timer.set();
  co_return false;
}

void MConnection::stop_for_error(Chan<Done>& done) {}

auto MConnection::stop() -> boost::asio::awaitable<Result<bool>> {
  std::scoped_lock _{stop_mtx};
  quit_send_routine_ch.close();
}

auto MConnection::stop_services() -> asio::awaitable<Result<bool>> {}

//auto MConnection::recv_routine(Chan<Done>& done) -> asio::awaitable<void>;

} //namespace tendermint::p2p::conn
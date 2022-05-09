// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <noir/common/buffered_writer.h>
#include <noir/common/ticker.h>
#include <noir/common/types.h>
#include <tendermint/p2p/conn/connection.h>

namespace tendermint::p2p::conn {

namespace detail {
  auto Channel::send_bytes(BytesPtr bytes) -> asio::awaitable<Result<bool>> {
    asio::steady_timer timer{io_context};
    timer.expires_after(std::chrono::seconds(10));
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
    Packet msg{};
    PacketMsg packet = next_packet_msg();
    msg.set_allocated_packet_msg(&packet);

    VarInt64 message_length = msg.ByteSizeLong();
    codec::datastream<size_t> ss(0);
    auto message_header_size = write_uleb128(ss, message_length).value();
    auto total_bytes = message_length + message_header_size;
    std::vector<char> buf(total_bytes);
    codec::datastream<char> ds(buf);
    write_uleb128(ds, message_length).value();
    msg.SerializeToArray(ds.ptr(), message_length);
    auto res = co_await w.write(buf);
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
  std::scoped_lock g{last_msg_recv.mtx};
  last_msg_recv.at = t;
}

auto MConnection::get_last_message_at() -> Time {
  std::scoped_lock g{last_msg_recv.mtx};
  return last_msg_recv.at;
}

//auto MConnection::stop_services() -> asio::awaitable<Result<bool>>;
//
//void MConnection::stop();

auto MConnection::string() -> std::string {
  return fmt::format("MConn{{}}", conn.address);
}

auto MConnection::send(ChannelId ch_id, BytesPtr msg_bytes) -> asio::awaitable<Result<bool>> {
  detail::ChannelPtr channel = channels_idx[ch_id];
  if (!channel) {
    co_return false;
  }

  auto success = (co_await channel->send_bytes(msg_bytes)).value();
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
    // TODO: flush timer
    auto res = co_await (ch_stats_timer.time_ch.async_receive(as_result(asio::use_awaitable)) ||
      ping_timer.time_ch.async_receive(as_result(asio::use_awaitable)) ||
      pong_ch.async_receive(as_result(asio::use_awaitable)) || done.async_receive(as_result(asio::use_awaitable)) ||
      quit_send_ch.async_receive(as_result(asio::use_awaitable)) ||
      pong_timeout.time_ch.async_receive(as_result(asio::use_awaitable)) ||
      send_ch.async_receive(as_result(asio::use_awaitable)));

    switch (res.index()) {
    case 0: // flush_timer
      // flush();
      break;
    case 1: // ch_stats_timer
      for (auto& channel : channels) {
        channel->update_stats();
      }
      break;
    case 2: // ping_timer
      break;
    case 3: // pong_ch
      break;
    case 4: // done
      break;
    case 5: // quit_send_ch
      break;
    case 6: // pong_timeout
      break;
    case 7: // send_ch
      auto eof = (co_await send_some_packet_msgs(done)).value();
      if (!eof) {
        co_await send_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
      }
      break;
    };
  }
}

auto MConnection::send_some_packet_msgs(Chan<Done>& done) -> asio::awaitable<Result<bool>> {
  for (auto i = 0; i < num_batch_packet_msgs; i++) {
    if ((co_await send_packet_msg(done)).value()) {
      co_return true;
    }
  }
  co_return false;
}

auto MConnection::send_packet_msg(Chan<Done>& done) -> asio::awaitable<Result<bool>> {
  auto least_ratio = std::numeric_limits<float_t>::max();
  detail::ChannelPtr least_channel;
  for (auto& channel : channels) {
    if (!channel->is_send_pending()) {
      continue;
    }
    auto ratio = static_cast<float_t>(channel->recently_sent) / static_cast<float_t>(channel->desc->priority);
    if (ratio < least_ratio) {
      least_ratio = ratio;
      least_channel = channel;
    }
  }

  if (!least_channel) {
    co_return true;
  }
  auto res = co_await least_channel->write_packet_msg_to(buf_conn_writer);
  if (res.has_error()) {
    stop_for_error(done);
  }
  // flush_timer();
  co_return false;
}

void MConnection::stop_for_error(Chan<Done>& done) {}

void MConnection::stop() {}

//auto MConnection::recv_routine(Chan<Done>& done) -> asio::awaitable<void>;
//
//void MConnection::max_packet_msg_size();

} //namespace tendermint::p2p::conn
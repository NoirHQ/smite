// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/datastream.h>
#include <noir/common/hex.h>
#include <noir/common/proto_readwrite.h>
#include <tendermint/p2p/conn/connection.h>
#include <eo/time.h>

namespace tendermint::p2p::conn {

using namespace noir;
using namespace std::chrono;

namespace detail {
  auto Channel::send_bytes(std::shared_ptr<std::vector<unsigned char>> bytes) -> asio::awaitable<Result<bool>> {
    auto timer = eo::time::new_timer(default_send_timeout);
    auto select = eo::Select{send_queue << bytes, *timer->c};

    switch (co_await select.index()) {
    case 0:
      co_await select.process<0>();
      co_return true;
    case 1:
      co_await select.process<1>();
      co_return false;
    }
    co_return false;
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
    if (res) {
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

auto MConnection::to_string() -> std::string {
  return fmt::format("MConn{{}}", conn->address);
}

auto MConnection::flush() -> boost::asio::awaitable<void> {
  co_await buf_conn_writer->flush();
}

auto MConnection::send(ChannelId ch_id, std::shared_ptr<std::vector<unsigned char>> msg_bytes)
  -> asio::awaitable<Result<bool>> {
  dlog("Send", "channel", ch_id, "conn", to_string(), "msg_bytes", hex::encode(*msg_bytes));

  if (!channels_idx.contains(ch_id)) {
    elog(fmt::format("Cannot send bytes, unknown channel {}", ch_id));
    co_return false;
  }
  auto res = co_await channels_idx[ch_id]->send_bytes(msg_bytes);
  if (res) {
    co_return res.error();
  }
  auto success = res.value();
  if (success) {
    boost::system::error_code ec{};
    co_await send_ch.async_send(ec, {}, asio::use_awaitable);
    if (ec) {
      co_return ec;
    }
  } else {
    dlog("Send failed", "channel", ch_id, "conn", to_string(), "msg_bytes", hex::encode(*msg_bytes));
  }
  co_return success;
}

auto MConnection::send_routine(eo::chan<std::monostate>& done) -> asio::awaitable<Result<void>> {
  Ticker pong_timeout{io_context, config.pong_timeout};
  pong_timeout.start();

  for (auto loop = true; loop;) {
    Error err{};

    auto select = eo::Select{*flush_timer.event_ch, *ch_stats_timer.time_ch, *ping_timer.time_ch, *pong_ch, *done,
      *quit_send_routine_ch, *pong_timeout.time_ch, *send_ch};

    switch (co_await select.index()) {
    case 0:
      co_await select.process<0>();
      co_await flush();
      break;
    case 1:
      co_await select.process<1>();
      for (auto& [_, channel] : channels_idx) {
        channel->update_stats();
      }
      break;
    case 2: {
      dlog("Send Ping");
      co_await select.process<2>();
      auto ping_res = co_await send_ping();
      if (!ping_res) {
        err = ping_res.error();
        elog("Failed to send PacketPing", "err", err);
      } else {
        co_await flush();
      }
      break;
    }
    case 3: {
      dlog("Send Pong");
      co_await select.process<3>();
      auto pong_res = co_await send_pong();
      if (!pong_res) {
        err = pong_res.error();
        elog("Failed to send PacketPong", "err", err);
      } else {
        co_await flush();
      }
      break;
    }
    case 4:
      co_await select.process<4>();
      loop = false;
      break;
    case 5:
      co_await select.process<5>();
      loop = false;
      break;
    case 6:
      co_await select.process<6>();
      break;
    case 7: {
      co_await select.process<7>();
      auto msg_res = co_await send_some_packet_msgs(done);
      if (msg_res) {
        auto eof = msg_res.value();
        if (!eof) {
          co_await send_ch.async_send(boost::system::error_code{}, {}, asio::use_awaitable);
        }
      }
      break;
    }
    }
    if (duration_cast<milliseconds>(system_clock::now() - system_clock::from_time_t(get_last_message_at())).count() >
      config.pong_timeout.count()) {
      err = Error("pong timeout");
    }

    if (err) {
      elog("Connection failed @ send_routine", "conn", to_string(), "err", err);
      co_await stop_for_error(err);
      loop = false;
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
  if (!res) {
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
  if (!res) {
    co_return res.error();
  } else {
    co_return success();
  }
}

auto MConnection::send_some_packet_msgs(eo::chan<std::monostate>& done) -> asio::awaitable<Result<bool>> {
  for (auto i = 0; i < num_batch_packet_msgs; i++) {
    auto res = co_await send_packet_msg(done);
    if (res && res.value()) {
      co_return true;
    }
  }
  co_return false;
}

auto MConnection::send_packet_msg(eo::chan<std::monostate>& done) -> asio::awaitable<Result<bool>> {
  auto least_ratio = std::numeric_limits<float_t>::max();
  detail::ChannelUptr* least_channel;
  for (auto& [_, channel] : channels_idx) {
    if (!channel->is_send_pending()) {
      continue;
    }
    auto ratio = static_cast<float_t>(channel->recently_sent) / static_cast<float_t>(channel->desc->priority);
    if (ratio < least_ratio) {
      least_ratio = ratio;
      least_channel = &channel;
    }
  }

  if (!least_channel) {
    co_return true;
  }
  auto res = co_await (*least_channel)->write_packet_msg_to(buf_conn_writer);
  if (!res) {
    elog("Failed to write PacketMsg", "err", res.error());
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
  for (auto loop = true; loop;) {
    auto select0 = eo::Select{*done, *done_send_routine_ch, eo::CaseDefault()};
    switch (co_await select0.index()) {
    case 0:
      co_await select0.process<0>();
      loop = false;
      break;
    case 1:
      co_await select0.process<1>();
      loop = false;
      break;
    default:
      break;
    }

    Packet packet{};
    auto res = co_await read_packet(packet);
    if (!res) {
      auto select1 = eo::Select{*done, *quit_recv_routine_ch, eo::CaseDefault()};
      switch (co_await select1.index()) {
      case 0:
        co_await select1.process<0>();
        break;
      case 1:
        co_await select1.process<1>();
        loop = false;
        break;
      default:
        break;
      }
      auto err = res.error();
      if (err == err_eof) {
        ilog("Connection is closed @ recv_routine (likely by the other side)", "conn", to_string());
      } else {
        dlog("Connection failed @ recv_routine (reading byte)", "conn", to_string(), "err", err);
      }
      co_await stop_for_error(err);
    }

    set_recv_last_msg_at(system_clock::to_time_t(system_clock::now()));

    if (packet.sum_case() == Packet::kPacketPing) {
      auto select2 = eo::Select{pong_ch << std::monostate{}, eo::CaseDefault()};
      switch (co_await select2.index()) {
      case 0:
        co_await select2.process<0>();
        break;
      default:
        break;
      }
    } else if (packet.sum_case() == Packet::kPacketPong) {
    } else if (packet.sum_case() == Packet::kPacketMsg) {
      auto channel_id = static_cast<uint16_t>(packet.packet_msg().channel_id());
      auto& channel = channels_idx[channel_id];
      if (packet.packet_msg().channel_id() < 0 ||
        packet.packet_msg().channel_id() > std::numeric_limits<uint8_t>::max() || channels_idx.contains(channel_id) ||
        channel == nullptr) {
        auto pkt_err = Error::format("unknown channel {}", packet.packet_msg().channel_id());
        dlog("Connection failed @ recv_routine", "conn", to_string(), "err", Error::fmt("unknown channel {}", pkt_err));
        co_await stop_for_error(pkt_err);
        loop = false;
        break;
      }
      auto msg_res = channel->recv_packet_msg(packet.packet_msg());
      if (!msg_res) {
        dlog("Connection failed @ recv_routine", "conn", to_string(), "err", msg_res.error());
        co_await stop_for_error(msg_res.error());
        loop = false;
        break;
      }
      if (msg_res && !msg_res.value().empty()) {
        auto msg_bytes = std::make_shared<std::vector<unsigned char>>(msg_res.value());
        dlog("Received bytes", "ch_id", channel_id, "msg_bytes", hex::encode(msg_bytes));
        co_await on_receive(channel_id, msg_bytes);
      }
    } else {
      auto sum_err = Error::format("unknown message type {}", packet.sum_case());
      elog("Connection failed @ recvRoutine", "conn", to_string(), "err", sum_err);
      co_await stop_for_error(sum_err);
      loop = false;
    }
  }
  pong_ch.close();

  co_return success();
}

auto MConnection::read_packet(Packet& packet) -> asio::awaitable<Result<void>> {
  Varint64 message_length = 0;
  auto length_res = co_await read_uleb128_async(*conn, message_length);
  if (!length_res) {
    co_return length_res.error();
  }
  std::vector<unsigned char> message_buffer(message_length);
  auto msg_res = co_await conn->read(message_buffer);
  if (!msg_res) {
    co_return msg_res.error();
  }
  packet.ParseFromArray(message_buffer.data(), message_buffer.size());
  co_return success();
}

} // namespace tendermint::p2p::conn

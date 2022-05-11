// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/buffered_writer.h>
#include <noir/common/throttle_timer.h>
#include <noir/common/ticker.h>
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <noir/net/conn.h>
#include <noir/net/tcp_conn.h>
#include <tendermint/core/core.h>
#include <tendermint/p2p/conn.pb.h>

namespace tendermint::p2p::conn {

const int num_batch_packet_msgs = 10;
const std::chrono::seconds update_stats_interval{2};

struct ChannelStatus {
  noir::ByteType id;
  int32_t send_queue_capacity;
  int32_t send_queue_size;
  int32_t priority;
  int64_t recently_sent;
};

using ChannelId = uint16_t;

class ChannelDescriptor {
public:
  ChannelId id;
  std::size_t send_queue_capacity;
  std::size_t recv_message_capacity;
  int32_t priority;
  // Message message_type;
  std::size_t recv_buffer_capacity;
  std::string name;
};

using ChannelDescriptorPtr = std::shared_ptr<ChannelDescriptor>;

using BytesPtr = std::shared_ptr<noir::Bytes>;

namespace detail {
  struct LastMsgRecv {
    std::mutex mtx;
    noir::Time at;
  };

  class Channel {
  public:
    Channel(asio::io_context& io_context, ChannelDescriptorPtr desc, std::size_t s)
      : io_context(io_context),
        desc(desc),
        send_queue(io_context, desc->send_queue_capacity),
        max_packet_msg_payload_size(s) {}

    auto send_bytes(BytesPtr bytes) -> asio::awaitable<Result<bool>>;
    auto is_send_pending() -> bool;
    auto write_packet_msg_to(noir::BufferedWriter<noir::net::Conn<noir::net::TcpConn>>& w)
      -> asio::awaitable<Result<std::size_t>>;
    auto next_packet_msg() -> PacketMsg;
    auto recv_packet_msg(PacketMsg packet) -> Result<noir::Bytes>;
    void update_stats();

  public:
    ChannelDescriptorPtr desc;
    std::atomic<int64_t> recently_sent{0};

  private:
    asio::io_context& io_context;

    Chan<BytesPtr> send_queue;
    std::atomic<int32_t> send_queue_size{0};
    noir::Bytes recving;
    noir::Bytes sending;

    std::size_t max_packet_msg_payload_size;
  };

  using ChannelUptr = std::unique_ptr<Channel>;
} // namespace detail

struct MConnConfig {
  std::size_t max_packet_msg_payload_size;
  std::chrono::seconds ping_interval;
  std::chrono::seconds pong_timeout;
  std::chrono::seconds flush_throttle;
};

class MConnection {
public:
  MConnection(asio::io_context& io_context,
    noir::net::Conn<noir::net::TcpConn>&& conn,
    std::vector<ChannelDescriptorPtr>& ch_descs,
    MConnConfig config)
    : io_context(io_context),
      conn(std::move(conn)),
      buf_conn_writer(this->conn),
      config(config),
      quit_send_routine_ch(io_context),
      done_send_routine_ch(io_context),
      quit_recv_routine_ch(io_context),
      flush_timer(io_context, config.flush_throttle),
      send_ch(io_context, 1),
      pong_ch(io_context, 1),
      ping_timer(io_context, config.ping_interval),
      ch_stats_timer(io_context, update_stats_interval) {
    for (auto& desc : ch_descs) {
      channels_idx[desc->id] = std::make_unique<detail::Channel>(io_context, desc, config.max_packet_msg_payload_size);
      this->max_packet_msg_size = calc_max_packet_msg_size(config.max_packet_msg_payload_size);
    }
  }

  void start(Chan<noir::Done>& done);
  void set_recv_last_msg_at(noir::Time&& t);
  auto get_last_message_at() -> noir::Time;
  auto stop_services() -> asio::awaitable<Result<bool>>;
  auto stop() -> boost::asio::awaitable<Result<bool>>;
  auto string() -> std::string;
  auto flush() -> asio::awaitable<void>;
  auto send(ChannelId ch_id, BytesPtr msg_bytes) -> asio::awaitable<Result<bool>>;
  auto send_routine(Chan<noir::Done>& done) -> asio::awaitable<void>;
  auto send_some_packet_msgs(Chan<noir::Done>& done) -> asio::awaitable<Result<bool>>;
  auto send_packet_msg(Chan<noir::Done>& done) -> asio::awaitable<Result<bool>>;
  auto recv_routine(Chan<noir::Done>& done) -> asio::awaitable<void>;
  void stop_for_error(Chan<noir::Done>& done);

  static auto calc_max_packet_msg_size(std::size_t max_packet_msg_payload_size) -> const std::size_t {
    PacketMsg msg{};
    msg.set_channel_id(1);
    msg.set_eof(true);
    std::string s;
    s.resize(max_packet_msg_payload_size);
    msg.set_allocated_data(&s);
    return msg.ByteSizeLong();
  }

private:
  auto send_ping() -> asio::awaitable<Result<void>>;
  auto send_pong() -> asio::awaitable<Result<void>>;

private:
  asio::io_context& io_context;
  detail::LastMsgRecv last_msg_recv;
  noir::net::Conn<noir::net::TcpConn> conn;
  noir::BufferedWriter<noir::net::Conn<noir::net::TcpConn>> buf_conn_writer;
  std::mutex stop_mtx;
  MConnConfig config;

  Chan<noir::Done> quit_send_routine_ch;
  Chan<noir::Done> done_send_routine_ch;
  Chan<noir::Done> quit_recv_routine_ch;

  std::map<ChannelId, detail::ChannelUptr> channels_idx;

  Chan<noir::Done> send_ch;
  Chan<noir::Done> pong_ch;

  noir::ThrottleTimer flush_timer;
  noir::Ticker ping_timer;
  noir::Ticker ch_stats_timer;

  noir::Time created;
  std::size_t max_packet_msg_size;
};

} // namespace tendermint::p2p::conn

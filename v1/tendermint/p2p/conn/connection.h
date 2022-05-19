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

const std::size_t default_send_queue_capacity{1};
const std::size_t default_recv_buffer_capacity{4096};
const std::size_t default_recv_message_capacity{22020096}; // 21MB
const std::size_t num_batch_packet_msgs{10};
const std::size_t default_max_packet_msg_payload_size{1400};
const std::chrono::milliseconds default_flush_throttle{100};
const std::chrono::milliseconds update_stats_interval{2000};
const std::chrono::milliseconds default_send_timeout{10000};
const std::chrono::milliseconds default_ping_interval{60000};
const std::chrono::milliseconds default_pong_timeout{90000};

using ChannelId = uint16_t;

class ChannelDescriptor {
public:
  ChannelDescriptor(ChannelId id,
    int32_t priority,
    std::string name,
    std::size_t send_queue_capacity = default_send_queue_capacity,
    std::size_t recv_message_capacity = default_recv_message_capacity,
    std::size_t recv_buffer_capacity = default_recv_buffer_capacity)
    : id(id),
      priority(priority),
      name(name),
      send_queue_capacity(send_queue_capacity),
      recv_message_capacity(recv_message_capacity),
      recv_buffer_capacity(recv_buffer_capacity) {}

public:
  ChannelId id;
  int32_t priority;
  std::string name;
  std::size_t send_queue_capacity;
  std::size_t recv_message_capacity;
  // Message message_type;
  std::size_t recv_buffer_capacity;
};

using ChannelDescriptorPtr = std::shared_ptr<ChannelDescriptor>;

using BytesPtr = std::shared_ptr<noir::Bytes>;

namespace detail {
  auto serialize_packet(Packet& msg) -> noir::Bytes;

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
    void set_next_packet_msg(PacketMsg* msg);
    auto recv_packet_msg(PacketMsg packet) -> Result<noir::Bytes>;
    void update_stats();

  public:
    ChannelDescriptorPtr desc;
    std::atomic<int64_t> recently_sent{0};

  private:
    asio::io_context& io_context;

    Chan<BytesPtr> send_queue;
    noir::Bytes recving;
    BytesPtr sending;
    std::size_t sent_pos;

    std::size_t max_packet_msg_payload_size;
  };

  using ChannelUptr = std::unique_ptr<Channel>;
} // namespace detail

class MConnConfig {
public:
  MConnConfig(std::size_t max_packet_msg_payload_size = default_max_packet_msg_payload_size,
    std::chrono::milliseconds ping_interval = default_ping_interval,
    std::chrono::milliseconds pong_timeout = default_pong_timeout,
    std::chrono::milliseconds flush_throttle = default_flush_throttle)
    : max_packet_msg_payload_size(max_packet_msg_payload_size),
      ping_interval(ping_interval),
      pong_timeout(pong_timeout),
      flush_throttle(flush_throttle) {}

  std::size_t max_packet_msg_payload_size;
  std::chrono::milliseconds ping_interval;
  std::chrono::milliseconds pong_timeout;
  std::chrono::milliseconds flush_throttle;
};

class MConnection {
public:
  MConnection(asio::io_context& io_context,
    std::shared_ptr<noir::net::Conn<noir::net::TcpConn>> conn,
    std::vector<ChannelDescriptorPtr>& ch_descs,
    MConnConfig config = MConnConfig{})
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
    this->max_packet_msg_size = calc_max_packet_msg_size(config.max_packet_msg_payload_size);
    for (auto& desc : ch_descs) {
      channels_idx[desc->id] = std::make_unique<detail::Channel>(io_context, desc, config.max_packet_msg_payload_size);
    }
  }

  void start(Chan<noir::Done>& done);
  void set_recv_last_msg_at(noir::Time&& t);
  auto get_last_message_at() -> noir::Time;
  auto stop_services() -> bool;
  void stop();
  auto string() -> std::string;
  auto flush() -> asio::awaitable<void>;
  auto send(ChannelId ch_id, BytesPtr msg_bytes) -> asio::awaitable<Result<bool>>;
  auto send_routine(Chan<noir::Done>& done) -> asio::awaitable<void>;
  auto send_some_packet_msgs(Chan<noir::Done>& done) -> asio::awaitable<Result<bool>>;
  auto send_packet_msg(Chan<noir::Done>& done) -> asio::awaitable<Result<bool>>;
  auto recv_routine(Chan<noir::Done>& done) -> asio::awaitable<void>;
  void stop_for_error(Chan<noir::Done>& done, noir::Error err);

  static auto calc_max_packet_msg_size(std::size_t max_packet_msg_payload_size) -> const std::size_t {
    PacketMsg msg{};
    msg.set_channel_id(1);
    msg.set_eof(true);
    msg.set_data(std::string(max_packet_msg_payload_size, ' '));
    return msg.ByteSizeLong();
  }

private:
  auto send_ping() -> asio::awaitable<Result<void>>;
  auto send_pong() -> asio::awaitable<Result<void>>;

private:
  asio::io_context& io_context;
  detail::LastMsgRecv last_msg_recv;
  std::shared_ptr<noir::net::Conn<noir::net::TcpConn>> conn;
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

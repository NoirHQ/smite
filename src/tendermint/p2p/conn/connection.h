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
#include <noir/net/tcp_conn.h>
#include <tendermint/p2p/conn.pb.h>
#include <tendermint/p2p/conn/secret_connection.h>

#include <utility>

namespace tendermint::p2p::conn {

using ChannelId = uint16_t;

class ChannelDescriptor {
public:
  ChannelDescriptor(ChannelId id,
    int32_t priority,
    std::string name,
    std::shared_ptr<google::protobuf::Message>& message_type,
    std::size_t send_queue_capacity = default_send_queue_capacity,
    std::size_t recv_message_capacity = default_recv_message_capacity,
    std::size_t recv_buffer_capacity = default_recv_buffer_capacity)
    : id(id),
      priority(priority),
      name(std::move(name)),
      message_type(message_type),
      send_queue_capacity(send_queue_capacity),
      recv_message_capacity(recv_message_capacity),
      recv_buffer_capacity(recv_buffer_capacity) {}

public:
  ChannelId id;
  int32_t priority;
  std::string name;
  std::size_t send_queue_capacity;
  std::size_t recv_message_capacity;
  std::shared_ptr<google::protobuf::Message> message_type;
  std::size_t recv_buffer_capacity;

private:
  const static std::size_t default_send_queue_capacity = 1;
  const static std::size_t default_recv_buffer_capacity = 4096;
  const static std::size_t default_recv_message_capacity = 22020096; // 21MB
};

namespace detail {
  struct LastMsgRecv {
    std::mutex mtx;
    std::time_t at;
  };

  class Channel {
  public:
    Channel(boost::asio::io_context& io_context, const std::shared_ptr<conn::ChannelDescriptor>& desc, std::size_t s)
      : io_context(io_context),
        desc(desc),
        send_queue(io_context, desc->send_queue_capacity),
        max_packet_msg_payload_size(s) {}

    auto send_bytes(std::shared_ptr<std::vector<unsigned char>> bytes) -> boost::asio::awaitable<noir::Result<bool>>;
    auto is_send_pending() -> bool;
    auto write_packet_msg_to(noir::BufferedWriterUptr<noir::net::Conn<noir::net::TcpConn>>& w)
      -> boost::asio::awaitable<noir::Result<std::size_t>>;
    void set_next_packet_msg(PacketMsg* msg);
    auto recv_packet_msg(const PacketMsg& packet) -> noir::Result<std::vector<unsigned char>>;
    void update_stats();

  public:
    std::shared_ptr<conn::ChannelDescriptor> desc;
    std::atomic<int64_t> recently_sent{0};

  private:
    boost::asio::io_context& io_context;

    eo::chan<std::shared_ptr<std::vector<unsigned char>>> send_queue;
    std::vector<unsigned char> recving;
    std::shared_ptr<std::vector<unsigned char>> sending;
    std::size_t sent_pos{0};

    std::size_t max_packet_msg_payload_size;

    constexpr static std::chrono::milliseconds default_send_timeout{10000};
  };

  using ChannelUptr = std::unique_ptr<Channel>;
} // namespace detail

class MConnConfig {
public:
  explicit MConnConfig(std::size_t max_packet_msg_payload_size = default_max_packet_msg_payload_size,
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

private:
  const static std::size_t default_max_packet_msg_payload_size = 1400;
  constexpr static std::chrono::milliseconds default_flush_throttle{100};
  constexpr static std::chrono::milliseconds default_ping_interval{60000};
  constexpr static std::chrono::milliseconds default_pong_timeout{90000};
};

class MConnection {
public:
  static auto new_mconnection_with_config(boost::asio::io_context& io_context,
    const std::shared_ptr<SecretConnection>& conn,
    std::vector<std::shared_ptr<conn::ChannelDescriptor>>& ch_descs,
    std::function<boost::asio::awaitable<void>(
      ChannelId channel_id, std::shared_ptr<std::vector<unsigned char>> payload)>&& on_receive,
    std::function<boost::asio::awaitable<void>(noir::Error error)>&& on_error,
    MConnConfig config = MConnConfig{}) -> std::shared_ptr<MConnection> {
    return std::shared_ptr<MConnection>(
      new MConnection(io_context, conn, ch_descs, std::move(on_receive), std::move(on_error), config));
  }

  void start(eo::chan<std::monostate>& done);
  void set_recv_last_msg_at(const std::time_t& t);
  auto get_last_message_at() -> std::time_t;
  auto stop_services() -> bool;
  void stop();
  auto to_string() -> std::string;
  auto flush() -> boost::asio::awaitable<void>;
  auto send(ChannelId ch_id, std::shared_ptr<std::vector<unsigned char>> msg_bytes)
    -> boost::asio::awaitable<noir::Result<bool>>;
  auto send_routine(eo::chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<void>>;
  auto send_some_packet_msgs(eo::chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<bool>>;
  auto send_packet_msg(eo::chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<bool>>;
  auto recv_routine(eo::chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<void>>;
  auto stop_for_error(const noir::Error& err) -> boost::asio::awaitable<void>;

  static auto calc_max_packet_msg_size(std::size_t max_packet_msg_payload_size) -> std::size_t {
    PacketMsg msg{};
    msg.set_channel_id(1);
    msg.set_eof(true);
    msg.set_data(std::string(max_packet_msg_payload_size, ' '));
    return msg.ByteSizeLong();
  }

private:
  MConnection(boost::asio::io_context& io_context,
    const std::shared_ptr<SecretConnection>& conn,
    std::vector<std::shared_ptr<conn::ChannelDescriptor>>& ch_descs,
    std::function<boost::asio::awaitable<void>(
      ChannelId channel_id, std::shared_ptr<std::vector<unsigned char>> payload)>&& on_receive,
    std::function<boost::asio::awaitable<void>(noir::Error error)>&& on_error,
    MConnConfig config)
    : io_context(io_context),
      config(config),
      quit_send_routine_ch(io_context),
      done_send_routine_ch(io_context),
      quit_recv_routine_ch(io_context),
      flush_timer(io_context, config.flush_throttle),
      send_ch(io_context, 1),
      pong_ch(io_context, 1),
      ping_timer(io_context, config.ping_interval),
      ch_stats_timer(io_context, update_stats_interval),
      on_receive(std::move(on_receive)),
      on_error(std::move(on_error)) {
    max_packet_msg_size = calc_max_packet_msg_size(config.max_packet_msg_payload_size);
    for (auto& desc : ch_descs) {
      channels_idx[desc->id] = std::make_unique<detail::Channel>(io_context, desc, config.max_packet_msg_payload_size);
    }
  }

  auto send_ping() -> boost::asio::awaitable<noir::Result<void>>;
  auto send_pong() -> boost::asio::awaitable<noir::Result<void>>;
  auto read_packet(Packet& packet) -> boost::asio::awaitable<noir::Result<void>>;

private:
  boost::asio::io_context& io_context;
  detail::LastMsgRecv last_msg_recv;
  std::shared_ptr<noir::net::Conn<noir::net::TcpConn>> conn;
  noir::BufferedWriterUptr<noir::net::Conn<noir::net::TcpConn>> buf_conn_writer;
  std::mutex stop_mtx;
  MConnConfig config;

  eo::chan<std::monostate> quit_send_routine_ch;
  eo::chan<std::monostate> done_send_routine_ch;
  eo::chan<std::monostate> quit_recv_routine_ch;

  std::map<ChannelId, detail::ChannelUptr> channels_idx;

  eo::chan<std::monostate> send_ch;
  eo::chan<std::monostate> pong_ch;

  noir::ThrottleTimer flush_timer;
  noir::Ticker ping_timer;
  noir::Ticker ch_stats_timer;

  std::time_t created;
  std::size_t max_packet_msg_size;

  std::function<boost::asio::awaitable<void>(
    ChannelId channel_id, std::shared_ptr<std::vector<unsigned char>> msg_bytes)>
    on_receive;
  std::function<boost::asio::awaitable<void>(noir::Error error)> on_error;

  const std::size_t num_batch_packet_msgs = 10;
  constexpr static std::chrono::milliseconds update_stats_interval{2000};
};

} // namespace tendermint::p2p::conn

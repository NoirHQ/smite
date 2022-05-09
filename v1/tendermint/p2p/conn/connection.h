// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/buffered_writer.h>
#include <noir/common/ticker.h>
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <noir/net/conn.h>
#include <noir/net/tcp_conn.h>
#include <tendermint/core/core.h>
#include <tendermint/p2p/conn.pb.h>
#include <cstdint>

namespace tendermint::p2p::conn {

const int num_batch_packet_msgs = 10;
const std::chrono::seconds update_stats_interval{2};

struct ChannelStatus {
  ByteType id;
  int32_t send_queue_capacity;
  int32_t send_queue_size;
  int32_t priority;
  int64_t recently_sent;
};

using ChannelId = uint16_t;

struct ChannelDescriptor {
  ChannelId id;
  int32_t priority;
  // Message message_type;
  std::size_t send_queue_capacity;
  std::size_t recv_message_capacity;
  std::size_t recv_buffer_capacity;
  std::string name;
};

using ChannelDescriptorPtr = std::shared_ptr<ChannelDescriptor>;

using BytesPtr = std::shared_ptr<Bytes>;

namespace detail {
  struct LastMsgRecv {
    std::mutex mtx;
    Time at;
  };

  class Channel {
  public:
    Channel(asio::io_context& io_context, ChannelDescriptorPtr desc, std::size_t s)
      : io_context(io_context),
        desc(desc),
        send_queue({io_context, desc->send_queue_capacity}),
        max_packet_msg_payload_size(s) {}

    auto send_bytes(BytesPtr bytes) -> asio::awaitable<Result<bool>>;
    auto is_send_pending() -> bool;
    auto write_packet_msg_to(BufferedWriter<net::Conn<net::TcpConn>>& w) -> asio::awaitable<Result<std::size_t>>;
    auto next_packet_msg() -> PacketMsg;
    auto recv_packet_msg(PacketMsg packet) -> Result<Bytes>;
    void update_stats();

  public:
    ChannelDescriptorPtr desc;
    std::atomic<int64_t> recently_sent{0};

  private:
    asio::io_context& io_context;

    Chan<BytesPtr> send_queue;
    std::atomic<int32_t> send_queue_size{0};
    Bytes recving;
    Bytes sending;

    std::size_t max_packet_msg_payload_size;
  };

  using ChannelPtr = std::shared_ptr<Channel>;
} // namespace detail

struct MConnConfig {
  int32_t max_packet_msg_payload_size;
  std::chrono::seconds ping_interval;
  std::chrono::seconds pong_timeout;
};

struct Send {};
struct Pong {};
struct QuitSend {};
struct DoneSend {};
struct QuitRecv {};

class MConnection {
public:
  MConnection(asio::io_context& io_context, MConnConfig config, net::Conn<net::TcpConn>&& conn)
    : io_context(io_context),
      conn(std::move(conn)),
      buf_conn_writer(this->conn),
      config(config),
      quit_send_ch({io_context}),
      done_send_ch({io_context}),
      quit_recv_ch({io_context}),
      send_ch({io_context, 1}),
      pong_ch({io_context, 1}),
      ping_timer({io_context, config.ping_interval}),
      ch_stats_timer({io_context, update_stats_interval}) {}

  void start(Chan<Done>& done);
  void set_recv_last_msg_at(Time&& t);
  auto get_last_message_at() -> Time;
  auto stop_services() -> asio::awaitable<Result<bool>>;
  void stop();
  auto string() -> std::string;
  auto send(ChannelId ch_id, BytesPtr msg_bytes) -> asio::awaitable<Result<bool>>;
  auto send_routine(Chan<Done>& done) -> asio::awaitable<void>;
  auto send_some_packet_msgs(Chan<Done>& done) -> asio::awaitable<Result<bool>>;
  auto send_packet_msg(Chan<Done>& done) -> asio::awaitable<Result<bool>>;
  auto recv_routine(Chan<Done>& done) -> asio::awaitable<void>;
  void max_packet_msg_size();
  void stop_for_error(Chan<Done>& done);

private:
  asio::io_context& io_context;
  detail::LastMsgRecv last_msg_recv;
  net::Conn<net::TcpConn> conn;
  BufferedWriter<net::Conn<net::TcpConn>> buf_conn_writer;
  std::mutex stop_mtx;
  MConnConfig config;

  Chan<QuitSend> quit_send_ch;
  Chan<DoneSend> done_send_ch;
  Chan<QuitRecv> quit_recv_ch;

  std::vector<detail::ChannelPtr> channels;
  std::map<ChannelId, detail::ChannelPtr> channels_idx;

  Chan<Send> send_ch;
  Chan<Pong> pong_ch;

  Ticker ping_timer;
  Ticker ch_stats_timer;

  Time created;
  //  int32_t max_packet_msg_size;
};

} // namespace tendermint::p2p::conn

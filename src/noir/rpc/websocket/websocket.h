// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/rpc/rpc.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <map>

namespace noir::rpc {

using connection_hdl = websocketpp::connection_hdl;
using server_type = websocketpp::server<websocketpp::config::asio>;
using connection_ptr = websocketpp::server<websocketpp::config::asio>::connection_ptr;
using message_ptr = websocketpp::server<websocketpp::config::asio>::message_ptr;

using message_router = std::function<void(connection_ptr, message_ptr)>;
using message_sender = std::function<void(std::string)>;
using message_handler = std::function<void(std::string, message_sender)>;
using internal_message_handler = std::function<void(connection_ptr, message_ptr, message_sender)>;

class websocket : std::enable_shared_from_this<websocket> {
public:
  void add_message_handler(std::string path, message_handler& handler, int priority = appbase::priority::medium_low);
  internal_message_handler make_app_thread_message_handler(int priority, message_handler& handler);
  message_sender make_message_sender(int priority, connection_ptr conn_ptr);

  std::map<std::string, internal_message_handler> message_handlers;
};
} // namespace noir::rpc

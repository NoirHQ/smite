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

using message_sender = std::function<void(std::optional<fc::variant>)>;
using message_handler = std::function<void(std::string, message_sender)>;
using internal_message_handler =
  std::function<void(websocketpp::server<websocketpp::config::asio>::connection_ptr, std::string, message_sender)>;

class websocket {
public:
  void add_message_api(const std::string&, message_handler, int priority = appbase::priority::medium_low);
  void add_message_handler(const std::string&, message_handler&, int priority);
  static internal_message_handler make_app_thread_message_handler(message_handler, int priority);
  static message_sender make_message_sender(
    websocketpp::server<websocketpp::config::asio>::connection_ptr, int priority = appbase::priority::medium_low);

  std::map<std::string, internal_message_handler> message_handlers;
};
} // namespace noir::rpc

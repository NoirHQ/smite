// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/rpc/websocket/websocket.h>
#include <appbase/application.hpp>
#include <fc/io/json.hpp>

namespace noir::rpc {

using namespace std;
using namespace appbase;

using connection_ptr = websocketpp::server<websocketpp::config::asio>::connection_ptr;
using message_ptr = websocketpp::server<websocketpp::config::asio>::message_ptr;

void websocket::add_message_api(const std::string& path, message_handler handler, int priority) {
  add_message_handler(path, handler, priority);
}

void websocket::add_message_handler(const std::string& path, message_handler& handler, int priority) {
  message_handlers[path] = make_app_thread_message_handler(app, handler, priority);
}

internal_message_handler websocket::make_app_thread_message_handler(
  appbase::application& app, message_handler next, int priority) {
  auto next_ptr = std::make_shared<message_handler>(std::move(next));
  return [&app, priority, next_ptr = std::move(next_ptr)](
           connection_ptr conn, const string& url, const string& payload, message_sender then) {
    message_sender wrapped_then = [then = std::move(then)](std::optional<fc::variant> msg) { then(std::move(msg)); };

    app.post(
      priority, [next_ptr, conn = std::move(conn), url, payload, wrapped_then = std::move(wrapped_then)]() mutable {
        try {
          (*next_ptr)(url, payload, std::move(wrapped_then));
        } catch (...) {
          conn->send("Internal Server Error");
        }
      });
  };
}

message_sender websocket::make_message_sender(appbase::application& app, connection_ptr conn, int priority) {
  return [&app, priority, conn = std::move(conn)](std::optional<fc::variant> msg) {
    app.post(priority, [conn, msg]() {
      try {
        if (msg.has_value()) {
          std::string json = fc::json::to_string(msg.value(), fc::time_point::maximum());
          conn->send(json);
        } else {
          conn->send("{}");
        }
      } catch (...) {
        conn->send("Internal Server Error");
      }
    });
  };
}

void websocket::handle_message(
  websocketpp::server<websocketpp::config::asio>::connection_ptr conn, ws_server_type::message_ptr msg) {
  std::string resource = conn->get_uri()->get_resource();
  if (message_handlers.contains(resource)) {
    message_handlers[resource](conn, resource, msg->get_payload(), make_message_sender(app, conn));
  } else {
    conn->send("Unknown Endpoint");
  }
}

} // namespace noir::rpc

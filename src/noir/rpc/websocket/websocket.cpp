// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/rpc/websocket/websocket.h>
#include <appbase/application.hpp>

namespace noir::rpc {

using namespace std;
using namespace appbase;

void websocket::add_message_handler(std::string path, message_handler& handler, int priority) {
  message_handlers[path] = make_app_thread_message_handler(priority, handler);
}

internal_message_handler websocket::make_app_thread_message_handler(int priority, message_handler& handler) {
  auto next_ptr = std::make_shared<message_handler>(std::move(handler));
  return [my = shared_from_this(), priority, next_ptr = std::move(next_ptr)](
           connection_ptr conn_ptr, message_ptr msg_ptr, message_sender then) {
    message_sender wrapped_then = [then = std::move(then)](string msg) { then(msg); };

    app().post(priority,
      [my = std::move(my), next_ptr, c = std::move(conn_ptr), m = std::move(msg_ptr),
        wrapped_then = std::move(wrapped_then)]() mutable {
        try {
          (*next_ptr)(std::move(m->get_payload()), std::move(wrapped_then));
        } catch (...) {
          c->send("Internal Server Error");
        }
      });
  };
}

message_sender websocket::make_message_sender(int priority, connection_ptr conn_ptr) {
  return [my = shared_from_this(), priority, conn_ptr](string msg) {
    app().post(priority, [my = std::move(my), c = std::move(conn_ptr), msg]() {
      try {
        c->send(msg);
      } catch (...) {
        c->send("Internal Server Error");
      }
    });
  };
}
} // namespace noir::rpc

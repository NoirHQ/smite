// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/base64.h>
#include <tendermint/p2p/transport_mconn.h>
#include <iostream>

using namespace tendermint;
using namespace tendermint::p2p;
using namespace tendermint::p2p::conn;
using namespace boost::asio;
using namespace noir;

int main() {
  boost::asio::io_context io_context{};

  co_spawn(
    io_context,
    [&]() -> awaitable<void> {
      MConnConfig config{};
      MConnTransportOptions options = {.max_accepted_connections = 2};
      auto transport = MConnTransport::new_mconn_transport(io_context, config, options);

      auto dial_res = co_await transport->dial("127.0.0.1:26658");
      if (dial_res.has_error()) {
        std::cout << dial_res.error().message() << std::endl;
        co_return;
      }

      Chan<std::monostate> done{io_context, 1};
      auto conn = dial_res.value();
      if (conn) {
        std::cout << "connected" << std::endl;
      }

      auto priv_str =
        base64_decode("x1eX2WKe+mhZwO7PLVgLdMZ4Ucr4NfdBxMtD/59mOfmk8GO0T1p8YNpObegcTLZmqnK6ffVtjvWjDSSVgVwGAw==");
      Bytes priv_key(priv_str.begin(), priv_str.end());
      NodeInfo node_info{.node_id = "dial_node"};
      auto handshake_res = co_await conn->handshake(done, node_info, priv_key);
      if (handshake_res.has_error()) {
        std::cout << handshake_res.error().message() << std::endl;
        co_return;
      }

      auto peer = handshake_res.value();
      auto peer_info = std::get<0>(peer);
      std::cout << "peer id: " << peer_info.node_id << std::endl;
    },
    detached);

  io_context.run();
}

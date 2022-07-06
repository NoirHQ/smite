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

      auto listen_res = co_await transport->listen("127.0.0.1:26658");
      if (!listen_res) {
        std::cout << listen_res.error().message() << std::endl;
        co_return;
      }

      Chan<std::monostate> done{io_context, 1};
      auto conn_res = co_await transport->accept(done);
      if (!conn_res) {
        std::cout << conn_res.error().message() << std::endl;
        co_return;
      }

      auto priv_str =
        base64_decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
      Bytes priv_key(priv_str.begin(), priv_str.end());
      auto conn = conn_res.value();
      if (conn) {
        std::cout << "connected" << std::endl;
      }

      NodeInfo node_info{.node_id = "accept_node"};
      auto handshake_res = co_await conn->handshake(done, node_info, priv_key);
      if (!handshake_res) {
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
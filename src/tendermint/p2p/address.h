// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/types/node_id.h>
#include <eo/core.h>

namespace tendermint::p2p {
class NodeAddress {
public:
  auto resolve(eo::chan<std::monostate>& done) -> noir::Result<std::vector<std::string>> {
    if (protocol.empty()) {
      return noir::Error("address has no protocol");
    }
    // TODO: lookup ips
    auto endpoint = fmt::format("{}://{}:{}/{}", protocol, hostname, port, path);
    return std::vector<std::string>{endpoint};
  }

  auto to_string() -> std::string {
    return fmt::format("{}://{}:{}/{}", protocol, hostname, port, path);
  }

public:
  NodeId node_id;
  std::string protocol;
  std::string hostname;
  uint16_t port;
  std::string path;
};
} // namespace tendermint::p2p

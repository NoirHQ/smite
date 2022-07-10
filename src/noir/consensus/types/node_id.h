// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/hex.h>
#include <noir/core/result.h>
#include <regex>

namespace noir::consensus {
static constexpr uint64_t node_id_byte_length = 20; // crypto address size
static std::regex regex_node_id{"^[0-9a-f]{40}$"};

struct node_id {
  std::string id;

  std::string address_string(std::string protocol_host_port) {
    std::string delim = "://";
    auto pos = protocol_host_port.find(delim);
    if (pos != std::string::npos) {
      return id + protocol_host_port.substr(pos + delim.length(), protocol_host_port.length());
    }
    return id + protocol_host_port;
  }

  std::string node_id_from_pub_key(Bytes pub_key) {
    return hex::encode(pub_key);
  }

  Bytes get_bytes() {
    return hex::decode(id);
  }

  Result<void> validate() {
    switch (id.length()) {
    case 0:
      return Error::format("Empty node ID");

    case 2 * node_id_byte_length:
      if (std::regex_search(id, regex_node_id))
        return success();
      return Error::format("Node ID can only contain 40 lowercased hex digits");

    default:
      return Error::format("Invalid node ID length {}", id.length());
    }
  }

  bool operator==(const node_id& rhs) const {
    return id == rhs.id;
  }

  bool operator<(const node_id& rhs) const {
    return id < rhs.id;
  }
};

} // namespace noir::consensus

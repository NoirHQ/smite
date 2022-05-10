// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/common/hex.h>
#include <noir/consensus/common.h>
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

  std::string node_id_from_pub_key(bytes pub_key) {
    return to_hex(pub_key);
  }

  bytes get_bytes() {
    return from_hex(id);
  }

  bool validate() {
    switch (id.length()) {
    case 0:
      dlog("Empty node ID");
      return false;

    case 2 * node_id_byte_length:
      if (std::regex_search(id, regex_node_id)) {
        return true;
      }
      dlog("Node ID can only contain 40 lowercased hex digits");
      return false;

    default:
      dlog("Invalid node ID length ${len}", ("len", id.length()));
      return false;
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

#include <fmt/core.h>

namespace fmt {
template<>
struct formatter<noir::consensus::node_id> {
  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) {
    return ctx.begin();
  }
  template<typename FormatContext>
  auto format(noir::consensus::node_id const& v, FormatContext& ctx) {
    return format_to(ctx.out(), "{}", v.id);
  }
};

} // namespace fmt

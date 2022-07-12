// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <tendermint/p2p/types.pb.h>
#include <tendermint/types/node_id.h>
#include <numeric>

namespace tendermint::type {

struct ProtocolVersion {
  std::uint64_t p2p;
  std::uint64_t block;
  std::uint64_t app;
};

struct NodeInfoOther {
  std::string tx_index;
  std::string rpc_address;
};

class NodeInfo {
public:
  auto to_proto() -> p2p::NodeInfo {
    p2p::NodeInfo dni{};

    auto mpv = dni.mutable_protocol_version();
    mpv->set_p2p(protocol_version.p2p);
    mpv->set_app(protocol_version.app);
    mpv->set_block(protocol_version.block);

    dni.set_node_id(node_id);
    dni.set_listen_addr(listen_addr);
    dni.set_network(network);
    dni.set_version(version);
    dni.set_channels(std::string(channels.begin(), channels.end()));
    dni.set_moniker(moniker);

    auto mo = dni.mutable_other();
    mo->set_tx_index(other.tx_index);
    mo->set_rpc_address(other.rpc_address);

    return dni;
  }

  static auto from_proto(tendermint::p2p::NodeInfo& pb) -> NodeInfo {
    return NodeInfo{.protocol_version =
                      ProtocolVersion{
                        .p2p = pb.protocol_version().p2p(),
                        .block = pb.protocol_version().block(),
                        .app = pb.protocol_version().app(),
                      },
      .node_id = pb.node_id(),
      .listen_addr = pb.listen_addr(),
      .network = pb.network(),
      .version = pb.version(),
      .channels = std::vector<unsigned char>(pb.channels().begin(), pb.channels().end()),
      .moniker = pb.moniker(),
      .other = NodeInfoOther{.tx_index = pb.other().tx_index(), .rpc_address = pb.other().rpc_address()}};
  }

  void add_channels(const std::uint16_t& channel) {
    for (auto& ch : channels) {
      if (ch == static_cast<unsigned char>(channel)) {
        return;
      }
    }
    channels.push_back(channel);
  }

  auto validate() -> noir::Result<void> {
    // TODO
    return noir::success();
  }

  // compatible_with checks if two NodeInfo are compatible with each other.
  // CONTRACT: two nodes are compatible if the Block version and network match
  // and they have at least one channel in common.
  auto compatible_with(const NodeInfo& other) -> noir::Result<void> {
    if (protocol_version.block != other.protocol_version.block) {
      return noir::Error::format("peer is on a different Block version. Got {}, expected {}",
        other.protocol_version.block, protocol_version.block);
    }
    if (network != other.network) {
      return noir::Error::format("peer is on a different network. Got {}, expected {}", other.network, network);
    }

    if (channels.size() == 0) {
      return noir::success();
    }

    auto found = false;
    auto outer_loop = true;
    for (auto ch1 = channels.begin(); ch1 != channels.end() && outer_loop; ++ch1) {
      for (auto& ch2 : other.channels) {
        if (*ch1 == ch2) {
          found = true;
          outer_loop = false;
          break;
        }
      }
    }
    if (!found) {
      return noir::Error::format("peer has no common channels. Our channels: {} ; Peer channels: {}",
        hex::encode(channels), hex::encode(other.channels));
    }
    return noir::success();
  }

public:
  ProtocolVersion protocol_version;
  NodeId node_id;
  std::string listen_addr;
  std::string network;
  std::string version;
  std::vector<unsigned char> channels;
  std::string moniker;
  NodeInfoOther other;
};

} // namespace tendermint::type

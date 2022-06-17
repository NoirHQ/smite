// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <tendermint/p2p/types.pb.h>
#include <tendermint/types/node_id.h>
#include <numeric>

namespace tendermint {

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
  auto to_proto() -> p2p::DefaultNodeInfo {
    p2p::DefaultNodeInfo dni{};

    auto mpv = dni.mutable_protocol_version();
    mpv->set_p2p(protocol_version.p2p);
    mpv->set_app(protocol_version.app);
    mpv->set_block(protocol_version.block);

    dni.set_default_node_id(node_id);
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

public:
  ProtocolVersion protocol_version;
  NodeId node_id;
  std::string listen_addr;
  std::string network;
  std::string version;
  noir::Bytes channels;
  std::string moniker;
  NodeInfoOther other;
};

auto node_info_from_proto(p2p::DefaultNodeInfo& pb) -> NodeInfo {
  return NodeInfo{.protocol_version =
                    ProtocolVersion{
                      .p2p = pb.protocol_version().p2p(),
                      .block = pb.protocol_version().block(),
                      .app = pb.protocol_version().app(),
                    },
    .node_id = pb.default_node_id(),
    .listen_addr = pb.listen_addr(),
    .network = pb.network(),
    .version = pb.version(),
    .channels = noir::Bytes{pb.channels().begin(), pb.channels().end()},
    .moniker = pb.moniker(),
    .other = NodeInfoOther{.tx_index = pb.other().tx_index(), .rpc_address = pb.other().rpc_address()}};
}

} //namespace tendermint

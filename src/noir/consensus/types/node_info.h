// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/common/types.h>
#include <noir/consensus/types/node_id.h>
#include <tendermint/p2p/types.pb.h>

namespace noir::consensus {

struct protocol_version {
  uint64_t p2p;
  uint64_t block;
  uint64_t app;
};

struct node_info_other {
  std::string tx_index;
  std::string rpc_address;
};

struct node_info {
  noir::consensus::protocol_version protocol_version;
  noir::consensus::node_id node_id;
  std::string listen_addr;
  std::string network;
  std::string version;
  Bytes channels;
  std::string moniker;
  node_info_other other;

  Result<void> validate() {
    // TODO : implement
    return success();
  }

  static std::unique_ptr<::tendermint::p2p::NodeInfo> to_proto(const node_info& n) {
    auto ret = std::make_unique<::tendermint::p2p::NodeInfo>();
    auto pb_proto_version = ret->mutable_protocol_version();
    pb_proto_version->set_p2p(n.protocol_version.p2p);
    pb_proto_version->set_block(n.protocol_version.block);
    pb_proto_version->set_app(n.protocol_version.app);
    ret->set_node_id(n.node_id.id);
    ret->set_listen_addr(n.listen_addr);

    ret->set_network(n.network);
    ret->set_version(n.version);
    ret->set_channels({n.channels.begin(), n.channels.end()});
    ret->set_moniker(n.moniker);

    auto pb_other = ret->mutable_other();
    pb_other->set_tx_index(n.other.tx_index);
    pb_other->set_rpc_address(n.other.rpc_address);

    return ret;
  }
  static std::shared_ptr<node_info> from_proto(const ::tendermint::p2p::NodeInfo& pb) {
    auto ret = std::make_shared<node_info>();
    ret->protocol_version = {
      .p2p = pb.protocol_version().p2p(), .block = pb.protocol_version().block(), .app = pb.protocol_version().app()};
    ret->node_id = {.id = pb.node_id()};
    ret->listen_addr = pb.listen_addr();
    ret->network = pb.network();
    ret->version = pb.version();
    ret->channels = pb.channels();
    ret->moniker = pb.moniker();
    ret->other = {.tx_index = pb.other().tx_index(), .rpc_address = pb.other().rpc_address()};
    return ret;
  }
};

} // namespace noir::consensus

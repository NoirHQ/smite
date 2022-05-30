// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
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
private:
  ProtocolVersion protocol_version;
  NodeId node_id;
  std::string listen_addr;
  std::string network;
  std::string version;
  noir::Bytes channels;
  std::string moniker;
  NodeInfoOther other;
};

} //namespace tendermint

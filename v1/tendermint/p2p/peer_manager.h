// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/p2p/address.h>
#include <tendermint/p2p/types.h>
#include <tendermint/p2p/waker.h>
#include <tendermint/types/node_id.h>

namespace tendermint::p2p {

using PeerStatus = std::string;

const PeerStatus peer_status_up = "up";
const PeerStatus peer_status_down = "down";
const PeerStatus peer_status_good = "good";
const PeerStatus peer_status_bad = "bad";

struct PeerUpdate {
  NodeId node_id;
  PeerStatus status;
  ChannelIdSet channels;
};

class PeerManager {
public:
  auto has_max_peer_capacity() -> bool;
  void errored(const NodeId& peer_id, Error& error);
  void process_peer_event(noir::Chan<std::monostate>& done, PeerUpdate pu);
  auto accepted(const NodeId& peer_id) -> noir::Result<void>;
  void ready(noir::Chan<std::monostate>& done, const NodeId& peer_id, ChannelIdSet& channels);
  auto dial_next(noir::Chan<std::monostate>& done) -> noir::Result<std::shared_ptr<NodeAddress>>;
  auto evict_next(noir::Chan<std::monostate>& done) -> boost::asio::awaitable<noir::Result<NodeId>>;
  auto inactivate(const NodeId& peer_id) -> noir::Result<void>;
  auto dial_failed(noir::Chan<std::monostate>& done, const std::shared_ptr<NodeAddress>& address) -> noir::Result<void>;
  auto dialed(const std::shared_ptr<NodeAddress>& address) -> noir::Result<void>;

public:
  Waker dial_waker;
};
} //namespace tendermint::p2p
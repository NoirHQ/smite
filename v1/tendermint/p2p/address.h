// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/types/node_id.h>

namespace tendermint::p2p {
class NodeAddress {
public:
  auto resolve(noir::Chan<std::monostate>& done) -> noir::Result<std::vector<std::string>>;
  auto to_string() -> std::string;

public:
  NodeId node_id;
  std::string protocol;
  std::string hostname;
  uint16_t port;
  std::string path;
};
} //namespace tendermint::p2p

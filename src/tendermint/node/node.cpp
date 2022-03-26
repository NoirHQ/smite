// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/node/node.h>
#include <iostream>

namespace tendermint::node {

result<void> Node::on_start() noexcept {
  std::cout << "Starting Node" << std::endl;
  return {};
}

void Node::on_stop() noexcept {
  std::cout << "Stopping Node" << std::endl;
}

} // namespace tendermint::node

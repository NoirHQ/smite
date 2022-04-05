// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/node/node.h>
#include <iostream>

namespace tendermint::node {

Node::Node(appbase::application& app): app(app), proxy_app("127.0.0.1:26658") {
  name = "Node";
}

result<void> Node::on_start() noexcept {
  return {};
}

void Node::on_stop() noexcept {}

} // namespace tendermint::node

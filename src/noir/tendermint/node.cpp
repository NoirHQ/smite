// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/tendermint.h>

namespace noir::tendermint::node {

void start() {
  tm_node_start();
}

void stop() {
  tm_node_stop();
}

} // namespace noir::tendermint::node

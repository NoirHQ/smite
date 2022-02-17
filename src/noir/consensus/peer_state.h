// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/node_id.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

struct peer_state {
  node_id peer_id;

  std::mutex mtx;
  bool is_running;
  // peer_round_state prs; // todo
};

} // namespace noir::consensus

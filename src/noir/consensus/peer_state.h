// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/node_id.h>
#include <noir/consensus/types.h>

#include <utility>

namespace noir::consensus {

struct peer_state {
  node_id peer_id;

  std::mutex mtx;
  bool is_running{};
  peer_round_state prs;

  static std::shared_ptr<peer_state> new_peer_state(node_id peer_id_) {
    auto ret = std::make_shared<peer_state>();
    ret->peer_id = std::move(peer_id_);
    peer_round_state prs_;
    ret->prs = prs_;
    return ret;
  }
};

} // namespace noir::consensus

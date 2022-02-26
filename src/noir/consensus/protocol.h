// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/p2p/types.h>

namespace noir::consensus {

enum event_type {
  EventNewRoundStep = 1,
  EventValidBlock,
  EventVote
};

struct timeout_info {
  std::chrono::system_clock::duration duration_;
  int64_t height;
  int32_t round;
  p2p::round_step_type step;
};
using timeout_info_ptr = std::shared_ptr<timeout_info>;

} // namespace noir::consensus

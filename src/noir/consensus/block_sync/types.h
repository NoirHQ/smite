// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <cinttypes>

namespace noir::consensus {

struct block_request {
  int64_t height;
};

struct block_response {
  // std::shared_ptr<block> block_; TODO
};

struct status_request {};

struct status_response {
  int64_t height;
  int64_t base;
};

struct no_block_response {
  int64_t height;
};

} // namespace noir::consensus

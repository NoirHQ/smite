// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <noir/common/types/bytes.h>
#include <cinttypes>

namespace noir::consensus {

struct block_request {
  int64_t height;
};

struct block_response {
  bytes block_; // use serialized block (in order to avoid include conflicts)
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

NOIR_REFLECT(noir::consensus::block_request, height)
NOIR_REFLECT(noir::consensus::block_response, block_)
NOIR_REFLECT(noir::consensus::status_request, )
NOIR_REFLECT(noir::consensus::status_response, height, base)
NOIR_REFLECT(noir::consensus::no_block_response, height)

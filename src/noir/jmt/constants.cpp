// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/types/bytes.h>
#include <noir/jmt/types.h>

namespace noir::jmt {

bytes32 sparse_merkle_placeholder_hash(std::span{"SPARSE_MERKLE_PLACEHOLDER_HASH"}, false);

version pre_genesis_version = std::numeric_limits<uint64_t>::max();

size_t root_nibble_height = 64;

} // namespace noir::jmt

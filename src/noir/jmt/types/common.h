// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>
#include <noir/common/types/bytes_n.h>

namespace noir::jmt {

using version = uint64_t;

size_t common_prefix_bits_len(const bytes32& a, const bytes32& b);

extern bytes32 sparse_merkle_placeholder_hash;

} // namespace noir::jmt

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>
#include <noir/common/bytes.h>

namespace noir::jmt {

using version = uint64_t;

size_t common_prefix_bits_len(const Bytes32& a, const Bytes32& b);

extern Bytes32 sparse_merkle_placeholder_hash;

} // namespace noir::jmt

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <noir/common/bytes.h>
#include <noir/crypto/hash.h>

#include <bit>

namespace noir::consensus::merkle {

using bytes_list = std::vector<Bytes>;

Bytes get_empty_hash();

Bytes leaf_hash_opt(const Bytes& leaf);

Bytes inner_hash_opt(const Bytes& left, const Bytes& right);

size_t get_split_point(size_t length);

Bytes hash_from_bytes_list(const bytes_list& list);

Bytes compute_hash_from_aunts(int64_t index, int64_t total, Bytes leaf_hash, bytes_list inner_hashes);

} // namespace noir::consensus::merkle

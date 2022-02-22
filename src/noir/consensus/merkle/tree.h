// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <noir/common/types/bytes.h>
#include <noir/crypto/hash.h>

#include <bit>

namespace noir::consensus::merkle {

using namespace noir;

using bytes_list = std::vector<bytes>;

bytes get_empty_hash();

bytes leaf_hash_opt(const bytes& leaf);

bytes inner_hash_opt(const bytes& left, const bytes& right);

size_t get_split_point(size_t length);

bytes hash_from_bytes_list(const bytes_list& list);

bytes compute_hash_from_aunts(int64_t index, int64_t total, bytes leaf_hash, bytes_list inner_hashes);

} // namespace noir::consensus::merkle

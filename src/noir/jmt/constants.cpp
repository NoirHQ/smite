// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/types/bytes.h>
#include <string_view>

namespace noir::jmt {

const char smph[] = "SPARSE_MERKLE_PLACEHOLDER_HASH\x00\x00";
bytes32 sparse_merkle_placeholder_hash(smph, 32);

} // namespace noir::jmt

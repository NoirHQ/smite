// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/types/bytes.h>
#include <noir/jmt/types/common.h>

namespace noir::jmt {

size_t common_prefix_bits_len(const bytes32& a, const bytes32& b) {
  auto a_bits = a.to_bitset();
  auto b_bits = b.to_bitset();
  auto a_pos = 0;
  for (auto b_pos = 0; a_pos < 256 && b_pos < 256; ++a_pos, ++b_pos) {
    if (a_bits[a_pos] == b_bits[b_pos])
      break;
  }
  return a_pos;
}

} // namespace noir::jmt

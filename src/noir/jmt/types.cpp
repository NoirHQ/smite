// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/jmt/types/common.h>
#include <bitset>
#include <numeric>

namespace noir::jmt {

template<size_t N, typename T>
std::bitset<8 * N> to_bitset(const bytesN<N, T>& bytes) {
  std::bitset<8 * N> out;
  return std::accumulate(bytes.begin(), bytes.end(), out, [](auto out, const auto& v) {
    decltype(out) current(v);
    return (std::move(out) << 8) | current;
  });
}

size_t common_prefix_bits_len(const bytes32& a, const bytes32& b) {
  auto a_bits = to_bitset(a);
  auto b_bits = to_bitset(b);
  auto a_pos = 0;
  for (auto b_pos = 0; a_pos < 256 && b_pos < 256; ++a_pos, ++b_pos) {
    if (a_bits[a_pos] == b_bits[b_pos])
      break;
  }
  return a_pos;
}

} // namespace noir::jmt

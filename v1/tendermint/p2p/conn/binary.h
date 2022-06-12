// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <numeric>
#include <vector>

namespace tendermint {

class LittleEndian {
public:
  static void put_uint32(std::vector<unsigned char>& b, std::uint32_t v) {
    b[0] = static_cast<unsigned char>(v);
    b[1] = static_cast<unsigned char>(v >> 8);
    b[2] = static_cast<unsigned char>(v >> 16);
    b[3] = static_cast<unsigned char>(v >> 24);
  }

  static uint32_t uint32(std::vector<unsigned char>& b) {
    return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
      static_cast<uint32_t>(b[3]) << 24;
  }
};

} //namespace tendermint

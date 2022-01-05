// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

namespace noir {

template<typename T>
static constexpr inline T pown(T x, unsigned p) {
  T result = 1;
  while (p) {
    if (p & 0x1) {
      result *= x;
    }
    x *= x;
    p >>= 1;
  }
  return result;
}

} // namespace noir

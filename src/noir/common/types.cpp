// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/hex.h>
#include <noir/common/types/bytes.h>

namespace noir {

std::string to_string(const bytes_vec<char>& v) {
  return to_hex(std::span((const byte_type*)v.data(), v.size()));
}

std::string to_string(const bytes_vec<unsigned char>& v) {
  return to_hex(std::span((const byte_type*)v.data(), v.size()));
}

} // namespace noir

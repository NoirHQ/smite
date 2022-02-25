// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/types/bytes.h>

namespace noir {

template<>
std::string to_string(const bytes& v) {
  return to_hex(v);
}

} // namespace noir

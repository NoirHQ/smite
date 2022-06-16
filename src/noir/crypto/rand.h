// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/core/core.h>

namespace noir::crypto {

/// \brief generates random bytes
/// \ingroup crypto
Result<void> rand_bytes(std::span<unsigned char> out);

Result<void> rand_bytes(BytesViewConstructible auto& out) {
  return rand_bytes(bytes_view(out));
}

/// \brief generates random bytes using separated PRNG
/// \ingroup crypto
Result<void> rand_priv_bytes(std::span<unsigned char> out);

Result<void> rand_priv_bytes(BytesViewConstructible auto& out) {
  return rand_priv_bytes(bytes_view(out));
}

} // namespace noir::crypto

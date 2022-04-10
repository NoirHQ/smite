// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <noir/crypto/openssl/hash.h>

namespace noir::crypto {

/// \brief generates sha256 hash
/// \ingroup crypto
struct sha256 : public hash<sha256>, openssl::hash {
  using crypto::hash<sha256>::final;

  crypto::hash<sha256>& init();
  crypto::hash<sha256>& update(std::span<const char> in);
  void final(std::span<char> out);
  std::size_t digest_size() const;
};

} // namespace noir::crypto

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <noir/crypto/openssl/hash.h>

namespace noir::crypto {

/// \brief generates ripemd160 hash
/// \ingroup crypto
struct ripemd160 : public hash<ripemd160>, openssl::hash {
  using crypto::hash<ripemd160>::final;

  crypto::hash<ripemd160>& init();
  crypto::hash<ripemd160>& update(std::span<const char> in);
  void final(std::span<char> out);
  std::size_t digest_size() const;
};

} // namespace noir::crypto

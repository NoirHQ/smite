// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <noir/crypto/openssl/message_digest.h>

namespace noir::crypto {

/// \brief generates sha256 hash
/// \ingroup crypto
struct Sha256 : public Hash<Sha256>, openssl::MessageDigest {
  using Hash::final;
  using Hash::update;

  auto init() -> Sha256&;
  auto update(BytesView in) -> Sha256&;
  void final(BytesViewMut out);

  auto digest_size() const -> size_t;
};

} // namespace noir::crypto

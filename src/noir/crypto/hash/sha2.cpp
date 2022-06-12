// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/sha2.h>

namespace noir::crypto {

auto Sha256::init() -> Sha256& {
  MessageDigest::init(EVP_sha256());
  return *this;
}

auto Sha256::update(BytesView in) -> Sha256& {
  if (!ctx) {
    init();
  }
  MessageDigest::update(in);
  return *this;
}

void Sha256::final(BytesViewMut out) {
  MessageDigest::final(out);
}

auto Sha256::digest_size() const -> size_t {
  return MessageDigest::digest_size(EVP_sha256());
}

} // namespace noir::crypto

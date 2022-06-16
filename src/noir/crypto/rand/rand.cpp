// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/rand.h>
#include <openssl/rand.h>

namespace noir::crypto {

const auto err_rand_bytes = user_error_registry().register_error("failed to generate random bytes");

Result<void> rand_bytes(std::span<unsigned char> out) {
  if (!RAND_bytes(out.data(), out.size())) {
    return err_rand_bytes;
  }
  return success();
}

Result<void> rand_priv_bytes(std::span<unsigned char> out) {
  if (!RAND_priv_bytes(out.data(), out.size())) {
    return err_rand_bytes;
  }
  return success();
}

} // namespace noir::crypto

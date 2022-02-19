// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/crypto/hash.h>
#include <openssl/evp.h>
#include <blake2.h>

namespace noir::crypto {

namespace unsafe {
  void blake2b_256(std::span<const char> in, std::span<char> out) {
    blake2b((unsigned char*)out.data(), (unsigned char*)in.data(), nullptr, 32, in.size(), 0);
  }
} // namespace unsafe

/// \cond PRIVATE
struct blake2b_256::blake2b_256_impl : public hash {
  hash& init() override {
    blake2b_init(&state, digest_size());
    return *this;
  };

  hash& update(std::span<const char> in) override {
    blake2b_update(&state, (const uint8_t*)in.data(), in.size());
    return *this;
  }

  void final(std::span<char> out) override {
    blake2b_final(&state, (uint8_t*)out.data(), digest_size());
  }

  size_t digest_size() override {
    return 32;
  }

private:
  blake2b_state state;
};
/// \endcond

} // namespace noir::crypto

NOIR_CRYPTO_HASH(blake2b_256)

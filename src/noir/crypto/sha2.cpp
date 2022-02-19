// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/crypto/openssl.h>

namespace noir::crypto {

namespace unsafe {
  void sha256(std::span<const char> in, std::span<char> out) {
    EVP_Digest(in.data(), in.size(), (unsigned char*)out.data(), nullptr, EVP_sha256(), nullptr);
  }
} // namespace unsafe

/// \cond PRIVATE
struct sha256::sha256_impl : public openssl::hash {
  const EVP_MD* type() override {
    return EVP_sha256();
  }
};
/// \endcond

} // namespace noir::crypto

NOIR_CRYPTO_HASH(sha256);

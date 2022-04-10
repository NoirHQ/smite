// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <openssl/evp.h>
#include <span>

namespace noir::openssl {

/// \cond PRIVATE
struct hash {
  ~hash();

  void init(const EVP_MD* type);
  void update(std::span<const char> in);
  void final(std::span<char> out);
  size_t digest_size(const EVP_MD* type) const;

protected:
  EVP_MD_CTX* ctx = nullptr;
};
/// \endcond

} // namespace noir::openssl

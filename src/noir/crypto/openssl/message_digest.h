// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <openssl/evp.h>

namespace noir::openssl {

/// \cond PRIVATE
struct MessageDigest {
  ~MessageDigest();

  void init(const EVP_MD* type);
  void update(BytesView in);
  void final(BytesViewMut out);
  size_t digest_size(const EVP_MD* type) const;

protected:
  EVP_MD_CTX* ctx = nullptr;
};
/// \endcond

} // namespace noir::openssl

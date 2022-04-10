// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/openssl/openssl.h>
#include <openssl/opensslv.h>

#if (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#include <atomic>
#endif

namespace noir::openssl {

void init() {
#if (OPENSSL_VERSION_MAJOR >= 3)
  static std::atomic<bool> inited{false};
  if (!inited.load()) {
    OSSL_PROVIDER_load(nullptr, "legacy");
    inited.store(true);
  }
#endif
}

} // namespace noir::openssl

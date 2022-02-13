// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <openssl/opensslv.h>
#include <atomic>

#if (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#endif

namespace noir::openssl {

std::atomic<bool> inited;

void init() {
  if (!inited.load()) {
#if (OPENSSL_VERSION_MAJOR >= 3)
    OSSL_PROVIDER_load(nullptr, "legacy");
#endif
    inited.store(true);
  }
}

} // namespace noir::openssl

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/consensus/crypto.h>

extern "C" {
#include <sodium.h>
}

using namespace noir;
using namespace noir::consensus;

TEST_CASE("crypto: sign and validate ed25519", "[noir][consensus]") {
  auto priv_key_ = priv_key::new_priv_key();
  auto pub_key_ = priv_key_.get_pub_key();
  CHECK(priv_key_.key.size() == crypto_sign_SECRETKEYBYTES);
  CHECK(pub_key_.key.size() == crypto_sign_PUBLICKEYBYTES);

  auto msg = from_hex("0123");
  auto sig = priv_key_.sign(msg);
  CHECK(sig.size() == crypto_sign_BYTES);
  CHECK(pub_key_.verify_signature(msg, sig));
  CHECK(!pub_key_.verify_signature(from_hex("abcd"), sig));
}

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/consensus/crypto.h>
#include <fc/crypto/base64.hpp>

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

TEST_CASE("crypto: encode keys", "[noir][consensus]") {
  auto priv_key_str =
    fc::base64_decode("i1QCJs9d74qK70C8aemx0FfjJCSa/UxkwIUGaS5T5IC1vmuNDpE1j3OD3oXDD2ilutJzr9+pU5JNtelPYKC3yA==");
  priv_key priv_key_{.key = Bytes(priv_key_str.begin(), priv_key_str.end())};
  auto pub_key_ = priv_key_.get_pub_key();
  std::string addr = to_hex(pub_key_.address());
  std::transform(addr.begin(), addr.end(), addr.begin(), ::toupper);
  CHECK(fc::base64_encode(pub_key_.key.data(), pub_key_.key.size()) == "tb5rjQ6RNY9zg96Fww9opbrSc6/fqVOSTbXpT2Cgt8g=");
  CHECK(addr == "BEB5FACCA0E17CF6C63DED5475A6E266120E692A");
}

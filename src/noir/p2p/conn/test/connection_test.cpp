// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>

#include <fc/crypto/private_key.hpp>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <sodium.h>

using namespace noir;

TEST_CASE("secret_connection: generate priv_key", "[noir][p2p]") {
  auto key1 = fc::crypto::private_key::generate_r1().to_string();
  std::cout << key1 << std::endl;

  // auto key2 = fc::to_base58(from_hex(key1), fc::yield_function_t());
  // std::cout << key2 << std::endl;

  std::cout << "base64 = " << fc::base64_encode("MC4CAQAwBQYDK2VwBCIEIFBkyMGbEaetGPnhoX9mYxRVNbyPyMQVxZEyspo6QtAq")
            << std::endl;

  EVP_PKEY* pkey = NULL;
  EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
  EVP_PKEY_keygen_init(pctx);
  EVP_PKEY_keygen(pctx, &pkey);
  EVP_PKEY_CTX_free(pctx);
  PEM_write_PrivateKey(stdout, pkey, NULL, NULL, 0, NULL, NULL);
}

int crypto_box_recover_public_key(uint8_t secret_key[]) {
  uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
  char phexbuf[2 * crypto_sign_PUBLICKEYBYTES + 1];
  crypto_scalarmult_curve25519_base(public_key, secret_key);
}

int crypto_sign_recover_public_key(uint8_t secret_key[]) {
  uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
  char phexbuf[2 * crypto_sign_PUBLICKEYBYTES + 1];
  memcpy(public_key, secret_key + crypto_sign_PUBLICKEYBYTES, crypto_sign_PUBLICKEYBYTES);
}

void crypto_box_example() {
  uint8_t public_key[crypto_box_PUBLICKEYBYTES];
  uint8_t secret_key[crypto_box_SECRETKEYBYTES];
  char phexbuf[2 * crypto_box_PUBLICKEYBYTES + 1];
  char shexbuf[2 * crypto_box_SECRETKEYBYTES + 1];

  crypto_box_keypair(public_key, secret_key);

  std::cout << "public_key " << noir::to_hex(phexbuf) << std::endl;
  std::cout << "secret_key " << noir::to_hex(shexbuf) << std::endl;

  crypto_box_recover_public_key(secret_key);
}

TEST_CASE("secret_connection: libsodium", "[noir][p2p]") {
  crypto_box_example();
}

TEST_CASE("secret_connection: libsodium - key generation", "[noir][p2p]") {
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  crypto_sign_keypair(pk, sk);

  std::cout << "pub_key = " << fc::base64_encode(pk, crypto_sign_PUBLICKEYBYTES) << std::endl;
  std::cout << "pri_key = " << fc::base64_encode(sk, crypto_sign_SECRETKEYBYTES) << std::endl;
}

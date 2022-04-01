// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/p2p/conn/secret_connection.h>

#include <fc/crypto/base64.hpp>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

#include <sodium.h>

namespace noir::p2p {

std::shared_ptr<secret_connection> secret_connection::make_secret_connection(bytes& loc_priv_key) {
  check(loc_priv_key.size() == 64, "unable to create a new create_connection: invalid private key size");
  auto sc = std::make_shared<secret_connection>();
  sc->loc_pub_key = bytes(loc_priv_key.begin() + 32, loc_priv_key.end());

  // Generate ephemeral local keys
  randombytes_buf(sc->loc_eph_priv.data(), sc->loc_eph_priv.size());
  crypto_scalarmult_base(reinterpret_cast<unsigned char*>(sc->loc_eph_pub.data()),
    reinterpret_cast<const unsigned char*>(sc->loc_eph_priv.data()));

  return sc;
}

std::optional<std::string> secret_connection::shared_eph_pub_key(bytes32& received_pub_key) {
  // Already exchanged eph_pub_keys with each other
  rem_eph_pub = received_pub_key;

  // Sort by lexical order
  bool loc_is_least = true;
  bytes32 lo_eph_pub = loc_eph_pub, hi_eph_pub = rem_eph_pub;
  if (loc_eph_pub > rem_eph_pub) {
    loc_is_least = false;
    lo_eph_pub = rem_eph_pub;
    hi_eph_pub = loc_eph_pub;
  }

  // Compute diffie hellman secret
  if (crypto_scalarmult(reinterpret_cast<unsigned char*>(dh_secret.data()),
        reinterpret_cast<const unsigned char*>(loc_eph_priv.data()),
        reinterpret_cast<const unsigned char*>(rem_eph_pub.data())) != 0) {
    return "unable to compute dh_secret";
  }

  // Generate secret keys used for receiving, sending, challenging via HKDF-SHA2
  EVP_KDF* kdf;
  EVP_KDF_CTX* kctx;
  unsigned char key[32 + 32 + 32];
  OSSL_PARAM params[5], *p = params;
  kdf = EVP_KDF_fetch(NULL, "HKDF", NULL);
  kctx = EVP_KDF_CTX_new(kdf);
  EVP_KDF_free(kdf);
  *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, SN_sha256, strlen(SN_sha256));
  *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void*)dh_secret.data(), dh_secret.size());
  *p++ =
    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void*)"TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN",
      strlen("TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN"));
  *p = OSSL_PARAM_construct_end();
  auto kdf_res = (EVP_KDF_derive(kctx, key, sizeof(key), params) <= 0);
  EVP_KDF_CTX_free(kctx);
  if (kdf_res)
    return "unable to derive secrets";

  if (loc_is_least) {
    recv_secret = bytes32(std::span(key, 32));
    send_secret = bytes32(std::span(key + 32, 32));
  } else {
    send_secret = bytes32(std::span(key, 32));
    recv_secret = bytes32(std::span(key + 32, 32));
  }
  chal_secret = bytes32(std::span(key + 64, 32));
  return {};
}

std::optional<std::string> secret_connection::shared_auth_sig(auth_sig_message& received_msg) {}

} // namespace noir::p2p

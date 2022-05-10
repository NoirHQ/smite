// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/common/types.h>
#include <noir/p2p/conn/merlin.h>
#include <noir/p2p/conn/secret_connection.h>

#include <fc/crypto/base64.hpp>

extern "C" {
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <sodium.h>
}

using namespace noir;

TEST_CASE("secret_connection: derive_secrets", "[noir][p2p]") {
  auto priv_key_str =
    fc::base64_decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  std::vector<char> loc_priv_key(priv_key_str.begin(), priv_key_str.end());
  auto c = p2p::secret_connection::make_secret_connection(loc_priv_key);

  auto tests = std::to_array<std::tuple<std::string, std::string, std::string, std::string>>({
    {"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03",
      "80a83ad6afcb6f8175192e41973aed31dd75e3c106f813d986d9567a4865eb2f",
      "96362a04f628a0666d9866147326898bb0847b8db8680263ad19e6336d4eed9e",
      "2632c3fd20f456c5383ed16aa1d56dc7875a2b0fc0d5ff053c3ada8934098c69"},
    {"0716764b370d543fee692af03832c16410f0a56e4ddb79604ea093b10bb6f654",
      "cba357ae33d7234520d5742102a2a6cdb39b7db59c14a58fa8aadd310127630f",
      "84f2b1e8658456529a2c324f46c3406c3c6fecd5fbbf9169f60bed8956a8b03d",
      "576643a8fcc1a4cf866db900f4a150dbe35d44a1b3ff36e4911565c3fa22fc32"},
    {"6104474c791cda24d952b356fb41a5d273c0ce6cc87d270b1701d0523cd5aa13",
      "1cb4397b9e478430321af4647da2ccbef62ff8888542d31cca3f626766c8080f",
      "673b23318826bd31ad1a4995c6e5095c4b092f5598aa0a96381a3e977bc0eaf9",
      "4a25a25c5f75d6cc512f2ba8c1546e6263e9ef8269f0c046c37838cc66aa83e6"},
  });
  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    bytes32 dh_secret{std::get<0>(t)};
    auto key = c->derive_secrets(dh_secret);
    CHECK(to_hex(std::span((const byte_type*)key.data(), 32)) == std::get<1>(t));
    CHECK(to_hex(std::span((const byte_type*)key.data() + 32, 32)) == std::get<2>(t));
    CHECK(to_hex(std::span((const byte_type*)key.data() + 64, 32)) == std::get<3>(t));
  });
}

TEST_CASE("secret_connection: verify key exchanges", "[noir][p2p]") {
  auto priv_key_str_peer1 =
    fc::base64_decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  std::vector<char> loc_priv_key_peer1(priv_key_str_peer1.begin(), priv_key_str_peer1.end());
  auto c_peer1 = p2p::secret_connection::make_secret_connection(loc_priv_key_peer1);
  auto priv_key_str_peer2 =
    fc::base64_decode("x1eX2WKe+mhZwO7PLVgLdMZ4Ucr4NfdBxMtD/59mOfmk8GO0T1p8YNpObegcTLZmqnK6ffVtjvWjDSSVgVwGAw==");
  std::vector<char> loc_priv_key_peer2(priv_key_str_peer2.begin(), priv_key_str_peer2.end());
  auto c_peer2 = p2p::secret_connection::make_secret_connection(loc_priv_key_peer2);

  bytes32 eph_pub_key_peer1 = c_peer1->loc_eph_pub;
  bytes32 eph_pub_key_peer2 = c_peer2->loc_eph_pub;
  c_peer1->shared_eph_pub_key(eph_pub_key_peer2);
  c_peer2->shared_eph_pub_key(eph_pub_key_peer1);
  CHECK(c_peer1->send_secret == c_peer2->recv_secret);
  CHECK(c_peer1->chal_secret == c_peer2->chal_secret);

  auto auth_sig_msg1 = p2p::auth_sig_message{c_peer1->loc_pub_key, c_peer1->loc_signature};
  auto auth_sig_msg2 = p2p::auth_sig_message{c_peer2->loc_pub_key, c_peer2->loc_signature};
  CHECK(!c_peer1->shared_auth_sig(auth_sig_msg2).has_value());
  CHECK(!c_peer2->shared_auth_sig(auth_sig_msg1).has_value());
}

std::string crypto_box_recover_public_key(uint8_t secret_key[]) {
  uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
  crypto_scalarmult_curve25519_base(public_key, secret_key);
  auto enc_pub_key = fc::base64_encode(public_key, crypto_box_PUBLICKEYBYTES);
  return enc_pub_key;
}

std::string crypto_sign_recover_public_key(uint8_t secret_key[]) {
  uint8_t public_key[crypto_sign_PUBLICKEYBYTES];
  memcpy(public_key, secret_key + crypto_sign_PUBLICKEYBYTES, crypto_sign_PUBLICKEYBYTES);
  auto enc_pub_key = fc::base64_encode(public_key, crypto_box_PUBLICKEYBYTES);
  return enc_pub_key;
}

TEST_CASE("secret_connection: libsodium - key gen : crypto_box", "[noir][p2p]") {
  uint8_t public_key[crypto_box_PUBLICKEYBYTES];
  uint8_t secret_key[crypto_box_SECRETKEYBYTES];
  crypto_box_keypair(public_key, secret_key);
  auto rec_pub_key = crypto_box_recover_public_key(secret_key);
  CHECK(rec_pub_key == fc::base64_encode(public_key, crypto_box_PUBLICKEYBYTES));
}

TEST_CASE("secret_connection: libsodium - key gen : crypto_sign", "[noir][p2p]") {
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  crypto_sign_keypair(pk, sk);
  auto rec_pub_key = crypto_sign_recover_public_key(sk);
  CHECK(rec_pub_key == fc::base64_encode(pk, crypto_sign_PUBLICKEYBYTES));
}

TEST_CASE("secret_connection: libsodium - derive pub_key from priv_key", "[noir][p2p]") {
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  auto priv_key =
    fc::base64_decode("q4BNZ9LFQw60L4UzkwkmRB2x2IPJGKwUaFXzbDTAXD5RezWnXQynrSHrYj602Dt6u6ga7T5Uc1pienw7b5JAbQ==");
  std::vector<char> raw(priv_key.begin(), priv_key.end());
  auto rec_pub_key = crypto_sign_recover_public_key(reinterpret_cast<uint8_t*>(raw.data()));
  CHECK(rec_pub_key == "UXs1p10Mp60h62I+tNg7eruoGu0+VHNaYnp8O2+SQG0=");
}

// TEST_CASE("secret_connection: derive address from pub_key", "[noir][p2p]") {
//  auto pub_key = fc::base64_decode("UXs1p10Mp60h62I+tNg7eruoGu0+VHNaYnp8O2+SQG0=");
//  auto h = crypto::sha256()(pub_key);
//  noir::bytes address = bytes(h.begin(), h.begin() + 20);
//  CHECK(to_hex(address) == "cbc837aced724b22dc0bff1821cdbdd96164d637");
// }

int sign(uint8_t sm[], const uint8_t m[], const int mlen, const uint8_t sk[]) {
  unsigned long long smlen;
  if (crypto_sign(sm, &smlen, m, mlen, sk) == 0) {
    return smlen;
  } else {
    return -1;
  }
}

int verify(uint8_t m[], const uint8_t sm[], const int smlen, const uint8_t pk[]) {
  unsigned long long mlen;
  if (crypto_sign_open(m, &mlen, sm, smlen, pk) == 0) {
    return mlen;
  } else {
    return -1;
  }
}

constexpr int MAX_MSG_LEN = 64;
TEST_CASE("secret_connection: libsodium - sign", "[noir][p2p]") {
  uint8_t sk[crypto_sign_SECRETKEYBYTES];
  uint8_t pk[crypto_sign_PUBLICKEYBYTES];
  uint8_t sm[MAX_MSG_LEN + crypto_sign_BYTES];
  uint8_t m[MAX_MSG_LEN + crypto_sign_BYTES + 1];

  memset(m, '\0', MAX_MSG_LEN);
  std::string msg{"Hello World"};
  strcpy((char*)m, msg.data());
  int mlen = msg.size();

  int rc = crypto_sign_keypair(pk, sk);
  CHECK(rc >= 0);
  int smlen = sign(sm, m, mlen, sk);
  CHECK(smlen >= 0);
  mlen = verify(m, sm, smlen, pk);
  CHECK(mlen >= 0);
}

TEST_CASE("secret_connection: openssl - hkdf", "[noir][p2p]") {
  EVP_KDF* kdf;
  EVP_KDF_CTX* kctx;
  static char const* digest = "SHA256";
  unsigned char key[32 + 32 + 32];
  OSSL_PARAM params[5], *p = params;
  kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
  kctx = EVP_KDF_CTX_new(kdf);
  EVP_KDF_free(kdf);

  *p++ = OSSL_PARAM_construct_utf8_string("digest", (char*)digest, strlen(digest));
  bytes32 secret{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};
  *p++ = OSSL_PARAM_construct_octet_string("key", (void*)secret.data(), secret.size());
  *p++ = OSSL_PARAM_construct_octet_string("info", (void*)"TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN",
    strlen("TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN"));
  *p = OSSL_PARAM_construct_end();
  if (EVP_KDF_derive(kctx, key, sizeof(key), params) <= 0) {
    std::cout << "Error: "
              << "EVP_KDF_derive" << std::endl;
  }
  EVP_KDF_CTX_free(kctx);
  // std::cout << "key = " << fc::base64_encode(key, sizeof(key)) << std::endl;
  // std::cout << "key1 = " << fc::base64_encode(key, 32) << std::endl;
  // std::cout << "key2 = " << fc::base64_encode(key + 32, 32) << std::endl;
  // std::cout << "challenge = " << fc::base64_encode(key + 64, 32) << std::endl;

  // std::cout << "key1_hex = " << to_hex(std::span((const byte_type*)key, 32)) << std::endl;
  // std::cout << "key2_hex = " << to_hex(std::span((const byte_type*)(key+32), 32)) << std::endl;
  // std::cout << "challenge_hex = " << to_hex(std::span((const byte_type*)(key+64), 32)) << std::endl;
  CHECK(
    to_hex(std::span((const byte_type*)key, 32)) == "80a83ad6afcb6f8175192e41973aed31dd75e3c106f813d986d9567a4865eb2f");
  CHECK(to_hex(std::span((const byte_type*)(key + 32), 32)) ==
    "96362a04f628a0666d9866147326898bb0847b8db8680263ad19e6336d4eed9e");
  CHECK(to_hex(std::span((const byte_type*)(key + 64), 32)) ==
    "2632c3fd20f456c5383ed16aa1d56dc7875a2b0fc0d5ff053c3ada8934098c69");
}

TEST_CASE("secret_connection: libsodium - chacha20", "[noir][p2p]") {
  bytes32 key{"9fe4a5a73df12dbd8659b1d9280873fe993caefec6b0ebc2686dd65027148e03"};
  uint8_t nonce[crypto_stream_chacha20_IETF_NONCEBYTES];
  randombytes_buf(nonce, sizeof nonce);
  std::string msg = "hello";
  unsigned long long ciphertext_len;
  unsigned char ciphertext[1024];
  unsigned char decrypted[1024];
  crypto_stream_chacha20_ietf_xor(ciphertext, reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), nonce,
    reinterpret_cast<const unsigned char*>(key.data()));
  crypto_stream_chacha20_ietf_xor(decrypted, reinterpret_cast<const unsigned char*>(ciphertext), msg.size(), nonce,
    reinterpret_cast<const unsigned char*>(key.data()));
  CHECK(std::string(decrypted, decrypted + 5) == msg);
}

TEST_CASE("secret_connection: merlin transcript", "[noir][p2p]") {
  p2p::merlin_transcript mctx{};
  static char const* init = "TEST";
  static char const* item = "TEST";
  static char const* challenge = "challenge";
  bytes32 challenge_buf;
  merlin_transcript_init(&mctx, reinterpret_cast<const uint8_t*>(init), strlen(init));
  merlin_transcript_commit_bytes(
    &mctx, reinterpret_cast<const uint8_t*>(item), strlen(item), reinterpret_cast<const uint8_t*>(item), strlen(item));
  merlin_transcript_challenge_bytes(&mctx, reinterpret_cast<const uint8_t*>(challenge), strlen(challenge),
    reinterpret_cast<uint8_t*>(challenge_buf.data()), challenge_buf.size());
  CHECK(to_hex(challenge_buf) == "c8cc8d7b4b3320f6a7a813c480d8f1ebd9cfb6873417eb69a44b4ed91b27af10");
}

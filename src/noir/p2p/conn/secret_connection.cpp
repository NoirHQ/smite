// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/p2p/conn/merlin.h>
#include <noir/p2p/conn/secret_connection.h>

extern "C" {
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <sodium.h>
}

namespace noir::p2p {

std::shared_ptr<secret_connection> secret_connection::make_secret_connection(Bytes& loc_priv_key) {
  if (!(loc_priv_key.size() == 64)) {
    throw std::runtime_error("unable to create a new create_connection: invalid private key size");
  }
  auto sc = std::make_shared<secret_connection>();
  sc->loc_priv_key = loc_priv_key;
  sc->loc_pub_key = Bytes(loc_priv_key.begin() + 32, loc_priv_key.end());

  // Generate ephemeral local keys
  randombytes_buf(sc->loc_eph_priv.data(), sc->loc_eph_priv.size());
  crypto_scalarmult_base(reinterpret_cast<unsigned char*>(sc->loc_eph_pub.data()),
    reinterpret_cast<const unsigned char*>(sc->loc_eph_priv.data()));

  return sc;
}

std::optional<std::string> secret_connection::shared_eph_pub_key(Bytes32& received_pub_key) {
  // By here, we have already exchanged eph_pub_keys with the other
  rem_eph_pub = received_pub_key;

  // Sort by lexical order
  bool loc_is_least = true;
  Bytes32 lo_eph_pub = loc_eph_pub, hi_eph_pub = rem_eph_pub;
  if (loc_eph_pub > rem_eph_pub) {
    loc_is_least = false;
    lo_eph_pub = rem_eph_pub;
    hi_eph_pub = loc_eph_pub;
  }

  // Compute diffie hellman secret
  Bytes32 dh_secret;
  if (crypto_scalarmult(reinterpret_cast<unsigned char*>(dh_secret.data()),
        reinterpret_cast<const unsigned char*>(loc_eph_priv.data()),
        reinterpret_cast<const unsigned char*>(rem_eph_pub.data())) != 0) {
    return "unable to compute dh_secret";
  }

  // Generate secret keys used for receiving, sending, challenging via HKDF-SHA2
  auto key = derive_secrets(dh_secret);
  if (key.size() < 96)
    return "unable to derive secrets";

  if (loc_is_least) {
    recv_secret = Bytes32(std::span(key.data(), 32));
    send_secret = Bytes32(std::span(key.data() + 32, 32));
  } else {
    send_secret = Bytes32(std::span(key.data(), 32));
    recv_secret = Bytes32(std::span(key.data() + 32, 32));
  }
  chal_secret = Bytes32(std::span(key.data() + 64, 32));

  // Find challenge secret by applying merlin transcript
  merlin_transcript mctx{};
  static char const* label = "TENDERMINT_SECRET_CONNECTION_TRANSCRIPT_HASH";
  merlin_transcript_init(&mctx, reinterpret_cast<const uint8_t*>(label), strlen(label));
  merlin_transcript_commit_bytes(&mctx, reinterpret_cast<const uint8_t*>("EPHEMERAL_LOWER_PUBLIC_KEY"),
    strlen("EPHEMERAL_LOWER_PUBLIC_KEY"), reinterpret_cast<const uint8_t*>(lo_eph_pub.data()), lo_eph_pub.size());
  merlin_transcript_commit_bytes(&mctx, reinterpret_cast<const uint8_t*>("EPHEMERAL_UPPER_PUBLIC_KEY"),
    strlen("EPHEMERAL_UPPER_PUBLIC_KEY"), reinterpret_cast<const uint8_t*>(hi_eph_pub.data()), hi_eph_pub.size());
  merlin_transcript_commit_bytes(&mctx, reinterpret_cast<const uint8_t*>("DH_SECRET"), strlen("DH_SECRET"),
    reinterpret_cast<const uint8_t*>(dh_secret.data()), dh_secret.size());
  merlin_transcript_challenge_bytes(&mctx, reinterpret_cast<const uint8_t*>("SECRET_CONNECTION_MAC"),
    strlen("SECRET_CONNECTION_MAC"), reinterpret_cast<uint8_t*>(chal_secret.data()), chal_secret.size());

  // Sign challenge Bytes for authentication
  Bytes sig(64);
  if (crypto_sign_detached(reinterpret_cast<unsigned char*>(sig.data()), nullptr,
        reinterpret_cast<const unsigned char*>(chal_secret.data()), chal_secret.size(),
        reinterpret_cast<const unsigned char*>(loc_priv_key.data())) == 0) {
    loc_signature = sig;
    return {};
  }
  return "unable to sign challenge";
}

std::optional<std::string> secret_connection::shared_auth_sig(auth_sig_message& received_msg) {
  // By here, we have already exchanged auth_sig_message with the other
  rem_pub_key = received_msg.key;

  // Verify signature
  if (crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(received_msg.sig.data()),
        reinterpret_cast<const unsigned char*>(chal_secret.data()), chal_secret.size(),
        reinterpret_cast<const unsigned char*>(rem_pub_key.data())) == 0) {
    is_authorized = true;
    return {};
  }
  return "unable to verify challenge";
}

Bytes secret_connection::derive_secrets(Bytes32& dh_secret) {
  EVP_KDF* kdf;
  EVP_KDF_CTX* kctx;
  unsigned char key[32 + 32 + 32];
  static char const* digest = "SHA256";
  static char const* info = "TENDERMINT_SECRET_CONNECTION_KEY_AND_CHALLENGE_GEN";
  OSSL_PARAM params[5], *p = params;
  kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
  kctx = EVP_KDF_CTX_new(kdf);
  EVP_KDF_free(kdf);
  *p++ = OSSL_PARAM_construct_utf8_string("digest", (char*)digest, strlen(digest));
  *p++ = OSSL_PARAM_construct_octet_string("key", (void*)dh_secret.data(), dh_secret.size());
  *p++ = OSSL_PARAM_construct_octet_string("info", (void*)info, strlen(info));
  *p = OSSL_PARAM_construct_end();
  auto kdf_res = (EVP_KDF_derive(kctx, key, sizeof(key), params) <= 0);
  EVP_KDF_CTX_free(kctx);
  if (kdf_res)
    return {};
  return {key, key + sizeof(key) / sizeof(key[0])};
}

Result<std::pair<int, std::vector<std::shared_ptr<Bytes>>>> secret_connection::write(const Bytes& data) {
  std::scoped_lock g(send_mtx);
  int n{};
  std::vector<std::shared_ptr<Bytes>> ret;
  auto data_pos = data.begin();
  while (data_pos != data.end()) {
    auto sealed_frame = std::make_shared<Bytes>(sealed_frame_size);
    Bytes frame(total_frame_size);
    Bytes chunk;
    auto data_size = std::distance(data_pos, data.end());
    if (data_max_size < data_size) {
      auto split_pos = data_pos + data_max_size;
      chunk.raw().resize(data_max_size);
      std::copy(data_pos, split_pos, chunk.data());
      data_pos = split_pos; // TODO : check
    } else {
      chunk.raw().resize(data_size);
      std::copy(data_pos, data.end(), chunk.data());
      data_pos = data.end();
    }
    auto chunk_length = chunk.size();
    const uint32_t payload_size = chunk_length;
    const char* const header = reinterpret_cast<const char* const>(&payload_size);
    std::memcpy(frame.data(), header, 4);
    std::memcpy(frame.data() + 4, chunk.data(), chunk.size());

    // Encrypt frame
    unsigned long long ciphertext_len{};
    crypto_aead_chacha20poly1305_ietf_encrypt(sealed_frame->data(), &ciphertext_len,
      reinterpret_cast<const unsigned char*>(frame.data()), frame.size(), nullptr, 0, nullptr, send_nonce.get(),
      reinterpret_cast<const unsigned char*>(send_secret.data()));
    send_nonce.increment();

    n += chunk_length;
    ret.push_back(sealed_frame);
  }
  return {n, ret};
}

Result<std::pair<int, std::shared_ptr<Bytes>>> secret_connection::read(const Bytes& data, bool is_peek) {
  std::scoped_lock g(recv_mtx);
  check(data.size() == 1044, "invalid sealed_frame size");

  Bytes frame(total_frame_size);
  unsigned long long decrypted_len{}, ciphertext_len{data.size()};
  auto r = crypto_aead_chacha20poly1305_ietf_decrypt(frame.data(), &decrypted_len, nullptr, data.data(), ciphertext_len,
    nullptr, 0, recv_nonce.get(), reinterpret_cast<const unsigned char*>(recv_secret.data()));
  if (r < 0)
    return Error::format("decryption failed: size={}", data.size());
  if (!is_peek)
    recv_nonce.increment();

  uint32_t chunk_length;
  std::memcpy(&chunk_length, frame.data(), data_len_size);
  if (chunk_length > data_max_size)
    return Error::format("chunk_length is greater than data_max_size");
  auto chunk = std::make_shared<Bytes>(chunk_length);
  std::memcpy(chunk->data(), frame.data() + data_len_size, chunk_length);
  return {chunk_length, chunk};
}

} // namespace noir::p2p

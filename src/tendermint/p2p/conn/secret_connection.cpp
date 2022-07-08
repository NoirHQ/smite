// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/binary.h>
#include <tendermint/p2p/conn/merlin.h>
#include <tendermint/p2p/conn/secret_connection.h>

extern "C" {
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <sodium.h>
}

namespace tendermint::p2p::conn {

using namespace noir;

std::shared_ptr<SecretConnection> SecretConnection::make_secret_connection(std::vector<unsigned char>& loc_priv_key,
  std::shared_ptr<net::Conn<net::TcpConn>>& conn) {
  if (loc_priv_key.size() == 64) {
    throw std::runtime_error("unable to create a new create_connection: invalid private key size");
  }
  auto sc = std::make_shared<SecretConnection>();
  sc->loc_priv_key = loc_priv_key;
  sc->loc_pub_key = std::vector<unsigned char>(loc_priv_key.begin() + 32, loc_priv_key.end());
  sc->conn = conn;

  // Generate ephemeral local keys
  randombytes_buf(sc->loc_eph_priv.data(), sc->loc_eph_priv.size());
  crypto_scalarmult_base(reinterpret_cast<unsigned char*>(sc->loc_eph_pub.data()),
    reinterpret_cast<const unsigned char*>(sc->loc_eph_priv.data()));

  return sc;
}

auto SecretConnection::shared_eph_pub_key(Bytes32& received_pub_key) -> noir::Result<void> {
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
    return Error("unable to compute dh_secret");
  }

  // Generate secret keys used for receiving, sending, challenging via HKDF-SHA2
  auto key = derive_secrets(dh_secret);
  if (key.size() < 96)
    return Error("unable to derive secrets");

  if (loc_is_least) {
    recv_secret = Bytes32(std::span(key.data(), 32));
    send_secret = Bytes32(std::span(key.data() + 32, 32));
  } else {
    send_secret = Bytes32(std::span(key.data(), 32));
    recv_secret = Bytes32(std::span(key.data() + 32, 32));
  }
  chal_secret = Bytes32(std::span(key.data() + 64, 32));

  // Find challenge secret by applying merlin transcript
  MerlinTranscript mctx{};
  static char const* label = "TENDERMINT_SECRET_CONNECTION_TRANSCRIPT_HASH";
  merlin_transcript_init(&mctx, reinterpret_cast<const uint8_t*>(label), strlen(label));
  merlin_transcript_commit_bytes(&mctx,
    reinterpret_cast<const uint8_t*>("EPHEMERAL_LOWER_PUBLIC_KEY"),
    strlen("EPHEMERAL_LOWER_PUBLIC_KEY"),
    reinterpret_cast<const uint8_t*>(lo_eph_pub.data()),
    lo_eph_pub.size());
  merlin_transcript_commit_bytes(&mctx,
    reinterpret_cast<const uint8_t*>("EPHEMERAL_UPPER_PUBLIC_KEY"),
    strlen("EPHEMERAL_UPPER_PUBLIC_KEY"),
    reinterpret_cast<const uint8_t*>(lo_eph_pub.data()),
    hi_eph_pub.size());
  merlin_transcript_commit_bytes(&mctx,
    reinterpret_cast<const uint8_t*>("DH_SECRET"),
    strlen("DH_SECRET"),
    reinterpret_cast<const uint8_t*>(dh_secret.data()),
    dh_secret.size());
  merlin_transcript_challenge_bytes(&mctx,
    reinterpret_cast<const uint8_t*>("SECRET_CONNECTION_MAC"),
    strlen("SECRET_CONNECTION_MAC"),
    reinterpret_cast<uint8_t*>(chal_secret.data()),
    chal_secret.size());

  // Sign challenge bytes for authentication
  uint8_t sm[32 + crypto_sign_BYTES];
  unsigned long long smlen;
  if (crypto_sign(sm,
        &smlen,
        reinterpret_cast<const unsigned char*>(chal_secret.data()),
        chal_secret.size(),
        reinterpret_cast<const unsigned char*>(loc_priv_key.data())) != 0)
    return Error("unable to sign challenge");
  loc_signature = std::vector<unsigned char>(sm, sm + smlen);
  return success();
}

auto SecretConnection::shared_auth_sig(AuthSigMessage& received_msg) -> noir::Result<void> {
  // By here, we have already exchanged auth_sig_message with the other
  rem_pub_key = received_msg.key;

  // Verify signature
  uint8_t m[32 + crypto_sign_BYTES + 1];
  unsigned long long mlen;
  if (crypto_sign_open(m,
        &mlen,
        reinterpret_cast<const unsigned char*>(received_msg.sig.data()),
        received_msg.sig.size(),
        reinterpret_cast<const unsigned char*>(rem_pub_key.data())) != 0)
    return Error("unable to verify challenge");
  is_authorized = true;
  return success();
}

std::vector<unsigned char> SecretConnection::derive_secrets(Bytes32& dh_secret) {
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

auto SecretConnection::read(std::span<unsigned char> data) -> asio::awaitable<Result<std::size_t>> {
  std::scoped_lock _{recv_mtx};

  // read off and update the recvBuffer, if non-empty
  if (recv_buffer) {
    if (data.size() < recv_buffer->size() - read_pos) {
      std::copy(recv_buffer->begin() + read_pos, recv_buffer->begin() + read_pos + data.size(), data.data());
      read_pos += data.size();
      co_return data.size();
    } else {
      std::copy(recv_buffer->begin() + read_pos, recv_buffer->end(), data.data());
      std::size_t n = recv_buffer->size() - read_pos;
      recv_buffer.reset();
      read_pos = 0;
      co_return n;
    }
  }

  // read off the conn
  std::vector<unsigned char> sealed_frame(total_frame_size + aead_size_overhead);
  auto res = co_await conn->read(std::span(sealed_frame.data(), sealed_frame.size()));
  if (res.has_error()) {
    co_return res.error();
  }

  std::vector<unsigned char> frame(total_frame_size);
  unsigned long long frame_size;
  auto nonce_bytes = recv_nonce.get();

  crypto_aead_chacha20poly1305_ietf_decrypt(frame.data(),
    &frame_size,
    nullptr,
    reinterpret_cast<const unsigned char*>(sealed_frame.data()),
    sealed_frame.size(),
    nullptr,
    0,
    reinterpret_cast<const unsigned char*>(nonce_bytes.data()),
    reinterpret_cast<const unsigned char*>(recv_secret.data()));

  recv_nonce.increment();
  auto chunk_length = LittleEndian::uint32(frame);
  if (chunk_length > data_max_size) {
    co_return Error("chunk_length is greater than data_max_size");
  }
  std::vector<unsigned char> chunk(frame.begin() + data_len_size, frame.begin() + data_len_size + chunk_length);
  std::size_t n;
  if (data.size() < chunk_length) {
    std::copy(chunk.begin(), chunk.begin() + data.size(), data.data());
    n = data.size();
    recv_buffer = std::make_unique<std::vector<unsigned char>>(chunk.begin() + data.size(), chunk.end());
  } else {
    std::copy(chunk.begin(), chunk.end(), data.data());
    n = chunk_length;
  }
  co_return n;
}

auto SecretConnection::write(std::span<const unsigned char> data)
  -> asio::awaitable<std::tuple<std::size_t, Result<void>>> {
  std::scoped_lock _{send_mtx};

  std::size_t n = 0;
  while (!data.empty()) {
    std::shared_ptr<std::vector<unsigned char>> chunk;
    std::vector<unsigned char> frame(total_frame_size);
    std::vector<unsigned char> sealed_frame(total_frame_size + aead_size_overhead);
    if (data_max_size < data.size()) {
      chunk = std::make_shared<std::vector<unsigned char>>(data.begin(), data.begin() + data_max_size);
      data = data.subspan(data_max_size);
    } else {
      chunk = std::make_shared<std::vector<unsigned char>>(data.begin(), data.end());
      data = data.subspan(data.size());
    }
    auto chunk_length = chunk->size();
    LittleEndian::put_uint32(frame, chunk_length);
    std::copy(chunk->begin(), chunk->end(), frame.begin() + data_len_size);

    auto nonce_bytes = send_nonce.get();
    unsigned long long int sealed_frame_size;
    crypto_aead_chacha20poly1305_ietf_encrypt(sealed_frame.data(),
      &sealed_frame_size,
      frame.data(),
      frame.size(),
      nullptr,
      0,
      nullptr,
      reinterpret_cast<const unsigned char*>(nonce_bytes.data()),
      reinterpret_cast<const unsigned char*>(send_secret.data()));

    send_nonce.increment();

    auto res = co_await conn->write(std::span<const unsigned char>(sealed_frame.data(), sealed_frame.size()));
    if (res.has_error()) {
      co_return std::make_tuple(n, res.error());
    }
    n += chunk->size();
  }
  co_return std::make_tuple(n, success());
}

} //namespace tendermint::p2p::conn

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/core/core.h>
#include <noir/net/tcp_conn.h>

namespace tendermint::p2p::conn {
template<size_t NumBytes>
struct Nonce {
  noir::BytesN<NumBytes> bz{};
  bool overflow{}; // Indicates if an overflow occurred

  /// Not thread safe
  void increment() {
    unsigned int carry = 1;
    for (int64_t i = bz.size() - 1; carry > 0; --i) {
      unsigned int current = *reinterpret_cast<unsigned char*>(&bz[i]);
      current += carry;
      *reinterpret_cast<unsigned char*>(&bz[i]) = current & 0xff;
      carry = current >> 8;
    }
    if (carry > 0)
      overflow = true;
  }

  noir::Bytes get() {
    return bz.to_bytes();
  }

  constexpr size_t size() {
    return bz.size();
  }

  bool operator==(const Nonce<NumBytes>& other) {
    return bz == other.bz and overflow == other.overflow;
  }
};
typedef Nonce<12> Nonce96;

struct AuthSigMessage {
  noir::Bytes key;
  noir::Bytes sig;
};

class SecretConnection {
public:
  static auto make_secret_connection(noir::Bytes& loc_priv_key,
    std::shared_ptr<noir::net::Conn<noir::net::TcpConn>>& conn) -> std::shared_ptr<SecretConnection>;
  auto shared_eph_pub_key(noir::Bytes32& received_pub_key) -> noir::Result<void>;
  auto shared_auth_sig(AuthSigMessage& received_msg) -> noir::Result<void>;
  auto derive_secrets(noir::Bytes32& dh_secret) -> noir::Bytes;
  auto read(std::span<unsigned char> data) -> boost::asio::awaitable<noir::Result<std::size_t>>;
  auto write(std::span<const unsigned char> msg) -> boost::asio::awaitable<std::tuple<std::size_t, noir::Result<void>>>;

public:
  bool is_authorized{};

  noir::Bytes loc_pub_key;
  noir::Bytes loc_signature;

  noir::Bytes rem_pub_key;

  noir::Bytes32 loc_eph_pub;
  noir::Bytes32 recv_secret;
  noir::Bytes32 send_secret;
  noir::Bytes32 chal_secret;

private:
  noir::Bytes loc_priv_key;

  noir::Bytes32 loc_eph_priv;
  noir::Bytes32 rem_eph_pub; // other's ephemeral pub key

  Nonce96 recv_nonce;
  Nonce96 send_nonce;

  std::mutex send_mtx;
  std::mutex recv_mtx;

  std::unique_ptr<noir::Bytes> recv_buffer;
  std::size_t read_pos;

  const std::size_t data_len_size = 4;
  const std::size_t data_max_size = 1024;
  const std::size_t total_frame_size = data_len_size + data_max_size;
  const std::size_t aead_size_overhead = 16;

  std::shared_ptr<noir::net::Conn<noir::net::TcpConn>> conn;
};

} //namespace tendermint::p2p::conn

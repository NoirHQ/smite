// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/result.h>
#include <noir/p2p/types.h>
#include <mutex>
#include <optional>

namespace noir::p2p {

constexpr auto data_len_size = 4;
constexpr auto data_max_size = 1024;
constexpr auto total_frame_size = data_max_size + data_len_size;
constexpr auto aead_size_overhead = 16;
constexpr auto aead_key_size = 32;
constexpr auto aead_nonce_size = 12;

template<size_t NumBytes>
struct nonce {
  BytesN<NumBytes> bz{};
  bool overflow{}; // Indicates if an overflow occurred

  /// Not thread safe
  void increment() {
    unsigned int carry = 1;
    for (int64_t i = 0; carry > 0; ++i) {
      unsigned int current = *reinterpret_cast<unsigned char*>(&bz[i + 4]);
      current += carry;
      *reinterpret_cast<unsigned char*>(&bz[i + 4]) = current & 0xff;
      carry = current >> 8;
    }
    if (carry > 0)
      overflow = true;
  }

  const unsigned char* get() {
    return reinterpret_cast<const unsigned char*>(bz.data());
  }

  constexpr size_t size() {
    return bz.size();
  }

  bool operator==(const nonce<NumBytes>& other) {
    return bz == other.bz and overflow == other.overflow;
  }
};
typedef nonce<12> nonce96;

struct auth_sig_message {
  Bytes key;
  Bytes sig;
};

struct secret_connection {
  Bytes loc_priv_key;
  Bytes loc_pub_key;
  Bytes loc_signature;
  Bytes rem_pub_key; // other's pub key

  Bytes32 loc_eph_pub;
  Bytes32 loc_eph_priv;
  Bytes32 rem_eph_pub; // other's ephemeral pub key

  Bytes32 recv_secret;
  Bytes32 send_secret;
  Bytes32 chal_secret;

  nonce96 recv_nonce;
  nonce96 send_nonce;

  std::mutex recv_mtx;
  std::mutex send_mtx;

  bool is_authorized{};

  static std::shared_ptr<secret_connection> make_secret_connection(Bytes& loc_priv_key);

  std::optional<std::string> shared_eph_pub_key(Bytes32& received_pub_key);

  std::optional<std::string> shared_auth_sig(auth_sig_message& received_msg);

  Bytes derive_secrets(Bytes32& dh_secret);

  Result<std::pair<int, std::vector<std::shared_ptr<Bytes>>>> write(const Bytes& data);
  Result<std::pair<int, std::shared_ptr<Bytes>>> read(const Bytes& data, bool is_peek = false);
};

} // namespace noir::p2p

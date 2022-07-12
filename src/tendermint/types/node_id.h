#pragma once
#include <noir/common/hex.h>
#include <noir/crypto/hash/sha2.h>
#include <string>

namespace tendermint {

using NodeId = std::string;

static auto node_id_from_pubkey(const std::vector<unsigned char>& pub_key) -> NodeId {
  if (pub_key.size() != 32) {
    throw std::runtime_error("pubkey is incorrect size");
  }
  auto sah256_sum = noir::crypto::Sha256()(pub_key);
  std::vector<unsigned char> address(sah256_sum.begin(), sah256_sum.begin() + 20);
  return noir::hex::encode(address.data(), 20);
}

} // namespace tendermint

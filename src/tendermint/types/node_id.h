#pragma once
#include <noir/crypto/hash/sha2.h>
#include <noir/common/hex.h>
#include <string>

namespace tendermint {

using NodeId = std::string;

static auto node_id_from_pubkey(const std::vector<unsigned char>& pub_key) -> NodeId {
  if (pub_key.size() != 32) {
    throw std::runtime_error("pubkey is incorrect size");
  }
  auto& d = noir::crypto::Sha256().init();
  d = d.update(std::span(pub_key.data(), pub_key.size()));
  std::vector<unsigned char> out(32);
  d.update(std::span(out.data(), out.size()));
  std::vector<unsigned char> address(out.begin(), out.begin() + 20);
  auto hex_str = noir::hex::encode(address.data(), address.size());
  return hex_str;
}

} // namespace tendermint

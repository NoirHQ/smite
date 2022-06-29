#pragma once
#include <string>

namespace tendermint {

using NodeId = std::string;

static auto node_id_from_pubkey(const std::vector<char>& pub_key) -> NodeId {
  // TODO: return NodeID(hex.EncodeToString(pubKey.Address()))
  return "";
}

} // namespace tendermint

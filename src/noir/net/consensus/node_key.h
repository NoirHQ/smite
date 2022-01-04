#pragma once
#include <noir/net/types.h>
#include <noir/common/hex.h>

namespace noir::net::consensus {

struct node_key {
  std::string node_id;
  bytes priv_key;

  bytes get_pub_key() {
    return std::vector<char>(from_hex("000000"));
  }

  void save_as() {
    // todo - persist to a file
  }

  static node_key load_or_gen_node_key(std::string file_path) {
    // todo - read from a file
    return gen_node_key();
  }

  static node_key gen_node_key() {
    // todo - generate ed25519 private key
    return node_key{"node_id_abcdefg", std::vector<char>(from_hex("000000"))};
  }

};

}

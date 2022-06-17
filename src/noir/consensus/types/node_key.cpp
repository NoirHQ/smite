// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/helper/variant.h>
#include <noir/consensus/types/node_key.h>
#include <noir/crypto/rand.h>

#include <cppcodec/base64_default_rfc4648.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

namespace noir::consensus {

std::shared_ptr<node_key> node_key::gen_node_key() {
  std::shared_ptr<node_key> key = std::make_shared<node_key>();
  BytesN<64> priv_key{};
  crypto::rand_bytes(priv_key);
  key->priv_key = {priv_key.begin(), priv_key.end()};
  key->node_id = node_id_from_pub_key(key->get_pub_key());
  return key;
}

std::shared_ptr<node_key> node_key::load_node_key(const std::filesystem::path& file_path) {
  std::shared_ptr<node_key> key{};
  try {
    node_key_json_obj json_obj;
    fc::variant obj = fc::json::from_file(file_path.string());
    fc::from_variant(obj, json_obj);
    key = std::make_shared<node_key>();
    auto key_str = base64::decode(json_obj.priv_key.value);
    key->priv_key = Bytes(key_str.begin(), key_str.end());
    key->node_id = node_id_from_pub_key(key->get_pub_key());
  } catch (...) {
    elog(fmt::format("error reading node_key from {}", file_path.string()));
  }
  return key;
}

std::string node_key::node_id_from_pub_key(const Bytes& pub_key) {
  check(pub_key.size() == 32, "unable to get a node_id: invalid public key size");
  auto h = crypto::Sha256()(pub_key);
  auto address = Bytes(h.begin(), h.begin() + 20);
  return to_hex(address);
}

void node_key::save_as(const std::filesystem::path& file_path) {
  check(!file_path.empty(), "cannot save node_key: file_path is not set");
  std::filesystem::path dir_path = std::filesystem::path{file_path}.remove_filename();
  if (!std::filesystem::exists(dir_path))
    std::filesystem::create_directories(dir_path);
  node_key_json_obj json_obj;
  json_obj.priv_key.type = "tendermint/PrivKeyEd25519";
  json_obj.priv_key.value = base64::encode(priv_key.data(), priv_key.size());
  fc::variant vo;
  fc::to_variant<node_key_json_obj>(json_obj, vo);
  fc::json::save_to_file(vo, file_path);
}

} // namespace noir::consensus

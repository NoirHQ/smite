// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <noir/common/refl.h>
#include <noir/consensus/common.h>
#include <filesystem>

namespace noir::consensus {

struct priv_key_json_obj {
  std::string type;
  std::string value;
};

struct node_key_json_obj {
  priv_key_json_obj priv_key;
};

struct node_key {
  std::string node_id;
  Bytes priv_key;

  Bytes get_pub_key() {
    check(priv_key.size() == 64, "unable to get a pub_key: invalid private key size");
    return {priv_key.begin() + 32, priv_key.end()};
  }

  static std::shared_ptr<node_key> load_or_gen_node_key(const std::filesystem::path& file_path) {
    std::shared_ptr<node_key> key{};
    if (std::filesystem::exists(file_path)) {
      key = load_node_key(file_path);
    }
    if (!key) {
      key = gen_node_key();
      key->save_as(file_path);
    }
    return key;
  }

  static std::shared_ptr<node_key> gen_node_key();

  static std::shared_ptr<node_key> load_node_key(const std::filesystem::path& file_path);

  static std::string node_id_from_pub_key(const Bytes& pub_key);

  void save_as(const std::filesystem::path& file_path);
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::priv_key_json_obj, type, value)
NOIR_REFLECT(noir::consensus::node_key_json_obj, priv_key)

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/common/check.h>
#include <noir/consensus/privval/file.h>
#include <fc/variant_object.hpp>

#include <noir/common/helper/variant.h>

namespace noir::consensus::privval {
namespace fs = std::filesystem;

void file_pv_key::save() {
  check(!file_path.empty(), "cannot save PrivValidator key: filePath not set");

  // TODO: save file in thread safe context (eg.tendermint tempfile)
  // https://pkg.go.dev/github.com/tendermint/tendermint/internal/libs/tempfile
  fc::variant vo;
  fc::to_variant<file_pv_key>(*this, vo);
  fc::json::save_to_file(vo, file_path);
}

bool file_pv_key::load(const fs::path& key_file_path, file_pv_key& priv_key) {
  fc::variant obj = fc::json::from_file(key_file_path.string());
  fc::from_variant(obj, priv_key);
  priv_key.file_path = key_file_path.string();
  return true;
}

void file_pv_last_sign_state::save() {
  check(!file_path.empty(), "cannot save PrivValidator key: filePath not set");
  // TODO: save file in thread safe context (eg.tendermint tempfile)
  // https://pkg.go.dev/github.com/tendermint/tendermint/internal/libs/tempfile
  fc::variant vo;
  fc::to_variant<file_pv_last_sign_state>(*this, vo);
  fc::json::save_to_file(vo, file_path);
}

bool file_pv_last_sign_state::load(const fs::path& state_file_path, file_pv_last_sign_state& lss) {
  fc::variant obj = fc::json::from_file(state_file_path.string());
  fc::from_variant(obj, lss);
  lss.file_path = state_file_path.string();
  return true;
}

std::shared_ptr<file_pv> file_pv::gen_file_pv(
  const fs::path& key_file_path, const fs::path& state_file_path, const std::string& key_type) {
  noir::consensus::priv_key priv_key_{};
  // TODO: connect appropriate key_type
  if (key_type == "secp256k1") {
    // priv_key_.type = "secp256k1";
    priv_key_.key = fc::from_base58(fc::crypto::private_key::generate().to_string());
  } else if (key_type == "ed25519") {
    // priv_key_.type = "ed25519";
    priv_key_.key = fc::from_base58(fc::crypto::private_key::generate_r1().to_string());
  } else {
    elog(fmt::format("key type: {} is not supported", key_type)); // return err
    return nullptr;
  }
  return std::make_shared<file_pv>(priv_key_, key_file_path, state_file_path);
}

std::shared_ptr<file_pv> file_pv::load_file_pv_internal(
  const fs::path& key_file_path, const fs::path& state_file_path, bool load_state) {
  auto ret = std::make_shared<file_pv>();
  check(ret != nullptr);
  {
    check(file_pv_key::load(key_file_path, ret->key) == true, "error reading PrivValidator key from {}",
      key_file_path.string());
    // overwrite pubkey and address for convenience, TODO: Is this needed?
    ret->key.pub_key = ret->key.priv_key.get_pub_key();
    ret->key.address = ret->key.pub_key.address();
  }
  if (load_state) {
    check(file_pv_last_sign_state::load(state_file_path, ret->last_sign_state) == true,
      "error reading PrivValidator state from {}", state_file_path.string());
  }

  return ret;
}

void file_pv::save() {
  key.save();
  last_sign_state.save();
}

} // namespace noir::consensus::privval

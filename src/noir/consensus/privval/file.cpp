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

} // namespace noir::consensus::privval

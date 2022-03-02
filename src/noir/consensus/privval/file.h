// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/refl.h>
#include <noir/common/types.h>
#include <noir/consensus/crypto.h>
#include <noir/consensus/priv_validator.h>
#include <fc/io/json.hpp>
#include <filesystem>

namespace noir::consensus::privval {

/// \addtogroup privval
/// \ingroup consensus
/// \{

enum class sign_step : int8_t {
  none = 0,
  propose = 1,
  prevote = 2,
  precommit = 3,
};

/// \brief stores the immutable part of PrivValidator.
struct file_pv_key {
  bytes address;
  noir::consensus::pub_key pub_key;
  noir::consensus::priv_key priv_key;
  std::string file_path;

  /// \brief Save persists the FilePVKey to its filePath.
  void save();

  /// \brief loads the immutable part of PrivValidator.
  /// \param[in] key_file_path
  /// \param[out] priv_key
  /// \return
  static bool load(const std::filesystem::path& key_file_path, file_pv_key& priv_key);
};

/// \brief FilePVLastSignState stores the mutable part of PrivValidator.
struct file_pv_last_sign_state {
  int64_t height;
  int32_t round;
  sign_step step;
  bytes signature;
  bytes sign_bytes; // hex? bytes?
  std::string file_path;

  /// \brief persists the FilePvLastSignState to its filePath.
  void save();

  /// \brief loads the FilePvLastSignState to its filePath.
  /// \param[in] state_file_path
  /// \param[out] lss
  /// \return
  static bool load(const std::filesystem::path& state_file_path, file_pv_last_sign_state& lss);
};

/// \}

} // namespace noir::consensus::privval

NOIR_REFLECT(noir::consensus::privval::file_pv_key, address, pub_key, priv_key)
NOIR_REFLECT(noir::consensus::privval::file_pv_last_sign_state, height, round, step, signature, sign_bytes)

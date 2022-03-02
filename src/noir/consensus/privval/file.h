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

struct file_pv : public noir::consensus::priv_validator {
  file_pv_key key;
  file_pv_last_sign_state last_sign_state;

  file_pv() = default;
  file_pv(const file_pv&) = default;
  file_pv(file_pv&&) = default;

  /// \brief generates a new validator from the given key and paths.
  /// \param[in] priv_key
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  file_pv(/* const */ noir::consensus::priv_key& priv_key, const std::filesystem::path& key_file_path,
    const std::filesystem::path& state_file_path)
    : key(file_pv_key{
        .address = priv_key.get_pub_key().address(),
        .pub_key = priv_key.get_pub_key(),
        .priv_key = priv_key,
        .file_path = key_file_path,
      }),
      last_sign_state(file_pv_last_sign_state{
        .step = sign_step::none,
        .file_path = state_file_path,
      }) {}

  priv_validator_type get_type() const override {
    return priv_validator_type::FileSignerClient;
  }

  pub_key get_pub_key() const override {
    return key.pub_key;
  }

  priv_key get_priv_key() const override {
    return key.priv_key;
  }

  std::optional<std::string> sign_vote(vote& vote_) override {
    return {};
  }
  std::optional<std::string> sign_proposal(proposal& proposal_) override {
    return {};
  }

  /// \brief generates a new validator with randomly generated private key and sets the filePaths, but does not call
  /// save()
  /// \todo need to check default key_type
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \param[in] key_type
  /// \return shared_ptr of file_pv
  static std::shared_ptr<file_pv> gen_file_pv(const std::filesystem::path& key_file_path,
    const std::filesystem::path& state_file_path, const std::string& key_type = "secp256k1");

  /// \brief loads a FilePV from the filePaths. The FilePV handles double signing prevention by persisting data to the
  /// stateFilePath. If either file path does not exist, the program will exit.
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \return shared_ptr of file_pv
  static inline std::shared_ptr<file_pv> load_file_pv(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path) {
    return load_file_pv_internal(key_file_path, state_file_path, true);
  }

  /// \brief LoadFilePVEmptyState loads a FilePV from the given keyFilePath, with an empty LastSignState. If the
  /// keyFilePath does not exist, the program will exit.
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \return shared_ptr of file_pv
  static inline std::shared_ptr<file_pv> load_file_pv_empty_state(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path) {
    return load_file_pv_internal(key_file_path, state_file_path, false);
  }

  /// \brief persists the FilePv to its filePath.
  void save();

private:
  /// \brief load FilePV from given arguments
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \param[in] load_state if true, we load from the stateFilePath. Otherwise, we use an empty LastSignState
  /// \return shared_ptr of file_pv
  static std::shared_ptr<file_pv> load_file_pv_internal(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path, bool load_state);
};

/// \}

} // namespace noir::consensus::privval

NOIR_REFLECT(noir::consensus::privval::file_pv_key, address, pub_key, priv_key)
NOIR_REFLECT(noir::consensus::privval::file_pv_last_sign_state, height, round, step, signature, sign_bytes)

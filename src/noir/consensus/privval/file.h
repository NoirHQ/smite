// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/refl.h>
#include <noir/common/types.h>
#include <noir/consensus/crypto.h>
#include <noir/consensus/types/priv_validator.h>
#include <noir/p2p/protocol.h>
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
  std::string signature;
  Bytes signbytes; // hex? bytes?
  std::string file_path;

  /// \brief CheckHRS checks the given height, round, step (HRS) against that of the
  /// FilePVLastSignState. It returns an error if the arguments constitute a regression,
  /// or if they match but the SignBytes are empty.
  /// It panics if the HRS matches the arguments, there's a SignBytes, but no Signature.
  /// \param[in] height
  /// \param[in] round
  /// \param[in] step
  /// \return indicates whether the last Signature should be reused - true if the HRS matches the arguments and the
  /// SignBytes are not empty (indicating we have already signed for this HRS, and can reuse the existing signature)
  Result<bool> check_hrs(int64_t height_, int32_t round_, sign_step step_) const;

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
  file_pv(/* const */ noir::consensus::priv_key& priv_key,
    const std::filesystem::path& key_file_path,
    const std::filesystem::path& state_file_path)
    : key(file_pv_key{
        .priv_key = priv_key,
        .file_path = key_file_path,
      }),
      last_sign_state(file_pv_last_sign_state{
        .step = sign_step::none,
        .file_path = state_file_path,
      }) {}

  /// \brief generates a new validator with randomly generated private key and sets the filePaths, but does not call
  /// save()
  /// \todo need to check default key_type
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \param[in] key_type
  /// \return shared_ptr of file_pv
  static std::shared_ptr<file_pv> gen_file_pv(const std::filesystem::path& key_file_path,
    const std::filesystem::path& state_file_path,
    const std::string& key_type = "ed25519");

  /// \brief loads a FilePV from the filePaths. The FilePV handles double signing prevention by persisting data to the
  /// stateFilePath. If either file path does not exist, the program will exit.
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \return shared_ptr of file_pv
  static inline Result<std::shared_ptr<file_pv>> load_file_pv(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path) {
    return load_file_pv_internal(key_file_path, state_file_path, true);
  }

  /// \brief LoadFilePVEmptyState loads a FilePV from the given keyFilePath, with an empty LastSignState. If the
  /// keyFilePath does not exist, the program will exit.
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \return shared_ptr of file_pv
  static inline Result<std::shared_ptr<file_pv>> load_file_pv_empty_state(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path) {
    return load_file_pv_internal(key_file_path, state_file_path, false);
  }

  /// \brief persists the FilePv to its filePath.
  void save();

  /// \brief loads a FilePV from the given filePaths or else generates a new one and saves it to the filePaths.
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \return shared_ptr of file_pv
  static Result<std::shared_ptr<file_pv>> load_or_gen_file_pv(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path) {
    std::shared_ptr<file_pv> ret;
    if (std::filesystem::exists(key_file_path)) {
      return load_file_pv(key_file_path, state_file_path);
    } else {
      ret = gen_file_pv(key_file_path, state_file_path);
      check(ret != nullptr);
      ret->save();
    }
    return ret;
  }

  /// \brief returns type of priv_validator
  /// \return priv_validator_type
  priv_validator_type get_type() const override {
    return priv_validator_type::FileSignerClient;
  }

  /// \brief returns the address of the validator
  /// \return address
  Bytes get_address() const {
    return get_pub_key().address();
  }

  /// \brief returns the public key of the validator
  /// \return the public key of the validator
  pub_key get_pub_key() const override {
    return get_priv_key().get_pub_key();
  }

  /// \brief returns the private key of the validator
  /// \return the private key of the validator
  priv_key get_priv_key() const override {
    return key.priv_key;
  }

  /// \brief signs a canonical representation of the vote, along with the chainID
  /// \param[in] vote_
  /// \return
  std::optional<std::string> sign_vote(noir::consensus::vote& vote) override {
    if (!sign_vote_internal(vote)) {
      return "error signing vote";
    }
    return {};
  }

  /// \brief signs a canonical representation of the proposal, along with the chainID
  /// \param[in] proposal_
  /// \return
  std::optional<std::string> sign_proposal(const std::string& chain_id, noir::p2p::proposal_message& proposal) override {
    if (!sign_proposal_internal(chain_id, proposal)) {
      return "error signing proposal";
    }
    return {};
  }

  Result<Bytes> sign_vote_pb(const std::string& chain_id, const ::tendermint::types::Vote& v) override {
    return success();
  }

  /// \brief returns a string representation of the FilePV.
  /// \return string representation of the FilePV.
  inline std::string string() const;

  /// \brief resets all fields in the FilePV.
  /// \note Unsafe!
  void reset() {
    save_signed(0, 0, sign_step::none, Bytes{}, Bytes{});
  }

  /// \brief  persist height/round/step and signature
  /// \param[in] height
  /// \param[in] round
  /// \param[in] step
  /// \param[in] sign_bytes
  /// \param[in] sig
  void save_signed(int64_t height, int32_t round, sign_step step, const Bytes& sign_bytes, const Bytes& sig);

private:
  /// \brief load FilePV from given arguments
  /// \param[in] key_file_path
  /// \param[in] state_file_path
  /// \param[in] load_state if true, we load from the stateFilePath. Otherwise, we use an empty LastSignState
  /// \return shared_ptr of file_pv
  static Result<std::shared_ptr<file_pv>> load_file_pv_internal(
    const std::filesystem::path& key_file_path, const std::filesystem::path& state_file_path, bool load_state);

  /// \brief signVote checks if the vote is good to sign and sets the vote signature. It may need to set the timestamp
  /// as well if the vote is otherwise the same as a previously signed vote (ie. we crashed after signing but before the
  /// vote hit the WAL).
  /// \param[in] vote
  /// \return true on success, false otherwise
  bool sign_vote_internal(noir::consensus::vote& vote);

  /// \brief signProposal checks if the proposal is good to sign and sets the proposal signature.
  /// It may need to set the timestamp as well if the proposal is otherwise the same as
  /// a previously signed proposal ie. we crashed after signing but before the proposal hit the WAL).
  /// \param[in] proposal
  /// \return true on success, false otherwise
  bool sign_proposal_internal(const std::string& chain_id, noir::p2p::proposal_message& proposal);
};

/// \}

struct key_json_obj {
  std::string type;
  std::string value;
};

struct file_pv_key_json_obj {
  std::string address;
  key_json_obj pub_key;
  key_json_obj priv_key;
};

} // namespace noir::consensus::privval

NOIR_REFLECT(noir::consensus::privval::file_pv_key, priv_key);
NOIR_REFLECT(noir::consensus::privval::file_pv_last_sign_state, height, round, step, signature, signbytes);
NOIR_REFLECT(noir::consensus::privval::key_json_obj, type, value);
NOIR_REFLECT(noir::consensus::privval::file_pv_key_json_obj, address, pub_key, priv_key);

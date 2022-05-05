// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <noir/consensus/privval/file.h>
#include <noir/consensus/types/canonical.h>
#include <fc/variant_object.hpp>

#include <noir/common/helper/variant.h>
#include <noir/core/codec.h>

namespace noir::consensus::privval {
namespace fs = std::filesystem;

sign_step vote_to_step(const noir::consensus::vote& vote) {
  switch (vote.type) {
  case noir::p2p::signed_msg_type::Prevote:
    return sign_step::prevote;
  case noir::p2p::signed_msg_type::Precommit:
    return sign_step::precommit;
  default:
    check(false, "Unknown vote type: {}", static_cast<int8_t>(vote.type)); // TODO: handle panic
  }
  return sign_step::none;
}

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

result<bool> file_pv_last_sign_state::check_hrs(int64_t height_, int32_t round_, sign_step step_) const {
  if (height > height_)
    return make_unexpected(fmt::format("height regression. Got {}, last height {}", height_, height));

  if (height == height_) {
    if (round > round_)
      return make_unexpected(
        fmt::format("round regression at height {}. Got {}, last round {}", height_, round_, round));

    if (round == round_) {
      if (step > step_)
        return make_unexpected(fmt::format("step regression at height {} round {}. Got {}, last step {}", height_,
          round_, static_cast<int8_t>(step_), static_cast<int8_t>(step)));
      if (step == step_) {
        check(!sign_bytes.empty(), "no sign_bytes found");
        if (signature.empty()) {
          // panics if the HRS matches the arguments, there's a SignBytes, but no Signature
          elog("pv: Signature is nil but sign_bytes is not!");
          assert(false);
        }
        return true;
      }
    }
  }
  return false;
}

void file_pv_last_sign_state::save() {
  check(!file_path.empty(), "cannot save PrivValidator key: filePath not set");

  // FIXME: temporary create lss path dir here
  fs::path dir_path = fs::path{file_path}.remove_filename();
  if (!fs::exists(dir_path)) {
    fs::create_directories(dir_path);
  }

  // TODO: save file in thread safe context (eg.tendermint tempfile)
  // https://pkg.go.dev/github.com/tendermint/tendermint/internal/libs/tempfile
  fc::variant vo;
  fc::to_variant<file_pv_last_sign_state>(*this, vo);
  fc::json::save_to_file(vo, file_path);
}

bool file_pv_last_sign_state::load(const fs::path& state_file_path, file_pv_last_sign_state& lss) {
  try {
    fc::variant obj = fc::json::from_file(state_file_path.string());
    fc::from_variant(obj, lss);
  } catch (...) {
    elog(fmt::format("error reading PrivValidator state from {}", state_file_path.string()));
    return false;
  }
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

result<std::shared_ptr<file_pv>> file_pv::load_file_pv_internal(
  const fs::path& key_file_path, const fs::path& state_file_path, bool load_state) {
  auto ret = std::make_shared<file_pv>();
  if (!file_pv_key::load(key_file_path, ret->key))
    return make_unexpected(fmt::format("error reading PrivValidator key from {}", key_file_path.string()));
  // Overwrite pubkey and address for convenience
  ret->key.pub_key = ret->key.priv_key.get_pub_key();
  ret->key.address = ret->key.pub_key.address();
  if (load_state) {
    if (!file_pv_last_sign_state::load(state_file_path, ret->last_sign_state))
      return make_unexpected(fmt::format("error reading PrivValidator state from {}", state_file_path.string()));
  }
  return ret;
}

void file_pv::save() {
  key.save();
  last_sign_state.save();
}

std::string file_pv::string() const {
  return fmt::format("PrivValidator {} LH:{}, LR:{}, LS:{}", to_hex(key.address), last_sign_state.height,
    last_sign_state.round, static_cast<int8_t>(last_sign_state.step));
}

bool check_only_differ_by_timestamp(const noir::consensus::vote& obj,
  const bytes& last_sign_bytes,
  const bytes& new_sign_bytes,
  noir::p2p::tstamp& timestamp) {
  auto last_vote = decode<canonical_vote>(last_sign_bytes);
  auto new_vote = decode<canonical_vote>(new_sign_bytes);
  timestamp = last_vote.timestamp;
  // set the times to the same value and check equality
  auto now = get_time();
  last_vote.timestamp = now;
  new_vote.timestamp = now;

  // FIXME: compare reflected value before encoding
  auto lhs = encode<canonical_vote>(last_vote);
  auto rhs = encode<canonical_vote>(new_vote);
  return lhs == rhs;
}

bool check_only_differ_by_timestamp(const noir::consensus::proposal& obj,
  const bytes& last_sign_bytes,
  const bytes& new_sign_bytes,
  noir::p2p::tstamp& timestamp) {
  auto last_vote = decode<canonical_proposal>(last_sign_bytes);
  auto new_vote = decode<canonical_proposal>(new_sign_bytes);
  timestamp = last_vote.timestamp;
  // set the times to the same value and check equality
  auto now = get_time();
  last_vote.timestamp = now;
  new_vote.timestamp = now;

  // FIXME: compare reflected value before encoding
  auto lhs = encode<canonical_proposal>(last_vote);
  auto rhs = encode<canonical_proposal>(new_vote);
  return lhs == rhs;
}

template<typename T>
bool sign_internal(T& obj, const bytes& sign_bytes, file_pv& pv, sign_step step) {
  auto& key = pv.key;
  auto& lss = pv.last_sign_state;
  auto height = obj.height;
  auto round = obj.round;

  auto same_hrs = lss.check_hrs(height, round, step);
  if (!same_hrs) {
    elog(same_hrs.error());
    return false;
  }
  p2p::tstamp timestamp;

  // We might crash before writing to the wal,
  // causing us to try to re-sign for the same HRS.
  // If signbytes are the same, use the last signature.
  // If they only differ by timestamp, use last timestamp and signature
  // Otherwise, return error
  if (same_hrs.value()) {
    if (sign_bytes == lss.sign_bytes) {
      obj.signature = lss.signature;
    } else if (check_only_differ_by_timestamp(obj, lss.sign_bytes, sign_bytes, timestamp)) {
      obj.timestamp = timestamp;
      obj.signature = lss.signature;
    } else {
      check(false, "conflicting data");
      // return false; // TODO: return errors
    }
    return true;
  }

  // It passed the checks. Sign the vote
  auto sig = key.priv_key.sign(sign_bytes);
  pv.save_signed(height, round, step, sign_bytes, sig);
  obj.signature = sig;
  return true;
}

bool file_pv::sign_vote_internal(noir::consensus::vote& vote) {
  return sign_internal<noir::consensus::vote>(
    vote, encode(canonical::canonicalize_vote(vote)), *this, vote_to_step(vote));
}

bool file_pv::sign_proposal_internal(noir::consensus::proposal& proposal) {
  return sign_internal<noir::consensus::proposal>(
    proposal, encode(canonical::canonicalize_proposal(proposal)), *this, sign_step::propose);
}

void file_pv::save_signed(int64_t height, int32_t round, sign_step step, const bytes& sign_bytes, const bytes& sig) {
  last_sign_state.height = height;
  last_sign_state.round = round;
  last_sign_state.step = step;
  last_sign_state.signature = sig;
  last_sign_state.sign_bytes = sign_bytes;
  last_sign_state.save();
}

} // namespace noir::consensus::privval

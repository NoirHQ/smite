// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <noir/consensus/privval/file.h>
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/types/proposal.h>

#include <noir/common/helper/variant.h>
#include <noir/core/codec.h>

#include <cppcodec/base64_default_rfc4648.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

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
  file_pv_key_json_obj json_obj;
  json_obj.priv_key.value = base64::encode(priv_key.key.data(), priv_key.key.size());
  auto pub_key_ = priv_key.get_pub_key();
  json_obj.pub_key.value = base64::encode(pub_key_.key.data(), pub_key_.key.size());
  std::string addr = to_hex(pub_key_.address());
  std::transform(addr.begin(), addr.end(), addr.begin(), ::toupper);
  json_obj.address = addr;
  json_obj.priv_key.type = "tendermint/PrivKeyEd25519";
  json_obj.pub_key.type = "tendermint/PubKeyEd25519";
  fc::variant vo;
  fc::to_variant<file_pv_key_json_obj>(json_obj, vo);
  fc::json::save_to_file(vo, file_path);
}

bool file_pv_key::load(const fs::path& key_file_path, file_pv_key& priv_key) {
  fc::variant obj = fc::json::from_file(key_file_path.string());
  file_pv_key_json_obj json_obj;
  fc::from_variant(obj, json_obj);
  auto priv_key_str = base64::decode(json_obj.priv_key.value);
  priv_key.priv_key.key = Bytes(priv_key_str.begin(), priv_key_str.end());
  priv_key.file_path = key_file_path.string();
  return true;
}

Result<bool> file_pv_last_sign_state::check_hrs(int64_t height_, int32_t round_, sign_step step_) const {
  if (height > height_)
    return Error::format("height regression. Got {}, last height {}", height_, height);

  if (height == height_) {
    if (round > round_)
      return Error::format("round regression at height {}. Got {}, last round {}", height_, round_, round);

    if (round == round_) {
      if (step > step_)
        return Error::format("step regression at height {} round {}. Got {}, last step {}", height_, round_,
          static_cast<int8_t>(step_), static_cast<int8_t>(step));
      if (step == step_) {
        check(!signbytes.empty(), "no signbytes found");
        if (signature.empty()) {
          // panics if the HRS matches the arguments, there's a SignBytes, but no Signature
          elog("pv: Signature is nil but signbytes is not!");
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
  auto priv_key_ = priv_key::new_priv_key();
  if (key_type == "ed25519") {
    // priv_key_.type = "ed25519";
  } else {
    elog(fmt::format("key type: {} is not supported", key_type)); // return err
    return nullptr;
  }
  return std::make_shared<file_pv>(priv_key_, key_file_path, state_file_path);
}

Result<std::shared_ptr<file_pv>> file_pv::load_file_pv_internal(
  const fs::path& key_file_path, const fs::path& state_file_path, bool load_state) {
  auto ret = std::make_shared<file_pv>();
  if (!file_pv_key::load(key_file_path, ret->key))
    return Error::format("error reading PrivValidator key from {}", key_file_path.string());
  // Overwrite pubkey and address for convenience
  if (load_state) {
    if (!file_pv_last_sign_state::load(state_file_path, ret->last_sign_state))
      return Error::format("error reading PrivValidator state from {}", state_file_path.string());
  }
  return ret;
}

void file_pv::save() {
  key.save();
  last_sign_state.save();
}

std::string file_pv::string() const {
  return fmt::format("PrivValidator {} LH:{}, LR:{}, LS:{}", to_hex(get_address()), last_sign_state.height,
    last_sign_state.round, static_cast<int8_t>(last_sign_state.step));
}

bool check_only_differ_by_timestamp(const noir::consensus::vote& obj,
  const Bytes& last_sign_bytes,
  const Bytes& new_sign_bytes,
  noir::tstamp& timestamp) {
  ::tendermint::types::CanonicalVote last_vote;
  last_vote.ParseFromArray(last_sign_bytes.data(), last_sign_bytes.size());
  ::tendermint::types::CanonicalVote new_vote;
  new_vote.ParseFromArray(new_sign_bytes.data(), new_sign_bytes.size());
  timestamp = ::google::protobuf::util::TimeUtil::TimestampToMicroseconds(last_vote.timestamp());
  // set times to the same value and check equality
  auto now = get_time();
  *last_vote.mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(now);
  *new_vote.mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(now);
  return last_vote.SerializeAsString() == new_vote.SerializeAsString();
}

bool check_only_differ_by_timestamp(const noir::p2p::proposal_message& obj,
  const Bytes& last_sign_bytes,
  const Bytes& new_sign_bytes,
  noir::tstamp& timestamp) {
  ::tendermint::types::CanonicalProposal last_vote;
  last_vote.ParseFromArray(last_sign_bytes.data(), last_sign_bytes.size());
  ::tendermint::types::CanonicalProposal new_vote;
  new_vote.ParseFromArray(new_sign_bytes.data(), new_sign_bytes.size());
  timestamp = ::google::protobuf::util::TimeUtil::TimestampToMicroseconds(last_vote.timestamp());
  // set times to the same value and check equality
  auto now = get_time();
  *last_vote.mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(now);
  *new_vote.mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(now);
  return last_vote.SerializeAsString() == new_vote.SerializeAsString();
}

template<typename T>
Result<void> sign_internal(T& obj, const Bytes& sign_bytes, file_pv& pv, sign_step step) {
  auto& key = pv.key;
  auto& lss = pv.last_sign_state;
  auto height = obj.height;
  auto round = obj.round;

  auto same_hrs = lss.check_hrs(height, round, step);
  if (!same_hrs)
    return same_hrs.error();

  // We might crash before writing to the wal,
  // causing us to try to re-sign for the same HRS.
  // If signbytes are the same, use the last signature.
  // If they only differ by timestamp, use last timestamp and signature
  // Otherwise, return error
  if (same_hrs.value()) {
    tstamp timestamp;
    auto decode_sig = base64::decode(lss.signature);
    if (sign_bytes == lss.signbytes) {
      obj.signature = {decode_sig.begin(), decode_sig.end()};
    } else if (check_only_differ_by_timestamp(obj, lss.signbytes, sign_bytes, timestamp)) {
      obj.timestamp = timestamp;
      obj.signature = {decode_sig.begin(), decode_sig.end()};
    } else {
      return Error::format("conflicting data");
    }
    return success();
  }

  // It passed the checks. Sign the vote
  auto sig = key.priv_key.sign(sign_bytes);
  pv.save_signed(height, round, step, sign_bytes, sig);
  auto decode_sig = base64::decode(lss.signature);
  obj.signature = {decode_sig.begin(), decode_sig.end()};
  return success();
}

Result<void> file_pv::sign_vote_internal(const std::string& chain_id, noir::consensus::vote& vote) {
  return sign_internal<noir::consensus::vote>(
    vote, vote::vote_sign_bytes(chain_id, *vote::to_proto(vote)), *this, vote_to_step(vote));
}

Result<void> file_pv::sign_proposal_internal(const std::string& chain_id, noir::p2p::proposal_message& msg) {
  return sign_internal<noir::p2p::proposal_message>(
    msg, proposal::proposal_sign_bytes(chain_id, *proposal::to_proto({msg})), *this, sign_step::propose);
}

void file_pv::save_signed(int64_t height, int32_t round, sign_step step, const Bytes& sign_bytes, const Bytes& sig) {
  last_sign_state.height = height;
  last_sign_state.round = round;
  last_sign_state.step = step;
  last_sign_state.signature = base64::encode(sig.data(), sig.size());
  last_sign_state.signbytes = sign_bytes;
  last_sign_state.save();
}

} // namespace noir::consensus::privval

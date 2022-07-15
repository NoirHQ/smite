// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <fmt/core.h>

#include <noir/common/hex.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/state.h>
#include <noir/core/codec.h>
#include <noir/db/rocks_session.h>
#include <noir/db/session.h>

namespace noir::consensus {

/// \addtogroup consensus
/// \{

/// \brief Store defines the state store interface
/// It is used to retrieve current state and save and load ABCI responses,
/// validators and consensus parameters
class state_store {
public:
  /// \brief Load loads the current state of the blockchain
  /// \return true on success, false otherwise
  virtual bool load(state& st) const = 0;
  /// \brief LoadValidators loads the validator set at a given height
  /// \param[in] height
  /// \param[out] v_set
  /// \return true on success, false otherwise
  virtual bool load_validators(int64_t height, std::shared_ptr<validator_set>& v_set) const = 0;
  /// \brief LoadABCIResponses loads the abciResponse for a given height
  /// \param[in] height
  /// \param[out] abci_rsp
  /// \return true on success, false otherwise
  virtual bool load_abci_responses(int64_t height, tendermint::state::ABCIResponses& abci_rsp) const = 0;
  /// \brief LoadConsensusParams loads the consensus params for a given height
  /// \param[in] height
  /// \param[out] cs_param
  /// \return true on success, false otherwise
  virtual bool load_consensus_params(int64_t height, consensus_params& cs_param) const = 0;
  /// \brief Save overwrites the previous state with the updated one
  /// \param[in] st
  /// \return true on success, false otherwise
  virtual bool save(const state& st) = 0;
  /// \brief SaveABCIResponses saves ABCIResponses for a given height
  /// \param[in] height
  /// \param[in] abci_rsp
  /// \return true on success, false otherwise
  virtual bool save_abci_responses(int64_t height, const tendermint::state::ABCIResponses& abci_rsp) = 0;
  /// \brief SaveValidatorSet saves the validator set at a given height
  /// \param[in] lower_height
  /// \param[in] upper_height
  /// \param[in] v_set
  /// \return true on success, false otherwise
  virtual bool save_validator_sets(
    int64_t lower_height, int64_t upper_height, const std::shared_ptr<validator_set>& v_set) = 0;
  /// \brief Bootstrap is used for bootstrapping state when not starting from a initial height.
  /// \param[in] st
  /// \return true on success, false otherwise
  virtual bool bootstrap(const state& st) = 0;
  /// \brief PruneStates takes the height from which to prune up to (exclusive)
  /// \param[in] height
  /// \return true on success, false otherwise
  virtual bool prune_states(int64_t height) = 0;
};

class db_store : public state_store {
  using db_session_type = noir::db::session::session<noir::db::session::rocksdb_t>;

private:
  using batch_type = std::vector<std::pair<Bytes, Bytes>>;

  // TEMP struct for encoding
  struct consensus_params_info {
    int64_t last_height_changed;
    std::optional<consensus_params> cs_param;
  };

public:
  explicit db_store(std::shared_ptr<db_session_type> session_)
    : db_session_(std::move(session_)), state_key_(encode(static_cast<char>(prefix::state))) {}

  db_store(db_store&& other) noexcept
    : db_session_(std::move(other.db_session_)), state_key_(encode(static_cast<char>(prefix::state))) {
    other.db_session_ = nullptr;
  }

  db_store(const db_store& other) noexcept
    : db_session_(other.db_session_), state_key_(encode(static_cast<char>(prefix::state))) {}

  bool load(state& st) const override {
    return load_internal(st);
  }

  bool load_validators(int64_t height, std::shared_ptr<validator_set>& v_set) const override {
    tendermint::state::ValidatorsInfo v_info;
    if (!load_validators_info(height, v_info))
      return false;
    if (!v_info.has_validator_set()) {
      int64_t last_stored_height = last_stored_height_for(height, v_info.last_height_changed());
      tendermint::state::ValidatorsInfo v_info2;
      if (!load_validators_info(last_stored_height, v_info2))
        return false;
      if (!v_info2.has_validator_set())
        return false;
      auto vs = validator_set::from_proto(v_info2.validator_set());
      // check(vs, "load_validators failed");
      v_set = vs.value();
      v_set->increment_proposer_priority(static_cast<int32_t>(height - v_info2.last_height_changed()));
    } else {
      v_set = validator_set::from_proto(v_info.validator_set()).value();
    }
    return true;
  }

  bool load_abci_responses(int64_t height, tendermint::state::ABCIResponses& rsp) const override {
    return load_abci_response_internal(height, rsp);
  }

  bool load_consensus_params(int64_t height, consensus_params& cs_param) const override {
    consensus_params_info cs_param_info{};
    if (auto ret = load_consensus_params_info(height, cs_param_info); !ret) {
      return false;
    }
    if (cs_param_info.cs_param == std::nullopt) {
      if (auto ret = load_consensus_params_info(cs_param_info.last_height_changed, cs_param_info);
          !ret || cs_param_info.cs_param == std::nullopt) {
        return false;
      }
    }
    cs_param = *cs_param_info.cs_param;

    return true;
  }

  // Save persists the State, the ValidatorsInfo, and the ConsensusParamsInfo to the database.
  // This flushes the writes (e.g. calls SetSync).
  bool save(const state& st) override {
    return save_internal(st);
  }

  bool save_abci_responses(int64_t height, const tendermint::state::ABCIResponses& rsp) override {
    return save_abci_responses_internal(height, rsp);
  }

  bool save_validator_sets(
    int64_t lower_height, int64_t upper_height, const std::shared_ptr<validator_set>& v_set) override {
    batch_type batch{};
    for (auto height = lower_height; height <= upper_height; ++height) {
      if (!save_validators_info(height, lower_height, v_set, batch)) {
        return false;
      }
    }
    db_session_->write_from_bytes(batch);
    db_session_->commit();
    return true;
  }

  bool bootstrap(const state& st) override {
    return bootstrap_internal(st);
  }

  bool prune_states(int64_t retain_height) override {
    if (retain_height <= 0) {
      return false;
    }

    if (!prune_consensus_param(retain_height)) {
      return false;
    }
    if (!prune_validator_sets(retain_height)) {
      return false;
    }
    if (!prune_abci_response(retain_height)) {
      return false;
    }
    return true;
  }

private:
  enum class prefix : char {
    validators = 5,
    consensus_params = 6,
    abci_response = 7,
    state = 8,
  };
  static constexpr int val_set_checkpoint_interval = 100000;
  std::shared_ptr<db_session_type> db_session_;
  Bytes state_key_;

  template<prefix key_prefix>
  static Bytes encode_key(int64_t val) {
    Bytes ret{};
    auto hex_ = hex::decode(fmt::format("{:016x}", static_cast<uint64_t>(val)));
    ret.raw().push_back(static_cast<char>(key_prefix));
    ret.raw().insert(ret.end(), hex_.begin(), hex_.end());
    return ret;
  }

  bool save_internal(const state& st) {
    batch_type batch{};
    auto next_height = st.last_block_height + 1;
    if (next_height == 1) {
      next_height = st.initial_height;
      if (!save_validators_info(next_height, next_height, st.validators, batch)) {
        return false;
      }
    }
    if (!save_validators_info(next_height + 1, st.last_height_validators_changed, st.next_validators, batch)) {
      return false;
    }

    if (!save_consensus_params_info(
          next_height, st.last_height_consensus_params_changed, st.consensus_params_, batch)) {
      return false;
    }

    batch.emplace_back(state_key_, encode(st));
    db_session_->write_from_bytes(batch);
    db_session_->commit();
    return true;
  }

  bool bootstrap_internal(const state& st) {
    batch_type batch{};
    auto height = st.last_block_height + 1;
    if (height == 1) {
      height = st.initial_height;
    } else if (st.last_validators && !st.last_validators->validators.empty()) { // height > 1, can height < 0 ?
      if (!save_validators_info(height - 1, height - 1, st.last_validators, batch)) {
        return false;
      }
    }
    if (!save_validators_info(height, height, st.validators, batch)) {
      return false;
    }
    if (!save_validators_info(height + 1, height + 1, st.next_validators, batch)) {
      return false;
    }
    if (!save_consensus_params_info(height, st.last_height_consensus_params_changed, st.consensus_params_, batch)) {
      return false;
    }
    batch.emplace_back(state_key_, encode(st));
    db_session_->write_from_bytes(batch);
    db_session_->commit();
    return true;
  }

  bool load_internal(state& st) const {
    auto ret = db_session_->read_from_bytes(state_key_);
    if (ret == std::nullopt || ret->size() == 0) {
      return false;
    }
    st = decode<state>(ret.value());
    return true;
  }

  bool save_validators_info(
    int64_t height, int64_t last_height_changed, const std::shared_ptr<validator_set>& v_set, batch_type& batch) {
    if (last_height_changed > height)
      return false;
    tendermint::state::ValidatorsInfo val_info;
    val_info.set_last_height_changed(last_height_changed);
    if (height == last_height_changed || height % val_set_checkpoint_interval == 0) {
      if (!v_set)
        val_info.mutable_validator_set(); // set empty validator_set
      else if (!v_set->validators.empty())
        val_info.set_allocated_validator_set(validator_set::to_proto(v_set).release());
    }
    Bytes bz(val_info.ByteSizeLong());
    val_info.SerializeToArray(bz.data(), val_info.ByteSizeLong());
    batch.emplace_back(encode_key<prefix::validators>(height), bz);
    return true;
  } // namespace noir::consensus

  bool load_validators_info(int64_t height, tendermint::state::ValidatorsInfo& val_info) const {
    auto ret = db_session_->read_from_bytes(encode_key<prefix::validators>(height));
    if (ret == std::nullopt || ret->size() == 0)
      return false;
    val_info.ParseFromArray(ret.value().data(), ret.value().size());
    return true;
  }

  static int64_t last_stored_height_for(int64_t height, int64_t last_height_changed) {
    int64_t checkpoint_height = height - height % val_set_checkpoint_interval;
    return std::max(checkpoint_height, last_height_changed);
  }

  bool save_consensus_params_info(
    int64_t next_height, int64_t change_height, const consensus_params& cs_params, batch_type& batch) {
    consensus_params_info cs_param_info{
      .last_height_changed = change_height,
      .cs_param = (change_height == next_height) ? std::optional<consensus_params>(cs_params) : std::nullopt,
    };
    auto buf = encode(cs_param_info);
    batch.emplace_back(encode_key<prefix::consensus_params>(next_height), buf);
    return true;
  }

  bool load_consensus_params_info(int64_t height, consensus_params_info& cs_param_info) const {
    auto ret = db_session_->read_from_bytes(encode_key<prefix::consensus_params>(height));
    if (ret == std::nullopt || ret->size() == 0) {
      return false;
    }
    cs_param_info = decode<consensus_params_info>(ret.value());
    return true;
  }

  bool save_abci_responses_internal(int64_t height, const tendermint::state::ABCIResponses& rsp) {
    Bytes buf(rsp.ByteSizeLong());
    rsp.SerializeToArray(buf.data(), rsp.ByteSizeLong());
    db_session_->write_from_bytes(encode_key<prefix::abci_response>(height), buf);
    db_session_->commit();
    return true;
  }

  bool load_abci_response_internal(int64_t height, tendermint::state::ABCIResponses& rsp) const {
    auto ret = db_session_->read_from_bytes(encode_key<prefix::abci_response>(height));
    if (ret == std::nullopt || ret->size() == 0) {
      return false;
    }
    rsp.ParseFromArray(ret.value().data(), ret.value().size());
    return true;
  }

  bool prune_consensus_param(int64_t retain_height) {
    consensus_params_info cs_info{};
    if (!load_consensus_params_info(retain_height, cs_info)) {
      return false;
    }
    if (cs_info.cs_param == std::nullopt) {
      if (auto ret = load_consensus_params_info(cs_info.last_height_changed, cs_info);
          !ret || (cs_info.cs_param == std::nullopt)) {
        return false;
      }

      if (!prune_range<prefix::consensus_params>(cs_info.last_height_changed + 1, retain_height)) {
        return false;
      }
    }

    return prune_range<prefix::consensus_params>(1, cs_info.last_height_changed);
  }

  bool prune_validator_sets(int64_t retain_height) {
    tendermint::state::ValidatorsInfo val_info{};
    if (!load_validators_info(retain_height, val_info))
      return false;
    int64_t last_recorded_height = last_stored_height_for(retain_height, val_info.last_height_changed());
    if (!val_info.has_validator_set()) {
      if (auto ret = load_validators_info(last_recorded_height, val_info); !ret || (!val_info.has_validator_set()))
        return false;
      if (last_recorded_height < retain_height) {
        if (!prune_range<prefix::validators>(last_recorded_height + 1, retain_height))
          return false;
      }
    }
    return prune_range<prefix::validators>(1, last_recorded_height);
  }

  bool prune_abci_response(int64_t height) {
    return prune_range<prefix::abci_response>(1, height);
  }

  template<prefix key_prefix>
  bool prune_range(int64_t start_, int64_t end_) {
    Bytes start;
    Bytes end;
    start = encode_key<key_prefix>(start_);
    end = encode_key<key_prefix>(end_);
    while (start != end) { // TODO: change to safer
      if (!reverse_batch_delete(start, end, end)) {
        return false;
      }
    }
    db_session_->commit();
    return true;
  }

  bool reverse_batch_delete(const Bytes& start, const Bytes& end, Bytes& new_end) {
    auto start_it = db_session_->lower_bound_from_bytes(start);
    auto end_it = db_session_->lower_bound_from_bytes(end);
    if (end_it == db_session_->begin()) {
      return false;
    }
    --end_it;
    size_t size = 0;

    for (auto it = end_it;; --it) {
      db_session_->erase(it.key());
      if (++size == 1000) {
        auto key_ = it.key();
        new_end = Bytes{std::vector<unsigned char>{key_.begin(), key_.end()}};
        break;
      } else if (it == start_it) {
        new_end = start;
        break;
      }
    }

    return true;
  }
};

/// }

} // namespace noir::consensus

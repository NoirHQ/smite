// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/codec/scale.h>
#include <noir/consensus/state.h>
#include <noir/consensus/db/db.h>

namespace noir::consensus {

class state_store {
public:
  virtual bool load(state&) const = 0;
  virtual bool load_validators(int64_t) const = 0;
  virtual bool load_abci_responses(int64_t) const = 0;
  virtual bool load_consensus_params(int64_t) const = 0;
  virtual bool save(state) = 0;
  virtual bool save_abci_responses(state) = 0;
  virtual bool save_validator_sets(state) = 0;
  virtual bool bootstrap(state) = 0;
  virtual bool prune_states(state) = 0;
};

class db_store : public state_store {
public:
  db_store(std::string db_type = "simple") : _db(new noir::consensus::simple_db) {}

  db_store(db_store&& other) noexcept: _db(std::move(other._db)) {}

  bool load(state& st) const override {
    return true;
  }

  bool load_validators(int64_t height) const override {
    return true;
  }

  bool load_abci_responses(int64_t height) const override {
    return true;
  }

  bool load_consensus_params(int64_t height) const override {
    return true;
  }

  bool save(state st) override {
    return true;
  }

  bool save_abci_responses(state st) override {
    return true;
  }

  bool save_validator_sets(state st) override {
    return true;
  }

  bool bootstrap(state st) override {
    return true;
  }

  bool prune_states(state st) override {
    return true;
  }

private:
  std::unique_ptr<noir::consensus::db> _db;
};

} // namespace noir::consensus

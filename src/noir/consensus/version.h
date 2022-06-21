// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <tendermint/state/types.pb.h>
#include <tendermint/version/types.pb.h>

namespace noir::consensus {

constexpr std::string_view tm_version_default = "0.35.0-unreleased";
constexpr std::string_view abci_sem_ver = "0.17.0";
constexpr std::string_view abci_version = abci_sem_ver;

constexpr uint64_t p2p_protocol = 8;
constexpr uint64_t block_protocol = 11;

struct consensus_version {
  uint64_t block;
  uint64_t app;

  static std::unique_ptr<::tendermint::version::Consensus> to_proto(const consensus_version& c) {
    auto ret = std::make_unique<::tendermint::version::Consensus>();
    ret->set_block(c.block);
    ret->set_app(c.app);
    return ret;
  }

  static std::unique_ptr<consensus_version> from_proto(const ::tendermint::version::Consensus& pb) {
    auto ret = std::make_unique<consensus_version>();
    ret->block = pb.block();
    ret->app = pb.app();
    return ret;
  }
};

struct version {
  consensus_version cs;
  std::string software;

  static version init_state_version() {
    return {{.block = block_protocol, .app = 0}, std::string(tm_version_default)};
  }

  static std::unique_ptr<::tendermint::state::Version> to_proto(const version& v) {
    auto ret = std::make_unique<::tendermint::state::Version>();
    auto c = ret->mutable_consensus();
    c->set_block(v.cs.block);
    c->set_app(v.cs.app);
    ret->set_software(v.software);
    return ret;
  }

  static std::unique_ptr<version> from_proto(const ::tendermint::state::Version& pb) {
    auto ret = std::make_unique<version>();
    ret->cs.block = pb.consensus().block();
    ret->cs.app = pb.consensus().app();
    ret->software = pb.software();
    return ret;
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::consensus_version, block, app);
NOIR_REFLECT(noir::consensus::version, cs, software);

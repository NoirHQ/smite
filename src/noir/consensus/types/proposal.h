// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

struct proposal : p2p::proposal_message {

  static std::shared_ptr<proposal> new_proposal(
    int64_t height_, int32_t round_, int32_t pol_round_, p2p::block_id block_id_) {
    auto ret = std::make_shared<proposal>();
    ret->type = p2p::Proposal;
    ret->height = height_;
    ret->round = round_;
    ret->pol_round = pol_round_;
    ret->block_id_ = block_id_;
    ret->timestamp = get_time();
    return ret;
  }

  Result<void> validate_basic() {
    // TODO : implement
    return success();
  }

  static std::unique_ptr<::tendermint::types::Proposal> to_proto(const proposal& p) {
    auto ret = std::make_unique<::tendermint::types::Proposal>();
    ret->set_type(static_cast<tendermint::types::SignedMsgType>(p.type));
    ret->set_height(p.height);
    ret->set_round(p.round);
    ret->set_pol_round(p.pol_round);
    ret->set_allocated_block_id(p2p::block_id::to_proto(p.block_id_).release());
    *ret->mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(p.timestamp);
    ret->set_signature({p.signature.begin(), p.signature.end()});
    return ret;
  }

  static std::shared_ptr<proposal> from_proto(const ::tendermint::types::Proposal& pb) {
    auto ret = std::make_shared<proposal>();
    ret->type = (p2p::signed_msg_type)pb.type();
    ret->height = pb.height();
    ret->round = pb.round();
    ret->pol_round = pb.pol_round();
    ret->block_id_ = *p2p::block_id::from_proto(pb.block_id());
    ret->timestamp = ::google::protobuf::util::TimeUtil::TimestampToMicroseconds(pb.timestamp());
    ret->signature = pb.signature();
    return ret;
  }

  static Bytes proposal_sign_bytes(const std::string& chain_id, const ::tendermint::types::Proposal& p);
};

} // namespace noir::consensus

NOIR_REFLECT_DERIVED(noir::consensus::proposal, noir::p2p::proposal_message);

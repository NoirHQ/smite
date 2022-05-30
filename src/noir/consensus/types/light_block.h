// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/validator.h>

namespace noir::consensus {

/// \brief SignedHeader is a header along with the commits that prove it.
struct signed_header {
  std::shared_ptr<noir::consensus::block_header> header;
  std::optional<noir::consensus::commit> commit;

  result<void> validate_basic(std::string chain_id) {
    return {}; // TODO
  }

  static std::unique_ptr<::tendermint::types::SignedHeader> to_proto(const signed_header& s) {
    auto ret = std::make_unique<::tendermint::types::SignedHeader>();
    ret->set_allocated_header(block_header::to_proto(*s.header).release());
    if (s.commit.has_value())
      ret->set_allocated_commit(commit::to_proto(s.commit.value()).release());
    return ret;
  }

  static std::shared_ptr<signed_header> from_proto(const ::tendermint::types::SignedHeader& pb) {
    auto ret = std::make_shared<signed_header>();
    if (pb.has_header())
      ret->header = block_header::from_proto(pb.header());
    if (pb.has_commit())
      ret->commit = *commit::from_proto(pb.commit());
    return ret;
  }
};

struct light_block {
  std::shared_ptr<signed_header> s_header;
  std::shared_ptr<validator_set> val_set;

  result<void> validate_basic(std::string chain_id) {
    if (!s_header)
      return make_unexpected("missing signed header");
    if (!val_set)
      return make_unexpected("missing validator set");
    if (auto ok = s_header->validate_basic(chain_id); !ok)
      return make_unexpected(fmt::format("invalid signed header: {}", ok.error()));
    if (auto ok = val_set->validate_basic(); !ok)
      return make_unexpected(fmt::format("invalid validator set: {}", ok.error()));
    if (auto val_set_hash = val_set->get_hash(); val_set_hash != s_header->header->validators_hash)
      return make_unexpected(fmt::format("miss match of hash between validator set hash"));
    return {};
  }

  static std::unique_ptr<::tendermint::types::LightBlock> to_proto(const light_block& l) {
    auto ret = std::make_unique<::tendermint::types::LightBlock>();
    if (!l.s_header)
      ret->set_allocated_signed_header(signed_header::to_proto(*l.s_header).release());
    if (!l.val_set)
      ret->set_allocated_validator_set(validator_set::to_proto(*l.val_set).release());
    return ret;
  }

  static result<std::shared_ptr<light_block>> from_proto(const ::tendermint::types::LightBlock& pb) {
    auto ret = std::make_shared<light_block>();
    if (pb.has_signed_header())
      ret->s_header = signed_header::from_proto(pb.signed_header());
    if (pb.has_validator_set()) {
      if (auto ok = validator_set::from_proto(pb.validator_set()); !ok)
        return make_unexpected(ok.error());
      else
        ret->val_set = ok.value();
    }
    return ret;
  }
};

} // namespace noir::consensus

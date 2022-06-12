// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/merkle/tree.h>
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/validation.h>
#include <noir/consensus/types/validator.h>
#include <noir/core/codec.h>

namespace noir::consensus {

Bytes validator_set::get_hash() {
  merkle::bytes_list items;
  for (const auto& val : validators) {
    auto bz = encode(val);
    items.push_back(bz);
  }
  return merkle::hash_from_bytes_list(items);
}

Result<void> validator_set::verify_commit_light(
  const std::string& chain_id_, p2p::block_id block_id_, int64_t height, const std::shared_ptr<commit>& commit_) {
  auto vals = std::make_shared<validator_set>(*this);
  auto err = noir::consensus::verify_commit_light(chain_id_, vals, block_id_, height, commit_);
  if (err.has_value())
    return Error::format("{}", err.value());
  return success();
}

Result<void> validator_set::verify_commit_light_trusting(
  const std::string& chain_id_, const std::shared_ptr<commit>& commit_) {
  auto vals = std::make_shared<validator_set>(*this);
  return noir::consensus::verify_commit_light_trusting(chain_id_, vals, commit_);
}

} // namespace noir::consensus

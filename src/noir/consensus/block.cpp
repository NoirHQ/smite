// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/consensus/block.h>

#include <noir/codec/scale.h>

namespace noir::consensus {

std::shared_ptr<part_set> block::make_part_set(uint32_t part_size) {
  std::lock_guard<std::mutex> g(mtx);
  auto bz = codec::scale::encode(*this);
  return part_set::new_part_set_from_data(bz, part_size);
}

} // namespace noir::consensus

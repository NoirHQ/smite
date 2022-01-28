// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/codec/scale.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

/// \addtogroup consensus
/// \{

/// \brief BlockMeta contains meta information.
struct block_meta {
  noir::p2p::block_id bl_id;
  int32_t bl_size;
  noir::consensus::block_header header;
  int32_t num_txs;
  static block_meta new_block_meta(const block& bl_, const part_set& bl_parts) {
    auto hash_ = const_cast<block&>(bl_).get_hash();
    auto parts_ = const_cast<part_set&>(bl_parts);
    return {
      .bl_id = noir::p2p::block_id{.hash{hash_}, .parts{parts_.header()}},
      .bl_size = static_cast<int32_t>(noir::codec::scale::encode_size(bl_)),
      .header = bl_.header,
      .num_txs = 0, // TODO: bl_.data.size()
    };
  }
};

/// \}

}

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/basic_datastream.h>
#include <noir/codec/protobuf.h>
#include <noir/common/varint.h>
#include <noir/consensus/types/canonical.h>
#include <noir/consensus/types/proposal.h>

namespace noir::consensus {

Bytes proposal::proposal_sign_bytes(const std::string& chain_id, const ::tendermint::types::Proposal& p) {
  auto pb = canonical::canonicalize_proposal_pb(chain_id, p);
  auto bz = codec::protobuf::encode(pb);

  Bytes buf_size(10);
  codec::BasicDatastream<unsigned char> ds(buf_size);
  auto n = write_uleb128(ds, Varuint64(bz.size()));
  Bytes sign_bytes(n.value() + bz.size());
  std::copy(buf_size.begin(), buf_size.end(), sign_bytes.begin());
  std::copy(bz.begin(), bz.end(), sign_bytes.begin() + n.value());
  return sign_bytes;
}

} // namespace noir::consensus

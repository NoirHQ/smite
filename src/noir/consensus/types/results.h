// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/merkle/tree.h>
#include <tendermint/abci/types.pb.h>

namespace noir::consensus {

using abci_dtxs = std::vector<std::shared_ptr<tendermint::abci::ResponseDeliverTx>>;

struct abci_results {

  abci_dtxs results;

  static abci_results new_results(
    const google::protobuf::RepeatedPtrField<tendermint::abci::ResponseDeliverTx>& responses) {
    abci_dtxs new_res(responses.size());
    for (auto i = 0; i < responses.size(); i++)
      new_res[i] = deterministic_response_deliver_tx(responses[i]);
    return {.results = new_res};
  }

  static std::shared_ptr<tendermint::abci::ResponseDeliverTx> deterministic_response_deliver_tx(
    const tendermint::abci::ResponseDeliverTx& response) {
    auto ret = std::make_shared<tendermint::abci::ResponseDeliverTx>();
    ret->set_code(response.code());
    ret->set_data(response.data());
    ret->set_gas_wanted(response.gas_wanted());
    ret->set_gas_used(response.gas_used());
    return ret;
  }

  Bytes get_hash() {
    return merkle::hash_from_bytes_list(to_byte_slices());
  }

  std::vector<Bytes> to_byte_slices() {
    std::vector<Bytes> bzs;
    for (auto& r : results) {
      Bytes bz(r->ByteSizeLong());
      r->SerializeToArray(bz.data(), r->ByteSizeLong());
      bzs.push_back(bz);
    }
    return bzs;
  }
};

} // namespace noir::consensus

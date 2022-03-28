// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/abci/types/messages.h>

namespace tendermint::abci {

std::unique_ptr<Request> to_request_echo(const std::string& message) {
  auto ret = std::make_unique<Request>();
  ret->mutable_echo()->set_message(message);
  return ret;
}

std::unique_ptr<Request> to_request_flush() {
  auto ret = std::make_unique<Request>();
  ret->mutable_flush();
  return ret;
}

std::unique_ptr<Request> to_request_info(const RequestInfo& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_info()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_set_option(const RequestSetOption& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_set_option()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_deliver_tx(const RequestDeliverTx& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_deliver_tx()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_check_tx(const RequestCheckTx& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_check_tx()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_commit(const RequestCommit& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_commit()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_query(const RequestQuery& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_query()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_init_chain(const RequestInitChain& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_init_chain()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_begin_block(const RequestBeginBlock& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_begin_block()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_end_block(const RequestEndBlock& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_end_block()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_list_snapshots(const RequestListSnapshots& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_list_snapshots()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_offer_snapshot(const RequestOfferSnapshot& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_offer_snapshot()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_load_snapshot_chunk(const RequestLoadSnapshotChunk& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_load_snapshot_chunk()->CopyFrom(req);
  return ret;
}

std::unique_ptr<Request> to_request_apply_snapshot_chunk(const RequestApplySnapshotChunk& req) {
  auto ret = std::make_unique<Request>();
  ret->mutable_apply_snapshot_chunk()->CopyFrom(req);
  return ret;
}

} // namespace tendermint::abci

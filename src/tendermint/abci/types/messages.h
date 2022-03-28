// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/abci/types.pb.h>
#include <tendermint/common/common.h>

namespace tendermint::abci {

std::unique_ptr<Request> to_request_echo(const std::string& message);
std::unique_ptr<Request> to_request_flush();
std::unique_ptr<Request> to_request_info(const RequestInfo& req);
std::unique_ptr<Request> to_request_set_option(const RequestSetOption& req);
std::unique_ptr<Request> to_request_deliver_tx(const RequestDeliverTx& req);
std::unique_ptr<Request> to_request_check_tx(const RequestCheckTx& req);
std::unique_ptr<Request> to_request_commit(const RequestCommit& req);
std::unique_ptr<Request> to_request_query(const RequestQuery& req);
std::unique_ptr<Request> to_request_init_chain(const RequestInitChain& req);
std::unique_ptr<Request> to_request_begin_block(const RequestBeginBlock& req);
std::unique_ptr<Request> to_request_end_block(const RequestEndBlock& req);
std::unique_ptr<Request> to_request_list_snapshots(const RequestListSnapshots& req);
std::unique_ptr<Request> to_request_offer_snapshot(const RequestOfferSnapshot& req);
std::unique_ptr<Request> to_request_load_snapshot_chunk(const RequestLoadSnapshotChunk& req);
std::unique_ptr<Request> to_request_apply_snapshot_chunk(const RequestApplySnapshotChunk& req);

} // namespace tendermint::abci

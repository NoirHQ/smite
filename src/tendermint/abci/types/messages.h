// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <noir/codec/protobuf.h>
#include <noir/common/varint.h>
#include <tendermint/abci/types.pb.h>
#include <eo/core.h>

namespace noir::abci {

using namespace tendermint::abci;
using namespace eo;

std::unique_ptr<Request> to_request_echo(const std::string& message);
std::unique_ptr<Request> to_request_flush();
std::unique_ptr<Request> to_request_info(const RequestInfo& req);
std::unique_ptr<Request> to_request_deliver_tx(const RequestDeliverTx& req);
std::unique_ptr<Request> to_request_check_tx(const RequestCheckTx& req);
std::unique_ptr<Request> to_request_commit();
std::unique_ptr<Request> to_request_query(const RequestQuery& req);
std::unique_ptr<Request> to_request_init_chain(const RequestInitChain& req);
std::unique_ptr<Request> to_request_begin_block(const RequestBeginBlock& req);
std::unique_ptr<Request> to_request_end_block(const RequestEndBlock& req);
std::unique_ptr<Request> to_request_list_snapshots(const RequestListSnapshots& req);
std::unique_ptr<Request> to_request_offer_snapshot(const RequestOfferSnapshot& req);
std::unique_ptr<Request> to_request_load_snapshot_chunk(const RequestLoadSnapshotChunk& req);
std::unique_ptr<Request> to_request_apply_snapshot_chunk(const RequestApplySnapshotChunk& req);

template<typename T, typename Writer>
func<Result<int>> write_message(T&& msg, Writer&& w) {
  auto size_buffer = Bytes(10);
  auto ds = codec::Datastream<unsigned char>(size_buffer);
  auto n = codec::protobuf::encode_size(msg);
  auto len_off = write_uleb128(ds, Varuint64(n));
  co_await w.write(std::span{size_buffer.data(), *len_off});
  auto buffer = codec::protobuf::encode(msg);
  co_await w.write(buffer);
  co_return *len_off + n;
}

template<typename T, typename Reader>
func<Result<int>> read_message(Reader&& r, T&& msg) {
  Varuint64 l{};
  auto n = co_await read_uleb128_async(r, l);
  if (!n) {
    // FIXME: original implementation returns both n and error
    co_return n.error();
  }

  // TODO: overflow check
  auto buffer = Bytes(l.value);
  co_await r.read(buffer);
  codec::protobuf::decode(buffer, msg);
  // XXX: should we make read/write return the size of transferred bytes?
  co_return *n + l.value;
}

} // namespace tendermint::abci

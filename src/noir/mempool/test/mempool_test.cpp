// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <tendermint/abci/client/socket_client.h>

#include <noir/mempool/mempool.h>
#include <noir/net/tcp_conn.h>

using namespace noir;
using namespace noir::mempool;

TEST_CASE("mempool: ", "[noir][mempool]") {
  auto mp = TxMempool<abci::SocketClient<net::TcpConn>>();
}

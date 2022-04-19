// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/abci/client/socket_client.h>
#include <tendermint/common/thread_utils.h>
#include <tendermint/net/tcp_conn.h>
#include <tendermint/proxy/app_conn.h>
#include <tendermint/service/service.h>

namespace tendermint::proxy {

template<typename Client>
class AppConns : public service::Service<AppConns<Client>> {
public:
  AppConns(const std::string& address)
    : thread_pool("proxy", 4),
      mempool(address, thread_pool.get_executor()),
      consensus(address, thread_pool.get_executor()),
      query(address, thread_pool.get_executor()),
      snapshot(address, thread_pool.get_executor()) {}

  named_thread_pool thread_pool;

  AppConnMempool<Client> mempool;
  AppConnConsensus<Client> consensus;
  AppConnQuery<Client> query;
  AppConnSnapshot<Client> snapshot;
};

using TCPAppConns = AppConns<abci::SocketClient<net::TCPConn>>;

} // namespace tendermint::proxy

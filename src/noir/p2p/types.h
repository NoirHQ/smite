// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types/bytes.h>
#include <chrono>
#include <cinttypes>
#include <mutex>
#include <vector>

namespace noir::p2p {

using node_id = std::string; // a hex-encoded crypto.Address. It must be lower-cased

using tstamp = std::chrono::system_clock::duration::rep;

using block_id_type = bytes32;

/**
 * default value initializers
 */
constexpr auto def_send_buffer_size_mb = 4;
constexpr auto def_send_buffer_size = 1024 * 1024 * def_send_buffer_size_mb;
constexpr auto def_max_write_queue_size = def_send_buffer_size * 10;
constexpr auto def_max_trx_in_progress_size = 100 * 1024 * 1024; // 100 MB
constexpr auto def_max_consecutive_immediate_connection_close = 9; // back off if client keeps closing
constexpr auto def_max_clients = 25; // 0 for unlimited clients
constexpr auto def_max_nodes_per_host = 1;
constexpr auto def_conn_retry_wait = 30;
constexpr auto def_txn_expire_wait = std::chrono::seconds(3);
constexpr auto def_resp_expected_wait = std::chrono::seconds(5);
constexpr auto def_sync_fetch_span = 100;
constexpr auto def_keepalive_interval = 32000;

constexpr auto message_header_size = 4;

} // namespace noir::p2p

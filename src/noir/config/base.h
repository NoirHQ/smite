// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/config/defaults.h>
#include <CLI/CLI11.hpp>
#include <array>
#include <filesystem>
#include <unistd.h>

namespace noir::config {

namespace detail {
  inline auto get_default_moniker() -> std::string {
    std::array<char, 256> moniker{};
    if (gethostname(moniker.data(), moniker.size())) {
      return "anonymous";
    }
    return std::string{moniker.data()};
  }
} // namespace detail

struct BaseConfig {
  std::string chain_id;
  std::filesystem::path root_dir;
  std::string proxy_app;
  std::string moniker;
  node::Mode mode;
  std::string db_backend;
  std::filesystem::path db_path;
  std::string log_level;
  std::string log_format;
  std::filesystem::path genesis;
  std::filesystem::path node_key;
  std::string abci;
  bool filter_peers;
  // others;

  BaseConfig() {
    genesis = default_config_dir / default_config_file_name;
    node_key = default_config_dir / default_node_key_name;
    mode = default_mode;
    moniker = detail::get_default_moniker();
    proxy_app = "tcp://127.0.0.1:26658";
    abci = "socket";
    log_level = default_log_level;
    // log_format = log::log_format_plain;
    filter_peers = false;
    db_backend = "rocksdb";
    db_path = "data";
  }

  void bind(CLI::App& cmd) {
    cmd.add_option("proxy-app", proxy_app)->group("");
    cmd.add_option("moniker", moniker)->group("");
    cmd.add_option("mode", mode)->group("");
    cmd.add_option("db-backend", db_backend)->group("");
    cmd.add_option("db-dir", db_path)->group("");
    cmd.add_option("log-level", log_level)->group("");
    cmd.add_option("log-format", log_format)->group("");
    cmd.add_option("genesis-file", genesis)->group("");
    cmd.add_option("node-key-file", node_key)->group("");
    cmd.add_option("abci", abci)->group("");
    cmd.add_option("filter-peers", filter_peers)->group("");
  }
};

} // namespace noir::config

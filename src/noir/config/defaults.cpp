// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/config/defaults.h>

namespace noir::config {

std::filesystem::path default_tendermint_dir = ".smite";
std::filesystem::path default_config_dir = "config";
std::filesystem::path default_data_dir = "data";

std::string default_config_file_name = "config.toml";
std::string default_genesis_json_name = "genesis.json";

std::string default_mode = "full";
std::string default_privval_key_name = "priv_validator_key.json";
std::string default_privval_state_name = "priv_validator_state.json";

std::string default_node_key_name = "node_key.json";
std::string default_addr_book_name = "addrbook.json";

} // namespace noir::config

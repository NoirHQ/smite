// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/config/config.h>
#include <CLI/CLI11.hpp>

namespace noir::commands {

auto config() -> std::shared_ptr<config::Config>;

extern std::shared_ptr<CLI::App> init_files_cmd;
extern std::shared_ptr<CLI::App> reset_all_cmd;
extern std::shared_ptr<CLI::App> root_cmd;
extern std::shared_ptr<CLI::App> version_cmd;

void prepare_base_cmd(CLI::App& cmd, std::filesystem::path default_home);
void parse_config(int argc, char** argv);

} // namespace noir::commands

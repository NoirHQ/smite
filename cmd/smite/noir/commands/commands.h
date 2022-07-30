// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eo/config.h>

#include <appbase/application.hpp>
#include <functional>

// forward declaration
namespace CLI {
class App;
}

extern appbase::application app;

namespace noir::commands {

using add_command_callback = std::function<CLI::App*(CLI::App&)>;

CLI::App* add_command(CLI::App& root, add_command_callback cb);

CLI::App* consensus_test(CLI::App&);
CLI::App* debug(CLI::App&);
CLI::App* init(CLI::App&);
CLI::App* start(CLI::App&);
CLI::App* unsafe_reset_all(CLI::App&);
CLI::App* version(CLI::App&);

} // namespace noir::commands

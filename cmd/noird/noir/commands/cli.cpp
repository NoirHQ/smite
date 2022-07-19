// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/config/config.h>

namespace noir::commands {

void prepare_base_cmd(CLI::App& cmd, std::filesystem::path default_home) {
  auto home = cmd.get_option_no_throw("--home");
  if (!home) {
    home = cmd.add_option("--home", "directory for config and data")->configurable(false);
  }
  home->default_str(default_home);
}

void parse_config(int argc, char** argv) {
  auto parse_option = [&](const char* option_name) -> std::optional<std::string> {
    auto it = std::find_if(argv, argv + argc, [&](const auto v) { return std::string(v).find(option_name) == 0; });
    if (it != argv + argc) {
      auto arg = std::string(*it);
      auto pos = arg.find("=");
      if (pos != std::string::npos) {
        arg = arg.substr(pos + 1);
      } else {
        arg = std::string(*++it);
      }
      return arg;
    }
    return std::nullopt;
  };
  if (auto arg = parse_option("--home")) {
    std::filesystem::path home_dir = *arg;
    if (home_dir.is_relative())
      home_dir = std::filesystem::current_path() / home_dir;
    root_cmd->get_option("--home")->add_result(home_dir);
  }
  auto home_dir = root_cmd->get_option("--home")->as<std::filesystem::path>();
  auto config_file = home_dir / config::default_config_dir / "config.toml";
  if (!std::filesystem::exists(config_file)) {
    if (!std::filesystem::exists(config_file.parent_path())) {
      std::filesystem::create_directories(config_file.parent_path());
    }
    std::ofstream out(std::filesystem::path(config_file).make_preferred().string());
    out << root_cmd->config_to_str(true, true) << std::endl;
    out.close();
  }
  root_cmd->set_config("-c", config_file.generic_string(), "Configuration file path")->group("");
}

} // noir:::commands

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/config/config.h>
#include <noir/consensus/privval/file.h>

namespace noir::commands {

auto remove_dir(std::filesystem::path target_dir) -> bool;

auto reset_all_cmd = []() {
  auto cmd = std::make_shared<CLI::App>(
    "(unsafe) Remove all the data and WAL, reset this node's validator to genesis state",
    "unsafe-reset-all");
  cmd->final_callback([&self{*cmd}]() {
    auto home_dir = self.get_parent()->get_option("--home")->as<std::filesystem::path>();
    remove_dir(home_dir / config::default_data_dir);

    // create data/priv_validator_state.json (old one was deleted above)
    consensus::privval::file_pv_last_sign_state lss{
      0, 0, consensus::privval::sign_step::none, "", {}, config()->priv_validator.state_file()};
    lss.save();
  });
  cmd->fallthrough();
  return cmd;
}();

auto remove_dir(std::filesystem::path target_dir) -> bool {
  if (std::filesystem::exists(target_dir)) {
    if (!std::filesystem::is_directory(target_dir)) {
      return false;
    }
  }
  auto n = std::filesystem::remove_all(target_dir);
  return n > 0;
}

} // namespace noir::commands

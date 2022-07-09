// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/consensus/config.h>
#include <noir/consensus/privval/file.h>
#include <noir/log/log.h>

namespace fs = std::filesystem;

namespace noir::commands {

bool remove_dir(fs::path target_dir);

CLI::App* unsafe_reset_all(CLI::App& root) {
  auto cmd =
    root
      .add_subcommand("unsafe-reset-all", "Remove all the data and WAL, reset this node's validator to genesis state")
      ->final_callback([]() {
        auto config_ = noir::consensus::config::get_default(); // TODO : read from file
        auto home_dir = app.home_dir();
        ilog("home_dir = {}", home_dir.string());
        remove_dir(home_dir / std::string(noir::consensus::default_data_dir));

        // create data/priv_validator_state.json (old one was deleted above)
        auto priv_val_state_path = config_.priv_validator.state;
        ilog("resetting {}/{}", home_dir.string(), priv_val_state_path);
        noir::consensus::privval::file_pv_last_sign_state lss{
          0, 0, noir::consensus::privval::sign_step::none, "", {}, home_dir / priv_val_state_path};
        lss.save();
        ilog("done");
      });
  cmd->group("");
  return cmd;
}

bool remove_dir(fs::path target_dir) {
  if (fs::exists(target_dir)) {
    if (!fs::is_directory(target_dir)) {
      elog("unable to delete {}; it's not a directory", target_dir.string());
      return false;
    }
  }
  ilog("deleting directory {}", target_dir.string());
  auto n = fs::remove_all(target_dir);
  return n > 0;
}

} // namespace noir::commands

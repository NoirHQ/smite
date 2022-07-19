// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/config/defaults.h>
#include <CLI/CLI11.hpp>
#include <filesystem>

namespace noir::config {

struct PrivValidatorConfig {
  std::filesystem::path root_dir;
  std::filesystem::path key;
  std::filesystem::path state;
  std::string listen_addr;
  std::filesystem::path client_certificate;
  std::filesystem::path client_key;
  std::filesystem::path root_ca;

  PrivValidatorConfig() {
    key = default_config_dir / default_privval_key_name;
    state = default_data_dir / default_privval_state_name;
  }

  void bind(CLI::App& cmd) {
    auto privval = cmd.add_section("priv-validator");

    privval->add_option("key-file", key);
    privval->add_option("state-file", state);
    privval->add_option("laddr", listen_addr);
    privval->add_option("client-certificate-file", client_certificate);
    privval->add_option("root-ca-file", root_ca);
  }

  auto client_key_file() -> std::filesystem::path {
    return detail::rootify(client_key, root_dir);
  }

  auto client_certificate_file() -> std::filesystem::path {
    return detail::rootify(client_certificate, root_dir);
  }

  auto root_ca_file() -> std::filesystem::path {
    return detail::rootify(root_ca, root_dir);
  }

  auto key_file() -> std::filesystem::path {
    return detail::rootify(key, root_dir);
  }

  auto state_file() -> std::filesystem::path {
    return detail::rootify(state, root_dir);
  }
};

} // namespace noir::config

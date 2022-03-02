// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/common/scope_exit.h>
#include <noir/consensus/privval/file.h>
#include <noir/crypto/rand.h>
#include <filesystem>

namespace {
using namespace noir::consensus::privval;
namespace fs = std::filesystem;

auto prepare_test_dir = []() {
  auto temp_dir = std::make_shared<fc::temp_directory>();
  auto tmp_path = temp_dir->path().string();

  return std::shared_ptr<fc::temp_directory>(std::move(temp_dir));
};

inline noir::bytes gen_random_bytes(size_t num) {
  noir::bytes ret(num);
  noir::crypto::rand_bytes(ret);
  return ret;
}

TEST_CASE("priv_val_file: save/load priv_key", "[noir][consensus]") {
  auto temp_dir = prepare_test_dir();
  auto temp_dir_path = temp_dir->path().string();
  auto defer = noir::make_scope_exit([&temp_dir_path]() { fs::remove_all(temp_dir_path); });
  auto json_file_path = fs::path{temp_dir_path};
  json_file_path /= "priv_validator_key";
  json_file_path.replace_extension("json");

  auto exp = file_pv_key{
    .address = gen_random_bytes(32),
    .pub_key =
      {
        .key = gen_random_bytes(32),
      },
    .priv_key =
      {
        .key = gen_random_bytes(32),
      },
    .file_path = json_file_path,
  };
  exp.save();

  file_pv_key ret{};
  file_pv_key::load(json_file_path, ret);

  CHECK(exp.address == ret.address);
  CHECK(exp.pub_key == ret.pub_key);
  CHECK(exp.priv_key == ret.priv_key);
}

TEST_CASE("priv_val_file: save/load lss", "[noir][consensus]") {
  auto temp_dir = prepare_test_dir();
  auto temp_dir_path = temp_dir->path().string();
  auto defer = noir::make_scope_exit([&temp_dir_path]() { fs::remove_all(temp_dir_path); });
  auto json_file_path = fs::path{temp_dir_path};
  json_file_path /= "priv_validator_state";
  json_file_path.replace_extension("json");

  auto exp = file_pv_last_sign_state{
    .height = 1,
    .round = 0,
    .step = sign_step::none,
    .signature = gen_random_bytes(32),
    .sign_bytes = gen_random_bytes(32),
    .file_path = json_file_path,
  };
  exp.save();

  file_pv_last_sign_state ret{};
  file_pv_last_sign_state::load(json_file_path, ret);

  CHECK(exp.height == ret.height);
  CHECK(exp.round == ret.round);
  CHECK(exp.step == ret.step);
  CHECK(exp.signature == ret.signature);
  CHECK(exp.sign_bytes == ret.sign_bytes);
}

} // namespace

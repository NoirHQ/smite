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

auto compare_file_pv_key = [](const file_pv_key& exp, const file_pv_key& ret) {
  CHECK(exp.address == ret.address);
  CHECK(exp.pub_key == ret.pub_key);
  CHECK(exp.priv_key == ret.priv_key);
  CHECK(exp.file_path == ret.file_path);
};

auto compare_file_pv_last_sign_state = [](const file_pv_last_sign_state& exp, const file_pv_last_sign_state& ret) {
  CHECK(exp.height == ret.height);
  CHECK(exp.round == ret.round);
  CHECK(exp.step == ret.step);
  CHECK(exp.signature == ret.signature);
  CHECK(exp.file_path == ret.file_path);
};

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

  compare_file_pv_key(exp, ret);
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

  compare_file_pv_last_sign_state(exp, ret);
}

TEST_CASE("test file_pv", "[priv_val_file]") {
  auto temp_dir = prepare_test_dir();
  auto temp_dir_path = temp_dir->path().string();
  auto defer = noir::make_scope_exit([&temp_dir_path]() { fs::remove_all(temp_dir_path); });

  auto key_file_path = fs::path{temp_dir_path};
  key_file_path /= "priv_validator_key";
  key_file_path.replace_extension("json");

  auto lss_file_path = fs::path{temp_dir_path};
  lss_file_path /= "priv_validator_state";
  lss_file_path.replace_extension("json");

  struct test_case {
    std::string name;
    std::string key_type; // TODO: connect key_type
  };
  auto tests = std::to_array<struct test_case>({
    test_case{"priv key_type: secp256k1", "secp256k1"},
    test_case{"priv key_type: ed25519", "ed25519"},
  });

  std::for_each(tests.begin(), tests.end(), [&](const test_case& t) {
    SECTION(t.name) {

      auto file_pv_ptr = file_pv::gen_file_pv(key_file_path, lss_file_path, "secp256k1");
      REQUIRE(file_pv_ptr != nullptr);

      SECTION("gen_file_pv") {
        {
          auto& file_pv_key_ = file_pv_ptr->key;
          CHECK(file_pv_key_.pub_key.empty() == false);
          CHECK(file_pv_key_.priv_key.key.empty() == false);
          CHECK(file_pv_key_.file_path == key_file_path);
        }
        {
          auto& lss = file_pv_ptr->last_sign_state;
          CHECK(lss.step == sign_step::none);
          CHECK(lss.file_path == lss_file_path);
        }
      }

      file_pv_ptr->save();

      SECTION("load_file_pv_empty_state") {
        {
          auto ret = file_pv::load_file_pv_empty_state(key_file_path, lss_file_path);
          REQUIRE(ret != nullptr);
          // load_file_pv_empty_state() does not load last_sign_state
          compare_file_pv_key(ret->key, file_pv_ptr->key);
        }
      }

      SECTION("load_file_pv") {
        {
          auto ret = file_pv::load_file_pv(key_file_path, lss_file_path);
          REQUIRE(ret != nullptr);
          compare_file_pv_key(ret->key, file_pv_ptr->key);
          compare_file_pv_last_sign_state(ret->last_sign_state, file_pv_ptr->last_sign_state);
        }
      }
    }
  });
}

} // namespace

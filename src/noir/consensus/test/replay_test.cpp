// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/helper/go.h>
#include <noir/consensus/common_test.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/wal.h>
#include <noir/crypto/rand.h>
#include <filesystem>

namespace {
using namespace noir::consensus;
using namespace noir::p2p;
namespace fs = std::filesystem;

auto prepare_consensus = [](auto num_validators) { // copy and modification of rand_cs
  auto local_config = config_setup();
  auto [state_, priv_vals] = rand_genesis_state(local_config, num_validators, false, 10);
  validator_stub_list vss;
  auto temp_dir = std::make_shared<fc::temp_directory>();
  auto tmp_path = temp_dir->path().string();
  auto db_dir = temp_dir->path() / "db";
  auto session =
    std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session(false, db_dir.string()));
  auto dbs = std::make_shared<noir::consensus::db_store>(session);
  auto proxyApp = std::make_shared<app_connection>();
  auto bls = std::make_shared<noir::consensus::block_store>(session);
  auto block_exec = block_executor::new_block_executor(dbs, proxyApp, bls);

  auto cs = consensus_state::new_state(local_config.consensus, state_, block_exec, bls);
  cs->set_priv_validator(priv_vals[0]); // todo - requires many other fields to be properly initialized
  cs->cs_config.root_dir = tmp_path;

  REQUIRE(cs->load_wal_file() == true);
  REQUIRE(cs->wal_->on_start() == true);

  // for (auto i = 0; i < num_validators; i++) {
  //   vss.push_back(validator_stub{i, 0, 0, priv_vals[i], test_min_power});
  // }
  //
  // increment_height(vss, 1);
  return std::tuple<std::shared_ptr<fc::temp_directory>, std::shared_ptr<consensus_state>>(std::move(temp_dir), cs);
};

auto process_block_wal_msg = [](wal& wal_, int64_t height) {
  wal_message msg;
  {
    noir::p2p::vote_message vote_{};
    vote_.timestamp = get_time();
    vote_.height = height;
    msg.msg = {internal_msg_info{vote_}};
    wal_.write(msg);
  }
  {
    timeout_info to_info_{};
    to_info_.height = height;
    msg.msg = {to_info_};
    wal_.write(msg);
  }
  {
    msg.msg = {end_height_message{.height = height}};
    wal_.write_sync(msg);
  }
};

TEST_CASE("state_wal: catchup_replay", "[noir][consensus]") {
  struct test_case {
    std::string name;
    int64_t start_height;
    int64_t end_height;
    int64_t target_height;
    bool expect_ret;
  };
  auto tests = std::to_array<test_case>({
    {"simple_positive_init", 0, 0, 1, true},
    {"simple_positive_1", 1, 10, 10, true},
    {"simple_positive_2", 1, 1000, 1000, true},
    {"wal should not catchup non-existing height_init", 0, 0, 2, false},
    {"wal should not catchup non-existing height_1", 1, 10, 11, false},
    {"wal should not catchup non-existing height_2", 0, 1000, 1001, false},
    {"wal should not contain #ENDHEIGHT_init", 0, 0, 0, false},
    {"wal should not contain #ENDHEIGHT_1", 1, 10, 9, false},
    {"wal should not contain #ENDHEIGHT_2", 1, 1000, 999, false},
  });

  auto [temp_dir, cs] = prepare_consensus(1);
  noir_defer([&temp_dir = temp_dir]() { fs::remove_all(temp_dir->path().string()); });
  auto& wal_ = cs->wal_;

  std::for_each(tests.begin(), tests.end(), [&, &cs = cs](const test_case& t) {
    SECTION(t.name) {
      for (int64_t height = t.start_height; height < t.end_height; ++height) {
        process_block_wal_msg(*wal_, height);
      }
      CHECK(cs->catchup_replay(t.target_height) == t.expect_ret);
    }
  });
}

auto corrupt_wal_file = [](const std::string& wal_path) {
  std::array<char, 100> corrupt_msg;
  size_t len = sizeof(corrupt_msg);
  char* corrupt_ptr = static_cast<char*>(static_cast<void*>(&corrupt_msg));
  noir::crypto::rand_bytes({corrupt_ptr, len});
  fc::cfile file_;
  file_.set_file_path(wal_path);
  file_.open(fc::cfile::update_rw_mode);
  file_.seek_end(0);
  file_.write(corrupt_ptr, len);
};

TEST_CASE("state_wal: repair_wal_file", "[noir][consensus]") {
  struct test_case {
    std::string name;
    int64_t start_height;
    int64_t corrupt_height;
    int64_t end_height;
  };
  auto tests = std::to_array<test_case>({
    {"corrupted at the middle of wal file", 1, 25, 50},
    {"corrupted at the beggining of wal file", 1, 1, 50},
    {"corrupted at the end of wal file", 1, 50, 50},
  });

  auto [temp_dir, cs] = prepare_consensus(1);
  auto wal_path = fs::path{(temp_dir->path() / cs->cs_config.wal_path / cs->wal_head_name).string()};
  noir_defer([&temp_dir = temp_dir]() { fs::remove_all(temp_dir->path().string()); });

  std::for_each(tests.begin(), tests.end(), [&, &cs = cs](const test_case& t) {
    SECTION(t.name) {
      for (int64_t height = t.start_height; height < t.corrupt_height; ++height) {
        process_block_wal_msg(*cs->wal_, height);
      }
      cs->wal_.reset();
      corrupt_wal_file(wal_path);
      REQUIRE(cs->load_wal_file() == true);

      for (int64_t height = t.corrupt_height; height < t.end_height; ++height) {
        process_block_wal_msg(*cs->wal_, height);
      }

      // 2) backup original WAL file
      auto corrupted_path = wal_path;
      corrupted_path += wal_file_manager::corrupted_postfix;
      fs::rename(wal_path, corrupted_path);

      CHECK(repair_wal_file(corrupted_path, wal_path) == true);

      {
        for (int64_t height = t.corrupt_height; height < t.end_height; ++height) {
          bool found;
          auto ret = cs->wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
          CHECK(ret == nullptr);
          CHECK(found == false);
        }
      }
      {
        for (int64_t height = t.start_height; height < t.corrupt_height; ++height) {
          bool found;
          auto ret = cs->wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
          CHECK(ret != nullptr);
          CHECK(found == true);
        }
      }
    }
  });
}

} // namespace

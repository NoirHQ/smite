// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/db/db.h>

TEST_CASE("get/has/set", "[simple_db]") {
  noir::consensus::simple_db<noir::p2p::bytes, noir::p2p::bytes> test_db;
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"hello", "world"},
    {"key1", "val1"},
    {"key2", "val2"},
    {"key3", "val3"},
    {"key4", "val4"},
    {"key5", "val5"},
    {"key6", "val6"},
    {"key7", "val7"},
    {"key8", "val8"},
    {"key9", "val9"},
  });

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    bool ret;

    test_db.has(key, ret);
    CHECK(ret == false);
    ret = test_db.get(key, expected);
    CHECK(ret == false);

    ret = test_db.set(key, val);
    CHECK(ret == true);
    test_db.has(key, ret);
    CHECK(ret == true);
    ret = test_db.get(key, expected);
    CHECK(ret == true);
    CHECK(val == expected);
  });
}

TEST_CASE("get/has/del", "[simple_db]") {
  noir::consensus::simple_db<noir::p2p::bytes, noir::p2p::bytes> test_db;
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"hello", "world"},
    {"key1", "val1"},
    {"key2", "val2"},
    {"key3", "val3"},
    {"key4", "val4"},
    {"key5", "val5"},
    {"key6", "val6"},
    {"key7", "val7"},
    {"key8", "val8"},
    {"key9", "val9"},
  });

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    bool ret;

    ret = test_db.set(key, val);
    CHECK(ret == true);
    test_db.has(key, ret);
    CHECK(ret == true);
    ret = test_db.get(key, expected);
    CHECK(ret == true);
    CHECK(val == expected);

    ret = test_db.del(key);

    test_db.has(key, ret);
    CHECK(ret == false);
    ret = test_db.get(key, expected);
    CHECK(ret == false);
  });
}

TEST_CASE("batch", "[simple_db]") {
  noir::consensus::simple_db<noir::p2p::bytes, noir::p2p::bytes> test_db;
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"hello", "world"},
    {"key1", "val1"},
    {"key2", "val2"},
    {"key3", "val3"},
    {"key4", "val4"},
    {"key5", "val5"},
    {"key6", "val6"},
    {"key7", "val7"},
    {"key8", "val8"},
    {"key9", "val9"},
  });

  auto batch = test_db.new_batch();

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    bool ret;

    ret = batch->set(key, val);
    CHECK(ret == true);
  });

  batch->write();
  auto batch2 = test_db.new_batch();

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    bool ret;

    test_db.has(key, ret);
    CHECK(ret == true);
    ret = test_db.get(key, expected);
    CHECK(ret == true);
    CHECK(val == expected);

    ret = batch2->del(key);
    CHECK(ret == true);
  });

  batch->close();
  batch2->write();

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    bool ret;
    test_db.has(key, ret);
    CHECK(ret == false);
    ret = test_db.get(key, expected);
    CHECK(ret == false);
  });
}

TEST_CASE("iterator", "[simple_db]") {
  noir::consensus::simple_db<noir::p2p::bytes, noir::p2p::bytes> test_db;
  auto db_input = std::to_array<std::pair<std::string, std::string>>({
    {"key1", "val1"},
    {"key2", "val2"},
    {"key3", "val3"},
    {"key4", "val4"},
    {"key5", "val5"},
    {"key6", "val6"},
    {"key7", "val7"},
    {"key8", "val8"},
    {"key9", "val9"},
  });
  auto batch = test_db.new_batch();

  std::for_each(db_input.begin(), db_input.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    CHECK(batch->set(key, val) == true);
  });
  batch->write();
  batch->close();

  struct test_case {
    std::string name;
    std::string start;
    std::string end;
    std::vector<std::pair<std::string, std::string>> expected;
  };

  auto tests = std::to_array<test_case>({
    test_case{"start <= it <= end", "key1", "key9",
      std::vector<std::pair<std::string, std::string>>{
        {"key1", "val1"},
        {"key2", "val2"},
        {"key3", "val3"},
        {"key4", "val4"},
        {"key5", "val5"},
        {"key6", "val6"},
        {"key7", "val7"},
        {"key8", "val8"},
        {"key9", "val9"},
      }},
    test_case{"start <= it <= middle", "key1", "key5",
      std::vector<std::pair<std::string, std::string>>{
        {"key1", "val1"},
        {"key2", "val2"},
        {"key3", "val3"},
        {"key4", "val4"},
        {"key5", "val5"},
      }},
    test_case{"middle <= it <= end", "key5", "key9",
      std::vector<std::pair<std::string, std::string>>{
        {"key5", "val5"},
        {"key6", "val6"},
        {"key7", "val7"},
        {"key8", "val8"},
        {"key9", "val9"},
      }},
    test_case{"middle1 <= it <= middle2", "key3", "key6",
      std::vector<std::pair<std::string, std::string>>{
        {"key3", "val3"},
        {"key4", "val4"},
        {"key5", "val5"},
        {"key6", "val6"},
      }},
    test_case{"middle1 < it < middle2", "key3XXX", "key6XXX",
      std::vector<std::pair<std::string, std::string>>{
        {"key4", "val4"},
        {"key5", "val5"},
        {"key6", "val6"},
      }},
  });
  std::for_each(tests.begin(), tests.end(), [&](const test_case& t) {
    SECTION(t.name) {
      auto expected = t.expected;
      auto start_s = t.start;
      auto end_s = t.end;
      noir::p2p::bytes start_b(start_s.begin(), start_s.end());
      noir::p2p::bytes end_b(end_s.begin(), end_s.end());

      auto db_it = test_db.get_iterator(start_b, end_b);
      auto exp_it = expected.begin();
      for (auto it = db_it.begin(); it != db_it.end(); ++it, ++exp_it) {
        auto& key = it.key();
        auto& val = it.val();
        CHECK(std::string(key.begin(), key.end()) == exp_it->first);
        CHECK(std::string(val.begin(), val.end()) == exp_it->second);
      }
      CHECK(exp_it == expected.end());
      exp_it = expected.begin();
      for (auto it = db_it.begin(); it != db_it.end(); ++it, exp_it++) {
        auto& key = it.key();
        auto& val = it.val();
        CHECK(std::string(key.begin(), key.end()) == exp_it->first);
        CHECK(std::string(val.begin(), val.end()) == exp_it->second);
      }
      CHECK(exp_it == expected.end());
    }
  });

  // reverse
  std::for_each(tests.begin(), tests.end(), [&](const test_case& t) {
    SECTION(t.name + "<reverse>") {
      auto expected = t.expected;
      auto start_s = t.start;
      auto end_s = t.end;
      noir::p2p::bytes start_b(start_s.begin(), start_s.end());
      noir::p2p::bytes end_b(end_s.begin(), end_s.end());

      auto db_it = test_db.get_reverse_iterator(start_b, end_b);
      auto exp_it = expected.rbegin();
      for (auto it = db_it.begin(); it != db_it.end(); ++it, ++exp_it) {
        auto& key = it.key();
        auto& val = it.val();
        CHECK(std::string(key.begin(), key.end()) == exp_it->first);
        CHECK(std::string(val.begin(), val.end()) == exp_it->second);
      }
      CHECK(exp_it == expected.rend());
      exp_it = expected.rbegin();
      for (auto it = db_it.begin(); it != db_it.end(); ++it, exp_it++) {
        auto& key = it.key();
        auto& val = it.val();
        CHECK(std::string(key.begin(), key.end()) == exp_it->first);
        CHECK(std::string(val.begin(), val.end()) == exp_it->second);
      }
      CHECK(exp_it == expected.rend());
    }
  });
}

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
  auto tests = std::to_array<std::pair<std::string, std::string>>({
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

  auto expected = std::to_array<std::pair<std::string, std::string>>({
    {"key3", "val3"}, {"key4", "val4"}, {"key5", "val5"}, {"key6", "val6"}, // TODO: TBD(inclusive)
  });

  auto batch = test_db.new_batch();

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    CHECK(batch->set(key, val) == true);
  });
  batch->write();
  batch->close();

  auto start_s = std::string("key3");
  auto end_s = std::string("key6");
  noir::p2p::bytes start_b(start_s.begin(), start_s.end());
  noir::p2p::bytes end_b(end_s.begin(), end_s.end());

  auto start = test_db.begin_iterator(start_b);
  auto end = test_db.end_iterator(end_b);

  auto exp_it = expected.begin();
  for (auto it = start; it != end; ++it, ++exp_it) {
    auto& key = it.key();
    auto& val = it.val();
    CHECK(std::string(key.begin(), key.end()) == exp_it->first);
    CHECK(std::string(val.begin(), val.end()) == exp_it->second);
  }
  CHECK(exp_it == expected.end());
  exp_it = expected.begin();
  for (auto it = start; it != end; ++it, exp_it++) {
    auto& key = it.key();
    auto& val = it.val();
    CHECK(std::string(key.begin(), key.end()) == exp_it->first);
    CHECK(std::string(val.begin(), val.end()) == exp_it->second);
  }
  CHECK(exp_it == expected.end());
}

TEST_CASE("reverse_iterator", "[simple_db]") {
  noir::consensus::simple_db<noir::p2p::bytes, noir::p2p::bytes> test_db;
  auto tests = std::to_array<std::pair<std::string, std::string>>({
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

  auto expected = std::to_array<std::pair<std::string, std::string>>({
    {"key6", "val6"}, {"key5", "val5"}, {"key4", "val4"}, {"key3", "val3"}, // TODO: TBD(inclusive)
  });

  auto batch = test_db.new_batch();

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    noir::p2p::bytes key(t.first.begin(), t.first.end());
    noir::p2p::bytes val(t.second.begin(), t.second.end());
    noir::p2p::bytes expected;
    CHECK(batch->set(key, val) == true);
  });
  batch->write();
  batch->close();

  auto start_s = std::string("key6");
  auto end_s = std::string("key4");
  noir::p2p::bytes start_b(start_s.begin(), start_s.end());
  noir::p2p::bytes end_b(end_s.begin(), end_s.end());

  auto start = test_db.rbegin_iterator(start_b);
  auto end = test_db.rend_iterator(end_b);

  auto exp_it = expected.begin();
  for (auto it = start; it != end; ++it, ++exp_it) {
    auto& key = it.base().key();
    auto& val = it.base().val();
    // CHECK(std::string(key.begin(), key.end()) == exp_it->first);
    // CHECK(std::string(val.begin(), val.end()) == exp_it->second);
  }
  // CHECK(exp_it == expected.end());
  exp_it = expected.begin();
  for (auto it = start; it != end; ++it, exp_it++) {
    auto& key = it.base().key();
    auto& val = it.base().val();
    // CHECK(std::string(key.begin(), key.end()) == exp_it->first);
    // CHECK(std::string(val.begin(), val.end()) == exp_it->second);
  }
  // CHECK(exp_it == expected.end());
}

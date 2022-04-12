// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/types/node_key.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("node_key: load or gen", "[noir][consensus]") {
  auto file_path = "/tmp/noir_test/node_key.json";
  auto key1 = node_key::load_or_gen_node_key(file_path);
  auto key2 = node_key::load_or_gen_node_key(file_path);
  CHECK(key1->node_id == key2->node_id);
}

TEST_CASE("node_key: load", "[noir][consensus]") {
  auto file_path = "/tmp/noir_test/node_key.json";
  std::filesystem::remove_all(std::filesystem::path{file_path}.remove_filename());
  auto key = node_key::load_node_key(file_path);
  CHECK(key == nullptr);
  key = node_key::load_or_gen_node_key(file_path);
  CHECK(key != nullptr);
}

TEST_CASE("node_key: save as", "[noir][consensus]") {
  auto file_path = "/tmp/noir_test/node_key.json";
  std::filesystem::remove_all(std::filesystem::path{file_path}.remove_filename());
  CHECK(!std::filesystem::exists(file_path));
  node_key::load_or_gen_node_key(file_path);
  CHECK(std::filesystem::exists(file_path));
}

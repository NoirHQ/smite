// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/merkle/proof.h>
#include <fc/crypto/private_key.hpp>

using namespace noir;
using namespace noir::consensus::merkle;

TEST_CASE("[tree] Find split point", "[tree]") {
  auto tests = std::to_array<std::pair<size_t, size_t>>({
    {1, 0},
    {2, 1},
    {3, 2},
    {4, 2},
    {5, 4},
    {10, 8},
    {20, 16},
    {100, 64},
    {255, 128},
    {256, 128},
    {257, 256},
  });
  std::for_each(tests.begin(), tests.end(), [&](auto& t) { CHECK(get_split_point(t.first) == t.second); });
}

TEST_CASE("[tree] Compute empty hash", "[tree]") {
  CHECK(get_empty_hash() == from_hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST_CASE("[tree] Verify hash inputs", "[tree]") {
  auto tests = std::to_array<std::pair<bytes_list, std::string>>({
    {{}, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {{{}}, "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d"},
    {{{1, 2, 3}}, "054edec1d0211f624fed0cbca9d4f9400b0e491c43742af2c5b0abebf0c990d8"},
    {{{1, 2, 3}, {4, 5, 6}}, "82e6cfce00453804379b53962939eaa7906b39904be0813fcadd31b100773c4b"},
    {{{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}}, "f326493eceab4f2d9ffbc78c59432a0a005d6ea98392045c74df5d14a113be18"},
  });
  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    // std::cout << to_hex(hash_from_bytes_list(t.first)) << std::endl;
    CHECK(hash_from_bytes_list(t.first) == from_hex(t.second));
  });
}

TEST_CASE("[tree] Verify proof", "[tree]") {
  // Empty proof
  auto [root_hash, proofs] = proofs_from_bytes_list({});
  // std::cout << to_hex(root_hash) << std::endl;
  CHECK(root_hash == from_hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK(proofs.empty());

  bytes_list items;
  int total{100};
  for (auto i = 0; i < total; i++) {
    auto new_key = fc::crypto::private_key::generate<fc::ecc::private_key_shim>();
    items.push_back(fc::from_base58(new_key.to_string()));
  }

  std::tie(root_hash, proofs) = proofs_from_bytes_list(items);
  auto root_hash2 = hash_from_bytes_list(items);
  // std::cout << "root_hash=" << to_hex(root_hash) << std::endl;
  // std::cout << "root_hash2=" << to_hex(root_hash2) << std::endl;
  CHECK(root_hash == root_hash2);

  for (auto i = 0; i < items.size(); i++) {
    auto proof = proofs[i];
    CHECK(proof->index == i);
    CHECK(proof->total == total);

    auto err = proof->verify(root_hash, items[i]);
    CHECK(!err.has_value());

    // Trail too long should fail
    auto orig_aunts = proof->aunts;
    proof->aunts.push_back({static_cast<char>(i % 256)});
    err = proof->verify(root_hash, items[i]);
    CHECK(err.value() == "invalid root hash");
    proof->aunts = orig_aunts;

    // Trail too short should fail
    proof->aunts.pop_back();
    err = proof->verify(root_hash, items[i]);
    CHECK(err.value() == "invalid root hash");
    proof->aunts = orig_aunts;
  }
}

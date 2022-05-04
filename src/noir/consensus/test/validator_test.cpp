// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/consensus/validator.h>

using namespace noir;
using namespace noir::consensus;

void print_validator_set(validator_set& vals) {
  auto index = 0;
  for (auto val : vals.validators)
    std::cout << index++ << ":" << to_hex(val.address) << " " << val.voting_power << " " << val.proposer_priority
              << std::endl;
}

TEST_CASE("validator_set: Basic", "[noir][consensus]") {
  validator_set vals;

  SECTION("empty validator") {
    CHECK_THROWS_WITH(vals.increment_proposer_priority(1), "empty validator set");
  }

  SECTION("non-positive times") {
    validator val;
    vals.validators.push_back(val);
    CHECK_THROWS_WITH(vals.increment_proposer_priority(0), "cannot call with non-positive times");
  }

  SECTION("get by") {
    validator val = {from_hex("0123456789")};
    vals.validators.push_back(val);
    CHECK(vals.get_by_index(-1) == std::nullopt);
    CHECK(vals.get_by_index(3) == std::nullopt);
    CHECK(vals.get_by_index(0).value().address == val.address);
  }
}

TEST_CASE("validator_set: Proposer Selection 1", "[noir][consensus]") {
  auto vals = validator_set::new_validator_set({{validator{from_hex("aa"), {}, 1000, 0}},
    {validator{from_hex("bb"), {}, 300, 0}}, {validator{from_hex("cc"), {}, 330, 0}}});
  std::string proposers;
  for (auto i = 0; i < 99; i++) {
    auto val = vals.get_proposer();
    if (val.has_value())
      proposers += to_hex(val.value().address) + " ";
    // print_validator_set(vals);
    vals.increment_proposer_priority(1);
  }
  std::string expected = "aa cc aa bb aa aa cc aa bb aa aa cc aa aa bb aa cc aa aa bb";
  expected += " aa aa cc aa bb aa aa cc aa bb aa aa cc aa aa bb aa cc aa aa bb";
  expected += " aa cc aa aa bb aa cc aa aa bb aa cc aa aa aa cc bb aa aa aa cc";
  expected += " aa bb aa aa cc aa bb aa aa cc aa bb aa aa cc aa bb aa aa cc aa";
  expected += " aa bb aa cc aa aa bb aa cc aa aa bb aa cc aa aa ";
  CHECK(proposers == expected);
}

TEST_CASE("validator_set: Proposer Selection 2", "[noir][consensus]") {
  std::vector<bytes> addrs = {from_hex("0000000000000000000000000000000000000000"),
    from_hex("0000000000000000000000000000000000000001"), from_hex("0000000000000000000000000000000000000002")};

  SECTION("all validators have equal power") {
    auto vals = validator_set::new_validator_set(
      {{validator{addrs[0], {}, 100, 0}}, {validator{addrs[1], {}, 100, 0}}, {validator{addrs[2], {}, 100, 0}}});
    for (auto i = 0; i < addrs.size() * 5; i++) {
      auto val = vals.get_proposer();
      vals.increment_proposer_priority(1);
      CHECK(val.value().address == addrs[i % addrs.size()]);
    }
  }

  SECTION("one validator has more than others") {
    auto vals = validator_set::new_validator_set(
      {{validator{addrs[0], {}, 100, 0}}, {validator{addrs[1], {}, 100, 0}}, {validator{addrs[2], {}, 400, 0}}});
    CHECK(vals.get_proposer()->address == addrs[2]);
    vals.increment_proposer_priority(1);
    CHECK(vals.get_proposer()->address == addrs[0]);
  }

  SECTION("one validator has more than others, enough to propose twice in a row") {
    auto vals = validator_set::new_validator_set(
      {{validator{addrs[0], {}, 100, 0}}, {validator{addrs[1], {}, 100, 0}}, {validator{addrs[2], {}, 401, 0}}});
    CHECK(vals.get_proposer()->address == addrs[2]);
    vals.increment_proposer_priority(1);
    CHECK(vals.get_proposer()->address == addrs[2]);
    vals.increment_proposer_priority(1);
    CHECK(vals.get_proposer()->address == addrs[0]);
  }

  SECTION("each validator should be the proposer a proportional number of times") {
    auto vals = validator_set::new_validator_set(
      {{validator{addrs[0], {}, 4, 0}}, {validator{addrs[1], {}, 5, 0}}, {validator{addrs[2], {}, 3, 0}}});
    int count_addr0{}, count_addr1{}, count_addr2{};
    for (auto i = 0; i < 120; i++) {
      auto val = vals.get_proposer();
      vals.increment_proposer_priority(1);
      if (vals.get_proposer()->address == addrs[0])
        count_addr0++;
      if (vals.get_proposer()->address == addrs[1])
        count_addr1++;
      if (vals.get_proposer()->address == addrs[2])
        count_addr2++;
    }
    CHECK(count_addr0 == 40);
    CHECK(count_addr1 == 50);
    CHECK(count_addr2 == 30);
  }
}

TEST_CASE("validator_set: Apply update", "[noir][consensus]") {
  std::vector<validator> start_vals;
  start_vals.push_back(validator{from_hex("0044"), {}, 44});
  start_vals.push_back(validator{from_hex("0066"), {}, 66});

  SECTION("insert head") {
    std::vector<validator> update_vals;
    update_vals.push_back(validator{from_hex("0011"), {}, 11});
    auto vals = validator_set::new_validator_set(start_vals);
    vals.apply_updates(update_vals);
    //    print_validator_set(vals);
    CHECK(vals.get_by_index(0)->address == std::vector<char>(from_hex("0011")));
    CHECK(vals.get_by_index(1)->address == std::vector<char>(from_hex("0044")));
    CHECK(vals.get_by_index(2)->address == std::vector<char>(from_hex("0066")));
  }

  SECTION("insert middle") {
    std::vector<validator> update_vals;
    update_vals.push_back(validator{from_hex("0055"), {}, 55});
    auto vals = validator_set::new_validator_set(start_vals);
    vals.apply_updates(update_vals);
    //    print_validator_set(vals);
    CHECK(vals.get_by_index(0)->address == std::vector<char>(from_hex("0044")));
    CHECK(vals.get_by_index(1)->address == std::vector<char>(from_hex("0055")));
    CHECK(vals.get_by_index(2)->address == std::vector<char>(from_hex("0066")));
  }

  SECTION("insert tail") {
    std::vector<validator> update_vals;
    update_vals.push_back(validator{from_hex("0077"), {}, 77});
    auto vals = validator_set::new_validator_set(start_vals);
    vals.apply_updates(update_vals);
    //    print_validator_set(vals);
    CHECK(vals.get_by_index(0)->address == std::vector<char>(from_hex("0044")));
    CHECK(vals.get_by_index(1)->address == std::vector<char>(from_hex("0066")));
    CHECK(vals.get_by_index(2)->address == std::vector<char>(from_hex("0077")));
  }

  SECTION("override voting_power") {
    std::vector<validator> update_vals;
    update_vals.push_back(validator{from_hex("0044"), {}, 444});
    update_vals.push_back(validator{from_hex("0066"), {}, 6});
    auto vals = validator_set::new_validator_set(start_vals);
    vals.apply_updates(update_vals);
    //    print_validator_set(vals);
    CHECK(vals.get_by_index(0)->voting_power == 444);
    CHECK(vals.get_by_index(1)->voting_power == 6);
  }
}

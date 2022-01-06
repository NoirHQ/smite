// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/net/consensus/validator.h>
#include <noir/common/hex.h>

using namespace noir;
using namespace noir::net::consensus;

void print_validator_set(validator_set& vals) {
  auto index = 0;
  for (auto val : vals.validators)
    std::cout << index++ << ":" << to_hex(val.address) << " " << val.voting_power << " " << val.proposer_priority
      << std::endl;
}

TEST_CASE("Basic", "[validator_set]") {
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

TEST_CASE("Proposer Selection 1", "[validator_set]") {
  validator_set vals;
  vals.validators.push_back(validator{from_hex("AAAA"), {}, 1000, 0});
  vals.validators.push_back(validator{from_hex("BBBB"), {}, 300, 0});
  vals.validators.push_back(validator{from_hex("CCCC"), {}, 330, 0});

  std::vector<std::string> proposers;
  for (auto i = 0; i < 99; i++) {
    auto val = vals.get_proposer();
//    std::cout << to_hex(val->address) << std::endl;
    if (val.has_value())
      proposers.push_back(to_hex(val.value().address));
//    print_validator_set(vals);
    vals.increment_proposer_priority(1);
  }

//  std::cout << "Summary:" << std::endl;
  int a{}, b{}, c{};
  for (const auto& e : proposers) {
    if (e == "aaaa") a++;
    if (e == "bbbb") b++;
    if (e == "cccc") c++;
  }
//  std::cout << "a=" << a << " b=" << b << " c=" << c << std::endl;
  CHECK((a == 61 && b == 18 && c == 20));
}

TEST_CASE("Apply update", "[validator_set]") {
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

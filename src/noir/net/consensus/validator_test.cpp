#include <catch2/catch_all.hpp>
#include <noir/net/consensus/validator.h>
#include <noir/common/hex.h>

using namespace noir;
using namespace noir::net::consensus;

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
  vals.validators.push_back(validator{from_hex("AAA"), {}, 1000, 0});
  vals.validators.push_back(validator{from_hex("BBB"), {}, 300, 0});
  vals.validators.push_back(validator{from_hex("CCC"), {}, 330, 0});

  std::vector<std::string> proposers;
  for (auto i = 0; i < 99; i++) {
    auto val = vals.get_proposer();
//    std::cout << to_hex(val->address) << std::endl;
    if (val.has_value())
      proposers.push_back(to_hex(val.value().address));
//    for (auto e : vals.validators) {
//      std::cout << i << ":" << to_hex(e.address) << " " << e.voting_power << " " << e.proposer_priority << std::endl;
//    }
    vals.increment_proposer_priority(1);
  }

  std::cout << "Summary:" << std::endl;
  int a{}, b{}, c{};
  for (auto e: proposers) {
    if (e == "0aaa") a++;
    if (e == "0bbb") b++;
    if (e == "0ccc") c++;
  }
  std::cout << "a=" << a << " b=" << b << " c=" << c << std::endl;
}

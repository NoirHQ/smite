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
    validator val = { from_hex("0123456789")};
    vals.validators.push_back(val);
    CHECK(vals.get_by_index(-1) == std::nullopt);
    CHECK(vals.get_by_index(3) == std::nullopt);
    CHECK(vals.get_by_index(0).value().address == val.address);
  }
}

TEST_CASE("Validate", "[validator_set]") {

}
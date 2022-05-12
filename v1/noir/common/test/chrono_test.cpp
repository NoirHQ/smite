#include <catch2/catch_all.hpp>
#include <noir/common/chrono.h>

const auto parse_duration = noir::parse_duration<std::chrono::nanoseconds>;

TEST_CASE("parse time duration", "[noir][common]") {
  CHECK(parse_duration("1h").value() == std::chrono::minutes(60));
  CHECK(parse_duration("1m").value() == std::chrono::seconds(60));
  CHECK(parse_duration("1s").value() == std::chrono::milliseconds(1000));
  CHECK(parse_duration("1ms").value() == std::chrono::microseconds(1000));
  CHECK(parse_duration("1us").value()  == std::chrono::nanoseconds(1000));

  CHECK(parse_duration("1m40s").value() == std::chrono::seconds(100));
  CHECK(parse_duration("2s50ms").value() == std::chrono::milliseconds(2050));

  CHECK(parse_duration("-1m40s").value() == std::chrono::seconds(-100));

  CHECK(noir::parse_duration<std::chrono::microseconds>("100ns").value() == std::chrono::microseconds(0));
  CHECK(noir::parse_duration<std::chrono::microseconds>("900ns").value() == std::chrono::microseconds(0));
  CHECK(noir::parse_duration<std::chrono::microseconds>("1100ns").value() == std::chrono::microseconds(1));
}

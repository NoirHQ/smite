#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/common/hex.h>

using namespace noir;
using namespace noir::codec;

TEMPLATE_TEST_CASE("Fixed-width integers/Boolean", "[codec][scale]", bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t) {
  TestType v = std::numeric_limits<TestType>::max();
  auto hex = to_hex((const char*)&v, sizeof(v));
  auto data = encode<scale>(v);
  CHECK(to_hex(data).compare(hex) == 0);

  v = decode<scale,TestType>(data);
  CHECK(v == std::numeric_limits<TestType>::max());

  v = std::numeric_limits<TestType>::min();
  hex = to_hex((const char*)&v, sizeof(v));
  data = encode<scale>(v);
  CHECK(to_hex(data).compare(hex) == 0);

  v = decode<scale,TestType>(data);
  CHECK(v == std::numeric_limits<TestType>::min());
}

TEST_CASE("Compact/general integers", "[codec][scale]") {
  auto v = unsigned_int(0);
  auto data = encode<scale>(v);
  CHECK(to_hex(data) == "00");

  v = 1;
  data = encode<scale>(v);
  CHECK(to_hex(data) == "04");

  v = 42;
  data = encode<scale>(v);
  CHECK(to_hex(data) == "a8");

  v = 69;
  data = encode<scale>(v);
  CHECK(to_hex(data) == "1501");

  v = 65535;
  data = encode<scale>(v);
  CHECK(to_hex(data) == "feff0300");

  // TODO: BigInt(100000000000000)
}

TEST_CASE("Options", "[codec][scale]") {
  SECTION("bool") {
    auto v = std::make_optional(true);
    auto data = encode<scale>(v);
    CHECK(to_hex(data) == "01");

    v = decode<scale,std::optional<bool>>(data);
    CHECK((v && *v == true));

    v = std::make_optional(false);
    data = encode<scale>(v);
    CHECK(to_hex(data) == "02");

    v = decode<scale,std::optional<bool>>(data);
    CHECK((v && *v == false));

    v = {};
    data = encode<scale>(v);
    CHECK(to_hex(data) == "00");

    v = decode<scale,std::optional<bool>>(data);
    CHECK(!v);
  }

  SECTION("except bool") {
    auto v = std::make_optional<uint32_t>(65535u);
    auto data = encode<scale>(v);
    CHECK(to_hex(data) == "01ffff0000");

    v = decode<scale,std::optional<uint32_t>>(data);
    CHECK((v && *v == 65535u));

    v = {};
    data = encode<scale>(v);
    CHECK(to_hex(data) == "00");

    v = decode<scale,std::optional<uint32_t>>(data);
    CHECK(!v);
  }
}

TEST_CASE("Results", "[codec][scale]") {
  using u8_b = noir::expected<uint8_t, bool>;

  auto v = u8_b(42);
  auto data = encode<scale>(v);
  CHECK(to_hex(data) == "002a");

  v = decode<scale, u8_b>(data);
  CHECK((v && *v == 42));

  v = noir::make_unexpected(false);
  data = encode<scale>(v);
  CHECK(to_hex(data) == "0100");

  v = decode<scale, u8_b>(data);
  CHECK((!v && v.error() == false));
}

TEST_CASE("Vectors", "[codec][scale]") {
  auto v = std::vector<uint16_t>{4, 8, 15, 16, 23, 42};
  auto data = encode<scale>(v);
  CHECK(to_hex(data) == "18040008000f00100017002a00");

  auto w = decode<scale, std::vector<uint16_t>>(data);
  CHECK(std::equal(v.begin(), v.end(), w.begin(), w.end()));
}

TEST_CASE("Strings", "[codec][scale]") {
  auto v = std::string("The quick brown fox jumps over the lazy dog.");
  auto data = encode<scale>(v);
  CHECK(to_hex(data) == "b054686520717569636b2062726f776e20666f78206a756d7073206f76657220746865206c617a7920646f672e");

  auto w = decode<scale,std::string>(data);
  CHECK(v == w);
}

TEST_CASE("Tuples", "[codec][scale]") {
  auto v = std::make_tuple((unsigned_int)3, false);
  auto data = encode<scale>(v);
  CHECK(to_hex(data) == "0c00");

  v = decode<scale, std::tuple<unsigned_int, bool>>(data);
  CHECK(v == std::make_tuple(3u, false));
}

TEST_CASE("Data structures", "[codec][scale]") {
  struct foo {
    std::string s;
    uint32_t i;
    std::optional<bool> b;
    std::vector<uint16_t> a;
  };

  auto v = foo{"Lorem ipsum", 42, false, {3, 5, 2, 8}};
  auto data = encode<scale>(v);
  CHECK(to_hex(data) == "2c4c6f72656d20697073756d2a00000002100300050002000800");

  v = decode<scale, foo>(data);
  auto a = std::vector<uint16_t>{3, 5, 2, 8};
  CHECK((v.s == "Lorem ipsum" && v.i == 42 && v.b && !*v.b && std::equal(v.a.begin(), v.a.end(), a.begin())));
}

TEST_CASE("Enumerations", "[codec][scale]") {
  using u8_b = std::variant<uint8_t, bool>;

  SECTION("int or bool") {
    auto v = u8_b((uint8_t) 42);
    auto data = encode<scale>(v);
    CHECK(to_hex(data) == "002a");

    v = decode<scale, u8_b>(data);
    CHECK((std::holds_alternative<uint8_t>(v) && v.index() == 0 && std::get<0>(v) == 42));

    v.emplace<1>(true);
    data = encode<scale>(v);
    CHECK(to_hex(data) == "0101");

    v = decode<scale, u8_b>(data);
    CHECK((std::holds_alternative<bool>(v) && v.index() == 1 && std::get<1>(v) == true));
  }

  SECTION("invalid index") {
    auto data = from_hex("0200");
    CHECK_THROWS_WITH((decode<scale, u8_b>(data)), "invalid variant index");
  }
}

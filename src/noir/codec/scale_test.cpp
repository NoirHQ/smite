#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/common/hex.h>

using namespace noir;
using namespace noir::codec;

TEMPLATE_TEST_CASE("Fixed-width integers/Boolean", "[codec][scale]", bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t) {
  TestType v = std::numeric_limits<TestType>::max();
  auto hex = to_hex({(const char*)&v, sizeof(v)});
  auto data = encode<scale>(v);
  CHECK(to_hex(data).compare(hex) == 0);

  v = decode<scale,TestType>(data);
  CHECK(v == std::numeric_limits<TestType>::max());

  v = std::numeric_limits<TestType>::min();
  hex = to_hex({(const char*)&v, sizeof(v)});
  data = encode<scale>(v);
  CHECK(to_hex(data).compare(hex) == 0);

  v = decode<scale,TestType>(data);
  CHECK(v == std::numeric_limits<TestType>::min());
}

TEST_CASE("Compact/general integers", "[codec][scale]") {
  auto tests = std::to_array<std::pair<unsigned_int, const char*>>({
    {0, "00"},
    {1, "04"},
    {42, "a8"},
    {69, "1501"},
    {65535, "feff0300"},
    // TODO: BigInt(100000000000000)
  });

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    CHECK(to_hex(encode<scale>(t.first)) == t.second);
    CHECK(t.first == decode<scale,unsigned_int>(from_hex(t.second)));
  });
}

TEST_CASE("Options", "[codec][scale]") {
  SECTION("bool") {
    auto tests = std::to_array<std::pair<std::optional<bool>, const char*>>({
      {true, "01"},
      {false, "02"},
      {{}, "00"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      CHECK(to_hex(encode<scale>(t.first)) == t.second);
      CHECK(t.first == decode<scale, std::optional<bool>>(from_hex(t.second)));
    });
  }

  SECTION("except bool") {
    auto tests = std::to_array<std::pair<std::optional<uint32_t>, const char*>>({
      {65535u, "01ffff0000"},
      {{}, "00"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      CHECK(to_hex(encode<scale>(t.first)) == t.second);
      CHECK(t.first == decode<scale, std::optional<uint32_t>>(from_hex(t.second)));
    });
  }
}

TEST_CASE("Results", "[codec][scale]") {
  using u8_b = noir::expected<uint8_t, bool>;

  auto tests = std::to_array<std::pair<u8_b, const char*>>({
    {42, "002a"},
    {make_unexpected(false), "0100"},
  });

  std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
    CHECK(to_hex(encode<scale>(t.first)) == t.second);
    CHECK(t.first == decode<scale, u8_b>(from_hex(t.second)));
  });
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
    auto tests = std::to_array<std::pair<u8_b, const char*>>({
      {(uint8_t)42, "002a"},
      {true, "0101"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      CHECK(to_hex(encode<scale>(t.first)) == t.second);
      CHECK(t.first == decode<scale, u8_b>(from_hex(t.second)));
    });
  }

  SECTION("invalid index") {
    auto data = from_hex("0200");
    CHECK_THROWS_WITH((decode<scale, u8_b>(data)), "invalid variant index");
  }
}

// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/rlp.h>
#include <noir/common/hex.h>
#include <noir/common/types.h>

using namespace noir;
using namespace noir::codec::rlp;

TEST_CASE("[rlp] booleans", "[codec]") {
  auto tests = std::to_array<std::pair<bool, const char*>>({
    {true, "01"},
    {false, "80"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(to_hex(encode(t.first)) == t.second);
    CHECK(t.first == decode<uint32_t>(from_hex(t.second)));
  });
}

TEST_CASE("[rlp] integers", "[codec]") {
  SECTION("uint32_t") {
    auto tests = std::to_array<std::pair<uint32_t, const char*>>({
      {0, "80"},
      {127, "7f"},
      {128, "8180"},
      {256, "820100"},
      {1024, "820400"},
      {0xffffff, "83ffffff"},
      {0xffffffff, "84ffffffff"},
    });

    std::for_each(tests.begin(), tests.end(), [&](auto& t) {
      CHECK(to_hex(encode(t.first)) == t.second);
      CHECK(t.first == decode<uint32_t>(from_hex(t.second)));
    });
  }

  SECTION("uint64_t") {
    auto tests = std::to_array<std::pair<uint64_t, const char*>>({
      {0xffffffff, "84ffffffff"},
      {0xffffffffff, "85ffffffffff"},
      {0xffffffffffff, "86ffffffffffff"},
      {0xffffffffffffff, "87ffffffffffffff"},
      {0xffffffffffffffff, "88ffffffffffffffff"},
    });

    std::for_each(tests.begin(), tests.end(), [&](auto& t) {
      CHECK(to_hex(encode(t.first)) == t.second);
      CHECK(t.first == decode<uint64_t>(from_hex(t.second)));
    });
  }
}

TEST_CASE("[rlp] strings", "[codec]") {
  auto tests = std::to_array<std::pair<std::string, const char*>>({
    {"", "80"},
    {"\x7e", "7e"},
    {"\x7f", "7f"},
    {"\x80", "8180"},
    {"dog", "83646f67"},
    {"Lorem ipsum dolor sit amet, consectetur adipisicing eli",
      "b74c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e7365637465747572206164697069736963696e6720656c6"
      "9"},
    {"Lorem ipsum dolor sit amet, consectetur adipisicing elit",
      "b8384c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e7365637465747572206164697069736963696e6720656c"
      "6974"},
    {"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur mauris magna, suscipit sed vehicula non, "
     "iaculis faucibus tortor. Proin suscipit ultricies malesuada. Duis tortor elit, dictum quis tristique eu, "
     "ultrices at risus. Morbi a est imperdiet mi ullamcorper aliquet suscipit nec lorem. Aenean quis leo mollis, "
     "vulputate elit varius, consequat enim. Nulla ultrices turpis justo, et posuere urna consectetur nec. Proin non "
     "convallis metus. Donec tempor ipsum in mauris congue sollicitudin. Vestibulum ante ipsum primis in faucibus orci "
     "luctus et ultrices posuere cubilia Curae; Suspendisse convallis sem vel massa faucibus, eget lacinia lacus "
     "tempor. Nulla quis ultricies purus. Proin auctor rhoncus nibh condimentum mollis. Aliquam consequat enim at "
     "metus luctus, a eleifend purus egestas. Curabitur at nibh metus. Nam bibendum, neque at auctor tristique, lorem "
     "libero aliquet arcu, non interdum tellus lectus sit amet eros. Cras rhoncus, metus ac ornare cursus, dolor justo "
     "ultrices metus, at ullamcorper volutpat",
      "b904004c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e73656374657475722061646970697363696e6720656c"
      "69742e20437572616269747572206d6175726973206d61676e612c20737573636970697420736564207665686963756c61206e6f6e2c2069"
      "6163756c697320666175636962757320746f72746f722e2050726f696e20737573636970697420756c74726963696573206d616c65737561"
      "64612e204475697320746f72746f7220656c69742c2064696374756d2071756973207472697374697175652065752c20756c747269636573"
      "2061742072697375732e204d6f72626920612065737420696d70657264696574206d6920756c6c616d636f7270657220616c697175657420"
      "7375736369706974206e6563206c6f72656d2e2041656e65616e2071756973206c656f206d6f6c6c69732c2076756c70757461746520656c"
      "6974207661726975732c20636f6e73657175617420656e696d2e204e756c6c6120756c74726963657320747572706973206a7573746f2c20"
      "657420706f73756572652075726e6120636f6e7365637465747572206e65632e2050726f696e206e6f6e20636f6e76616c6c6973206d6574"
      "75732e20446f6e65632074656d706f7220697073756d20696e206d617572697320636f6e67756520736f6c6c696369747564696e2e205665"
      "73746962756c756d20616e746520697073756d207072696d697320696e206661756369627573206f726369206c756374757320657420756c"
      "74726963657320706f737565726520637562696c69612043757261653b2053757370656e646973736520636f6e76616c6c69732073656d20"
      "76656c206d617373612066617563696275732c2065676574206c6163696e6961206c616375732074656d706f722e204e756c6c6120717569"
      "7320756c747269636965732070757275732e2050726f696e20617563746f722072686f6e637573206e69626820636f6e64696d656e74756d"
      "206d6f6c6c69732e20416c697175616d20636f6e73657175617420656e696d206174206d65747573206c75637475732c206120656c656966"
      "656e6420707572757320656765737461732e20437572616269747572206174206e696268206d657475732e204e616d20626962656e64756d"
      "2c206e6571756520617420617563746f72207472697374697175652c206c6f72656d206c696265726f20616c697175657420617263752c20"
      "6e6f6e20696e74657264756d2074656c6c7573206c65637475732073697420616d65742065726f732e20437261732072686f6e6375732c20"
      "6d65747573206163206f726e617265206375727375732c20646f6c6f72206a7573746f20756c747269636573206d657475732c2061742075"
      "6c6c616d636f7270657220766f6c7574706174"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(to_hex(encode(t.first)) == t.second);
    CHECK(t.first == decode<std::string>(from_hex(t.second)));
  });
}

TEST_CASE("[rlp] list", "[codec]") {
  SECTION("a list of uint32_t") {
    auto tests = std::to_array<std::pair<std::vector<uint32_t>, const char*>>({
      {{}, "c0"},
      {{1, 2, 3, 4, 5}, "c50102030405"},
    });

    std::for_each(tests.begin(), tests.end(), [&](auto& t) {
      CHECK(to_hex(encode(t.first)) == t.second);
      CHECK(t.first == decode<std::vector<uint32_t>>(from_hex(t.second)));
    });
  }

  SECTION("a list of lists of strings") {
    auto t = std::pair<std::vector<std::vector<std::string>>, const char*>(
      {{{"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"},
         {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}, {"asdf", "qwer", "zxcv"}},
        "f90200cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf846173"
        "64668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572"
        "847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84"
        "617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf8461736466847177"
        "6572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376"
        "cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf846173646684"
        "71776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a78"
        "6376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364"
        "668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf8461736466847177657284"
        "7a786376cf84617364668471776572847a786376"});

    CHECK(to_hex(encode(t.first)) == t.second);
    CHECK(t.first == decode<std::vector<std::vector<std::string>>>(from_hex(t.second)));
  }

  SECTION("a list of strings, c-array") {
    std::string test[] = {
      "aaa", "bbb", "ccc", "ddd", "eee", "fff", "ggg", "hhh", "iii", "jjj", "kkk", "lll", "mmm", "nnn", "ooo"};
    auto data_s = "f83c836161618362626283636363836464648365656583666666836767678368686883696969836a6a6a836b6b6b836c6c6c"
                  "836d6d6d836e6e6e836f6f6f";

    CHECK(to_hex(encode(test)) == data_s);

    std::string decoded[std::span(test).size()];
    auto data = from_hex(data_s);
    datastream<const char> ds(data);
    ds >> decoded;

    CHECK(std::equal(std::begin(test), std::end(test), std::begin(decoded)));
  }

  SECTION("byte array") {
    {
      auto s = std::string{"949c1185a5c5e9fc54612808977ee8f548b2258d31"};
      auto v = bytes20(s.substr(2));
      auto data = encode(v);
      CHECK(to_hex(data) == s);
    }

    {
      auto s = std::string{"a0e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
      auto v = bytes32(s.substr(2));
      auto data = encode(v);
      CHECK(to_hex(data) == s);
    }
  }
}

TEST_CASE("[rlp] structs", "[codec]") {
  struct simplestruct {
    unsigned int A;
    std::string B;
  };

  SECTION("empty") {
    auto v = simplestruct{};
    auto data = encode(v);
    CHECK(to_hex(data) == "c28080");

    auto w = decode<simplestruct>(data);
    CHECK(std::tie(v.A, v.B) == std::tie(w.A, w.B));
  }

  SECTION("with value") {
    auto v = simplestruct{3, "foo"};
    auto data = encode(v);
    CHECK(to_hex(data) == "c50383666f6f");

    auto w = decode<simplestruct>(data);
    CHECK(std::tie(v.A, v.B) == std::tie(w.A, w.B));
  }
}

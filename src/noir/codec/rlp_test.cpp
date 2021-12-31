#include <catch2/catch_all.hpp>
#include <noir/codec/rlp.h>
#include <noir/common/hex.h>

using namespace noir;
using namespace noir::codec;

TEST_CASE("booleans", "[codec][rlp]") {
  auto tests = std::to_array<std::pair<bool, const char*>>({
    {true, "01"},
    {false, "80"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(to_hex(encode<rlp>(t.first)) == t.second);
    CHECK(t.first == decode<rlp,uint32_t>(from_hex(t.second)));
  });
}

TEST_CASE("integers", "[codec][rlp]") {
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
      CHECK(to_hex(encode<rlp>(t.first)) == t.second);
      CHECK(t.first == decode<rlp,uint32_t>(from_hex(t.second)));
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
      CHECK(to_hex(encode<rlp>(t.first)) == t.second);
      CHECK(t.first == decode<rlp,uint64_t>(from_hex(t.second)));
    });
  }
}

TEST_CASE("strings", "[codec][rlp]") {
  auto tests = std::to_array<std::pair<std::string, const char*>>({
    {"", "80"},
    {"\x7e", "7e"},
    {"\x7f", "7f"},
    {"\x80", "8180"},
    {"dog", "83646f67"},
    {"Lorem ipsum dolor sit amet, consectetur adipisicing eli",
     "b74c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e7365637465747572206164697069736963696e6720656c69"},
    {"Lorem ipsum dolor sit amet, consectetur adipisicing elit",
     "b8384c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e7365637465747572206164697069736963696e6720656c6974"},
    {"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur mauris magna, suscipit sed vehicula non, iaculis faucibus tortor. Proin suscipit ultricies malesuada. Duis tortor elit, dictum quis tristique eu, ultrices at risus. Morbi a est imperdiet mi ullamcorper aliquet suscipit nec lorem. Aenean quis leo mollis, vulputate elit varius, consequat enim. Nulla ultrices turpis justo, et posuere urna consectetur nec. Proin non convallis metus. Donec tempor ipsum in mauris congue sollicitudin. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Suspendisse convallis sem vel massa faucibus, eget lacinia lacus tempor. Nulla quis ultricies purus. Proin auctor rhoncus nibh condimentum mollis. Aliquam consequat enim at metus luctus, a eleifend purus egestas. Curabitur at nibh metus. Nam bibendum, neque at auctor tristique, lorem libero aliquet arcu, non interdum tellus lectus sit amet eros. Cras rhoncus, metus ac ornare cursus, dolor justo ultrices metus, at ullamcorper volutpat",
     "b904004c6f72656d20697073756d20646f6c6f722073697420616d65742c20636f6e73656374657475722061646970697363696e6720656c69742e20437572616269747572206d6175726973206d61676e612c20737573636970697420736564207665686963756c61206e6f6e2c20696163756c697320666175636962757320746f72746f722e2050726f696e20737573636970697420756c74726963696573206d616c6573756164612e204475697320746f72746f7220656c69742c2064696374756d2071756973207472697374697175652065752c20756c7472696365732061742072697375732e204d6f72626920612065737420696d70657264696574206d6920756c6c616d636f7270657220616c6971756574207375736369706974206e6563206c6f72656d2e2041656e65616e2071756973206c656f206d6f6c6c69732c2076756c70757461746520656c6974207661726975732c20636f6e73657175617420656e696d2e204e756c6c6120756c74726963657320747572706973206a7573746f2c20657420706f73756572652075726e6120636f6e7365637465747572206e65632e2050726f696e206e6f6e20636f6e76616c6c6973206d657475732e20446f6e65632074656d706f7220697073756d20696e206d617572697320636f6e67756520736f6c6c696369747564696e2e20566573746962756c756d20616e746520697073756d207072696d697320696e206661756369627573206f726369206c756374757320657420756c74726963657320706f737565726520637562696c69612043757261653b2053757370656e646973736520636f6e76616c6c69732073656d2076656c206d617373612066617563696275732c2065676574206c6163696e6961206c616375732074656d706f722e204e756c6c61207175697320756c747269636965732070757275732e2050726f696e20617563746f722072686f6e637573206e69626820636f6e64696d656e74756d206d6f6c6c69732e20416c697175616d20636f6e73657175617420656e696d206174206d65747573206c75637475732c206120656c656966656e6420707572757320656765737461732e20437572616269747572206174206e696268206d657475732e204e616d20626962656e64756d2c206e6571756520617420617563746f72207472697374697175652c206c6f72656d206c696265726f20616c697175657420617263752c206e6f6e20696e74657264756d2074656c6c7573206c65637475732073697420616d65742065726f732e20437261732072686f6e6375732c206d65747573206163206f726e617265206375727375732c20646f6c6f72206a7573746f20756c747269636573206d657475732c20617420756c6c616d636f7270657220766f6c7574706174"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(to_hex(encode<rlp>(t.first)) == t.second);
    CHECK(t.first == decode<rlp,std::string>(from_hex(t.second)));
  });
}

TEST_CASE("list", "[codec][rlp]") {
  SECTION("a list of uint32_t") {
    auto tests = std::to_array<std::pair<std::vector<uint32_t>, const char*>>({
      {{}, "c0"},
      {{1, 2, 3, 4, 5}, "c50102030405"},
    });

    std::for_each(tests.begin(), tests.end(), [&](auto& t) {
      CHECK(to_hex(encode<rlp>(t.first)) == t.second);
      CHECK(t.first == decode<rlp,std::vector<uint32_t>>(from_hex(t.second)));
    });
  }

  SECTION("a list of lists of strings") {
    auto t = std::pair<std::vector<std::vector<std::string>>, const char*>({
      {{"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"},
       {"asdf", "qwer", "zxcv"}},
       "f90200cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376cf84617364668471776572847a786376"
    });

    CHECK(to_hex(encode<rlp>(t.first)) == t.second);
    CHECK(t.first == decode<rlp,std::vector<std::vector<std::string>>>(from_hex(t.second)));
  }
}

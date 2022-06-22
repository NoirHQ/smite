// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/protobuf.h>
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/encoding_helper.h>
#include <noir/consensus/types/evidence.h>
#include <noir/core/codec.h>

#include <date/tz.h>

using namespace noir;
using namespace noir::consensus;

p2p::block_id make_block_id(Bytes hash, uint32_t part_set_size, Bytes part_set_hash) {
  return {.hash = std::move(hash), .parts = {.total = part_set_size, .hash = std::move(part_set_hash)}};
}

Bytes string_to_bytes(std::string_view s) {
  return {s.begin(), s.end()};
}

TEST_CASE("block: make part_set", "[noir][consensus]") {
  block org{block_header{}, block_data{.hash = {0, 1, 2, 3, 4, 5}}, {}, nullptr};
  uint32_t part_size{block_part_size_bytes};
  // uint32_t part_size{3};
  auto ps = org.make_part_set(part_size);
  auto restored = block::new_block_from_part_set(ps);
  CHECK(org.data.hash == restored->data.hash);
}

TEST_CASE("block: encode using datastream", "[noir][consensus]") {
  block org{block_header{}, block_data{.txs = {{0}, {1}, {2}}}, {}, std::make_unique<commit>()};
  auto data = encode(org);
  auto decoded = decode<block>(data);
  CHECK(org.data.get_hash() == decoded.data.get_hash());
}

TEST_CASE("block: encode using protobuf", "[noir][consensus]") {
  block org{block_header{}, block_data{.txs = {{0}, {1}, {2}}}, {}, nullptr};
  auto pb = block::to_proto(org);
  Bytes bz(pb->ByteSizeLong());
  pb->SerializeToArray(bz.data(), pb->ByteSizeLong());

  ::tendermint::types::Block pb_block;
  pb_block.ParseFromArray(bz.data(), bz.size());
  auto decoded = block::from_proto(pb_block);
  CHECK(org.data.get_hash() == decoded->data.get_hash());
}

TEST_CASE("block: decode Bytes", "[noir][consensus]") {
  // Generated raw bytes from golang implementation
  Bytes bz(
    "0A9B010A02080B18E5C8EF81FDA8E38854220B088092B8C398FEFFFFFF012A0212003220719623891E9BE9264D843E8D09ED0E7C1ADCAB9536"
    "151B549B798E2C261FF5B53A209D1843C1A24A654A39FF3448714BC9FE69DA0170FA390B1AC67047377BA72F0E6A20782B9CDBA39567A3856F"
    "4D9885AF0E810C4F1D6724722410C5DD88D0E057F96C72141F4F060D6582D8B6BBAF5DDFFD2A35A75BA8F93A12030A01011A84030A81030AFE"
    "020AB601080210E5C8EF81FDA8E3885422480A20901FDF29BF67C81F143140F670D580506DF52CA2241E82C239AD6F6A3AFE20241224080112"
    "20170B2B8240767286AB6D638F7B5610058A4A65270F773EF1E0A97F15D30987B72A060880DBAAE10532145B7B55EC3297A2FB02101128F133"
    "A6B388DF403F4240BD2C2FE45D8E436F553D2BAF0506DE847134217DAC0B7091A5BACB60706BE9F47DE1B0ECF2ADFC5B735EA6D1D2CA83C9C9"
    "52D67EB8FB77E7523BEB059A865F0212B601080210E5C8EF81FDA8E3885422480A20B72EAA2D1CF0C554BB636DD20B1D994C46A2CE69EC4138"
    "8A1496C447026089E712240801122016752B3E9CDD3EEBBCD0E2D33D17F61FDC4FC9F9C85635296A8E0893D9E379AE2A060880DBAAE1053214"
    "5B7B55EC3297A2FB02101128F133A6B388DF403F42401296C2D7C85A9AFDD0F44B8D333C7A0928B51D7594077C78CDCA3F66710BE94431CBA5"
    "456AAE668DB88158B03DF9DA8514A23A6697961F4B4832EDAAF3698D06180A200A2A060880DBAAE10522F208080210011A480A20FCF931AA32"
    "31272DB13DA3DC0243BD575AB812A9796BAE10B04B2B9F8B09E40D1224087B12207DD685EA7E951E8AE2B16B60F702F32E46A28C1CEEB96F59"
    "EC520E58CFAA8B102268080212142901C622299D37139E430BBA96BD9418C66555951A0C08D7F7C4950610B8B7B2C7022240358E316149B7B1"
    "2A4C588A69A195A46D69F91CCBDA21CCD57F957877E5495147A99AB84C2D80E4D05211DB2BFB4981C302493201FF2EF85F9CC1CBD7129E190F"
    "22680802121454742AAE037F942D040E4332A56C129EDE18FB411A0C08D7F7C4950610B8B7B2C7022240A4C8C2656532E80724425DB61B3C37"
    "D1E1E96C1D3F2B96B428776F68339086AC467AD8F02F8BC6F488B00FAE0364B5146A1ABB66B7C39EC272C55BD7178BA8002268080212145B0C"
    "1C2141B41412C56E3D202CEBDF9712E665D01A0C08D7F7C4950610B8B7B2C70222400CBEABAD6B92045B40A646495C6E61DE05D9090B6BACA4"
    "9BF407C03A5FB3B2E112A78A57E86B6DDA698B68CDBF6C9D03875395A95A4FEBCBAF22D62FBF8750012268080212147D026B0FBB09EB9CD203"
    "EC857B314FE8E8FF0C091A0C08D7F7C4950610B8B7B2C70222408C5A9C7E7A654E758250C87EA48D54156582BB739205735D64ED398EA55AA7"
    "CCB013251EE68699D468F902B1E1CC8560DD2DB3240F03C270659CFAF363F785072268080212148CE013FC552F4EB187234C1788E1C3574C49"
    "43F61A0C08D7F7C4950610B8B7B2C70222401CD6221DF16A98213F01794AEBCB6BB2F58BC53F24B2227A15D7904CED96AA46D947BCB67323E4"
    "C771CEEB86416C7F50FFF01E8DFD27BF0128972473231E7D02226808021214BAD9CB832AF8A606AF91F93AADBC9BA176FEAFBC1A0C08D7F7C4"
    "950610B8B7B2C7022240A7E9A86212EB2AD2E4FF60C6369C195FCF56FA699AD62356AE831793EB18CDC84E2DB228958FF34A95CAB7C4F2FA79"
    "B031728761F9FCB7BF5ADC09C66347130B226808021214CE98A34CD2B6BB43FD4888A2B6D3722C1231EEAE1A0C08D7F7C4950610B8B7B2C702"
    "224024169CD69AF9B8F2B8C3D1E805B5F43FCE52677A98EDA7B2D8F8FFC1067D34626C21122EA94834578557FE59007960F9E0F695C3F9F0A6"
    "35F7E566BB41AB110B226808021214D05634869B325C4C4D79DA1E51F21931294549181A0C08D7F7C4950610B8B7B2C70222405A0823AAF636"
    "039EDD41FD989371C7E9BB4C8F804C9C32BC74FF2AEEF269FFD14A9B78F140AC03684F25AEEA690C7370B0DDA4121C9C2732114CC8BE4F6C7B"
    "06226808021214D9C9A20766DC95ED07AC2038C675C245E6FE2AD21A0C08D7F7C4950610B8B7B2C70222408A66A272FE626598CC72759EA083"
    "E95F5F720113EEB29B3F6B74A388F46592FFFF5AF649E3EFB90EC114D6D608A95110FE58ACC4DB619F0ADC2D64670C746506226808021214E4"
    "081D4A0EDB53E9163C465EE1D6CC634C1D72BC1A0C08D7F7C4950610B8B7B2C7022240F210E14CE4B8ED967A6A1D18FA42B51F695D557CF057"
    "1B169369253F81D42221A415BB38AE51C9908AFB40CB198793E49D3549C9E144354EE3B346D9F0C02F04");
  // Block{
  //   Header{
  //     Version:        {11 0}
  //     ChainID:
  //     Height:         6057778313365808229
  //     Time:           0001-01-01 00:00:00 +0000 UTC
  //     LastBlockID:    :0:000000000000
  //     LastCommit:     719623891E9BE9264D843E8D09ED0E7C1ADCAB9536151B549B798E2C261FF5B5
  //     Data:           9D1843C1A24A654A39FF3448714BC9FE69DA0170FA390B1AC67047377BA72F0E
  //     Validators:
  //     NextValidators:
  //     App:
  //     Consensus:
  //     Results:
  //     Evidence:       782B9CDBA39567A3856F4D9885AF0E810C4F1D6724722410C5DD88D0E057F96C
  //     Proposer:       1F4F060D6582D8B6BBAF5DDFFD2A35A75BA8F93A
  //   }#
  //   Data{
  //     4BF5122F344554C53BDE2EBB8CD2B7E3D1600AD631C385A5D7CCE23C7785459A (1 bytes)
  //   }#9D1843C1A24A654A39FF3448714BC9FE69DA0170FA390B1AC67047377BA72F0E
  //   EvidenceData{
  //     Evidence:DuplicateVoteEvidence{VoteA: Vote{0:5B7B55EC3297
  //     6057778313365808229/00/SIGNED_MSG_TYPE_PRECOMMIT(Precommit) 901FDF29BF67 BD2C2FE45D8E @ 2019-01-01T00:00:00Z},
  //     VoteB: Vote{0:5B7B55EC3297 6057778313365808229/00/SIGNED_MSG_TYPE_PRECOMMIT(Precommit) B72EAA2D1CF0
  //     1296C2D7C85A @ 2019-01-01T00:00:00Z}}
  //   }#782B9CDBA39567A3856F4D9885AF0E810C4F1D6724722410C5DD88D0E057F96C
  //   Commit{
  //     Height:     2
  //     Round:      1
  //     BlockID:    FCF931AA3231272DB13DA3DC0243BD575AB812A9796BAE10B04B2B9F8B09E40D:123:7DD685EA7E95
  //     Signatures:
  //       CommitSig{358E316149B7 by 2901C622299D on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{A4C8C2656532 by 54742AAE037F on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{0CBEABAD6B92 by 5B0C1C2141B4 on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{8C5A9C7E7A65 by 7D026B0FBB09 on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{1CD6221DF16A by 8CE013FC552F on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{A7E9A86212EB by BAD9CB832AF8 on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{24169CD69AF9 by CE98A34CD2B6 on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{5A0823AAF636 by D05634869B32 on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{8A66A272FE62 by D9C9A20766DC on 2 @ 2022-06-21T03:32:39.686595Z}
  //       CommitSig{F210E14CE4B8 by E4081D4A0EDB on 2 @ 2022-06-21T03:32:39.686595Z}
  //   }#719623891E9BE9264D843E8D09ED0E7C1ADCAB9536151B549B798E2C261FF5B5
  // }#
  ::tendermint::types::Block pb_block;
  pb_block.ParseFromArray(bz.data(), bz.size());
  auto decoded = block::from_proto(pb_block);

  auto& header = decoded->header;
  CHECK(header.get_hash().empty()); // due to empty validators
  CHECK((header.version.block == 11 && header.version.app == 0));
  CHECK(header.height == 6057778313365808229);
  auto sample_t = date::make_zoned(date::current_zone(), date::sys_days{date::January / 1 / 1});
  tstamp ts = sample_t.get_sys_time().time_since_epoch().count() * 1'000'000;
  CHECK(header.time == ts);
  CHECK(header.last_commit_hash == Bytes("719623891e9be9264d843e8d09ed0e7c1adcab9536151b549b798e2c261ff5b5"));
  CHECK(header.data_hash == Bytes("9d1843c1a24a654a39ff3448714bc9fe69da0170fa390b1ac67047377ba72f0e"));
  CHECK(header.evidence_hash == Bytes("782b9cdba39567a3856f4d9885af0e810c4f1d6724722410c5dd88d0e057f96c"));
  CHECK(header.proposer_address == Bytes("1f4f060d6582d8b6bbaf5ddffd2a35a75ba8f93a"));

  auto& data = decoded->data;
  CHECK(data.get_hash() == header.data_hash);
  if (!data.txs.empty())
    CHECK(crypto::Sha256()(data.txs[0]) == Bytes("4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a"));

  auto& evidence_ = decoded->evidence;
  CHECK(evidence_.get_hash() == header.evidence_hash);

  auto& commit_ = *decoded->last_commit;
  CHECK(commit_.get_hash() == header.last_commit_hash);
  CHECK(commit_.height == 2);
  CHECK(commit_.round == 1);
  CHECK(commit_.my_block_id.hash == Bytes("fcf931aa3231272db13da3dc0243bd575ab812a9796bae10b04b2b9f8b09e40d"));

  auto encoded = block::to_proto(*decoded);
  auto new_bz = codec::protobuf::encode(*encoded);
  CHECK(new_bz == bz);

  auto original_bz = codec::protobuf::encode(pb_block);
  CHECK(original_bz == bz);
}

TEST_CASE("block: cdc_encode", "[noir][consensus]") {
  SECTION("string") {
    std::string item{"abc"};
    auto bz = cdc_encode(item);
    CHECK(bz == Bytes("0a03616263"));
  }
  SECTION("int64") {
    int64_t item{100};
    auto bz = cdc_encode(item);
    CHECK(bz == Bytes("0864"));
  }
  SECTION("bytes") {
    Bytes item("61626364");
    auto bz = cdc_encode(item);
    CHECK(bz == Bytes("0a0461626364"));
  }
  SECTION("time") {
    // UTC to local time
    auto sample_t = date::make_zoned(date::current_zone(), date::sys_days{date::January / 1 / 2019});
    tstamp ts = sample_t.get_sys_time().time_since_epoch().count() * 1'000'000;
    auto bz = cdc_encode_time(ts);
    CHECK(bz == Bytes("0880dbaae105"));
  }
}

TEST_CASE("block: verify header hash", "[noir][consensus]") {
  using namespace std::chrono_literals;
  auto sample_t = date::make_zoned(date::current_zone(), date::sys_days{date::October / 13 / 2019} + 16h + 14min + 44s);
  tstamp ts = sample_t.get_sys_time().time_since_epoch().count() * 1'000'000;

  auto sha256 = crypto::Sha256();
  auto proposer_address_hash = sha256(string_to_bytes("proposer_address"));

  block_header h{
    .version = {.block = 1, .app = 2},
    .chain_id = "chainId",
    .height = 3,
    .time = ts,
    .last_block_id = make_block_id(Bytes(32), 6, Bytes(32)),
    .last_commit_hash = sha256(string_to_bytes("last_commit_hash")),
    .data_hash = sha256(string_to_bytes("data_hash")),
    .validators_hash = sha256(string_to_bytes("validators_hash")),
    .next_validators_hash = sha256(string_to_bytes("next_validators_hash")),
    .consensus_hash = sha256(string_to_bytes("consensus_hash")),
    .app_hash = sha256(string_to_bytes("app_hash")),
    .last_results_hash = sha256(string_to_bytes("last_results_hash")),
    .evidence_hash = sha256(string_to_bytes("evidence_hash")),
    .proposer_address = {proposer_address_hash.begin(), proposer_address_hash.begin() + 20},
  };
  CHECK(h.get_hash() == Bytes("f740121f553b5418c3efbd343c2dbfe9e007bb67b0d020a0741374bab65242a4"));
}

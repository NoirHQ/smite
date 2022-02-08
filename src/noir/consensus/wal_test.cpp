// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/wal.h>

namespace {

using namespace noir::consensus;

int count_dir(const std::string& tmp_path) {
  int cnt = 0;
  for (auto it = fc::directory_iterator(tmp_path); it != fc::directory_iterator(); ++it) {
    if (fc::is_regular_file(*it)) {
      ++cnt;
    }
  }
  return cnt;
}

auto prepare_codec = [](size_t enc_size) {
  static constexpr size_t rotation_file_size = 5;
  auto temp_dir = std::make_shared<fc::temp_directory>();
  auto tmp_path = temp_dir->path().string();
  REQUIRE(fc::is_directory(tmp_path) == true);
  REQUIRE(count_dir(tmp_path) == 0);

  return std::tuple<std::shared_ptr<fc::temp_directory>, std::shared_ptr<wal_codec>>(
    std::move(temp_dir), std::make_shared<wal_codec>(tmp_path, enc_size, rotation_file_size));
};

TEST_CASE("simple encode/decode", "[wal_codec]") {
  static constexpr size_t enc_size = 0x100;
  auto [temp_dir, wal_codec_] = prepare_codec(enc_size);

  int exp = 0;
  timed_wal_message msg{};
  size_t tmp;
  for (size_t size_ = 0; size_ < enc_size; ++exp, size_ += tmp) {
    auto ret = wal_codec_->encode(msg, tmp);
    CHECK(ret == true);
    ++msg.time;
  }
  CHECK(wal_codec_->flush_and_sync() == true);

  int count = 0;
  timed_wal_message ret{.msg = {end_height_message{}}};
  timed_wal_message exp_msg{};
  auto decoder = wal_codec_->new_wal_decoder(0);
  while (count <= exp) {
    auto res = decoder->decode(ret);
    CHECK(res != wal_decoder::result::corrupted);
    if (res == wal_decoder::result::eof) {
      break;
    }
    CHECK(ret.time == exp_msg.time);
    ++exp_msg.time;
    ++count;
  }
  CHECK(exp == count);
}

TEST_CASE("encode/decode with file rotation", "[wal_codec]") {
  static constexpr size_t enc_size = 0x100;
  auto [temp_dir, wal_codec_] = prepare_codec(enc_size);
  auto tmp_path = temp_dir->path().string();

  timed_wal_message msg{};
  int exp = 0;
  for (auto i = 0; i < 5; ++i) {
    for (size_t size_ = 0; size_ < enc_size;) {
      size_t tmp;
      CHECK(wal_codec_->encode(msg, tmp) == true);
      size_ += tmp;
      ++msg.time;
      exp++;
    }
  }
  CHECK(wal_codec_->flush_and_sync() == true);
  CHECK(count_dir(tmp_path) == 5);

  int count = 0;
  timed_wal_message exp_msg{};
  for (auto i = 0; i < 5; ++i) {
    auto decoder = wal_codec_->new_wal_decoder(i);
    timed_wal_message ret{.msg = {end_height_message{}}};
    while (count <= exp) {
      auto res = decoder->decode(ret);
      CHECK(res != wal_decoder::result::corrupted);
      if (res == wal_decoder::result::eof) {
        break;
      }
      CHECK(ret.time == exp_msg.time);
      ++exp_msg.time;
      ++count;
    }
  }
  CHECK(exp == count);

  for (auto i = 0; i < 5; ++i) {
    for (size_t size_ = 0; size_ < enc_size;) {
      size_t tmp;
      CHECK(wal_codec_->encode(msg, tmp) == true);
      size_ += tmp;
    }
  }
  CHECK(wal_codec_->flush_and_sync() == true);
  CHECK(count_dir(tmp_path) == 5);

  fc::remove_all(tmp_path);
}

} // namespace

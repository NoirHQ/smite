// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/scope_exit.h>
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

auto prepare_wal = [](size_t enc_size) {
  static constexpr size_t rotation_file_size = 5;
  auto temp_dir = std::make_shared<fc::temp_directory>();
  auto tmp_path = temp_dir->path().string();
  REQUIRE(fc::is_directory(tmp_path) == true);
  REQUIRE(count_dir(tmp_path) == 0);

  auto wal_ = std::make_shared<base_wal>(tmp_path, enc_size, rotation_file_size);
  wal_->set_flush_interval(std::chrono::milliseconds(100));

  return std::tuple<std::shared_ptr<fc::temp_directory>, std::shared_ptr<base_wal>>(
    std::move(temp_dir), std::move(wal_));
};

inline noir::bytes gen_random_bytes(size_t num) {
  noir::bytes ret(num);
  fc::rand_pseudo_bytes(ret.data(), static_cast<int>(ret.size()));
  return ret;
}

TEST_CASE("WAL write", "[basic_wal]") {
  static constexpr size_t enc_size = 0x100;
  auto [temp_dir, wal_] = prepare_wal(enc_size);
  wal_->on_start();
  auto defer = noir::make_scoped_exit([&wal_ = wal_]() { wal_->on_stop(); });

  noir::p2p::block_part_message bp_msg{
    .height = 1,
    .round = 1,
    .index = 1,
    .bytes_ = gen_random_bytes(32),
    .proof = gen_random_bytes(32),
  };

  CHECK(wal_->write({bp_msg}) == true);
  CHECK(wal_->flush_and_sync() == true);
}

TEST_CASE("WAL search_for_end_height", "[basic_wal]") {
  static constexpr size_t enc_size = 4096;
  auto [temp_dir, wal_] = prepare_wal(enc_size);

  {
    int64_t height = 0;
    bool found;
    auto dec = wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
    CHECK(found == false);
    CHECK(dec == nullptr);
  }

  wal_->on_start();
  auto defer = noir::make_scoped_exit([&wal_ = wal_]() { wal_->on_stop(); });
  {
    int64_t height = 0;
    bool found;
    // starting wal should write end_height_message with height 0
    auto dec = wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
    CHECK(found == true);
    CHECK(dec != nullptr);
  }

  for (int64_t height = 1; height < 50; ++height) {
    uint32_t index;
    for (index = 1; index < 10; ++index) {
      noir::p2p::block_part_message bp_msg{
        .height = height,
        .round = 1,
        .index = index,
        .bytes_ = gen_random_bytes(32),
        .proof = gen_random_bytes(32),
      };
      CHECK(wal_->write({bp_msg}) == true);
    }
    CHECK(wal_->write({noir::consensus::end_height_message{height}}) == true);
  }
  CHECK(wal_->flush_and_sync() == true);

  {
    int64_t height = 49;
    bool found;
    auto dec = wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
    CHECK(found == true);
    REQUIRE(dec != nullptr);
    timed_wal_message msg{};
    CHECK(dec->decode(msg) == wal_decoder::result::eof);
  }
  {
    int64_t height;
    bool deleted = false;
    for (height = 48; height >= 0; --height) {
      bool found;
      auto dec = wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
      if (deleted) {
        // decoder with deleted height should not be found
        CHECK(found == false);
      }
      if (!found) {
        deleted = true;
        CHECK(dec == nullptr);
        continue;
      }
      REQUIRE(dec != nullptr);
      timed_wal_message msg{};
      CHECK(dec->decode(msg) == wal_decoder::result::success);
      auto* body = get_if<noir::p2p::block_part_message>(&msg.msg.msg);
      REQUIRE(body != nullptr);
      CHECK(body->height == height + 1);
    }
  }
  CHECK(wal_->flush_and_sync() == true);
}

} // namespace

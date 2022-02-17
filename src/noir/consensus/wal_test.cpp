// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/scope_exit.h>
#include <noir/consensus/wal.h>
#include <filesystem>

namespace {

using namespace noir::consensus;
namespace fs = std::filesystem;

int count_dir(const std::string& tmp_path) {
  int cnt = 0;
  for (auto it = fs::directory_iterator(tmp_path); it != fs::directory_iterator(); ++it) {
    if (fs::is_regular_file(*it)) {
      ++cnt;
    }
  }
  return cnt;
}

TEST_CASE("simple encode/decode", "[wal_codec]") {
  static constexpr size_t enc_size = 0x100;
  static constexpr size_t rotation_file_num = 5;
  auto [temp_dir, wal_manager_] = [](size_t enc_size) {
    auto temp_dir = std::make_shared<fc::temp_directory>();
    auto tmp_path = temp_dir->path().string();
    auto wal_head_name = "wal";
    REQUIRE(fs::is_directory(tmp_path) == true);
    REQUIRE(count_dir(tmp_path) == 0);

    return std::tuple<std::shared_ptr<fc::temp_directory>, std::shared_ptr<wal_file_manager>>(
      std::move(temp_dir), std::make_shared<wal_file_manager>(tmp_path, wal_head_name, rotation_file_num, enc_size));
  }(enc_size);
  auto tmp_path = temp_dir->path().string();
  auto write_msg_to_wal_file = [&wal_manager_ = wal_manager_](const timed_wal_message& msg, size_t& len) {
    wal_manager_->update();
    auto wal_encoder_ = wal_manager_->get_wal_encoder();
    return wal_encoder_->encode(msg, len);
  };
  auto flush_to_wal_file = [&wal_manager_ = wal_manager_]() {
    auto wal_encoder_ = wal_manager_->get_wal_encoder();
    return wal_encoder_->flush_and_sync();
  };

  SECTION("simple encode/decode") {
    int exp = 0;
    timed_wal_message msg{};
    size_t tmp;

    for (size_t size_ = 0; size_ < enc_size; ++exp, size_ += tmp) {
      auto ret = write_msg_to_wal_file(msg, tmp);
      CHECK(ret == true);
      ++msg.time;
    }
    CHECK(flush_to_wal_file() == true);

    int count = 0;
    timed_wal_message ret{.msg = {end_height_message{}}};
    timed_wal_message exp_msg{};
    auto decoder = wal_manager_->get_wal_decoder(0);
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

  SECTION("encode/decode with file rotation") {
    timed_wal_message msg{};
    int exp = 0;
    for (auto i = 0; i < rotation_file_num; ++i) {
      for (size_t size_ = 0; size_ < enc_size;) {
        size_t tmp;
        CHECK(write_msg_to_wal_file(msg, tmp) == true);
        size_ += tmp;
        ++msg.time;
        exp++;
      }
    }
    CHECK(flush_to_wal_file() == true);
    CHECK(count_dir(tmp_path) == 5);

    int count = 0;
    timed_wal_message exp_msg{};
    for (auto i = 0; i < rotation_file_num; ++i) {
      auto decoder = wal_manager_->get_wal_decoder(i);
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

    for (auto i = 0; i < rotation_file_num; ++i) {
      for (size_t size_ = 0; size_ < enc_size;) {
        size_t tmp;
        CHECK(write_msg_to_wal_file(msg, tmp) == true);
        size_ += tmp;
      }
    }
    CHECK(flush_to_wal_file() == true);
    CHECK(count_dir(tmp_path) == rotation_file_num + 1); // wal, wal000, wal001, wal00x

    fs::remove_all(tmp_path);
  }

  SECTION("async test") {
    uint64_t max_thread_num = 10;
    auto thread = std::make_unique<noir::named_thread_pool>("test_thread", max_thread_num);

    uint64_t thread_num = std::min<uint64_t>(5, max_thread_num);
    uint64_t total_msg_num = 1000;
    std::atomic<uint64_t> token = thread_num;
    std::future<std::vector<std::optional<std::future<bool>>>> res[thread_num];
    SECTION("write in multi thread") {
      std::future<size_t> write_wal[thread_num];
      for (auto& res : write_wal) {
        res = noir::async_thread_pool(thread->get_executor(), [&]() {
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread

          timed_wal_message msg{};
          size_t count = 0;
          for (auto i = 0; i < total_msg_num; ++i) {
            size_t tmp;
            write_msg_to_wal_file(msg, tmp);
            ++count;
          }
          return count;
        });
      }
      size_t sum = 0;
      for (auto& res : write_wal) {
        res.wait();
        CHECK(res.valid() == true);
        sum += res.get();
      }
      CHECK(sum == total_msg_num * thread_num);
    }

    SECTION("write and flush in multi thread") {
      std::future<size_t> write_wal[thread_num];
      for (auto& res : write_wal) {
        res = noir::async_thread_pool(thread->get_executor(), [&]() {
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread

          timed_wal_message msg{};
          size_t count = 0;
          for (auto i = 0; i < total_msg_num; ++i) {
            size_t tmp;
            write_msg_to_wal_file(msg, tmp);
            flush_to_wal_file();
            ++count;
          }
          return count;
        });
      }
      size_t sum = 0;
      for (auto& res : write_wal) {
        res.wait();
        CHECK(res.valid() == true);
        sum += res.get();
      }
      CHECK(sum == total_msg_num * thread_num);
    }

    SECTION("mixed write and flush in multi thread") {
      std::future<size_t> write_wal[thread_num];
      for (auto& res : write_wal) {
        res = noir::async_thread_pool(thread->get_executor(), [&]() {
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread

          timed_wal_message msg{};
          size_t count = 0;
          for (auto i = 0; i < total_msg_num; ++i) {
            size_t tmp;
            if (i & 1) {
              write_msg_to_wal_file(msg, tmp);
            } else {
              write_msg_to_wal_file(msg, tmp);
              flush_to_wal_file();
            }
            ++count;
          }
          return count;
        });
      }
      size_t sum = 0;
      for (auto& res : write_wal) {
        res.wait();
        CHECK(res.valid() == true);
        sum += res.get();
      }
      CHECK(sum == total_msg_num * thread_num);
    }
  }
}

inline noir::bytes gen_random_bytes(size_t num) {
  noir::bytes ret(num);
  noir::crypto::rand_bytes(ret);
  return ret;
}

TEST_CASE("basic_wal test", "[basic_wal]") {
  static constexpr size_t enc_size = 4096;
  auto [temp_dir, wal_] = [](size_t enc_size) {
    static constexpr size_t rotation_file_num = 5;
    auto temp_dir = std::make_shared<fc::temp_directory>();
    auto tmp_path = temp_dir->path().string();
    auto wal_head_name = "wal";
    REQUIRE(fs::is_directory(tmp_path) == true);
    REQUIRE(count_dir(tmp_path) == 0);

    auto wal_ = std::make_shared<base_wal>(tmp_path, wal_head_name, rotation_file_num, enc_size);
    wal_->set_flush_interval(std::chrono::milliseconds(100));

    return std::tuple<std::shared_ptr<fc::temp_directory>, std::shared_ptr<base_wal>>(
      std::move(temp_dir), std::move(wal_));
  }(enc_size);
  auto tmp_path = temp_dir->path().string();
  auto defer = noir::make_scope_exit([&wal_ = wal_]() { wal_->on_stop(); });

  SECTION("WAL write") {
    noir::p2p::block_part_message bp_msg{
      .height = 1,
      .round = 1,
      .index = 1,
      .bytes_ = gen_random_bytes(32),
      .proof{.leaf_hash = gen_random_bytes(32)},
    };

    wal_->on_start();
    CHECK(wal_->write({noir::p2p::msg_info{.msg = bp_msg}}) == true);
    CHECK(wal_->flush_and_sync() == true);
  }

  SECTION("WAL search_for_end_height") {
    // initial state
    {
      int64_t height = 0;
      bool found;
      auto dec = wal_->search_for_end_height(height, {.ignore_data_corruption_errors = false}, found);
      CHECK(found == false);
      CHECK(dec == nullptr);
    }
    wal_->on_start();
    // starting wal should write end_height_message with height 0
    {
      int64_t height = 0;
      bool found;
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
          .proof{.leaf_hash = gen_random_bytes(32)},
        };
        CHECK(wal_->write({noir::p2p::msg_info{.msg = bp_msg}}) == true);
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
        auto* msg_body = get_if<noir::p2p::msg_info>(&msg.msg.msg);
        REQUIRE(msg_body != nullptr);
        auto* bp_msg = get_if<noir::p2p::block_part_message>(&msg_body->msg);
        REQUIRE(bp_msg != nullptr);
        CHECK(bp_msg->height == height + 1);
      }
    }
    CHECK(wal_->flush_and_sync() == true);
  }
}

} // namespace

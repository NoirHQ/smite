// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/thread_pool.h>
#include <noir/consensus/types.h>
#include <noir/consensus/types/events.h>
#include <noir/p2p/protocol.h>
#include <boost/asio/steady_timer.hpp>
#include <boost/filesystem/operations.hpp>
#include <fc/io/cfile.hpp>
#include <filesystem>

namespace noir::consensus {

/// \addtogroup consensus
/// \{

/// \brief EndHeightMessage marks the end of the given height inside WAL.
struct end_height_message {
  int64_t height;
};

using wal_message_body_t =
  std::variant<end_height_message, p2p::internal_msg_info, timeout_info, events::event_data_round_state>;

/// \brief default WALMessage type
struct wal_message {
  wal_message_body_t msg;
};

/// \brief TimedWALMessage wraps WALMessage and adds Time for debugging purposes
struct timed_wal_message {
  noir::p2p::tstamp time;
  wal_message msg;
};

/// \brief WALSearchOptions are optional arguments to SearchForEndHeight.
struct wal_search_options {
  bool ignore_data_corruption_errors;
};

/// \brief A WALDecoder reads and decodes custom-encoded WAL messages from an input
/// stream. See WALEncoder for the format used.
/// It will also compare the checksums and make sure data size is equal to the
/// length from the header. If that is not the case, error will be returned.
class wal_decoder {
public:
  enum class result {
    success = 0,
    eof,
    corrupted,
  };
  wal_decoder(const std::string& full_path);

  /// \brief reads the next custom-encoded value from its reader and returns it.
  /// \param[out] msg decoded timed_wal_message
  /// \return result
  result decode(timed_wal_message& msg);

private:
  std::unique_ptr<::fc::cfile> file_;
  std::mutex mtx_;
};

/// \brief A WALEncoder writes custom-encoded WAL messages to an output stream.
/// Format: 4 bytes CRC sum + 4 bytes length + arbitrary-length value
class wal_encoder {
  friend class wal_file_manager;

public:
  wal_encoder(const std::string& full_path);

  /// \brief writes the custom encoding of v to the stream. It returns an error if the encoded size of v is
  /// greater than 1MB. Any error encountered during the write is also returned.
  /// \param[in] msg
  /// \param[out] size written bytes size
  /// \return true on success, false otherwise
  bool encode(const timed_wal_message& msg, size_t& size);

  /// \brief flushes and fsync the underlying group's data to disk.
  /// \return true on success, false otherwise
  bool flush_and_sync();

  /// brief gets the size of wal file of current index
  /// \return size
  size_t size();

private:
  std::unique_ptr<::fc::cfile> file_;
  std::mutex mtx_;
};

/// \brief WALFileManager manages wal file and mutex
/// The first file to be written in the WALFileManager.Dir is the head file.
///
///	Dir/
///	- <HeadPath>
///
/// Once the Head file reaches the size limit, it will be rotated.
///
///	Dir/
///	- <HeadPath>.000   // First rolled file
///	- <HeadPath>       // New head path, starts empty.
///										 // The implicit index is 001.
///
/// As more files are written, the index numbers grow...
///
///	Dir/
///	- <HeadPath>.000   // First rolled file
///	- <HeadPath>.001   // Second rolled file
///	- ...
///	- <HeadPath>       // New head path
class wal_file_manager {
public:
  wal_file_manager(const std::string& dir, const std::string& file_name, size_t num_file, size_t rotate_size)
    : dir_path_(dir), head_name_(file_name), num_file_(num_file), rotate_size_(rotate_size) {
    head_path_ = dir_path_ / head_name_;
    if (!load_min_max_index()) { // no previous wal file found
      min_index = max_index = 0;
    } else {
      ++max_index;
    }
    current_index = max_index;
    encoder_ = get_wal_encoder(current_index);
  }

  /// \brief rotate wal file if necessary
  /// \return true on success, false otherwise
  inline bool update() {
    if (need_rotate()) {
      return rotate();
    }
    return true;
  }

  /// \brief gets wal_encoder of wal file of given index
  /// \param[in] index returns current using wal_encoder if index is -1
  /// \return shared_ptr of wal_encoder
  std::shared_ptr<wal_encoder> get_wal_encoder(int64_t index = -1) {
    check(index <= max_index);
    if (index < 0) { // returns current using encoder
      std::scoped_lock g(mtx_);
      return encoder_;
    }
    if (index == current_index) { // returns new encoder with implicit index
      return make_shared<wal_encoder>(head_path_.string());
    }
    return make_shared<wal_encoder>(full_path(dir_path_, index).string());
  }

  /// \brief gets wal_decoder of wal file of given index
  /// \param[in] index
  /// \return shared_ptr of wal_decoder
  std::shared_ptr<wal_decoder> get_wal_decoder(int64_t index) {
    check(index <= max_index && index >= min_index);
    return make_shared<wal_decoder>(full_path(dir_path_, index).string());
  }

  std::filesystem::path full_path(std::filesystem::path dir_path, int64_t index, bool implicit = true) {
    if (implicit && index == current_index) {
      return head_path_;
    }
    auto ret = head_path_;
    ret += fmt::format("{:03}", index);
    return ret;
  }

  /// \brief indicates lowest index of available WAL file
  int64_t min_index = -1;

  /// \brief indicates highest index of available WAL file
  int64_t max_index = -1;

  static constexpr std::string_view corrupted_postfix = ".CORRUPTED";
  // FIXME: below constants are not correct
  static constexpr size_t max_msg_size = 1048576; // 1 MB; NOTE: keep in sync with types.PartSet sizes.
  static constexpr size_t max_msg_size_bytes = max_msg_size + 24; // time.Time + max consensus msg size

private:
  std::shared_ptr<wal_encoder> encoder_;
  std::filesystem::path dir_path_;
  std::filesystem::path head_name_;
  std::filesystem::path head_path_;
  size_t num_file_;
  size_t rotate_size_;
  std::mutex mtx_;
  int64_t current_index = -1; ///< indicates implicit index of current opened file

  int64_t index_from_file_name(const std::filesystem::path& file_name) {
    auto wal_name = head_name_.string();
    size_t len = wal_name.length();
    auto name = file_name.string();
    if (name.compare(0, len, wal_name) != 0) {
      return -1;
    }
    auto sub_name = name.substr(len);
    if (sub_name.empty()) { // head file
      return -1;
    }
    if (sub_name.compare(corrupted_postfix) == 0) {
      return -1;
    }
    return static_cast<int64_t>(std::stoull(name.substr(len)));
  }

  bool need_rotate() {
    std::scoped_lock g(mtx_);
    return encoder_->size() >= rotate_size_; // TODO: handle exception
  }

  bool rotate() {
    std::scoped_lock g(mtx_);
    auto& file_ = encoder_->file_;
    auto new_file_path = full_path(dir_path_, current_index, false);
    try {
      encoder_->flush_and_sync();
      if (current_index - min_index >= num_file_) {
        std::filesystem::remove(full_path(dir_path_, min_index));
        min_index++;
      }
      if (std::filesystem::exists(new_file_path)) {
        check(std::filesystem::is_regular_file(new_file_path),
          fmt::format("unable to replace existing file: {}", new_file_path.string()));
        wlog(fmt::format("wal file for new index {} already exists: {}", current_index, new_file_path.string()));
      }
      std::filesystem::rename(file_->get_file_path().string(), new_file_path);
      current_index = ++max_index;
      encoder_ = get_wal_encoder(current_index);
      ilog(fmt::format("created wal file for new index: {}", current_index));
    } catch (...) {
      return false;
    }
    return true;
  }

  bool load_min_max_index() {
    std::scoped_lock g(mtx_);
    for (auto it = std::filesystem::directory_iterator(dir_path_); it != std::filesystem::end(it); ++it) {
      if (std::filesystem::is_regular_file(*it)) {
        auto index_ = index_from_file_name(it->path().filename());
        if (index_ < 0) {
          continue;
        }
        if (min_index < 0 || index_ < min_index) {
          min_index = index_;
        }
        if (index_ > max_index) {
          max_index = index_;
        }
      }
    }

    return (min_index >= 0 && max_index >= 0);
  }
};

/// \brief WAL is an interface for any write-ahead logger.
class wal {
public:
  virtual ~wal() = default;
  /// \brief Write is called in newStep and for each receive on the peerMsgQueue and the timeoutTicker.
  /// \param[in] msg
  /// \return true on success, false otherwise
  virtual bool write(const wal_message& msg) = 0;

  /// \brief WriteSync is called when we receive a msg from ourselves so that we write to disk before sending signed
  /// messages.
  /// \param[in] msg
  /// \return true on success, false otherwise
  virtual bool write_sync(const wal_message& msg) = 0;

  /// \brief FlushAndSync flushes and fsync the underlying group's data to disk.
  /// \return true on success, false otherwise
  virtual bool flush_and_sync() = 0;

  /// \brief SearchForEndHeight searches for the EndHeightMessage with the given height
  /// \param[in] height
  /// \param[in] options
  /// \param[out] found
  /// \return shared_ptr of wal_decoder, nullptr if not found
  virtual std::shared_ptr<wal_decoder> search_for_end_height(
    int64_t height, wal_search_options options, bool& found) = 0;

  /// \brief base service method
  /// \todo base service logic is not defined
  /// \return true on success, false otherwise
  virtual bool on_start() = 0;

  /// \brief base service method
  /// \todo base service logic is not defined
  /// \return true on success, false otherwise
  virtual bool on_stop() = 0;
};

/// \brief Write ahead logger writes msgs to disk before they are processed.
/// Can be used for crash-recovery and deterministic replay.
/// \todo currently the wal is overwritten during replay catchup, give it a mode so it's either reading or
/// appending - must read to end to start appending again.
class base_wal : public wal {
public:
  base_wal(const base_wal&) = delete; // do not allow copy
  base_wal(const std::string& dir, const std::string& file_name, size_t num_file, size_t rotate_size)
    : file_manager_(std::make_unique<wal_file_manager>(dir, file_name, num_file, rotate_size)),
      flush_interval(std::chrono::system_clock::duration(2000000)) { // 2seconds = 2000000 microseconds
    thread_pool.emplace("consensus", thread_pool_size);
    {
      // std::scoped_lock g(flush_ticker_mtx);
      flush_ticker = std::make_unique<boost::asio::steady_timer>(thread_pool->get_executor());
    }
  }
  ~base_wal() override = default;

  bool write(const wal_message& msg) override {
    size_t len;
    if (!file_manager_->update()) {
      elog("Failed to rotate wal file");
    }
    auto enc = file_manager_->get_wal_encoder();
    if (!enc->encode(timed_wal_message{.time = get_time(), .msg = msg}, len) || len == 0) {
      elog("Error writing msg to consensus wal. WARNING: recover may not be possible for the current height");
      return false;
    }
    return true;
  }

  bool write_sync(const wal_message& msg) override {
    if (!write(msg)) {
      return false;
    }
    if (!flush_and_sync()) {
      elog("WriteSync failed to flush consensus wal.\n"
           "\t\tWARNING: may result in creating alternative proposals / votes for the current height iff the node "
           "restarted");
      return false;
    }
    return true;
  }

  bool flush_and_sync() override {
    return file_manager_->get_wal_encoder()->flush_and_sync();
  }

  bool on_start() override {
    if (file_manager_->get_wal_encoder()->size() == 0) {
      end_height_message msg{.height = 0};
      if (write_sync({msg}) == false) {
        return false;
      }
    }

    // TODO: define codec as service
    // codec_.start();

    // initialize flush ticker
    flush_ticker->cancel();
    flush_ticker->expires_from_now(flush_interval);

    flush_ticker->async_wait(process_flush_ticks);

    return true;
  }

  bool on_stop() override {
    flush_ticker->cancel();
    if (!flush_and_sync()) {
      elog("error on flush data to disk");
      return false;
    }
    // TODO: define codec as service
    // if(!codec_->stop()) {
    //   elog("error trying to stop wal");
    //   return false;
    // }

    return true;
  }

  void wait() {
    // TODO: define codec as service
    // codec_->wait();
  }

  std::shared_ptr<wal_decoder> search_for_end_height(int64_t height, wal_search_options options, bool& found) override {
    int64_t last_height_found{-1};
    auto max_index = file_manager_->max_index;
    auto min_index = file_manager_->min_index;
    if (min_index < 0 || max_index < 0) {
      found = false;
      return {nullptr};
    }
    ilog(
      "Searching for height=${height} min=${min} max=${max}", ("height", height)("min", min_index)("max", max_index));
    // NOTE: starting from the last file in the group because we're usually
    // searching for the last height. See replay.cpp(?)
    for (auto index = max_index; index >= min_index; --index) {
      auto decoder = file_manager_->get_wal_decoder(index);

      while (true) { // TODO: check exit condition
        using result = wal_decoder::result;
        timed_wal_message msg{};
        auto ret = decoder->decode(msg);
        if (ret == result::eof) {
          if (last_height_found > 0 && height > last_height_found) {
            found = false; // OPTIMIZATION: no need to look for height in older files if we've seen h < height
            return {nullptr};
          }
          break; // check next file
        }
        if (options.ignore_data_corruption_errors && ret == result::corrupted) {
          elog("Corrupted entry. Skipping...");
          // do nothing
          continue;
        } else if (ret != result::success) {
          found = false;
          return {nullptr};
        }

        if (auto* ptr = std::get_if<end_height_message>(&msg.msg.msg); ptr) {
          if (ptr->height == height) { // found
            found = true;
            return std::move(decoder);
          }
          last_height_found = ptr->height;
        }
      }
    }
    found = false;
    return {nullptr};
  }

  bool set_flush_interval(std::chrono::system_clock::duration interval) {
    // TODO: should flush ticker restart?
    flush_interval = interval;
    return false;
  }

private:
  std::unique_ptr<wal_file_manager> file_manager_;
  // flush ticker
  std::unique_ptr<boost::asio::steady_timer> flush_ticker;
  std::chrono::system_clock::duration flush_interval;
  uint16_t thread_pool_size = 2;
  std::optional<named_thread_pool> thread_pool;

  std::function<void(boost::system::error_code)> process_flush_ticks = [this](boost::system::error_code ec) {
    if (ec.failed()) {
      wlog("wal flush ticker error: ${m}", ("m", ec.message()));
      // return;
    }
    flush_and_sync();
    flush_ticker->expires_from_now(flush_interval);
    flush_ticker->async_wait(process_flush_ticks);
  };
};

class nil_wal : public wal {
  bool write(const wal_message& msg) override {
    return true;
  }

  bool write_sync(const wal_message& msg) override {
    return true;
  }

  bool flush_and_sync() override {
    return true;
  }

  bool on_start() override {
    return true;
  }

  bool on_stop() override {
    return true;
  }

  std::shared_ptr<wal_decoder> search_for_end_height(int64_t height, wal_search_options options, bool& found) override {
    return {nullptr};
  }
};

/// \}

} // namespace noir::consensus

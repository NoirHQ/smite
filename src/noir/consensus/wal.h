// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/thread_pool.h>
#include <noir/consensus/types.h>
#include <noir/p2p/protocol.h>
#include <boost/filesystem/operations.hpp>
#include <fc/io/cfile.hpp>

namespace noir::consensus {

/// \addtogroup consensus
/// \{

/// \brief EndHeightMessage marks the end of the given height inside WAL.
struct end_height_message {
  int64_t height;
};

using wal_message_body_t =
  std::variant<end_height_message, /* p2p::proposal_message, */ p2p::block_part_message /*, p2p::vote_message */>;

/// \brief default WALMessage type
struct wal_message {
  // p2p::net_message msg; // TODO: replace after scale codec support
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
/// stream. See WALCodec for the format used.
/// It will also compare the checksums and make sure data size is equal to the
/// length from the header. If that is not the case, error will be returned.
class wal_decoder {
public:
  enum class result {
    success = 0,
    eof,
    corrupted,
  };

  wal_decoder(const std::string& full_path, std::shared_ptr<std::mutex> mtx_);

  /// \brief reads the next custom-encoded value from its reader and returns it.
  /// \param[out] msg decoded timed_wal_message
  /// \return result
  result decode(timed_wal_message& msg);

private:
  std::unique_ptr<::fc::cfile> file_;
  std::shared_ptr<std::mutex> mtx;
};

/// \brief A WALCodec writes custom-encoded WAL messages to an output stream.
/// Format: 4 bytes CRC sum + 4 bytes length + arbitrary-length value
class wal_codec {
public:
  wal_codec(const std::string& dir, size_t rotate_size = maxMsgSizeBytes, size_t num_file = default_num_file);
  ~wal_codec();

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

  /// \brief gets list of wal file indexes ordered by last modified
  /// \return vector of index
  std::vector<int> reverse_file_index();

  /// \brief gets wal_decoder of wal file of given index
  /// \param[in] index
  /// \return shared_ptr of wal_decoder
  std::shared_ptr<wal_decoder> new_wal_decoder(int index);

private:
  static constexpr size_t maxMsgSize = 1048576; // 1 MB; NOTE: keep in sync with types.PartSet sizes.
  static constexpr size_t maxMsgSizeBytes = maxMsgSize + 24; // time.Time + max consensus msg size
  static constexpr size_t default_num_file = 1000; // TODO: find this value

  std::shared_ptr<std::mutex> mtx;
  std::unique_ptr<class wal_codec_impl> impl_;
};

/// \brief WAL is an interface for any write-ahead logger.
class wal {
public:
  virtual ~wal() {}
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
  base_wal(const std::string& path, size_t rotate_size, size_t num_file)
    : codec_(std::make_unique<wal_codec>(path, rotate_size, num_file)) {

    thread_pool.emplace("consensus", thread_pool_size);
    {
      // std::lock_guard<std::mutex> g(flush_ticker_mtx);
      flush_ticker.reset(new boost::asio::steady_timer(thread_pool->get_executor()));
    }
  }
  ~base_wal() override {}

  bool write(const wal_message& msg) override {
    size_t len;
    if (!codec_->encode(timed_wal_message{.time = get_time(), .msg = msg}, len) || len == 0) {
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
    return codec_->flush_and_sync();
  }

  bool on_start() override {
    if (codec_->size() == 0) {
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
    // NOTE: starting from the last file in the group because we're usually
    // searching for the last height. See replay.cpp(?)
    auto f_indexes = codec_->reverse_file_index(); // TODO: index?
    for (auto it = f_indexes.begin(); it != f_indexes.end(); ++it) {
      auto decoder = codec_->new_wal_decoder(*it);

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

        if (auto* ret = std::get_if<end_height_message>(&msg.msg.msg); ret) {
          if (ret->height == height) { // found
            found = true;
            return std::move(decoder);
          }
          last_height_found = ret->height;
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
  std::unique_ptr<wal_codec> codec_;
  std::unique_ptr<boost::asio::steady_timer> flush_ticker;
  std::chrono::system_clock::duration flush_interval;
  uint16_t thread_pool_size = 2;
  std::optional<named_thread_pool> thread_pool;

  std::function<void(boost::system::error_code)> process_flush_ticks = [this](boost::system::error_code ec) {
    if (ec) {
      wlog("wal flush ticker error: ${m}", ("m", ec.message()));
      // return;
    }
    flush_and_sync();
    flush_ticker->expires_from_now(flush_interval);
    flush_ticker->async_wait(process_flush_ticks);
  };
};

/// \}

} // namespace noir::consensus

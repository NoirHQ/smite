// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/types.h>
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
};

/// \brief Write ahead logger writes msgs to disk before they are processed.
/// Can be used for crash-recovery and deterministic replay.
/// \todo currently the wal is overwritten during replay catchup, give it a mode so it's either reading or
/// appending - must read to end to start appending again.
class base_wal : public wal {
public:
  base_wal(const std::string& path): codec_(std::make_unique<wal_codec>(path)) {}
  bool write(const wal_message& msg) override {
    // TODO: implement
    return false;
  }

  bool write_sync(const wal_message& msg) override {
    // TODO: implement
    return false;
  }

  bool flush_and_sync() override {
    // TODO: implement
    return false;
  }

  bool on_start() {
    // TODO: implement
    return false;
  }

  bool on_stop() {
    // TODO: implement
    return false;
  }

  void wait() {
    // TODO: implement
  }

  std::shared_ptr<wal_decoder> search_for_end_height(int64_t height, wal_search_options options, bool& found) override {
    // TODO: implement
    return {nullptr};
  }

  bool set_flush_interval(std::chrono::system_clock::duration interval) {
    // TODO: implement
    return false;
  }

private:
  std::unique_ptr<wal_codec> codec_;
};

/// \}

} // namespace noir::consensus

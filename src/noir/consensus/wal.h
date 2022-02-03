// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/types.h>

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

  wal_decoder(){};

  /// \brief reads the next custom-encoded value from its reader and returns it.
  /// \param[out] msg decoded timed_wal_message
  /// \return result
  result decode(timed_wal_message& msg) {
    return result::eof;
  }
};

/// \brief A WALCodec writes custom-encoded WAL messages to an output stream.
/// Format: 4 bytes CRC sum + 4 bytes length + arbitrary-length value
class wal_codec {
public:
  wal_codec();

  /// \brief writes the custom encoding of v to the stream. It returns an error if the encoded size of v is
  /// greater than 1MB. Any error encountered during the write is also returned.
  /// \param[in] msg
  /// \param[out] size written bytes size
  /// \return true on success, false otherwise
  bool encode(const timed_wal_message& msg, size_t& size) {
    return false;
  }

  /// \brief flushes and fsync the underlying group's data to disk.
  /// \return true on success, false otherwise
  bool flush_and_sync() {
    return false;
  }

  /// brief gets the size of wal file of current index
  /// \return size
  size_t size() {
    return 0;
  }

  /// \brief gets list of wal file indexes ordered by last modified
  /// \return vector of index
  std::vector<int> reverse_file_index() {
    return {};
  }

  /// \brief gets wal_decoder of wal file of given index
  /// \param[in] index
  /// \return shared_ptr of wal_decoder
  std::shared_ptr<wal_decoder> new_wal_decoder(int index) {
    return {nullptr};
  }
};

/// \}

} // namespace noir::consensus

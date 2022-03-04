// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/common/scope_exit.h>
#include <noir/consensus/wal.h>
#include <noir/core/codec.h>

namespace noir::consensus {
using ::fc::cfile;

wal_decoder::wal_decoder(const std::string& full_path): file_(std::make_unique<::fc::cfile>()) {
  file_->set_file_path(full_path);
  file_->open(cfile::update_rw_mode); // TODO: handle panic
}

wal_decoder::result wal_decoder::decode(timed_wal_message& msg) {
  std::lock_guard<std::mutex> g(mtx_);
  try {
    {
      noir::bytes crc(4);
      file_->read(crc.data(), crc.size());
      // TODO: check CRC32
    }
    size_t len;
    {
      noir::bytes len_(4);
      file_->read(len_.data(), len_.size());
      len = stoull(to_hex(len_), nullptr, 16);
      if (len > wal_file_manager::max_msg_size_bytes) {
        return result::corrupted;
      }
    }
    {
      noir::bytes dat(len);
      file_->read(dat.data(), len);
      msg = noir::decode<timed_wal_message>(dat);
    }
  } catch (...) {
    if (file_->eof()) {
      return result::eof;
    }
    return result::corrupted;
  }

  return result::success;
}

wal_encoder::wal_encoder(const std::string& full_path): file_(std::make_unique<::fc::cfile>()) {
  file_->set_file_path(full_path);
  file_->open(cfile::create_or_update_rw_mode); // TODO: handle panic
}

bool wal_encoder::encode(const timed_wal_message& msg, size_t& size) {
  size = 0;
  std::lock_guard<std::mutex> g(mtx_);
  auto is_closed = !file_->is_open();
  if (is_closed) {
    wlog("wal file not opened");
    try {
      file_->open(cfile::update_rw_mode);
    } catch (...) {
      elog("wal file does not exist: ${path}", ("path", file_->get_file_path().string()));
      return false; // TODO: handle error
    }
  }
  auto defer = make_scope_exit([&file_ = file_, is_closed]() {
    if (is_closed) {
      file_->close();
    }
  });

  auto dat = noir::encode(msg);
  if (dat.size() > wal_file_manager::max_msg_size_bytes) { // TODO: handle error
    elog("msg is too big: ${length} bytes, max: ${maxMsgSizeBytes} bytes",
      ("length", dat.size())("maxMsgSizeBytes", wal_file_manager::max_msg_size_bytes));
    return false;
  }

  noir::bytes crc(4); // TODO: CRC32
  noir::bytes buf = crc;
  noir::bytes len_hdr = from_hex(fmt::format("{:08x}", static_cast<uint32_t>(dat.size()))); // TODO: optimize
  buf.insert(buf.end(), len_hdr.begin(), len_hdr.end());
  buf.insert(buf.end(), dat.begin(), dat.end());

  file_->write(buf.data(), buf.size());
  size = buf.size();
  return true;
}

bool wal_encoder::flush_and_sync() {
  std::lock_guard<std::mutex> g(mtx_);
  if (!file_->is_open()) { // file is already closed no need to flush
    return true;
  }
  try {
    file_->flush();
    file_->sync();
  } catch (...) {
    elog("fail to flush and sync");
    return false;
  }
  return true;
}

size_t wal_encoder::size() {
  std::lock_guard<std::mutex> g(mtx_);
  if (!file_->is_open()) { // file is already closed no need to flush
    return fc::file_size(file_->get_file_path());
  }
  return file_->tellp(); // TODO: handle exception
}

} // namespace noir::consensus

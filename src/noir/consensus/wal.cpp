// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/consensus/wal.h>

#include <noir/codec/scale.h>

namespace noir::consensus {
using ::fc::cfile;

class wal_codec_impl {
public:
  wal_codec_impl(const std::string& dir, std::shared_ptr<std::mutex> mtx, size_t rotate_size, size_t num_file);
  ~wal_codec_impl();

  bool encode(const timed_wal_message& msg, size_t& size);
  bool flush_and_sync();
  size_t size();
  std::vector<int> reverse_file_index();
  std::shared_ptr<wal_decoder> new_wal_decoder(int index);

private:
  static constexpr std::basic_string_view file_name_prefix_{"cs_wal"};
  std::unique_ptr<::fc::cfile> file_;
  size_t rotate_size_;
  size_t num_file_; // TODO: configurable
  int index_ = -1;
  std::string dir_path_;
  std::shared_ptr<std::mutex> mtx; // Is mutex needed?

  static inline fc::path full_path(std::string dir_path, int index) {
    return {dir_path + "/" + std::string(file_name_prefix_) + std::to_string(index)};
  }

  bool need_rotate();
  bool rotate();
};

wal_codec_impl::wal_codec_impl(
  const std::string& dir, std::shared_ptr<std::mutex> mtx_, size_t rotate_size, size_t num_file)
  : file_(std::make_unique<::fc::cfile>()), rotate_size_(rotate_size), num_file_(num_file), dir_path_(dir), mtx(mtx_) {
  std::lock_guard<std::mutex> g(*mtx);
  std::time_t temp{};
  size_t last_write_index = 0;
  for (auto i = 0; i < num_file_; ++i) {
    auto file_path = full_path(dir_path_, i);
    if (!fc::exists(file_path)) {
      index_ = i;
      break;
    } else {
      auto time_ = boost::filesystem::last_write_time(file_path);
      if (temp < time_) {
        temp = time_;
        last_write_index = i;
      }
    }
  }
  if (index_ == -1) {
    index_ = last_write_index;
  }

  file_->set_file_path(full_path(dir_path_, index_));
  file_->open(cfile::create_or_update_rw_mode);
  file_->seek_end(0);
}

wal_codec_impl::~wal_codec_impl() {
  file_->close();
}

bool wal_codec_impl::encode(const timed_wal_message& msg, size_t& size) {
  size = 0;
  std::lock_guard<std::mutex> g(*mtx);
  if (need_rotate()) {
    rotate();
  }

  if (!file_->is_open()) {
    elog("wal file not opened"); // TODO: handle panic
    return false;
  }
  auto dat = noir::codec::scale::encode(msg);

  if (dat.size() > rotate_size_) { // TODO: handle error
    elog("msg is too big: ${length} bytes, max: ${maxMsgSizeBytes} bytes",
      ("length", dat.size())("maxMsgSizeBytes", rotate_size_));
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

bool wal_codec_impl::flush_and_sync() {
  std::lock_guard<std::mutex> g(*mtx);
  try {
    file_->flush();
    file_->sync();
  } catch (...) {
    elog("fail to flush and sync");
    return false;
  }
  return true;
}

size_t wal_codec_impl::size() {
  std::lock_guard<std::mutex> g(*mtx);
  return file_->tellp(); // TODO: handle exception
}

std::vector<int> wal_codec_impl::reverse_file_index() {
  std::vector<int> ret{};
  std::lock_guard<std::mutex> g(*mtx);
  for (auto i = 0; i < num_file_; ++i) {
    auto f_index = (index_ + num_file_ - i) % num_file_;
    if (!fc::exists(full_path(dir_path_, f_index))) {
      break;
    }
    ret.push_back(f_index);
  }
  return ret;
}

bool wal_codec_impl::need_rotate() {
  return file_->tellp() >= rotate_size_; // TODO: handle exception
}

bool wal_codec_impl::rotate() {
  file_->flush();
  file_->sync();
  index_ = (index_ + 1) % num_file_;
  auto path_ = full_path(dir_path_, index_);

  try {
    file_->close();
    fc::remove(path_);
    file_->set_file_path(path_);
    file_->open(cfile::create_or_update_rw_mode);
  } catch (...) {
    return false;
  }
  return true;
}

std::shared_ptr<wal_decoder> wal_codec_impl::new_wal_decoder(int index) {
  return make_shared<wal_decoder>(full_path(dir_path_, index).string(), mtx);
}

wal_codec::wal_codec(const std::string& dir, size_t rotate_size, size_t num_file)
  : mtx(std::make_shared<std::mutex>()), impl_(std::make_unique<wal_codec_impl>(dir, mtx, rotate_size, num_file)) {}

wal_codec::~wal_codec() = default;

bool wal_codec::encode(const timed_wal_message& msg, size_t& size) {
  return impl_->encode(msg, size);
}

bool wal_codec::flush_and_sync() {
  return impl_->flush_and_sync();
}

size_t wal_codec::size() {
  return impl_->size();
}

std::vector<int> wal_codec::reverse_file_index() {
  return impl_->reverse_file_index();
}

std::shared_ptr<wal_decoder> wal_codec::new_wal_decoder(int index) {
  return std::move(impl_->new_wal_decoder(index));
}

wal_decoder::wal_decoder(const std::string& full_path, std::shared_ptr<std::mutex> mtx_)
  : file_(std::make_unique<::fc::cfile>()), mtx(mtx_) {
  std::lock_guard<std::mutex> g(*mtx);
  file_->set_file_path(full_path);
  file_->open(cfile::update_rw_mode); // TODO: handle panic
  // file_->seek(0);
}

wal_decoder::result wal_decoder::decode(timed_wal_message& msg) {
  std::lock_guard<std::mutex> g(*mtx);
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
    }
    {
      noir::bytes dat(len);
      file_->read(dat.data(), len);
      msg = noir::codec::scale::decode<timed_wal_message>(dat);
    }
  } catch (...) {
    if (file_->eof()) {
      return result::eof;
    }
    return result::corrupted;
  }

  return result::success;
}

} // namespace noir::consensus

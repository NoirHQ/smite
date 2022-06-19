// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <noir/common/varint.h>
#include <google/protobuf/type.pb.h>

namespace noir {
class ProtoReadWrite {
public:
  template<typename ProtoType, typename R>
  static auto write_msg(google::protobuf::Message& msg) -> std::vector<R> {
    noir::VarInt64 message_length = static_cast<ProtoType&>(msg).ByteSizeLong();
    noir::codec::datastream<size_t> ss(0);
    auto message_header_size = write_uleb128(ss, message_length).value();
    auto total_bytes = message_length + message_header_size;
    std::vector<R> buf(total_bytes);
    noir::codec::datastream<R> ds(buf);
    write_uleb128(ds, message_length).value();
    static_cast<ProtoType&>(msg).SerializeToArray(ds.ptr(), message_length);
    return buf;
  }

  template<typename ProtoType, typename Stream>
  static auto read_msg_async(Stream& s) -> boost::asio::awaitable<ProtoType> {
    ProtoType msg{};
    noir::VarInt64 message_length = 0;
    co_await read_uleb128_async(*s, message_length);
    std::vector<unsigned char> message_buffer(message_length);
    co_await s->read(message_buffer);
    msg.ParseFromArray(message_buffer.data(), message_buffer.size());
    co_return msg;
  }

  template<typename ProtoType, typename Stream>
  static auto read_msg(Stream& s) -> ProtoType {
    noir::VarInt64 message_length = 0;
    read_uleb128(s, message_length);
    std::vector<char> message_buffer(message_length);
    s.read(message_buffer.data(), message_length);
    ProtoType msg{};
    msg.ParseFromArray(message_buffer.data(), message_buffer.size());
    return msg;
  }
};
} //namespace noir

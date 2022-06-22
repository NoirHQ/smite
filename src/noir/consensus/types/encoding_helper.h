// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <google/protobuf/wrappers.pb.h>

namespace noir::consensus {

inline Bytes cdc_encode(const std::string& item) {
  google::protobuf::StringValue i;
  i.set_value(item);
  Bytes bz(i.ByteSizeLong());
  i.SerializeToArray(bz.data(), i.ByteSizeLong());
  return bz;
}

inline Bytes cdc_encode(const int64_t& item) {
  google::protobuf::Int64Value i;
  i.set_value(item);
  Bytes bz(i.ByteSizeLong());
  i.SerializeToArray(bz.data(), i.ByteSizeLong());
  return bz;
}

inline Bytes cdc_encode(const Bytes& item) {
  google::protobuf::BytesValue i;
  i.set_value({item.begin(), item.end()});
  Bytes bz(i.ByteSizeLong());
  i.SerializeToArray(bz.data(), i.ByteSizeLong());
  return bz;
}

inline Bytes cdc_encode_time(const tstamp& item) {
  google::protobuf::Timestamp i;
  int64_t second_precision = 1'000'000; // assume tstamp is in microseconds
  int64_t s = item / second_precision;
  int32_t ns = (item % second_precision) * (1'000'000'000 / second_precision);
  i.set_seconds(s);
  i.set_nanos(ns);
  Bytes bz(i.ByteSizeLong());
  i.SerializeToArray(bz.data(), i.ByteSizeLong());
  return bz;
}

} // namespace noir::consensus

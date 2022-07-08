// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <tendermint/p2p/channel.h>
#include <boost/asio/io_context.hpp>
#include <eo/core.h>
#include <cstddef>

namespace tendermint::p2p {
class FifoQueue {
public:
  static auto new_fifo_queue(boost::asio::io_context& context, const std::size_t& size) -> std::shared_ptr<FifoQueue> {
    return std::shared_ptr<FifoQueue>(new FifoQueue(context, size));
  }

  void close() {
    close_ch.close();
  }

public:
  std::shared_ptr<eo::chan<EnvelopePtr>> queue_ch;
  eo::chan<std::monostate> close_ch;

private:
  FifoQueue(boost::asio::io_context& context, const std::size_t& size)
    : io_context(context), queue_ch(std::make_shared<eo::chan<EnvelopePtr>>(context, size)), close_ch{context, 1} {}
  boost::asio::io_context& io_context;
};
} // namespace tendermint::p2p

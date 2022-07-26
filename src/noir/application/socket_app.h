// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/application/app.h>

namespace noir::application {

class socket_app : public base_application {
public:
  socket_app(std::string_view address);

  virtual std::unique_ptr<ResponseInfo> info_sync(const RequestInfo& req) override;

  virtual std::unique_ptr<ResponseInitChain> init_chain(const RequestInitChain& req) override;

  virtual std::unique_ptr<ResponseBeginBlock> begin_block(const RequestBeginBlock& req) override;
  virtual std::unique_ptr<ResponseEndBlock> end_block(const RequestEndBlock& req) override;
  virtual std::unique_ptr<ResponseDeliverTx> deliver_tx_async(const RequestDeliverTx& req) override;

  virtual std::unique_ptr<ResponseCommit> commit() override;

private:
  std::shared_ptr<struct cli_impl> my_cli;
};

} // namespace noir::application

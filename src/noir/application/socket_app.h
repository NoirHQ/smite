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
  socket_app();
  virtual consensus::response_init_chain& init_chain() override;
  virtual consensus::response_begin_block& begin_block() override;
  virtual consensus::req_res<consensus::response_deliver_tx>& deliver_tx_async() override;
  virtual consensus::response_end_block& end_block() override;

private:
  std::shared_ptr<struct cli_impl> my_cli;
};

} // namespace noir::application

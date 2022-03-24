// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <map>
#include <string>
#include <vector>

namespace noir::rpc {
class resource {
public:
  resource(const std::string& res) {
    std::size_t found = res.find('?');
    if (found != std::string::npos) {
      path_ = res.substr(0, found);
      full_query_ = res.substr(found + 1);
      if (!full_query_.empty()) {
        query_parse(full_query_, this->query_);
      }
    } else {
      path_ = res;
    }
  }

  std::string path_;
  std::string full_query_;
  std::map<std::string, std::string> query_;

private:
  void query_parse(const std::string& in, std::map<std::string, std::string>& out) {
    std::vector<std::string> queries;
    boost::split(queries, in, boost::is_any_of("&"));
    if (queries.empty())
      return;
    std::string key, value;
    for (auto it = queries.begin(); it != queries.end(); ++it) {
      std::size_t pos = (*it).find('=');
      check(pos != std::string::npos, "invalid uri format");
      key = (*it).substr(0, pos);
      value = (*it).substr(pos + 1);

      out[key] = value;
      key.clear();
      value.clear();
    }
  }
};
} // namespace noir::rpc

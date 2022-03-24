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
  explicit resource(const std::string& res) {
    std::size_t found = res.find('?');
    if (found != std::string::npos) {
      path = res.substr(0, found);
      full_query = res.substr(found + 1);
      if (!full_query.empty()) {
        query_parse(full_query, query);
      }
    } else {
      path = res;
    }
  }

  std::string path;
  std::string full_query;
  std::map<std::string, std::string> query;

private:
  static void query_parse(const std::string& in, std::map<std::string, std::string>& out) {
    std::vector<std::string> queries;
    boost::split(queries, in, boost::is_any_of("&"));
    if (queries.empty())
      return;
    std::string key, value;
    for (auto& query : queries) {
      std::size_t pos = query.find('=');
      check(pos != std::string::npos, "invalid uri format");
      key = query.substr(0, pos);
      value = query.substr(pos + 1);

      out[key] = value;
      key.clear();
      value.clear();
    }
  }
};
} // namespace noir::rpc

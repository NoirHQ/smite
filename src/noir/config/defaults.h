#pragma once
#include <filesystem>

namespace noir::node {

enum Mode {
  Full = 1,
  Validator = 2,
  Seed = 3,
};

inline auto to_string(Mode mode) -> std::string {
  switch (mode) {
  case Full:
    return "full";
  case Validator:
    return "validator";
  case Seed:
    return "seed";
  }
}

template<typename T, typename U>
T into(U&&);

template<>
inline auto into(const std::string& s) -> Mode {
  if (s == "full") {
    return Mode::Full;
  } else if (s == "validator") {
    return Mode::Validator;
  } else if (s == "seed") {
    return Mode::Seed;
  }
  __builtin_unreachable();
}

} // namespace noir::node


namespace noir::config {

// global constants
constexpr std::string_view default_log_level = "info";

// global variables
extern std::filesystem::path default_tendermint_dir;
extern std::filesystem::path default_config_dir;
extern std::filesystem::path default_data_dir;

extern std::string default_config_file_name;
extern std::string default_genesis_json_name;

extern node::Mode default_mode;
extern std::string default_privval_key_name;
extern std::string default_privval_state_name;

extern std::string default_node_key_name;
extern std::string default_addr_book_name;

namespace detail {
  inline auto rootify(const std::filesystem::path& path, const std::filesystem::path& root) -> std::filesystem::path {
    if (path.is_absolute()) {
      return path;
    }
    return root / path;
  }
} // namespace detail

} // namespace noir::config

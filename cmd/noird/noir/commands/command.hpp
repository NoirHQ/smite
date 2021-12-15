#pragma once
#include <functional>

// forward declaration
namespace CLI {
  class App;
}

namespace noir::commands {

using add_command_callback = std::function<CLI::App*(CLI::App&)>;

auto add_command(CLI::App& root, add_command_callback cb) {
  return cb(root);
};

} // namespace noir::commands

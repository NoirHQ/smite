#pragma once
#include <functional>

// forward declaration
namespace CLI {
  class App;
}

namespace noir::commands {

using add_command_callback = std::function<CLI::App*(CLI::App&)>;

CLI::App* add_command(CLI::App& root, add_command_callback cb);

CLI::App* debug(CLI::App&);
CLI::App* init(CLI::App&);
CLI::App* start(CLI::App&);
CLI::App* version(CLI::App&);

} // namespace noir::commands

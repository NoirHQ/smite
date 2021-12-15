#pragma once
#include <appbase/application.hpp>
#include <iosfwd>

namespace noir::commands {

CLI::App* version(CLI::App& root) {
  return root.add_subcommand("version", "Show version info")->parse_complete_callback([]() {
    std::cout << "v0.0.1" << std::endl;
    throw CLI::Success();
  });
}

} // namespace noir::commands

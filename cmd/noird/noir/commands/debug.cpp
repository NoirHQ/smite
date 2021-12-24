#include <appbase/application.hpp>

namespace noir::commands {

CLI::App* debug(CLI::App& root) {
  auto cmd = root.add_subcommand("debug", "Debugging mode")->final_callback([]() {
  });
  cmd->group("");
  return cmd;
}

} // namespace noir::commands

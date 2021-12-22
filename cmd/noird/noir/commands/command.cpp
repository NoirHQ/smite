#include <noir/commands/commands.h>

namespace noir::commands {

CLI::App* add_command(CLI::App& root, add_command_callback cb) {
  return cb(root);
}

} // namespace noir::commands

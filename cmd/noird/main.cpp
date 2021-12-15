#include <noir/tendermint/tendermint.h>
#include <noir/commands/commands.hpp>

#include <appbase/application.hpp>

namespace cmd = noir::commands;

using tendermint = noir::tendermint::tendermint;

int main(int argc, char** argv) {
  auto& app = appbase::app();

  // set default configuration
  app.cli().require_subcommand();
  app.cli().failure_message(CLI::FailureMessage::help);
  app.cli().name("noird");
  app.cli().description("NOIR Protocol App");
  std::filesystem::path home_dir;
  if (auto arg = std::getenv("HOME")) {
    home_dir = arg;
  }
  app.set_home_dir(home_dir / ".noir");
  app.set_config_file("app.toml");

  // add subcommands
  cmd::add_command(app.cli(), &cmd::init);
  cmd::add_command(app.cli(), &cmd::start);
  cmd::add_command(app.cli(), &cmd::version);

  // register plugins
  app.register_plugin<tendermint>();

  return app.run(argc, argv);
}

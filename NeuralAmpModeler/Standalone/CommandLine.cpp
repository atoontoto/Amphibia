#include "CommandLine.h"

#include <sstream>

namespace amphibia::setup
{
CommandLineOptions ParseCommandLine(const std::vector<std::string>& arguments)
{
  CommandLineOptions result;
  for (const auto& argument : arguments)
  {
    if (argument == "--first-run") result.forceFirstRun = true;
    else if (argument == "--setup") result.openSetup = true;
    else if (argument == "--audio-settings") result.openAudioSettings = true;
    else if (argument == "--safe-mode") result.safeMode = true;
    else if (argument == "--help") result.showHelp = true;
    else if (argument == "--version") result.showVersion = true;
    else result.unknownOptions.push_back(argument.substr(0, 256));
  }
  return result;
}

std::string CommandLineHelp()
{
  return "Amphibia options:\n"
         "  --first-run       Force the first-run setup wizard\n"
         "  --setup           Open the setup wizard\n"
         "  --audio-settings  Open standalone audio settings\n"
         "  --safe-mode       Start without restoring the previous model or IR\n"
         "  --help            Show this help\n"
         "  --version         Show version and build commit";
}

std::string VersionText(const std::string& version, const std::string& buildCommit)
{
  std::ostringstream stream;
  stream << "Amphibia " << version << " (commit " << (buildCommit.empty() ? "unknown" : buildCommit) << ')';
  return stream.str();
}
} // namespace amphibia::setup

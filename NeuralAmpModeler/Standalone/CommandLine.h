#pragma once

#include <string>
#include <vector>

namespace amphibia::setup
{
struct CommandLineOptions
{
  bool forceFirstRun{};
  bool openSetup{};
  bool openAudioSettings{};
  bool safeMode{};
  bool showHelp{};
  bool showVersion{};
  std::vector<std::string> unknownOptions;

  bool IsTerminal() const { return showHelp || showVersion || !unknownOptions.empty(); }
};

CommandLineOptions ParseCommandLine(const std::vector<std::string>& arguments);
std::string CommandLineHelp();
std::string VersionText(const std::string& version, const std::string& buildCommit);
} // namespace amphibia::setup

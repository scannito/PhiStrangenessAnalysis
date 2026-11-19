#include "Logger.h"
#include "RootIOHelpers.h"
#include "RunAnalysisChain.h"
#include "WorkflowManager.h"

#include "TROOT.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// The files a group of a parallel run leaves in its run directory: the files it opened (RootIO::WriteOpenRecordTo) and
// its warnings (Logger::WriteSummaryTo). The one place that names them.
std::string RunFile(const std::string& runDir, const std::string& group, const char* extension)
{
  return (std::filesystem::path(runDir) / (group + extension)).string();
}

// The groups of a parallel run (runAnalysis.sh -j) judged together, from what each process left in 'runDir': every
// group of the configuration must have left both files. Reports the files one group wrote and another opened, and
// the warnings of all groups; 1 when a file was shared or a group left nothing.
int CheckRun(const WorkflowManager& manager, const std::string& runDir)
{
  std::map<std::string, std::vector<RootIO::OpenedFile>> openedByGroup;
  bool complete = true;
  for (const std::string& group : manager.GroupNames()) {
    const std::string record = RunFile(runDir, group, ".opens");
    if (!std::filesystem::exists(record) || !Logger::AddSummaryFrom(RunFile(runDir, group, ".summary"))) {
      Logger::Error("check-run: group '{}' left no record of its run in '{}': it did not reach its end, and the files "
                    "it opened are not known.", group, runDir);
      complete = false;
      continue;
    }
    openedByGroup[group] = RootIO::ReadOpenRecord(record, "check-run");
  }

  const std::vector<std::string> shared = WorkflowManager::FilesSharedBetweenGroups(openedByGroup);
  for (const std::string& file : shared)
    Logger::Error("check-run: two groups of active_tasks shared a file: {}. Its content depends on the order the "
                  "processes ran in: put those blocks in one group and regenerate the file.", file);
  if (shared.empty())
    std::cout << "No file written by one group was opened by another.\n";
  Logger::PrintSummary();
  return (shared.empty() && complete) ? 0 : 1;
}

// Entry point of the compiled executable, and with the ROOT macro the only place
// aware of process-wide ROOT state: RunAnalysisChain stays free of ROOT globals.
//
//   phistrangeanalysis <configFile> [baseConfigFile] [--group <name> [--run-dir <dir>]]
//   phistrangeanalysis <configFile> [baseConfigFile] --list-groups
//   phistrangeanalysis <configFile> [baseConfigFile] --check-run <dir>
//
// --group runs one group of active_tasks; with --run-dir it leaves in <dir> the files it opened and its warnings
// (RunFile); --list-groups prints one "GROUP <name>" line per group and runs nothing; --check-run judges the groups of
// a parallel run together. All of them are what runAnalysis.sh --jobs uses.
int main(int argc, char* argv[])
{
  // Equivalent of `root -b` used by the interpreted mode: no windows, no display.
  gROOT->SetBatch(kTRUE);

  std::vector<std::string> positional;
  std::string group;
  std::string runDir;
  std::string checkRunDir;
  bool listGroups = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool hasValue = i + 1 < argc;
    if (arg == "--group" && hasValue) {
      group = argv[++i];
    } else if (arg == "--run-dir" && hasValue) {
      runDir = argv[++i];
    } else if (arg == "--check-run" && hasValue) {
      checkRunDir = argv[++i];
    } else if (arg == "--list-groups") {
      listGroups = true;
    } else if (arg.starts_with("--")) {
      std::cerr << "Unknown or incomplete option: " << arg << "\n";
      return 1;
    } else {
      positional.push_back(arg);
    }
  }

  try {
    if (positional.empty() || positional.size() > 2 || (!runDir.empty() && group.empty())) {
      std::cerr << "Usage: " << argv[0]
                << " <configFile> [baseConfigFile] [--group <name> [--run-dir <dir>] | --list-groups | --check-run <dir>]\n";
      return 1;
    }
    const std::string configFile = positional[0];
    const std::string baseConfigFile = positional.size() > 1 ? positional[1] : "";

    if (listGroups || !checkRunDir.empty()) {
      const WorkflowManager manager(configFile, baseConfigFile);
      if (!checkRunDir.empty())
        return CheckRun(manager, checkRunDir);
      for (const std::string& name : manager.GroupNames())
        std::cout << "GROUP " << name << "\n";
      return 0;
    }
    if (!runDir.empty())
      RootIO::WriteOpenRecordTo(RunFile(runDir, group, ".opens"));

    const int status = RunAnalysisChain(configFile, baseConfigFile, group);
    if (!runDir.empty())
      Logger::WriteSummaryTo(RunFile(runDir, group, ".summary"));
    return status;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}

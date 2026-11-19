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

// The groups of a parallel run (runAnalysis.sh -j) judged together, from what each process left in 'runDir':
// <group>.opens (RootIO::WriteOpenRecordTo) and <group>.summary (Logger::WriteSummaryTo). Prints the files one group
// wrote and another opened, then the warnings of all groups; 1 when a file was shared or a record is missing.
int CheckRun(const std::string& runDir)
{
  std::map<std::string, std::vector<RootIO::OpenedFile>> recordOfGroup;
  bool complete = true;
  for (const auto& entry : std::filesystem::directory_iterator(runDir)) {
    if (entry.path().extension() != ".opens")
      continue;
    const std::string group = entry.path().stem().string();
    recordOfGroup[group] = RootIO::ReadOpenRecord(entry.path().string(), "check-run");
    const std::string summary = (entry.path().parent_path() / (group + ".summary")).string();
    if (!Logger::AddSummaryFrom(summary)) {
      std::cout << "  " << group << ": no summary of its warnings (the process did not reach its end).\n";
      complete = false;
    }
  }

  const std::vector<RootIO::SharedFile> shared = RootIO::FilesSharedBetweenGroups(recordOfGroup);
  if (shared.empty()) {
    std::cout << "No file written by one group was opened by another.\n";
  } else {
    std::cout << "Files written by one group and opened by another: each holds what the last group to write it left, "
                 "and a reader got whichever version was there.\n";
    for (const RootIO::SharedFile& f : shared)
      std::cout << "  " << f.path << "\n    written by: " << Logger::Join(f.writers, " ")
                << "; opened by: " << Logger::Join(f.openers, " ") << "\n";
    std::cout << "Put the blocks that share a file in one group of active_tasks, then regenerate these files by "
                 "running the groups again.\n";
  }
  Logger::PrintSummary();
  return (shared.empty() && complete) ? 0 : 1;
}

// Entry point of the compiled executable, and with the ROOT macro the only place
// aware of process-wide ROOT state: RunAnalysisChain stays free of ROOT globals.
//
//   phistrangeanalysis <configFile> [baseConfigFile] [--group <name>] [--open-record <file>] [--summary-to <file>]
//   phistrangeanalysis <configFile> [baseConfigFile] --list-groups
//   phistrangeanalysis --check-run <runDir>
//
// --group runs one group of active_tasks; --open-record writes the files the run opens to <file>
// (RootIO::WriteOpenRecordTo) and --summary-to its warnings (Logger::WriteSummaryTo); --list-groups prints one
// "GROUP <name>" line per group and runs nothing; --check-run judges the groups of a parallel run together. All of them
// are what runAnalysis.sh --jobs uses.
int main(int argc, char* argv[])
{
  // Equivalent of `root -b` used by the interpreted mode: no windows, no display.
  gROOT->SetBatch(kTRUE);

  std::vector<std::string> positional;
  std::string group;
  std::string openRecordFile;
  std::string summaryFile;
  std::string checkRunDir;
  bool listGroups = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool hasValue = i + 1 < argc;
    if (arg == "--group" && hasValue) {
      group = argv[++i];
    } else if (arg == "--open-record" && hasValue) {
      openRecordFile = argv[++i];
    } else if (arg == "--summary-to" && hasValue) {
      summaryFile = argv[++i];
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
    if (!checkRunDir.empty())
      return CheckRun(checkRunDir);

    if (positional.empty() || positional.size() > 2) {
      std::cerr << "Usage: " << argv[0]
                << " <configFile> [baseConfigFile] [--group <name>] [--open-record <file>] [--summary-to <file>]"
                   " | --list-groups\n       "
                << argv[0] << " --check-run <runDir>\n";
      return 1;
    }
    const std::string configFile = positional[0];
    const std::string baseConfigFile = positional.size() > 1 ? positional[1] : "";

    if (listGroups) {
      const WorkflowManager manager(configFile, baseConfigFile);
      for (const std::string& name : manager.GroupNames())
        std::cout << "GROUP " << name << "\n";
      return 0;
    }
    if (!openRecordFile.empty())
      RootIO::WriteOpenRecordTo(openRecordFile);

    const int status = RunAnalysisChain(configFile, baseConfigFile, group);
    if (!summaryFile.empty())
      Logger::WriteSummaryTo(summaryFile);
    return status;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}

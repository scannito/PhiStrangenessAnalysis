#pragma once

#include "Logger.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The files each block of a workflow run opens, so that a run whose blocks would read a stale file or overwrite each
// other's output stops instead of giving wrong numbers. Within one run every file another task reads is looked up by
// name: a block that reads a file before the block that writes it has run reads the previous run's version, with the
// right stamps and no error; and of two blocks writing one file the second clears or replaces what the first wrote,
// after a third may already have read it.
//
// A header of its own and not part of RootIO: the rule is about the blocks of a workflow, which the helpers that open
// files know nothing of, and it needs no ROOT. RootIO::OpenOrThrow tells it of every open
// (Note); WorkflowManager names the running block (BlockScope) and clears it when a workflow starts. Process state,
// like the Logger counters.
namespace OpenRecord
{

// One open of a file by a block of the workflow.
struct OpenedFile {
  bool write;
  std::string block;
  std::string path;
};

namespace Detail
{
struct State {
  std::string currentBlock;                                // empty outside a workflow block: nothing is recorded
  std::map<std::string, std::set<std::string>> openedBy;   // canonical path -> the blocks that opened it
  std::vector<OpenedFile> opened;                          // every open of this workflow, in order
  std::ofstream sink;                                      // the same, written out for runAnalysis.sh -j, if asked
};

inline State& GetState()
{
  static State state;
  return state;
}
} // namespace Detail

// Called before the file is opened: RECREATE has replaced it by the time TFile::Open returns. A file is written by one
// block of a run, before every block that reads it; a block may read and then write its own files (the projection
// caches). The rule is per file, while the framework owns a file per subtree ({binning_name}, Extract1D/Extract2D):
// one binning_name per run leaves the two engines of one PhiAssocSpectra.root as the only shared write that would be
// legitimate, and no configuration has it; within one run, two engines sharing a file have to be two runs.
inline void Note(const std::string& path, bool write, std::string_view errCtx)
{
  Detail::State& state = Detail::GetState();
  if (state.currentBlock.empty())
    return;
  const std::string& block = state.currentBlock;

  // One file is spelled in more than one way: a full path in corrections_file, directory + prefix + name where it is
  // written. A path that cannot be made canonical is recorded as written, and a second spelling of it is then missed.
  std::error_code ec;
  const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
  if (ec)
    Logger::Warn("{}: '{}' cannot be made canonical ({}): the check of which blocks open it only sees this spelling.",
                 errCtx, path, ec.message());
  const std::string key = ec ? path : canonical.string();
  std::set<std::string>& blocks = state.openedBy[key];

  if (write) {
    for (const std::string& other : blocks) {
      if (other != block)
        throw std::runtime_error(std::format(
          "[FATAL] {}: block '{}' writes '{}', which block '{}' of this run has already opened: if '{}' read it, it read "
          "the version of a previous run, and if it wrote it, this block clears or replaces what it wrote. List the "
          "writer before its readers in active_tasks, or give two writers different output_prefix values.",
          errCtx, block, path, other, other));
    }
  }
  blocks.insert(block);
  state.opened.push_back({write, block, key});
  // One line per open, flushed: a process that dies leaves the lines of what it opened before.
  if (state.sink.is_open())
    state.sink << (write ? "W" : "R") << '\t' << block << '\t' << key << std::endl;
}

// A new workflow in this process: the files the previous one opened are not this run's.
inline void Clear()
{
  Detail::GetState().openedBy.clear();
  Detail::GetState().opened.clear();
}

// What the blocks of this workflow opened, in order.
inline const std::vector<OpenedFile>& Opened() { return Detail::GetState().opened; }

// Writes the record to 'path' as well, one line per open: W or R, block, canonical path, tab-separated. The groups of
// a parallel run are separate processes, so the files one group writes and another opens are only seen by putting
// the records together (runAnalysis.sh -j). Read back by Read.
inline void WriteTo(const std::string& path)
{
  Detail::GetState().sink.open(path, std::ios::trunc);
  if (!Detail::GetState().sink)
    throw std::runtime_error(std::format("[FATAL] OpenRecord: cannot write the record of opened files to '{}'", path));
}

// Empty when the file cannot be read: the process that should have written it did not get that far.
inline std::optional<std::vector<OpenedFile>> Read(const std::string& path)
{
  std::ifstream in(path);
  if (!in)
    return std::nullopt;
  std::vector<OpenedFile> opened;
  std::string mode, block, file;
  while (std::getline(in, mode, '\t') && std::getline(in, block, '\t') && std::getline(in, file))
    opened.push_back({mode == "W", block, file});
  return opened;
}

// The files that one group wrote and another opened, given what each group's blocks opened: one description each,
// naming the group/block pairs. Groups that share a file cannot run in parallel: the reader gets whichever version
// is there when it opens it. Used on the record of a whole run in one process (WorkflowManager::RunWorkflow) and on
// the records of the processes of a parallel one (phistrangeanalysis --check-run).
inline std::vector<std::string> SharedBetweenGroups(const std::map<std::string, std::vector<OpenedFile>>& openedByGroup)
{
  struct Use {
    std::set<std::string> groups, writers, openers;
  };
  std::map<std::string, Use> byPath;
  for (const auto& [group, opened] : openedByGroup) {
    for (const OpenedFile& o : opened) {
      Use& use = byPath[o.path];
      use.groups.insert(group);
      (o.write ? use.writers : use.openers).insert(std::format("{}/{}", group, o.block));
    }
  }
  std::vector<std::string> shared;
  for (const auto& [path, use] : byPath) {
    if (!use.writers.empty() && use.groups.size() > 1)
      shared.push_back(std::format("'{}' written by {}, read by {}", path, Logger::Join(use.writers, " "),
                                   use.openers.empty() ? "nobody else" : Logger::Join(use.openers, " ")));
  }
  return shared;
}

// Names the workflow block that is running, until it goes out of scope: a block that throws leaves no name behind.
class BlockScope
{
 public:
  explicit BlockScope(std::string block) { Detail::GetState().currentBlock = std::move(block); }
  ~BlockScope() { Detail::GetState().currentBlock.clear(); }
  BlockScope(const BlockScope&) = delete;
  BlockScope& operator=(const BlockScope&) = delete;
};

} // namespace OpenRecord

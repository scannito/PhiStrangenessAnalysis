#pragma once

#include "BinningUtils.h"
#include "Logger.h"
#include "RunEnvironment.h"

#include "TDirectory.h"
#include "TKey.h"
#include "TNamed.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"

#include <concepts>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <set>
#include <span>
#include <string_view>
#include <vector>
#include <type_traits>
#include <utility>

namespace RootIO
{
// Anything these helpers can read out of a TDirectory: histograms, THnSparse,
// canvases. Not narrowed to TH1 on purpose - THnSparse is not one.
template <typename T>
concept RootObject = std::derived_from<T, TObject>;

// A directory on disk and its parents, created if missing: TFile::Open and SaveAs do not create them.
inline void CreateDirectoriesOrThrow(const std::filesystem::path& dir, std::string_view errCtx)
{
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec)
    throw std::runtime_error(std::format("[FATAL] {}: cannot create the directory '{}': {}", errCtx, dir.string(), ec.message()));
}

// The files each block of a workflow run opens, so that a run whose blocks would read a stale file or overwrite each
// other's output stops instead of giving wrong numbers. Within one run every file another task reads is looked up by
// name: a block that reads a file before the block that writes it has run reads the previous run's version, with the
// right stamps and no error; and of two blocks writing one file the second clears or replaces what the first wrote,
// after a third may already have read it. Process state, like the Logger counters; WorkflowManager clears it when a
// workflow starts.
// One open of a file by a block of the workflow, as the record keeps it.
struct OpenedFile {
  bool write;
  std::string block;
  std::string path;
};

namespace Detail
{
struct OpenRecord {
  std::string currentBlock;                                // empty outside a workflow block: nothing is recorded
  std::map<std::string, std::set<std::string>> openedBy;   // canonical path -> the blocks that opened it
  std::vector<OpenedFile> opened;                          // every open of this workflow, in order
  std::ofstream sink;                                      // the same, written out for runAnalysis.sh -j, if asked
};

inline OpenRecord& Record()
{
  static OpenRecord record;
  return record;
}

// Before the file is opened: RECREATE has replaced it by the time TFile::Open returns. A file is written by one block
// of a run, before every block that reads it; a block may read and then write its own files (the projection caches). The rule is per file, while the framework owns a file per subtree
// ({binning_name}, Extract1D/Extract2D): one binning_name per run leaves the two engines of one PhiAssocSpectra.root
// as the only shared write that would be legitimate, and no configuration has it; within one run, two engines sharing
// a file have to be two runs.
inline void RecordOpen(const std::string& path, bool write, std::string_view errCtx)
{
  OpenRecord& record = Record();
  if (record.currentBlock.empty())
    return;
  const std::string& block = record.currentBlock;

  // One file is spelled in more than one way: a full path in corrections_file, directory + prefix + name where it is
  // written. A path that cannot be made canonical is recorded as written, and a second spelling of it is then missed.
  std::error_code ec;
  const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
  if (ec)
    Logger::Warn("{}: '{}' cannot be made canonical ({}): the check of which blocks open it only sees this spelling.",
                 errCtx, path, ec.message());
  const std::string key = ec ? path : canonical.string();
  std::set<std::string>& blocks = record.openedBy[key];

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
  record.opened.push_back({write, block, key});
  // One line per open, flushed: a process that dies leaves the lines of what it opened before.
  if (record.sink.is_open())
    record.sink << (write ? "W" : "R") << '\t' << block << '\t' << key << std::endl;
}

inline std::unique_ptr<TFile> OpenChecked(const std::string& path, const char* mode, std::string_view errCtx)
{
  std::unique_ptr<TFile> f(TFile::Open(path.c_str(), mode));
  if (!f || f->IsZombie())
    throw std::runtime_error(std::format("[FATAL] {}: Cannot open '{}'", errCtx, path));
  // A file a run did not close, because it was interrupted while writing it, is opened by ROOT in recovery: the keys
  // it can find are rebuilt, with a Warning and nothing else, and what was written last may be missing. On
  // 2 November 2026 a phi fit written into such a file lost h2TriggerSignal, and the run stopped one task later, on
  // the missing object. Stopped here instead, on the file that is damaged.
  if (f->TestBit(TFile::kRecovered))
    throw std::runtime_error(std::format("[FATAL] {}: '{}' was not closed properly (a run was interrupted while writing it): "
                                         "ROOT recovered what it could, and objects may be missing. Delete it and re-run the "
                                         "task that writes it, or restore it from a copy.",
                                         errCtx, path));
  return f;
}
} // namespace Detail

// A new workflow in this process: the files the previous one opened are not this run's.
inline void ClearOpenRecord()
{
  Detail::Record().openedBy.clear();
  Detail::Record().opened.clear();
}

// What the blocks of this workflow opened, in order.
inline const std::vector<OpenedFile>& OpenedFiles() { return Detail::Record().opened; }

// Writes the record to 'path' as well, one line per open: W or R, block, canonical path, tab-separated. The groups of
// a parallel run are separate processes, so the files one group writes and another opens are only seen by putting
// the records together (runAnalysis.sh -j). Read back by ReadOpenRecord.
inline void WriteOpenRecordTo(const std::string& path)
{
  Detail::Record().sink.open(path, std::ios::trunc);
  if (!Detail::Record().sink)
    throw std::runtime_error(std::format("[FATAL] RootIO: cannot write the record of opened files to '{}'", path));
}

inline std::vector<OpenedFile> ReadOpenRecord(const std::string& path, std::string_view errCtx)
{
  std::ifstream in(path);
  if (!in)
    throw std::runtime_error(std::format("[FATAL] {}: cannot read the record of opened files '{}'", errCtx, path));
  std::vector<OpenedFile> opened;
  std::string mode, block, file;
  while (std::getline(in, mode, '\t') && std::getline(in, block, '\t') && std::getline(in, file))
    opened.push_back({mode == "W", block, file});
  return opened;
}

// Names the workflow block that is running, for the record of the files it opens, until it goes out of scope: a
// block that throws leaves no name behind.
class BlockScope
{
 public:
  explicit BlockScope(std::string block) { Detail::Record().currentBlock = std::move(block); }
  ~BlockScope() { Detail::Record().currentBlock.clear(); }
  BlockScope(const BlockScope&) = delete;
  BlockScope& operator=(const BlockScope&) = delete;
};

inline std::unique_ptr<TFile> OpenOrThrow(const std::string& path, const char* mode, std::string_view errCtx)
{
  // Every caller spells its mode in capitals; "" is TFile's default, a read.
  const std::string_view modeName(mode);
  Detail::RecordOpen(path, !(modeName.empty() || modeName == "READ"), errCtx);
  // A file to be updated is checked on a read-only handle first. The UPDATE handle, closed by the throw above, would
  // write back what ROOT recovered, and the next run would find a file that looks sound. Measured on 19 November 2026
  // (ROOT 6.36.10), on a file created and never closed: kRecovered 1 on every READ open, also repeated; one UPDATE
  // open and close, and the next READ has kRecovered 0. A purity file killed mid-write stopped one run and went
  // through the next.
  // A path that cannot be checked is left to the UPDATE open below, which then fails with its own message.
  std::error_code ec;
  if (modeName == "UPDATE" && std::filesystem::exists(path, ec))
    Detail::OpenChecked(path, "READ", errCtx);
  return Detail::OpenChecked(path, mode, errCtx);
}

// Which file a directory belongs to. Without it a "missing object" message names
// the path but not the file, and a task that reads three files - data, mixed
// events, corrections - sends you to look in the wrong one.
inline std::string FileNameOf(const TDirectory* dir)
{
  if (!dir)
    return "<no directory>";
  const TFile* file = dir->GetFile();
  return file ? file->GetName() : dir->GetName();
}

template <RootObject T>
inline std::unique_ptr<T> GetUniqueOrThrow(TDirectory* dir, const std::string& objPath, std::string_view errCtx, bool detachFromFile = true)
{
  // dynamic_cast and not static_cast: Get() returns whatever is under that name, and
  // a static_cast to the wrong class yields a pointer that is not caught by the null
  // check below and blows up on first use. The name being right does not make the
  // class right - a foreign file may hold a TGraphErrors where a TH1 was expected -
  // and this turns that into the message already written here.
  if (!dir)
    throw std::runtime_error(std::format("[FATAL] {}: no directory to read '{}' from (a null TDirectory was passed).",
                                         errCtx, objPath));
  T* obj = dynamic_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj)
    throw std::runtime_error(std::format("[FATAL] {}: Missing object '{}' in '{}', or it is not a {}",
                                         errCtx, objPath, FileNameOf(dir), T::Class_Name()));

  if constexpr (std::is_base_of_v<TH1, T>) {
    if (detachFromFile)
      obj->SetDirectory(0);
  }

  return std::unique_ptr<T>(obj);
}

template <RootObject T>
inline std::unique_ptr<T> GetUniqueOrWarn(TDirectory* dir, const std::string& objPath, std::string_view warnCtx, bool detachFromFile = true)
{
  // dynamic_cast, for the reason spelled out in GetUniqueOrThrow above.
  // A null directory is not a missing object, which is what this function forgives: it is a
  // caller with nothing to read from, so it stops here as in GetUniqueOrThrow instead of
  // dereferencing it.
  if (!dir)
    throw std::runtime_error(std::format("[FATAL] {}: no directory to read '{}' from (a null TDirectory was passed).",
                                         warnCtx, objPath));
  T* obj = dynamic_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj) {
    Logger::Warn("{}: Missing object '{}' in '{}', or it is not a {}",
                 warnCtx, objPath, FileNameOf(dir), T::Class_Name());
    return nullptr;
  }

  if constexpr (std::is_base_of_v<TH1, T>) {
    if (detachFromFile)
      obj->SetDirectory(0);
  }

  return std::unique_ptr<T>(obj);
}

// ---------------------------------------------------------------------------
// Navigation inside a TFile
// ---------------------------------------------------------------------------

// The ROOT directory prefix "node1/node2/", trailing slash included: a directory as a message names it. The path
// of an object below it, as Get() takes it, is ObjectPath.
inline std::string MakeDirPath(const std::vector<std::string>& path)
{
  std::string fullPath = "";
  for (const auto& dir : path) {
    fullPath += dir;
    fullPath += '/';
  }
  return fullPath;
}

// The path of an object below those directories, "node1/node2/name", as Get() takes it.
inline std::string ObjectPath(const std::vector<std::string>& path, std::string_view name)
{
  std::string objectPath = MakeDirPath(path);
  objectPath += name;
  return objectPath;
}

// The directory at baseDir/pathNodes if it exists, or nullptr: read-only, so a missing directory is an
// answer and the caller decides what it means (a cache miss, an optional section).
inline TDirectory* FindPath(TDirectory* baseDir, const std::vector<std::string>& pathNodes)
{
  TDirectory* dir = baseDir;
  for (const auto& node : pathNodes) {
    if (!dir)
      return nullptr;
    if (!node.empty())
      dir = dir->GetDirectory(node.c_str());
  }
  return dir;
}

// The directory at baseDir/pathNodes, created where missing; never nullptr. A directory that cannot be created
// would lose whatever was to be written there, so it stops the run: the function that did both, with a flag,
// used to log and return nullptr, and each caller then threw, skipped the write or wrote into the current
// directory instead.
inline TDirectory* MakePath(TDirectory* baseDir, const std::vector<std::string>& pathNodes)
{
  if (!baseDir)
    throw std::runtime_error(std::format("[FATAL] RootIO::MakePath: no directory to create '{}' in.", MakeDirPath(pathNodes)));

  TDirectory* dir = baseDir;
  for (const auto& node : pathNodes) {
    if (node.empty())
      continue;
    // The existing directory if there is one, a new one otherwise. returnExistingDirectory finds one made earlier
    // in the same run too: ROOT 6.36 appends a new directory's key to its mother at once (InitDirectoryFile), and
    // GetKey searches that list. nullptr when the disk is full, the file is closed or read-only, or the name is
    // taken by an object that is not a directory.
    TDirectory* next = dir->mkdir(node.c_str(), "", true);
    if (!next)
      throw std::runtime_error(std::format("[FATAL] RootIO::MakePath: cannot create '{}' in '{}' of {}.",
                                           node, dir->GetPath(), FileNameOf(baseDir)));
    dir = next;
  }
  return dir;
}

// Removes a subtree, so that a task about to rewrite it does not leave the previous
// run's objects beside the new ones.
//
// A task OWNS the directory it writes. When what it produces changes - a different
// layout, an object that used to be flat and is now nested - opening the file in
// UPDATE keeps both generations and nothing says which is current. This is why the
// pair UPDATE + ClearPath is not a roundabout way of writing RECREATE: it clears
// only the owned path, so a sibling scheme or the other Extract1D/2D in the same
// file survive. That coexistence is what 'binningName' and the Extract split are
// for, and it is not currently exercised - which is a reason to keep it working, not
// evidence that it is unwanted.
//
// Delete("name;*") on the parent removes every cycle of the key, the directory and
// its contents included - the ROOT documentation is explicit about the namecycle
// form.
//
// What it does NOT do is shrink the file: the freed bytes become free segments that
// ROOT reuses for later keys, so an output rewritten on every run does not grow
// without bound, but it will not be as small as the same content written with
// RECREATE either. If one of these files ever looks disproportionate, that is why,
// and TFile::Purge or one RECREATE pass is the answer - not a change here.
inline void ClearPath(TDirectory* baseDir, const std::vector<std::string>& pathNodes)
{
  if (!baseDir || pathNodes.empty())
    return;

  // Read-only navigation is the existence test used throughout this header: if the
  // path is not there, there is nothing to clear and nothing to report.
  if (!FindPath(baseDir, pathNodes))
    return;

  // The parent must resolve, since the full path just did.
  const std::vector<std::string> parentPath(pathNodes.begin(), pathNodes.end() - 1);
  TDirectory* parent = parentPath.empty() ? baseDir : FindPath(baseDir, parentPath);

  parent->Delete(std::format("{};*", pathNodes.back()).c_str());
}

// -----------------------------------------------------------------------------
// Binning stamps: provenance for cached projections
// -----------------------------------------------------------------------------

// The names of the stamps, the same in every file that carries them (the spectra, the
// corrections, the purities, the projection caches): one spelling, here, for every writer and
// reader.
inline const std::string kStampMult{"binning_mult"};
inline const std::string kStampPtPhi{"binning_ptPhi"};
inline const std::string kStampPtAssoc{"binning_ptAssoc"};

// Records a binning inside a file as an empty TH1D whose axis IS the binning.
// Storing it as a histogram rather than as a bare array means it reads back
// through AxisEdges like any other axis, and is inspectable in a TBrowser.
inline void WriteBinningStamp(TDirectory* dir, const std::string& name, std::span<const double> edges)
{
  // A stamp not written reads back as empty, "nothing to compare": the check would be off with no word.
  if (!dir)
    throw std::runtime_error(std::format("[FATAL] RootIO::WriteBinningStamp: no directory to write '{}' into: the "
                                         "output file is not open for writing.",
                                         name));
  if (edges.size() < 2)
    throw std::runtime_error(std::format("[FATAL] RootIO::WriteBinningStamp: '{}' into {} has {} edges: the binning was "
                                         "never resolved from the input (Resolve*Binning).",
                                         name, dir->GetPath(), edges.size()));

  TH1D stamp(name.c_str(), "binning stamp (bin edges only, contents unused)",
             BinningUtils::NBins(edges), edges.data());
  stamp.SetDirectory(0);

  dir->cd();
  stamp.Write(name.c_str(), TObject::kOverwrite);
}

// Empty vector when no stamp is present, e.g. a cache produced before stamps
// existed. That case is a warning, not an error: see RequireMatchingBinningStamp.
// Empty when the directory holds no stamp of that name (a file produced before stamps existed). A directory that
// is not there is not that case: the file holds nothing of this scheme, and that stops the run.
inline std::vector<double> ReadBinningStamp(TDirectory* dir, const std::string& name, std::string_view errCtx)
{
  if (!dir)
    throw std::runtime_error(std::format("[FATAL] {}: no directory to read the '{}' stamp from: the file holds nothing "
                                         "of this binning scheme or section. Check 'binning_name', or re-run the task "
                                         "that produces it under this scheme.",
                                         errCtx, name));

  // dynamic_cast for the reason given at GetUniqueOrThrow: a stamp that is not a histogram
  // reads as no stamp, which RequireMatchingBinningStamp reports, instead of a bad pointer.
  auto* raw = dynamic_cast<TH1*>(dir->Get(name.c_str()));
  if (!raw)
    return {};

  raw->SetDirectory(0);
  std::unique_ptr<TH1> stamp(raw);
  return BinningUtils::AxisEdges(stamp->GetXaxis());
}

// Was the file this directory belongs to produced with the binning of the run reading it? Used for the
// corrections, the purities and the files ComparisonTask divides, so the messages name none of them and
// errCtx says which. The objects are named by interval, so a mismatch would also show up as a missing object; the
// stamp stops the run earlier and better, before anything is read, with the edge that differs and both
// values, where a missing object says only what was expected.
inline void RequireMatchingBinningStamp(TDirectory* dir, const std::string& name,
                                        std::span<const double> current, std::string_view errCtx)
{
  const std::vector<double> stamped = ReadBinningStamp(dir, name, errCtx);

  if (stamped.empty()) {
    Logger::Warn("{}: no '{}' stamp, so the binning it was produced with cannot be verified. Re-produce it "
                 "if the input production has changed since.",
                 errCtx, name);
    return;
  }

  const std::string diff = BinningUtils::Compare(stamped, current, "that file", "current run");
  if (!diff.empty()) {
    throw std::runtime_error(std::format(
      "[FATAL] {}: it was produced with a different '{}':\n{}"
      "The two do not describe the same intervals: re-run the task that produced it with this binning.",
      errCtx, name, diff));
  }
}

// ---------------------------------------------------------------------------
// Provenance: what produced this file
// ---------------------------------------------------------------------------
// Unlike the binning stamps above, this is documentation and never a check. A
// path that moved is not a reason to stop a run, and the stamps already refuse
// the mismatches that would actually corrupt a result.
//
// What goes in is the merged configuration block, serialised: not a hand-picked
// list of facts, which would silently stop covering the keys added after it was
// written. The configuration on disk describes the NEXT run; this describes THIS
// file, and travels with it - to a colleague, to CCDB - where the JSON does not.
//
// One TNamed per entry so that it reads in a TBrowser, or with "rootls -l",
// without any code, and so that adding an entry breaks nobody.

// Where the provenance is: a directory below 'owner' - usually the file's binning_name; ""
// for the top of the file - named the same in every file the framework writes. These functions
// find it themselves, and decide whether it is created (writing) or only looked up (reading),
// so that no caller spells either. A file that keeps several records side by side
// (comparison_task: one per input, next to its own) qualifies them with 'record'.
namespace Detail
{
inline const std::string kProvenanceDir{"Provenance"};

inline std::string ProvenanceDirName(std::string_view record)
{
  return record.empty() ? kProvenanceDir : std::format("{}_{}", kProvenanceDir, record);
}
} // namespace Detail

// What every task records before anything of its own: when, its whole merged configuration block (so a key
// added to the JSON tomorrow is recorded without touching the task), and the facts about the run itself.
// 'configBlock' is that block already serialised (JsonConfig::Serialize), so that RootIO, which writes files,
// does not depend on the format of the configuration. Documentation, not a check: see WriteProvenance.
inline std::map<std::string, std::string> StartProvenance(std::string configBlock)
{
  std::map<std::string, std::string> facts = RunEnvironment::Facts();
  facts["produced_at"] = RunEnvironment::TimestampNow();
  facts["config_block"] = std::move(configBlock);
  return facts;
}

inline void WriteProvenance(TDirectory* base, const std::string& owner, const std::map<std::string, std::string>& facts,
                            std::string_view record = {})
{
  // MakePath throws if the directory cannot be created: that is the output file failing, and the results
  // written beside the provenance are lost with it. "Provenance never throws" is about the facts recorded.
  MakePath(base, {owner, Detail::ProvenanceDirName(record)})->cd();
  for (const auto& [key, value] : facts) {
    TNamed entry(key.c_str(), value.c_str());
    entry.Write(key.c_str(), TObject::kOverwrite);
  }
}

inline std::map<std::string, std::string> ReadProvenance(TDirectory* base, const std::string& owner)
{
  std::map<std::string, std::string> facts;
  TDirectory* dir = FindPath(base, {owner, Detail::kProvenanceDir});
  if (!dir || !dir->GetListOfKeys())
    return facts;

  for (TObject* keyObj : *dir->GetListOfKeys()) {
    auto* key = static_cast<TKey*>(keyObj);
    // Entries are TNamed exactly. A subdirectory is a TNamed too, and reading it here would
    // record it as a fact and delete a directory its file still owns.
    if (std::string(key->GetClassName()) != "TNamed")
      continue;
    std::unique_ptr<TNamed> entry(dynamic_cast<TNamed*>(key->ReadObj()));
    if (entry)
      facts[entry->GetName()] = entry->GetTitle();
  }
  return facts;
}

// For the task consuming the file. The serialised configuration is one long line,
// so it is announced rather than printed: it is there to be diffed, not read in a
// log.
inline void PrintProvenance(TDirectory* base, const std::string& owner, std::string_view fileLabel)
{
  const std::map<std::string, std::string> facts = ReadProvenance(base, owner);
  if (facts.empty()) {
    Logger::Info("{}: no provenance recorded (written before it existed).", fileLabel);
    return;
  }

  Logger::Info("{} was produced by:", fileLabel);
  for (const auto& [key, value] : facts) {
    // The serialised blocks are one long line each: announced, not printed. They
    // are there to be diffed against another file, not read in a log.
    if (value.size() > 120)
      Logger::Info("         {} = <{} chars, read it from the file>", key, value.size());
    else
      Logger::Info("         {} = {}", key, value);
  }
}

// -----------------------------------------------------------------------------
// Files for the CCDB
// -----------------------------------------------------------------------------

// A file for the CCDB holds its object at the top, named "ccdb_object": the name the uploader passes to o2-ccdb-upload
// (-k) and the one CCDB itself gives every object (CcdbApi::CCDBOBJECT_ENTRY), which the O2Physics task reads. Given
// here, where the file is written, so no writer spells it. Next to it, in Provenance/, the provenance of the task
// that made it: the uploader takes the object's CCDB metadata from there, so they describe this very object, written
// at the same moment, and not whatever the task's main file holds by then. o2-ccdb-upload sends only the key it is
// given, so the provenance stays on disk.
inline void WriteCcdbFile(const std::string& path, TNamed* object, const std::map<std::string, std::string>& provenance,
                          std::string_view errCtx)
{
  std::unique_ptr<TFile> file = OpenOrThrow(path, "RECREATE", errCtx);
  object->SetName("ccdb_object");
  file->WriteTObject(object, nullptr, "Overwrite");
  WriteProvenance(file.get(), "", provenance);
  file->Close();
}

} // namespace RootIO

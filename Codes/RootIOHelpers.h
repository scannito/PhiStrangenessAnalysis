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
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <span>
#include <string_view>
#include <vector>
#include <type_traits>

namespace RootIO
{
// Anything these helpers can read out of a TDirectory: histograms, THnSparse,
// canvases. Not narrowed to TH1 on purpose - THnSparse is not one.
template <typename T>
concept RootObject = std::derived_from<T, TObject>;

inline std::unique_ptr<TFile> OpenOrThrow(const std::string& path, const char* mode, std::string_view errCtx)
{
  std::unique_ptr<TFile> f(TFile::Open(path.c_str(), mode));
  if (!f || f->IsZombie())
    throw std::runtime_error(std::format("[FATAL] {}: Cannot open '{}'", errCtx, path));
  return f;
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

inline TDirectory* GetOrCreatePath(TDirectory* baseDir, const std::vector<std::string>& pathNodes, bool isReadOnly = false)
{
  if (!baseDir)
    return nullptr;

  TDirectory* currentDir = baseDir;

  for (const auto& node : pathNodes) {
    if (node.empty())
      continue;

    // Search for the subdirectory in the current level
    TDirectory* nextDir = currentDir->GetDirectory(node.c_str());
    if (!nextDir) {
      if (isReadOnly) {
        // In read-only mode, do not create anything; fail silently (Cache Miss)
        return nullptr;
      } else {
        nextDir = currentDir->mkdir(node.c_str());
      }

      // Safety check (e.g., disk full, file closed, permission denied)
      if (!nextDir) {
        Logger::Error("RootIO::GetOrCreatePath - Failed to create TDirectory '{}'.", node);
        return nullptr;
      }
    }

    // Move down one level for the next iteration
    currentDir = nextDir;
  }

  return currentDir;
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
  if (!GetOrCreatePath(baseDir, pathNodes, true))
    return;

  // The parent must resolve, since the full path just did.
  const std::vector<std::string> parentPath(pathNodes.begin(), pathNodes.end() - 1);
  TDirectory* parent = parentPath.empty() ? baseDir : GetOrCreatePath(baseDir, parentPath, true);

  parent->Delete((pathNodes.back() + ";*").c_str());
}

// Builds the ROOT directory prefix "node1/node2/", trailing slash included, so
// that the caller can append the object name and hand the result to Get().
inline std::string MakeDirPath(const std::vector<std::string>& path)
{
  std::string fullPath = "";
  for (const auto& dir : path) {
    fullPath += dir + "/";
  }
  return fullPath;
}

// -----------------------------------------------------------------------------
// Files for the CCDB
// -----------------------------------------------------------------------------

// A file for the CCDB holds one object, at its top, named "ccdb_object": the name Macros/uploadToCCDB.C and the
// O2Physics task read, so not ours to change. Given here, where the file is written, so no writer spells it.
inline void WriteCcdbFile(const std::string& path, TNamed* object, std::string_view errCtx)
{
  std::unique_ptr<TFile> file = OpenOrThrow(path, "RECREATE", errCtx);
  object->SetName("ccdb_object");
  file->WriteTObject(object, nullptr, "Overwrite");
  file->Close();
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
  if (!dir || edges.size() < 2)
    return;

  TH1D stamp(name.c_str(), "binning stamp (bin edges only, contents unused)",
             BinningUtils::NBins(edges), edges.data());
  stamp.SetDirectory(0);

  dir->cd();
  stamp.Write(name.c_str(), TObject::kOverwrite);
}

// Empty vector when no stamp is present, e.g. a cache produced before stamps
// existed. That case is a warning, not an error: see RequireMatchingBinningStamp.
inline std::vector<double> ReadBinningStamp(TDirectory* dir, const std::string& name)
{
  if (!dir)
    return {};

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
  const std::vector<double> stamped = ReadBinningStamp(dir, name);

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

inline void WriteProvenance(TDirectory* base, const std::string& owner, const std::map<std::string, std::string>& facts,
                            std::string_view record = {})
{
  TDirectory* dir = GetOrCreatePath(base, {owner, Detail::ProvenanceDirName(record)}, false);
  if (!dir)
    return;

  dir->cd();
  for (const auto& [key, value] : facts) {
    TNamed entry(key.c_str(), value.c_str());
    entry.Write(key.c_str(), TObject::kOverwrite);
  }
}

inline std::map<std::string, std::string> ReadProvenance(TDirectory* base, const std::string& owner)
{
  std::map<std::string, std::string> facts;
  TDirectory* dir = GetOrCreatePath(base, {owner, Detail::kProvenanceDir}, true);
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

} // namespace RootIO

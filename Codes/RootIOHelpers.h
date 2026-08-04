#pragma once

#include "BinningUtils.h"
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

template <RootObject T>
inline std::unique_ptr<T> GetUniqueOrThrow(TDirectory* dir, const std::string& objPath, std::string_view errCtx, bool detachFromFile = true)
{
  T* obj = static_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj)
    throw std::runtime_error(std::format("[FATAL] {}: Missing object '{}'", errCtx, objPath));

  if constexpr (std::is_base_of_v<TH1, T>) {
    if (detachFromFile)
      obj->SetDirectory(0);
  }

  return std::unique_ptr<T>(obj);
}

template <RootObject T>
inline std::unique_ptr<T> GetUniqueOrWarn(TDirectory* dir, const std::string& objPath, std::string_view warnCtx, bool detachFromFile = true)
{
  T* obj = static_cast<T*>(dir->Get(objPath.c_str()));
  if (!obj) {
    std::cerr << "[WARNING] " << warnCtx << ": Missing object '" << objPath << "'" << std::endl;
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
        std::cerr << "[ERROR] RootIO::GetOrCreatePath - Failed to create TDirectory '" << node << "'." << std::endl;
        return nullptr;
      }
    }

    // Move down one level for the next iteration
    currentDir = nextDir;
  }

  return currentDir;
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
// Binning stamps: provenance for cached projections
// -----------------------------------------------------------------------------

// Records a binning inside a file as an empty TH1D whose axis IS the binning.
// Storing it as a histogram rather than as a bare array means it reads back
// through AxisEdges like any other axis, and is inspectable in a TBrowser.
inline void WriteBinningStamp(TDirectory* dir, const std::string& name, std::span<const double> edges)
{
  if (!dir || edges.size() < 2)
    return;

  TH1D stamp(name.c_str(), "binning stamp (bin edges only, contents unused)",
             BinningUtils::NBins(edges), edges.data());
  stamp.SetDirectory(nullptr);

  dir->cd();
  stamp.Write(name.c_str(), TObject::kOverwrite);
}

// Empty vector when no stamp is present, e.g. a cache produced before stamps
// existed. That case is a warning, not an error: see RequireMatchingBinningStamp.
inline std::vector<double> ReadBinningStamp(TDirectory* dir, const std::string& name)
{
  if (!dir)
    return {};

  auto* raw = static_cast<TH1*>(dir->Get(name.c_str()));
  if (!raw)
    return {};

  raw->SetDirectory(nullptr);
  std::unique_ptr<TH1> stamp(raw);
  return BinningUtils::AxisEdges(stamp->GetXaxis());
}

// Cached histograms are addressed by bin INDEX (they are named ..._ptBin7), so
// reusing a cache built with a different binning silently mixes two
// segmentations: index 31 stops meaning the same pT interval.
inline void RequireMatchingBinningStamp(TDirectory* dir, const std::string& name,
                                        std::span<const double> current, std::string_view errCtx)
{
  const std::vector<double> cached = ReadBinningStamp(dir, name);

  if (cached.empty()) {
    std::cerr << "[WARNING] " << errCtx << ": the cache carries no '" << name
              << "' stamp, so the binning it was built with cannot be verified. Delete the cache "
                 "files if the input production has changed since they were produced."
              << std::endl;
    return;
  }

  const std::string diff = BinningUtils::Compare(cached, current, "cache", "current run");
  if (!diff.empty()) {
    throw std::runtime_error(std::format(
      "[FATAL] {}: the cached projections were built with a different '{}':\n{}"
      "Cached histograms are addressed by bin index, so reusing them would mix "
      "two binnings. Re-run with the cache disabled to rebuild them.",
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

inline void WriteProvenance(TDirectory* dir, const std::map<std::string, std::string>& facts)
{
  if (!dir)
    return;

  dir->cd();
  for (const auto& [key, value] : facts) {
    TNamed entry(key.c_str(), value.c_str());
    entry.Write(key.c_str(), TObject::kOverwrite);
  }
}

inline std::map<std::string, std::string> ReadProvenance(TDirectory* dir)
{
  std::map<std::string, std::string> facts;
  if (!dir || !dir->GetListOfKeys())
    return facts;

  for (TObject* keyObj : *dir->GetListOfKeys()) {
    auto* key = static_cast<TKey*>(keyObj);
    std::unique_ptr<TNamed> entry(dynamic_cast<TNamed*>(key->ReadObj()));
    if (entry)
      facts[entry->GetName()] = entry->GetTitle();
  }
  return facts;
}

// For the task consuming the file. The serialised configuration is one long line,
// so it is announced rather than printed: it is there to be diffed, not read in a
// log.
inline void PrintProvenance(TDirectory* dir, std::string_view fileLabel)
{
  const std::map<std::string, std::string> facts = ReadProvenance(dir);
  if (facts.empty()) {
    std::cout << "[INFO] " << fileLabel << ": no provenance recorded (written before it existed)." << std::endl;
    return;
  }

  std::cout << "[INFO] " << fileLabel << " was produced by:" << std::endl;
  for (const auto& [key, value] : facts) {
    // The serialised blocks are one long line each: announced, not printed. They
    // are there to be diffed against another file, not read in a log.
    if (value.size() > 120)
      std::cout << "         " << key << " = <" << value.size() << " chars, read it from the file>" << std::endl;
    else
      std::cout << "         " << key << " = " << value << std::endl;
  }
}

} // namespace RootIO

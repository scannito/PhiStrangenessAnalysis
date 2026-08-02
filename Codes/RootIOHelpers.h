#pragma once

#include "BinningUtils.h"

#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"

#include <concepts>
#include <format>
#include <iostream>
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
  // std::unique_ptr<TFile> f = std::make_unique<TFile>(path.c_str(), mode);
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

} // namespace RootIO

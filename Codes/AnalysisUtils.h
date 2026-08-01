#pragma once

#include "AnalysisDataStructures.h"
#include "BinningUtils.h"

#include "TDirectory.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"
#include "TString.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace AnalysisUtils
{
// -----------------------------------------------------------------------------
// Math & Projection Utilities
// -----------------------------------------------------------------------------

// Computes the integral and error in a specific range for 1D histograms
inline std::pair<double, double> IntegralAndErrorPair(TH1* h1, double x1, double x2, Option_t* option = "")
{
  double integral{0.0};
  double error{0.0};
  double epsilon{0.00001};

  int binX1 = h1->GetXaxis()->FindBin(x1 + epsilon);
  int binX2 = h1->GetXaxis()->FindBin(x2 - epsilon);

  integral = h1->IntegralAndError(binX1, binX2, error, option);

  return std::make_pair(integral, error);
}

// Computes the integral and error in a specific range for 2D histograms
inline std::pair<double, double> IntegralAndErrorPair(TH2* h2, double x1, double x2, double y1, double y2, Option_t* option = "")
{
  double integral{0.0};
  double error{0.0};
  double epsilon{0.00001};

  int binX1 = h2->GetXaxis()->FindBin(x1 + epsilon);
  int binX2 = h2->GetXaxis()->FindBin(x2 - epsilon);
  int binY1 = h2->GetYaxis()->FindBin(y1 + epsilon);
  int binY2 = h2->GetYaxis()->FindBin(y2 - epsilon);

  integral = h2->IntegralAndError(binX1, binX2, binY1, binY2, error, option);

  return std::make_pair(integral, error);
}

// Struct required by the projection utility
struct AxisToCut {
  int axis;
  int binLow;
  int binUp;
};

// Projects a THnSparse into a lower-dimensional histogram
template <typename THType>
inline std::unique_ptr<THType> ProjectTHnSparse(THnSparse* hnSparse,
                                                const std::vector<AxisToCut>& axesToBeCut,
                                                const std::vector<int>& axesToProject,
                                                const std::string& histName)
{
  if (!hnSparse)
    return nullptr;

  for (const auto& axisToCut : axesToBeCut) {
    hnSparse->GetAxis(axisToCut.axis)->SetRange(axisToCut.binLow, axisToCut.binUp);
  }

  std::unique_ptr<THType> hProjection;

  if constexpr (std::is_base_of_v<TH3, THType>) {
    hProjection.reset(static_cast<THType*>(hnSparse->Projection(axesToProject[0], axesToProject[1], axesToProject[2])));
  } else if constexpr (std::is_base_of_v<TH2, THType>) {
    hProjection.reset(static_cast<THType*>(hnSparse->Projection(axesToProject[0], axesToProject[1])));
  } else if constexpr (std::is_base_of_v<TH1, THType>) {
    hProjection.reset(static_cast<THType*>(hnSparse->Projection(axesToProject[0])));
  }

  if (hProjection) {
    hProjection->SetName(histName.c_str());
    hProjection->SetDirectory(0);
    // THnSparse::Projection already propagates the errors when the sparse
    // stores them, so the projection usually comes with Sumw2 already set.
    // Calling it again is a no-op that only prints a warning.
    if (hProjection->GetSumw2N() == 0)
      hProjection->Sumw2();
  }

  return hProjection;
}

// -----------------------------------------------------------------------------
// pT and Multiplicity Trend Utilities
// -----------------------------------------------------------------------------

// Construct pT spectrum from a vector of histograms
inline std::unique_ptr<TH1> ConstructSpectrum(const std::vector<TH1*>& hContainer,
                                       const std::vector<double>& binsVec,
                                       const std::string& histName,
                                       double absLimToIntegrate)
{
  if (binsVec.size() - 1 != hContainer.size()) {
    throw std::runtime_error("Size of histogram container must be equal to number of bins - 1");
  }

  std::unique_ptr<TH1> hSpectrum = std::make_unique<TH1D>(histName.c_str(), "; p_{T} (GeV/#it{c}); 1/N_{trig} d^{2}N/dp_{T}d#Deltay", binsVec.size() - 1, binsVec.data());

  for (size_t i{0}; i < hContainer.size(); i++) {
    auto [binContent, binError] = IntegralAndErrorPair(hContainer[i], -absLimToIntegrate, absLimToIntegrate);
    double normalizationFactor = (binsVec[i + 1] - binsVec[i]);

    hSpectrum->SetBinContent(i + 1, binContent / normalizationFactor);
    hSpectrum->SetBinError(i + 1, binError / normalizationFactor);
  }

  hSpectrum->SetDirectory(0);

  return hSpectrum;
}

// Content and error of a bin, as one value. The fallback is returned when the
// histogram is absent, so callers do not have to spell out the same ternary
// twice - once for the content and once for the error, with the risk of the two
// disagreeing on what "missing" means.
inline std::pair<double, double> BinValueAndError(const TH1* h, int bin,
                                                  std::pair<double, double> fallback = {1.0, 0.0})
{
  if (!h)
    return fallback;
  return {h->GetBinContent(bin), h->GetBinError(bin)};
}

// Folds a relative uncertainty into the error of one bin.
//
// Meant for quantities that multiply the whole distribution - a normalisation,
// a scale factor - which are 100% correlated across bins and must therefore be
// applied where the distribution has already been reduced to a single number,
// not bin by bin on the distribution itself.
inline void AddRelativeError(TH1* h, int bin, double relativeError)
{
  if (!h || relativeError <= 0.0)
    return;

  const double content = h->GetBinContent(bin);
  const double error = h->GetBinError(bin);
  h->SetBinError(bin, std::hypot(error, content * relativeError));
}

// Construct multiplicity trends from pT spectra
inline void ConstructMultTrend(TH1* hMultTrend,
                        std::variant<TH1*, ExtrapolationResult> source,
                        int i)
{
  /*auto [content, error] = IntegralAndErrorPair(hPtSpectrum, hPtSpectrum->GetXaxis()->GetXmin(), hPtSpectrum->GetXaxis()->GetXmax(), "width");

  if (applyExtrapolation)
  {
      content += extrapolatedYield;
      error = std::sqrt((error * error) + (extrapolatedError * extrapolatedError));
  }*/

  double content{};
  double error{};

  if (std::holds_alternative<TH1*>(source)) {
    TH1* hPtSpectrum = std::get<TH1*>(source);
    auto [c, e] = IntegralAndErrorPair(hPtSpectrum, hPtSpectrum->GetXaxis()->GetXmin(), hPtSpectrum->GetXaxis()->GetXmax(), "width");
    content = c;
    error = e;
  } else {
    const auto& ext = std::get<ExtrapolationResult>(source);
    content = ext.yield;
    error = ext.yieldStatErr;
  }

  /*if (applyExtrapolation) {
    content = totalYield;
    error = totalError;
  } else {
    auto pair = IntegralAndErrorPair(hPtSpectrum, hPtSpectrum->GetXaxis()->GetXmin(), hPtSpectrum->GetXaxis()->GetXmax(), "width");
    content = pair.first;
    error = pair.second;
  }*/

  hMultTrend->SetBinContent(i + 1, content);
  hMultTrend->SetBinError(i + 1, error);
}

// -----------------------------------------------------------------------------
// Graphic Utilities
// -----------------------------------------------------------------------------

// Applies the standard visual style to 1D histograms
inline void SetHistogramStyle(TH1* h1, int color)
{
  h1->SetMarkerStyle(20);
  h1->SetMarkerColor(color);
  h1->SetMarkerSize(1.5);
  h1->SetLineColor(color);
  h1->SetLineWidth(2);
  h1->SetFillStyle(3001);
  h1->SetFillColor(color);
  h1->GetYaxis()->SetTitleSize(0.045);
  h1->GetYaxis()->SetTitleOffset(1.0);
  h1->GetYaxis()->SetLabelSize(0.045);
}

// ------------------------------------------------------------------------------
// TFile navigation utility
// ------------------------------------------------------------------------------

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
        std::cerr << "[ERROR] AnalysisUtils::GetOrCreatePath - Failed to create TDirectory '" << node << "'." << std::endl;
        return nullptr;
      }
    }

    // Move down one level for the next iteration
    currentDir = nextDir;
  }

  return currentDir;
}

inline std::string VectorToPath(const std::vector<std::string>& path)
{
  std::string fullPath = "";
  for (const auto& dir : path) {
    fullPath += dir + "/";
  }
  return fullPath;
}

// -----------------------------------------------------------------------------
// Histogram ratios utility
// -----------------------------------------------------------------------------

inline std::unique_ptr<TH1> MakeRatioHist(TH1* num, TH1* den, const std::string& name,
                                          const std::string& title, int color,
                                          double numScale = 1.0, double denScale = 1.0)
{
  std::unique_ptr<TH1> h(static_cast<TH1*>(num->Clone(name.c_str())));
  h->SetTitle(title.c_str());
  h->SetDirectory(0);
  h->Divide(num, den, numScale, denScale);
  // SetHistogramStyle(h.get(), color);
  return h;
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
             static_cast<int>(edges.size()) - 1, edges.data());
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
                                        std::span<const double> current, const std::string& errCtx)
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
    throw std::runtime_error("[FATAL] " + errCtx + ": the cached projections were built with a different '" +
                             name + "':\n" + diff +
                             "Cached histograms are addressed by bin index, so reusing them would mix two "
                             "binnings. Re-run with the cache disabled to rebuild them.");
  }
}

// -----------------------------------------------------------------------------
// Rebinng utilities
// -----------------------------------------------------------------------------

struct RebinEdgeMismatch {
  double targetEdge{0.0};
  int sourceBin{-1};
  double sourceBinLowEdge{0.0};
  double sourceBinUpEdge{0.0};
  bool outOfSourceRange{false};
};

inline std::vector<RebinEdgeMismatch> FindRebinMismatches(const TH1* hSource, const std::vector<double>& targetBins, double epsilon = 1e-9)
{
  std::vector<RebinEdgeMismatch> mismatches;
  const double xMin = hSource->GetXaxis()->GetXmin();
  const double xMax = hSource->GetXaxis()->GetXmax();

  for (double edge : targetBins) {
    // Out of range check
    if (edge < xMin - epsilon || edge > xMax + epsilon) {
      mismatches.push_back({edge, -1, xMin, xMax, true});
      continue;
    }

    int bin = hSource->GetXaxis()->FindFixBin(edge);
    bin = std::clamp(bin, 1, hSource->GetNbinsX()); // extra defense for edge cases, though FindFixBin should handle this

    double lowEdge = hSource->GetXaxis()->GetBinLowEdge(bin);
    double upEdge = hSource->GetXaxis()->GetBinLowEdge(bin + 1);
    if (std::abs(lowEdge - edge) > epsilon && std::abs(upEdge - edge) > epsilon)
      mismatches.push_back({edge, bin, lowEdge, upEdge, false});
  }
  return mismatches;
}

inline bool IsRebinCompatible(const TH1* hSource, const std::vector<double>& targetBins, double epsilon = 1e-9)
{
  return FindRebinMismatches(hSource, targetBins, epsilon).empty();
}

template <typename THType>
inline std::unique_ptr<THType> RebinToTargetBinning(std::unique_ptr<THType> h, const std::vector<double>& targetBins, const std::string& errCtx)
{
  std::vector<RebinEdgeMismatch> mismatches = FindRebinMismatches(h.get(), targetBins);

  if (!mismatches.empty()) {
    std::string msg = "[FATAL] " + errCtx + ": Histogram '" + std::string(h->GetName()) +
                      "' binning is not compatible with the target binning (" +
                      std::to_string(mismatches.size()) + " edge(s) misaligned):\n";
    for (const auto& m : mismatches) {
      if (m.outOfSourceRange) {
        msg += "  - target edge " + std::to_string(m.targetEdge) +
               " is outside the source histogram's axis range [" +
               std::to_string(m.sourceBinLowEdge) + ", " + std::to_string(m.sourceBinUpEdge) + "]\n";
      } else {
        msg += "  - target edge " + std::to_string(m.targetEdge) +
               " falls inside source bin " + std::to_string(m.sourceBin) + " [" +
               std::to_string(m.sourceBinLowEdge) + ", " + std::to_string(m.sourceBinUpEdge) +
               "] instead of aligning with a bin boundary\n";
      }
    }
    throw std::runtime_error(msg);
  }

  int nTargetBins = static_cast<int>(targetBins.size()) - 1;
  return std::unique_ptr<THType>(static_cast<THType*>(h->Rebin(nTargetBins, (std::string(h->GetName()) + "_rebinned").c_str(), targetBins.data())));
}

/*inline bool IsRebinCompatible(const TH1* hSource, const std::vector<double>& targetBins, double epsilon = 1e-9)
{
  for (double edge : targetBins) {
    int bin = hSource->GetXaxis()->FindFixBin(edge);
    double lowEdge = hSource->GetXaxis()->GetBinLowEdge(bin);
    double upEdge = hSource->GetXaxis()->GetBinLowEdge(bin + 1);
    if (std::abs(lowEdge - edge) > epsilon && std::abs(upEdge - edge) > epsilon)
      return false;
  }

  return true;
}

template <typename THType>
inline std::unique_ptr<THType> RebinToTargetBinning(std::unique_ptr<THType> h, const std::vector<double>& targetBins, const std::string& errCtx)
{
  if (!IsRebinCompatible(h.get(), targetBins))
    throw std::runtime_error("[FATAL] " + errCtx + ": Histogram '" + std::string(h->GetName()) +
                             "' binning is not compatible with analysis binning (bin edges do not align).");

  int nTargetBins = static_cast<int>(targetBins.size()) - 1;
  return std::unique_ptr<THType>(static_cast<THType*>(h->Rebin(nTargetBins, (std::string(h->GetName()) + "_rebinned").c_str(), targetBins.data())));
}*/

// -----------------------------------------------------------------------------
// String formattig utility
// -----------------------------------------------------------------------------

inline std::pair<std::string, std::string> FormatDeltaY(double dyLimit)
{
  std::string title = Form("%.2f", dyLimit);
  std::string name = title;
  std::replace(name.begin(), name.end(), '.', '_');
  return {title, name};
}
} // namespace AnalysisUtils

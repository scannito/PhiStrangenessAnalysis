#pragma once

#include "AnalysisDataStructures.h"
#include "BinningUtils.h"

#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"
#include "TString.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <format>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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

// Constrains the helpers below to the ROOT histogram hierarchy. Without it the
// if-constexpr chain in ProjectTHnSparse has no else branch: an unrelated type
// would silently yield a null pointer instead of a diagnostic at the call site.
// With it, the three cases are exhaustive over every type that can get here.
template <typename T>
concept RootHistogram = std::derived_from<T, TH1>;

// Struct required by the projection utility
struct AxisToCut {
  int axis{0};
  BinningUtils::BinRange bins{}; // which bins of that axis to keep
};

// Projects a THnSparse into a lower-dimensional histogram
template <RootHistogram THType>
inline std::unique_ptr<THType> ProjectTHnSparse(THnSparse* hnSparse,
                                                const std::vector<AxisToCut>& axesToBeCut,
                                                const std::vector<int>& axesToProject,
                                                const std::string& histName)
{
  if (!hnSparse)
    return nullptr;

  for (const auto& axisToCut : axesToBeCut) {
    hnSparse->GetAxis(axisToCut.axis)->SetRange(axisToCut.bins.first, axisToCut.bins.last);
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
inline std::unique_ptr<TH1> ConstructSpectrum(std::span<TH1* const> hContainer,
                                              std::span<const double> binsVec,
                                              const std::string& histName,
                                              double absLimToIntegrate)
{
  // Checked before the subtraction below: size() is unsigned, so on an empty
  // span size() - 1 would wrap around instead of failing the comparison.
  if (binsVec.size() < 2) {
    throw std::runtime_error("ConstructSpectrum: the binning needs at least two edges");
  }
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

// Construct multiplicity trends from pT spectra
inline void ConstructMultTrend(TH1* hMultTrend,
                               const std::variant<TH1*, ExtrapolationResult>& source,
                               int i)
{
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

// -----------------------------------------------------------------------------
// Histogram ratios utility
// -----------------------------------------------------------------------------

// The caller styles the result itself (SetHistogramStyle, marker overrides),
// so this only builds the ratio.
//
// 'option' is TH1::Divide's: pass "B" whenever the numerator counts a subset of
// the denominator, as for an efficiency, otherwise the errors come out too large
// and can exceed the range the ratio is bounded to.
inline std::unique_ptr<TH1> MakeRatioHist(TH1* num, TH1* den, const std::string& name,
                                          const std::string& title,
                                          double numScale = 1.0, double denScale = 1.0,
                                          Option_t* option = "")
{
  std::unique_ptr<TH1> h(static_cast<TH1*>(num->Clone(name.c_str())));
  h->SetTitle(title.c_str());
  h->SetDirectory(0);
  h->Divide(num, den, numScale, denScale, option);
  return h;
}

// -----------------------------------------------------------------------------
// Rebinning utilities
// -----------------------------------------------------------------------------

struct RebinEdgeMismatch {
  double targetEdge{0.0};
  int sourceBin{-1};
  double sourceBinLowEdge{0.0};
  double sourceBinUpEdge{0.0};
  bool outOfSourceRange{false};
};

inline std::vector<RebinEdgeMismatch> FindRebinMismatches(const TH1* hSource, std::span<const double> targetBins, double epsilon = 1e-9)
{
  std::vector<RebinEdgeMismatch> mismatches;
  const double xMin = hSource->GetXaxis()->GetXmin();
  const double xMax = hSource->GetXaxis()->GetXmax();

  for (double edge : targetBins) {
    // Out of range check
    if (edge < xMin - epsilon || edge > xMax + epsilon) {
      mismatches.push_back({.targetEdge = edge, .sourceBin = -1, .sourceBinLowEdge = xMin,
                            .sourceBinUpEdge = xMax, .outOfSourceRange = true});
      continue;
    }

    int bin = hSource->GetXaxis()->FindFixBin(edge);
    bin = std::clamp(bin, 1, hSource->GetNbinsX()); // extra defense for edge cases, though FindFixBin should handle this

    double lowEdge = hSource->GetXaxis()->GetBinLowEdge(bin);
    double upEdge = hSource->GetXaxis()->GetBinLowEdge(bin + 1);
    if (std::abs(lowEdge - edge) > epsilon && std::abs(upEdge - edge) > epsilon)
      mismatches.push_back({.targetEdge = edge, .sourceBin = bin, .sourceBinLowEdge = lowEdge,
                            .sourceBinUpEdge = upEdge, .outOfSourceRange = false});
  }
  return mismatches;
}

template <RootHistogram THType>
inline std::unique_ptr<THType> RebinToTargetBinning(std::unique_ptr<THType> h, std::span<const double> targetBins, std::string_view errCtx)
{
  std::vector<RebinEdgeMismatch> mismatches = FindRebinMismatches(h.get(), targetBins);

  if (!mismatches.empty()) {
    std::string msg = std::format(
      "[FATAL] {}: histogram '{}' binning is not compatible with the target "
      "binning ({} edge(s) misaligned):\n",
      errCtx, h->GetName(), mismatches.size());
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

  const int nTargetBins = BinningUtils::NBins(targetBins);
  return std::unique_ptr<THType>(static_cast<THType*>(h->Rebin(nTargetBins, (std::string(h->GetName()) + "_rebinned").c_str(), targetBins.data())));
}

// -----------------------------------------------------------------------------
// String formatting utility
// -----------------------------------------------------------------------------

inline std::pair<std::string, std::string> FormatDeltaY(double dyLimit)
{
  std::string title = Form("%.2f", dyLimit);
  std::string name = title;
  std::replace(name.begin(), name.end(), '.', '_');
  return {title, name};
}
} // namespace AnalysisUtils

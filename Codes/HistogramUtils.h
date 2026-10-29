#pragma once

#include "AnalysisDataStructures.h"
#include "BinningUtils.h"

#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace HistogramUtils
{
// -----------------------------------------------------------------------------
// Math & Projection Utilities
// -----------------------------------------------------------------------------

// The content and error of a bin, read and written together, by its 0-based index (BinningUtils::RootBin).
inline std::pair<double, double> BinValueAndError(const TH1* h, int index)
{
  const int bin = BinningUtils::RootBin(index);
  return {h->GetBinContent(bin), h->GetBinError(bin)};
}
inline std::pair<double, double> BinValueAndError(const TH2* h, int indexX, int indexY)
{
  const int binX = BinningUtils::RootBin(indexX), binY = BinningUtils::RootBin(indexY);
  return {h->GetBinContent(binX, binY), h->GetBinError(binX, binY)};
}
inline void SetBinValueAndError(TH1* h, int index, std::pair<double, double> valueAndError)
{
  const int bin = BinningUtils::RootBin(index);
  h->SetBinContent(bin, valueAndError.first);
  h->SetBinError(bin, valueAndError.second);
}
inline void SetBinValueAndError(TH2* h, int indexX, int indexY, std::pair<double, double> valueAndError)
{
  const int binX = BinningUtils::RootBin(indexX), binY = BinningUtils::RootBin(indexY);
  h->SetBinContent(binX, binY, valueAndError.first);
  h->SetBinError(binX, binY, valueAndError.second);
}

// The integral of a histogram of counts over [x1, x2] - the sum of the contents, as TH1::IntegralAndError with option
// "" (not "width") - a bin cut by an end counted for the fraction of its width inside, as if its entries were spread
// evenly across it; its variance scaled by the square of that fraction, as TH1::Scale does. For a range that does not
// come from the binning.
inline std::pair<double, double> IntegralAndErrorFractional(const TH1* h, double x1, double x2)
{
  constexpr double tolerance = BinningUtils::kEdgeTolerance;
  const TAxis* axis = h->GetXaxis();
  // Under- and overflow have no width to take a fraction of: a range beyond the axis would silently lose its part.
  if (!(x1 < x2) || x1 < axis->GetXmin() - tolerance || x2 > axis->GetXmax() + tolerance)
    throw std::runtime_error(std::format("[FATAL] IntegralAndErrorFractional: [{}, {}] is not a range inside the axis [{}, {}] of '{}'.",
                                         BinningUtils::FormatEdge(x1), BinningUtils::FormatEdge(x2),
                                         BinningUtils::FormatEdge(axis->GetXmin()), BinningUtils::FormatEdge(axis->GetXmax()),
                                         h->GetName()));
  double integral = 0.0, variance = 0.0;
  for (int bin = 1; bin <= h->GetNbinsX(); ++bin) {
    const double width = axis->GetBinWidth(bin);
    const double overlap = std::min(axis->GetBinUpEdge(bin), x2) - std::max(axis->GetBinLowEdge(bin), x1);
    if (overlap <= tolerance)
      continue;
    // A bin inside the range counts exactly once: its edges are computed, and 1 - 1e-13 would move the sum.
    const double fraction = overlap >= width - tolerance ? 1.0 : overlap / width;
    integral += fraction * h->GetBinContent(bin);
    variance += fraction * fraction * h->GetBinError(bin) * h->GetBinError(bin);
  }
  return {integral, std::sqrt(variance)};
}

// The integral and error over [x1, x2] in whole bins, as a pair: TH1::IntegralAndError and its error by reference stay
// here, out of the code that uses the result. x1 and x2 must be bin edges: an end inside a bin would take that bin
// whole, a wider range than asked for, with no word. A range that does not come from the binning goes through
// IntegralAndErrorFractional. 'option' is TH1::IntegralAndError's ("width" for a density).
inline std::pair<double, double> IntegralAndErrorWholeBins(const TH1* h, double x1, double x2, Option_t* option = "")
{
  const std::vector<double> edges = BinningUtils::AxisEdges(h->GetXaxis());
  const auto low = BinningUtils::FindEdgeIndex(edges, x1), up = BinningUtils::FindEdgeIndex(edges, x2);
  if (!low || !up)
    throw std::runtime_error(std::format(
      "[FATAL] HistogramUtils::IntegralAndErrorWholeBins of '{}': the range [{}, {}] must start and end on bin edges of "
      "the axis [{}, {}]{}{}. A range that does not come from the binning goes through IntegralAndErrorFractional.",
      h->GetName(), BinningUtils::FormatEdge(x1), BinningUtils::FormatEdge(x2), BinningUtils::FormatEdge(edges.front()),
      BinningUtils::FormatEdge(edges.back()), low ? "" : std::format("; {} is not one", BinningUtils::FormatEdge(x1)),
      up ? "" : std::format("; {} is not one", BinningUtils::FormatEdge(x2))));
  // An empty range must not reach ROOT: TH1::IntegralAndError with the first bin past the last takes up to the overflow.
  if (!(*low < *up))
    throw std::runtime_error(std::format("[FATAL] HistogramUtils::IntegralAndErrorWholeBins of '{}': [{}, {}] is an empty range.",
                                         h->GetName(), BinningUtils::FormatEdge(x1), BinningUtils::FormatEdge(x2)));
  double error{0.0};
  const double integral = h->IntegralAndError(BinningUtils::RootBin(*low), BinningUtils::RootBin(*up - 1), error, option);
  return {integral, error};
}

// The integral and error of a TH3 over ranges of bins, as a pair, for a region already given in ROOT bins
// (BinRange::Single, an axis' 1..N).
inline std::pair<double, double> IntegralAndErrorInBins(const TH3* h, BinningUtils::BinRange x, BinningUtils::BinRange y,
                                                         BinningUtils::BinRange z)
{
  double error{0.0};
  const double integral = h->IntegralAndError(x.first, x.last, y.first, y.last, z.first, z.last, error);
  return {integral, error};
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
  // Which bins of that axis to keep, ROOT numbering. None: the whole axis, under- and overflow excluded, of the
  // container being projected - resolved there, so that it cannot come from another container.
  std::optional<BinningUtils::BinRange> bins;

  // One bin of a binning, by its 0-based index.
  static AxisToCut Bin(int axis, int index) { return {axis, BinningUtils::BinRange::Single(index)}; }
  static AxisToCut Whole(int axis) { return {axis, std::nullopt}; }
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
    TAxis* axis = hnSparse->GetAxis(axisToCut.axis);
    const BinningUtils::BinRange bins = axisToCut.bins.value_or(BinningUtils::BinRange{1, axis->GetNbins()});
    axis->SetRange(bins.first, bins.last);
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
                                              std::span<const double> binning,
                                              const std::string& histName,
                                              double absLimToIntegrate)
{
  if (binning.size() < 2) {
    throw std::runtime_error("[FATAL] ConstructSpectrum: the binning needs at least two edges");
  }
  if (static_cast<size_t>(BinningUtils::NBins(binning)) != hContainer.size()) {
    throw std::runtime_error("[FATAL] ConstructSpectrum: the number of histograms must be the number of bins, edges - 1");
  }

  std::unique_ptr<TH1> hSpectrum = std::make_unique<TH1D>(histName.c_str(), "; p_{T} (GeV/#it{c}); 1/N_{trig} d^{2}N/dp_{T}d#Deltay", BinningUtils::NBins(binning), binning.data());

  for (int ptBin : BinningUtils::BinIndices(binning)) {
    auto [binContent, binError] = IntegralAndErrorWholeBins(hContainer[ptBin], -absLimToIntegrate, absLimToIntegrate);
    double normalizationFactor = (binning[ptBin + 1] - binning[ptBin]);

    SetBinValueAndError(hSpectrum.get(), ptBin, {binContent / normalizationFactor, binError / normalizationFactor});
  }

  hSpectrum->SetDirectory(0);

  return hSpectrum;
}

// The measured trend: the bin of class 'multBin' gets the integral of the spectrum.
inline void FillTrendFromSpectrum(TH1* hTrend, TH1* hSpectrum, int multBin)
{
  auto [content, error] = IntegralAndErrorWholeBins(hSpectrum, hSpectrum->GetXaxis()->GetXmin(),
                                                    hSpectrum->GetXaxis()->GetXmax(), "width");

  SetBinValueAndError(hTrend, multBin, {content, error});
}

// The extrapolated trend: the bin of class 'multBin' gets res.yield (README.md, The extrapolated yield) and
// the toy error, which is the uncertainty on the TOTAL: the toys sample the measured and
// the extrapolated parts together, with their correlation, so an error taken from the
// histogram plus something for the model would either double count or throw that
// correlation away.
inline void FillTrendFromExtrapolation(TH1* hTrend, const ExtrapolationResult& res, int multBin)
{
  SetBinValueAndError(hTrend, multBin, {res.yield, res.yieldStatErr});
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

inline std::vector<RebinEdgeMismatch> FindRebinMismatches(const TH1* hSource, std::span<const double> targetBinning,
                                                         double epsilon = BinningUtils::kEdgeTolerance)
{
  std::vector<RebinEdgeMismatch> mismatches;
  const std::vector<double> edges = BinningUtils::AxisEdges(hSource->GetXaxis());

  for (double edge : targetBinning) {
    if (BinningUtils::FindEdgeIndex(edges, edge, epsilon))
      continue;
    if (edge < edges.front() || edge > edges.back()) {
      mismatches.push_back({.targetEdge = edge, .sourceBin = -1, .sourceBinLowEdge = edges.front(),
                            .sourceBinUpEdge = edges.back(), .outOfSourceRange = true});
      continue;
    }
    const int bin = hSource->GetXaxis()->FindFixBin(edge);
    mismatches.push_back({.targetEdge = edge, .sourceBin = bin, .sourceBinLowEdge = hSource->GetXaxis()->GetBinLowEdge(bin),
                          .sourceBinUpEdge = hSource->GetXaxis()->GetBinUpEdge(bin), .outOfSourceRange = false});
  }
  return mismatches;
}

// The target binning may cover less than the source: an efficiency computed up to
// 10 GeV/c merged onto an analysis binning that stops at 6.4 is the normal case.
// What happens to the rest is worth knowing and was measured, not assumed:
//
//   100 bins of content 1 over [0, 10], rebinned onto {0, 2, 4, 6}
//   -> visible 60, overflow 40, nothing dropped
//
// So the counts above the target range accumulate in the overflow bin. The visible
// bins are exact either way, and nothing in this framework reads the overflow -
// IntegralAndErrorWholeBins works on explicit limits and TH1::Integral() starts at bin
// 1 - but GetEntries() and the statistics box do count it.
template <RootHistogram THType>
inline std::unique_ptr<THType> RebinToTargetBinning(std::unique_ptr<THType> h, std::span<const double> targetBinning, std::string_view errCtx)
{
  std::vector<RebinEdgeMismatch> mismatches = FindRebinMismatches(h.get(), targetBinning);

  if (!mismatches.empty()) {
    std::string msg = std::format(
      "[FATAL] {}: histogram '{}' binning is not compatible with the target "
      "binning ({} edge(s) misaligned):\n",
      errCtx, h->GetName(), mismatches.size());
    for (const auto& m : mismatches) {
      if (m.outOfSourceRange) {
        msg += std::format("  - target edge {} is outside the source histogram's axis range [{}, {}]\n",
                           BinningUtils::FormatEdge(m.targetEdge), BinningUtils::FormatEdge(m.sourceBinLowEdge),
                           BinningUtils::FormatEdge(m.sourceBinUpEdge));
      } else {
        msg += std::format("  - target edge {} falls inside source bin {} [{}, {}] instead of aligning with a bin boundary\n",
                           BinningUtils::FormatEdge(m.targetEdge), m.sourceBin,
                           BinningUtils::FormatEdge(m.sourceBinLowEdge), BinningUtils::FormatEdge(m.sourceBinUpEdge));
      }
    }
    throw std::runtime_error(msg);
  }

  const int nTargetBins = BinningUtils::NBins(targetBinning);
  return std::unique_ptr<THType>(static_cast<THType*>(h->Rebin(nTargetBins, (std::string(h->GetName()) + "_rebinned").c_str(), targetBinning.data())));
}

} // namespace HistogramUtils

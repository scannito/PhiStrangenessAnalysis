#pragma once

// Reading and comparing bin edges. Depends on TAxis and nothing else - no
// framework header - so the standalone diagnostic macros can share it while
// still being able to run on files the analysis chain would refuse.

#include "TAxis.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace BinningUtils
{

// Two values closer than this are the same edge: the tolerance of every "is x an edge" of the framework (Compare,
// FindEdgeIndex, MapToSourceBins, the integrals and the rebinning of HistogramUtils). Absolute: the edges are pT in GeV/c
// (0-20), multiplicity percentiles (0-100), Delta y (-1..1) and masses in GeV/c^2 (~1), so 1e-9 sits far below any
// difference that could be physical and far above double rounding.
inline constexpr double kEdgeTolerance = 1e-9;

// GetBinLowEdge is used on purpose: GetXbins() returns an empty array for
// fixed-width axes, which would look like "this axis has no binning at all".
// GetXmax() closes the sequence and is by definition the upper edge of the
// last bin, for both fixed and variable binning.
inline std::vector<double> AxisEdges(const TAxis* axis)
{
  if (!axis)
    throw std::runtime_error("[FATAL] BinningUtils::AxisEdges: null axis");

  const int n = axis->GetNbins();
  std::vector<double> edges;
  edges.reserve(n + 1);
  for (int bin = 1; bin <= n; ++bin)
    edges.push_back(axis->GetBinLowEdge(bin));
  edges.push_back(axis->GetXmax());

  return edges;
}

// Formats a bin edge compactly: 6.4 stays "6.4", 7.0 becomes "7".
//
// std::format("{:g}") would be the natural choice, but its consteval validation
// of the format string cannot be evaluated by Cling: for floating-point specs
// libc++ reaches __builtin_clzg through the unicode handling, and the macro
// fails to load with "call to consteval function is not a constant expression".
// Plain "{}" placeholders are fine - it is only the float spec that breaks.
inline std::string FormatEdge(double value)
{
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return buffer;
}

// A bin named by its interval, "10-15", never by its index. An object that crosses a task boundary is
// found by name, and an index matches whatever binning the reader has: "_multBin3" exists on both sides
// and means two intervals. "_mult10-15" is simply absent when the two disagree, so the mismatch is a
// missing object instead of a wrong number (CLAUDE.md, the rule the framework is built on).
// 'index' is 0-based, the interval edges[index]..edges[index + 1]; its ROOT bin is RootBin(index).
inline std::string BinLabel(std::span<const double> edges, int index)
{
  return FormatEdge(edges[index]) + "-" + FormatEdge(edges[index + 1]);
}

// A delta-y limit as written in titles and messages, to two decimals ("0.80"). The form used
// in object names is OutputNames::DyTag.
inline std::string FormatDeltaY(double dyLimit)
{
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2f", dyLimit);
  return buffer;
}

// Number of bins described by a sequence of edges. Provided so that the count
// is never stored alongside the edges: a cached size is state that can go stale,
// and this is a subtraction.
inline int NBins(std::span<const double> edges)
{
  return edges.empty() ? 0 : static_cast<int>(edges.size()) - 1;
}

// The bin indices of a binning, for a loop that names its index once:
// for (int multBin : BinningUtils::BinIndices(multBinning)). 0-based, as the edges are indexed; the ROOT bin is
// RootBin(index). The first std::views of the framework: it is not known whether the interpreted mode
// accepts it (that mode does not start in this environment, CLAUDE.md), and behind one function only this body would
// have to change if it does not.
inline auto BinIndices(std::span<const double> edges)
{
  return std::views::iota(0, NBins(edges));
}

// The ROOT bin of a 0-based bin index (BinIndices, the multBin/ptBin of the loops): the one place the + 1 is written.
// Every helper that takes an index goes through it, and so does a ROOT call given one.
inline int RootBin(int index)
{
  return index + 1;
}

// Returns an empty string when the two binnings agree within epsilon, otherwise
// a report listing EVERY difference. Reporting all of them at once matters:
// a stale configuration or a wrong production usually differs in several places,
// and fixing them one run at a time is expensive.
//
// The tolerance is absolute, kEdgeTolerance below.
inline std::string Compare(std::span<const double> lhs, std::span<const double> rhs,
                           std::string_view lhsName = "expected", std::string_view rhsName = "found",
                           double epsilon = kEdgeTolerance)
{
  std::string report;

  if (lhs.size() != rhs.size()) {
    report += std::format("  - number of bins: {} has {}, {} has {}\n",
                          lhsName, NBins(lhs), rhsName, NBins(rhs));
  }

  // The common part is compared even when the sizes differ, so the report shows
  // WHERE the two sequences start to diverge and not merely that they do.
  const size_t n = std::min(lhs.size(), rhs.size());
  for (size_t i = 0; i < n; ++i) {
    if (std::abs(lhs[i] - rhs[i]) > epsilon)
      // report += std::format("  - edge {}: {} = {:g}, {} = {:g}\n", i, lhsName, lhs[i], rhsName, rhs[i]);
      report += std::format("  - edge {}: {} = {}, {} = {}\n", i, lhsName, FormatEdge(lhs[i]),
                            rhsName, FormatEdge(rhs[i]));
  }

  return report;
}

// The index 0..N of the edge of 'edges' at x, within epsilon, or none: the one test of "x is an edge" (a merge of bins,
// a whole-bin integral, a rebinning).
inline std::optional<int> FindEdgeIndex(std::span<const double> edges, double x, double epsilon = kEdgeTolerance)
{
  const auto above = std::ranges::lower_bound(edges, x - epsilon);
  if (above != edges.end() && std::abs(*above - x) <= epsilon)
    return static_cast<int>(above - edges.begin());
  return std::nullopt;
}

// The same, where x must be an edge: [FATAL] naming what asked and the edges around x.
inline int EdgeIndex(std::span<const double> edges, double x, std::string_view what, double epsilon = kEdgeTolerance)
{
  if (const auto index = FindEdgeIndex(edges, x, epsilon))
    return *index;
  const auto above = std::ranges::lower_bound(edges, x);
  throw std::runtime_error(std::format("[FATAL] {}: {} is not an edge of the binning [{}, {}] (it lies between {} and {}).",
                                       what, FormatEdge(x), FormatEdge(edges.front()), FormatEdge(edges.back()),
                                       above == edges.begin() ? "-" : FormatEdge(*(above - 1)),
                                       above == edges.end() ? "-" : FormatEdge(*above)));
}

// A range of bins in ROOT numbering (1-based, both ends inclusive), as ProjectionZ, IntegralAndError and
// THnSparse::SetRange expect them: one bin (Single), or the source bins one target bin covers (MapToSourceBins).
struct BinRange {
  int first{0};
  int last{0};

  // One bin, by its 0-based index.
  static BinRange Single(int index) { return {RootBin(index), RootBin(index)}; }
};

// Maps each target bin onto the source bins it covers. Requires the target edges
// to be a subset of the source ones - which is what makes a merge well defined.
//
// Used to merge BEFORE deriving a quantity rather than after: a coarse purity or
// efficiency must come from the merged counts, never from the merged ratios.
inline std::vector<BinRange> MapToSourceBins(std::span<const double> source, std::span<const double> target,
                                             double epsilon = kEdgeTolerance)
{
  std::vector<BinRange> ranges;
  ranges.reserve(NBins(target));

  const std::string_view what = "BinningUtils::MapToSourceBins (a target edge must be a source edge for the merge to be defined)";
  for (int t : BinIndices(target)) {
    const int low = EdgeIndex(source, target[t], what, epsilon);
    const int up = EdgeIndex(source, target[t + 1], what, epsilon);
    ranges.push_back({RootBin(low), RootBin(up - 1)}); // the source bins between edges low and up
  }

  return ranges;
}

// Two containers that are combined bin by bin - divided, multiplied, subtracted -
// must share the axis. This holds for physical reasons and does not depend on
// anything being declared in the configuration, so it is checked directly
// between the two axes rather than against a reference.
inline void RequireSameAxis(const TAxis* lhs, const TAxis* rhs,
                            std::string_view lhsName, std::string_view rhsName)
{
  const std::string diff = Compare(AxisEdges(lhs), AxisEdges(rhs), lhsName, rhsName);
  if (!diff.empty()) {
    throw std::runtime_error(std::format(
      "[FATAL] Incompatible axes between '{}' and '{}':\n{}"
      "These containers are combined bin by bin, so their axes must be identical.",
      lhsName, rhsName, diff));
  }
}

} // namespace BinningUtils

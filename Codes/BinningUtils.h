#pragma once

// Reading and comparing bin edges. Depends on TAxis and nothing else - no
// framework header - so the standalone diagnostic macros can share it while
// still being able to run on files the analysis chain would refuse.

#include "TAxis.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace BinningUtils
{

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
  for (int i = 1; i <= n; ++i)
    edges.push_back(axis->GetBinLowEdge(i));
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

// Number of bins described by a sequence of edges. Provided so that the count
// is never stored alongside the edges: a cached size is state that can go stale,
// and this is a subtraction.
inline int NBins(std::span<const double> edges)
{
  return edges.empty() ? 0 : static_cast<int>(edges.size()) - 1;
}

// Returns an empty string when the two binnings agree within epsilon, otherwise
// a report listing EVERY difference. Reporting all of them at once matters:
// a stale configuration or a wrong production usually differs in several places,
// and fixing them one run at a time is expensive.
//
// The tolerance is absolute. Bin edges here are pT in GeV/c (0-20) or
// multiplicity percentiles (0-100), so 1e-9 sits far below any difference that
// could be physical and far above double rounding.
inline std::string Compare(std::span<const double> lhs, std::span<const double> rhs,
                           std::string_view lhsName = "expected", std::string_view rhsName = "found",
                           double epsilon = 1e-9)
{
  std::string report;

  if (lhs.size() != rhs.size()) {
    report += std::format("  - number of bins: {} has {}, {} has {}\n",
                          lhsName, lhs.empty() ? 0 : lhs.size() - 1,
                          rhsName, rhs.empty() ? 0 : rhs.size() - 1);
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

// Two containers that are combined bin by bin - divided, multiplied, subtracted -
// must share the axis. This holds for physical reasons and does not depend on
// anything being declared in the configuration, so it is checked directly
// between the two axes rather than against a reference.
inline void RequireSameAxis(const TAxis* lhs, const TAxis* rhs,
                            const std::string& lhsName, const std::string& rhsName)
{
  const std::string diff = Compare(AxisEdges(lhs), AxisEdges(rhs), lhsName, rhsName);
  if (!diff.empty()) {
    throw std::runtime_error("[FATAL] Incompatible axes between '" + lhsName + "' and '" + rhsName + "':\n" + diff +
                             "These containers are combined bin by bin, so their axes must be identical.");
  }
}

} // namespace BinningUtils

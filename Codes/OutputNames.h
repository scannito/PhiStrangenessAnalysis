#pragma once

#include "BinningUtils.h"

#include "TDirectory.h"

#include <format>
#include <string>
#include <string_view>

// The names and the layout of what the correlation tasks write into PhiAssocSpectra.root,
// in one place for the writer (CorrelationTaskBase) and for every reader (ComparisonTask).
//
// Built by hand on both sides, a rename on one would leave the other looking for objects that
// no longer exist. The names shared by every file the framework writes - the binning stamps and
// the provenance directory - are RootIO's, next to the functions that write them.
//
// What an object name does NOT carry is the meaning of its multiplicity index: "multBin3"
// is the fourth interval of the binning_mult stamp of its scheme, and only the stamp says
// which interval that is (CLAUDE.md, the rule the framework is built on).
namespace OutputNames
{

// The directories. A scheme (the binning_name) holds one section per engine, each section
// one directory per associated species and one for the cross-species ratios.
inline const std::string kExtract1D{"Extract1D"};
inline const std::string kExtract2D{"Extract2D"};
inline const std::string kSpectraDir{"Spectra"};
inline const std::string kTrendsDir{"Trends"};
inline const std::string kRatiosDir{"Ratios"};

inline std::string SectionName(bool engine2D) { return engine2D ? kExtract2D : kExtract1D; }

// A species directory of a section is one that holds per-species results, which is what
// tells it from the Ratios sibling. Recognised by what it contains and not by excluding the
// names of everything it is not: that exclusion would need a new entry the first time
// another sibling appears (systematics, QA), and whoever added it would have no reason to
// look here.
inline bool IsSpeciesDir(TDirectory* dir)
{
  return dir && (dir->GetDirectory(kTrendsDir.c_str()) || dir->GetDirectory(kSpectraDir.c_str()));
}

// "1.00" -> "1_00": the delta-y limit as it appears in a name.
inline std::string DyTag(double dyLimit) { return BinningUtils::FormatDeltaY(dyLimit).second; }

// ---------------------------------------------------------------------------
// Builders, one per kind of object
// ---------------------------------------------------------------------------
inline std::string Spectrum(std::string_view species, double dy, int multBin)
{
  return std::format("h1SpectrumPhi{}_dy{}_multBin{}", species, DyTag(dy), multBin);
}
// The spectrum with the extrapolated bin down to pT = 0 in front of the measured ones.
inline std::string SpectrumExtended(std::string_view spectrumName) { return std::format("{}_extended", spectrumName); }
inline std::string Trend(std::string_view species, double dy) { return std::format("h1MultTrend_{}_dy{}", species, DyTag(dy)); }
inline std::string TrendExtrapolated(std::string_view species, double dy) { return std::format("h1MultTrendExtrap_{}_dy{}", species, DyTag(dy)); }
inline std::string TrendExtrapOverMeas(std::string_view species, double dy) { return std::format("Ratio_Extrap_Meas_{}_dy{}", species, DyTag(dy)); }
inline std::string SpeciesRatio(std::string_view num, std::string_view den, bool extrapolated, double dy)
{
  return std::format("Ratio_{}_{}_{}_dy{}", num, den, extrapolated ? "Extrap" : "Meas", DyTag(dy));
}

} // namespace OutputNames

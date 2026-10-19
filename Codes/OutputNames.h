#pragma once

#include "BinningUtils.h"

#include "TDirectory.h"

#include <array>
#include <cstdio>
#include <format>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

// The names and the layout of what the correlation tasks write into PhiAssocSpectra.root,
// in one place for the writer (CorrelationTaskBase) and for every reader that has to find an
// object by what it means instead of by a name copied from a file (the plotting tool).
//
// Before this header the names were built by hand where each object was written, and a
// reader had to spell them again: a rename on one side would have left the other looking
// for objects that no longer exist. Parse() is the inverse of the builders, and it accepts a
// name only if building it back gives the same string, so a builder that changes and a
// pattern that does not cannot agree on a wrong reading: the name is reported as not
// recognised instead.
//
// What an object name does NOT carry is the meaning of its multiplicity index: "multBin3"
// is the fourth interval of the binning_mult stamp of its scheme, and only the stamp says
// which interval that is (CLAUDE.md, the rule the framework is built on).
namespace OutputNames
{

// The directories. A scheme (the binning_name) holds one section per engine, each section
// one directory per associated species and one for the cross-species ratios.
inline constexpr std::string_view kExtract1D = "Extract1D";
inline constexpr std::string_view kExtract2D = "Extract2D";
inline constexpr std::string_view kSpectraDir = "Spectra";
inline constexpr std::string_view kTrendsDir = "Trends";
inline constexpr std::string_view kRatiosDir = "Ratios";
inline constexpr std::string_view kProvenanceDir = "Provenance";

// The binning stamps (RootIO::WriteBinningStamp): mult and ptPhi in the scheme directory,
// ptAssoc in each species directory.
inline constexpr std::string_view kStampMult = "binning_mult";
inline constexpr std::string_view kStampPtPhi = "binning_ptPhi";
inline constexpr std::string_view kStampPtAssoc = "binning_ptAssoc";

inline std::string SectionName(bool engine2D) { return std::string(engine2D ? kExtract2D : kExtract1D); }

// A species directory of a section is one that holds per-species results, which is what
// tells it from the Ratios sibling. Recognised by what it contains and not by excluding the
// names of everything it is not: that exclusion would need a new entry the first time
// another sibling appears (systematics, QA), and whoever added it would have no reason to
// look here. (Moved from ComparisonTask::ListParticles, which now asks this.)
inline bool IsSpeciesDir(TDirectory* dir)
{
  return dir && (dir->GetDirectory(std::string(kTrendsDir).c_str()) || dir->GetDirectory(std::string(kSpectraDir).c_str()));
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

// ---------------------------------------------------------------------------
// What an object is, by meaning
// ---------------------------------------------------------------------------
enum class Kind { Spectrum,
                  SpectrumExtended,
                  Trend,
                  TrendExtrapolated,
                  TrendExtrapOverMeas,
                  SpeciesRatioMeasured,
                  SpeciesRatioExtrapolated };

// One row per kind: its name in a plot specification and in a listing, and what it needs.
// The table is what the specification is checked against and what its schema is built
// from (PlotSpec), so the three cannot list different kinds.
struct KindInfo {
  Kind kind;
  std::string_view name;
  bool perMultBin;      // one object per multiplicity interval: the X axis is the associated pT
  bool speciesPair;     // a ratio of two species, filed under Ratios
  std::string_view what; // for listings and for the plain-words description
};

inline constexpr std::array<KindInfo, 7> kKinds{{
  {Kind::Spectrum, "spectrum", true, false, "pT spectrum"},
  {Kind::SpectrumExtended, "spectrum_extended", true, false, "pT spectrum with the extrapolated bin down to 0"},
  {Kind::Trend, "trend", false, false, "measured yield against multiplicity"},
  {Kind::TrendExtrapolated, "trend_extrapolated", false, false, "extrapolated yield against multiplicity"},
  {Kind::TrendExtrapOverMeas, "trend_extrap_over_meas", false, false, "extrapolated over measured yield"},
  {Kind::SpeciesRatioMeasured, "species_ratio_measured", false, true, "ratio of two species, measured yields"},
  {Kind::SpeciesRatioExtrapolated, "species_ratio_extrapolated", false, true, "ratio of two species, extrapolated yields"},
}};

inline std::optional<Kind> KindByName(std::string_view name)
{
  for (const KindInfo& k : kKinds)
    if (k.name == name)
      return k.kind;
  return std::nullopt;
}

inline const KindInfo& Info(Kind kind)
{
  for (const KindInfo& k : kKinds)
    if (k.kind == kind)
      return k;
  throw std::runtime_error("[FATAL] OutputNames::Info: a Kind missing from kKinds");
}

struct ObjectId {
  Kind kind{Kind::Trend};
  std::string species;    // the numerator for a species ratio
  std::string speciesDen; // species ratios only
  double dy{0.0};
  int multBin{-1}; // per-multiplicity kinds only: an index into the binning_mult stamp
};

inline std::string Name(const ObjectId& id)
{
  switch (id.kind) {
    case Kind::Spectrum:
      return Spectrum(id.species, id.dy, id.multBin);
    case Kind::SpectrumExtended:
      return SpectrumExtended(Spectrum(id.species, id.dy, id.multBin));
    case Kind::Trend:
      return Trend(id.species, id.dy);
    case Kind::TrendExtrapolated:
      return TrendExtrapolated(id.species, id.dy);
    case Kind::TrendExtrapOverMeas:
      return TrendExtrapOverMeas(id.species, id.dy);
    case Kind::SpeciesRatioMeasured:
      return SpeciesRatio(id.species, id.speciesDen, false, id.dy);
    case Kind::SpeciesRatioExtrapolated:
      return SpeciesRatio(id.species, id.speciesDen, true, id.dy);
  }
  throw std::runtime_error("[FATAL] OutputNames::Name: unknown Kind");
}

// Where the object lives, below the section directory.
inline std::vector<std::string> Location(const ObjectId& id)
{
  const KindInfo& info = Info(id.kind);
  if (info.speciesPair)
    return {std::string(kRatiosDir)};
  return {id.species, std::string(info.perMultBin ? kSpectraDir : kTrendsDir)};
}

// The inverse of Name(). A species name is taken as letters and digits (K0S, Xi, Pi); the
// round trip below is what makes that assumption safe: a name that only looks right does not
// come back equal and is not recognised.
inline std::optional<ObjectId> Parse(const std::string& name)
{
  static const std::string sp = "([A-Za-z0-9]+)";
  static const std::string dy = "_dy([0-9]+)_([0-9]+)";
  static const std::regex reSpectrum("h1SpectrumPhi" + sp + dy + "_multBin([0-9]+)(_extended)?");
  static const std::regex reTrend("h1MultTrend(Extrap)?_" + sp + dy);
  static const std::regex reExtrapMeas("Ratio_Extrap_Meas_" + sp + dy);
  static const std::regex reSpeciesRatio("Ratio_" + sp + "_" + sp + "_(Meas|Extrap)" + dy);

  const auto toDy = [](const std::string& intPart, const std::string& fracPart) { return std::stod(intPart + "." + fracPart); };

  ObjectId id;
  std::smatch m;
  if (std::regex_match(name, m, reSpectrum)) {
    id.kind = m[5].matched ? Kind::SpectrumExtended : Kind::Spectrum;
    id.species = m[1];
    id.dy = toDy(m[2], m[3]);
    id.multBin = std::stoi(m[4]);
  } else if (std::regex_match(name, m, reTrend)) {
    id.kind = m[1].matched ? Kind::TrendExtrapolated : Kind::Trend;
    id.species = m[2];
    id.dy = toDy(m[3], m[4]);
  } else if (std::regex_match(name, m, reExtrapMeas)) {
    id.kind = Kind::TrendExtrapOverMeas;
    id.species = m[1];
    id.dy = toDy(m[2], m[3]);
  } else if (std::regex_match(name, m, reSpeciesRatio)) {
    id.kind = m[3] == "Extrap" ? Kind::SpeciesRatioExtrapolated : Kind::SpeciesRatioMeasured;
    id.species = m[1];
    id.speciesDen = m[2];
    id.dy = toDy(m[4], m[5]);
  } else {
    return std::nullopt;
  }

  if (Name(id) != name)
    return std::nullopt;
  return id;
}

} // namespace OutputNames

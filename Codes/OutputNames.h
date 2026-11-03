#pragma once

#include "BinningUtils.h"

#include "TDirectory.h"

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The names and the layout of what the correlation tasks write into PhiAssocSpectra.root, the
// names of the files that more than one place in the code spells (the trigger matrices, the
// purities, the projections, the results), and of the directories and objects in them that another
// task looks up (the corrections, the trigger matrices, the purity spectra), with the cell tags they
// are built from and the title form of a cell (CellTitle). The directories of PhiAssocSpectra.root
// are written by CorrelationTaskBase and read by ComparisonTask; the object names are built here and
// not where each object is written, so that a reader that looks an object up by what it is builds
// the same string, and a rename is one change. The names shared by every file the framework writes
// - the binning stamps and the provenance directory - and the object of a CCDB file are RootIO's,
// next to the functions that write them.
//
// A multiplicity class is named by its interval ("mult10-15", BinningUtils::BinLabel), never by
// its index: two productions with different binnings then miss each other's objects instead of
// matching on an index that means two intervals (CLAUDE.md, the rule the framework is built on).
namespace OutputNames
{

// The directories. A scheme (the binning_name) holds one section per engine, each section
// one directory per associated species and one for the cross-species ratios.
inline const std::string kExtract1D{"Extract1D"};
inline const std::string kExtract2D{"Extract2D"};
inline const std::string kSpectraDir{"Spectra"};
inline const std::string kTrendsDir{"Trends"};
inline const std::string kRatiosDir{"Ratios"};
// At the root of a section, next to the species: the relative uncertainty of the per-trigger normalisation, per
// multiplicity class (CorrelationTaskBase::GenerateSpectraAndTrends).
inline const std::string kNormalisationRelError{"h1NormalisationRelError"};

// The files between the fit tasks and CorrelationTask: the name that follows the output prefix.
// Written once here, since the writer and the reader would otherwise each spell it, and a
// configuration key for it would be one more copy to keep equal on both sides (it was one, for
// the purities, until 21 October 2026).
inline const std::string kPhiDataFile{"PhiDataHistograms.root"}; // PhiFitTask -> CorrelationTask
inline const std::string kPurityFile{"Purity.root"};             // PurityTask -> CorrelationTask, every particle
// The directory of each, below the binning_name, that holds what the other task reads.
inline std::vector<std::string> SummaryPath(const std::string& binningName) { return {binningName, "Summary"}; }
// Written by both correlation tasks, CorrelationTask and CorrelationWPDGTask: the results, and the
// projections of one associated particle, which a later run reads back (use_projection_cache).
inline const std::string kSpectraFile{"PhiAssocSpectra.root"};
inline std::string ProjectionFile(std::string_view particle) { return std::format("Phi{}DataHistograms.root", particle); }


// A result kept also at the binning of its source, when 'rebinning_pt' merged the analysis one (MCTask,
// PurityTask): in a subdirectory of its own, and its canvas named with a suffix.
enum class SpectrumBinning { Analysis,
                             Source };
inline std::string_view BinningSuffix(SpectrumBinning binning) { return binning == SpectrumBinning::Source ? "_sourceBinning" : ""; }
inline std::vector<std::string> SourceBinningPath(std::vector<std::string> path)
{
  path.push_back("SourceBinning");
  return path;
}

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
inline std::string DyTag(double dyLimit)
{
  std::string tag = BinningUtils::FormatDeltaY(dyLimit);
  std::replace(tag.begin(), tag.end(), '.', '_');
  return tag;
}

// ---------------------------------------------------------------------------
// Cells, by interval
// ---------------------------------------------------------------------------
// A multiplicity class, "mult10-15": in object names and as the key of an extrapConfig.json override.
inline std::string MultTag(std::span<const double> multBinning, int multBin)
{
  return std::format("mult{}", BinningUtils::BinLabel(multBinning, multBin));
}
// The tag of what is integrated over every class, where a MultTag would stand.
inline const std::string kMultIntegrated{"multIntegrated"};
// A pT interval, "pt1.2-1.6"; the trigger's is "ptPhi" + BinLabel, where both appear in one name.
inline std::string PtTag(std::span<const double> ptBinning, int ptBin)
{
  return std::format("pt{}", BinningUtils::BinLabel(ptBinning, ptBin));
}
// A (multiplicity, pT) cell, "mult10-15_pt1.2-1.6": the fit projections and canvases, and the key of a
// fitConfig.json override, which is therefore the suffix of the canvas of the fit it acts on.
inline std::string CellTag(std::span<const double> multBinning, int multBin, std::span<const double> ptBinning, int ptBin)
{
  return std::format("{}_{}", MultTag(multBinning, multBin), PtTag(ptBinning, ptBin));
}

// The same cell as a title, for a reader: "K0S, mult 10-15%, p_{T} 1.2-1.6 GeV/#it{c}" (TLatex). Not a name: it is
// here because it says the cell with the same intervals as CellTag.
inline std::string CellTitle(std::string_view particle, std::span<const double> multBinning, int multBin,
                             std::span<const double> ptBinning, int ptBin)
{
  return std::format("{}, mult {}%, p_{{T}} {} GeV/#it{{c}}", particle, BinningUtils::BinLabel(multBinning, multBin),
                     BinningUtils::BinLabel(ptBinning, ptBin));
}

// ---------------------------------------------------------------------------
// Builders, one per kind of object
// ---------------------------------------------------------------------------
// MCTask -> CorrelationTask: the maps of one class (MultTag) or integrated (kMultIntegrated).
// At the source binning (SpectrumBinning::Source) with its suffix, as every object kept at both binnings.
inline std::string Efficiency(std::string_view particle, std::string_view multTag, SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return std::format("h1{}Efficiency_{}{}", particle, multTag, BinningSuffix(binning));
}
inline std::string SignalLoss(std::string_view particle, std::string_view multTag, SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return std::format("h1{}SigLoss_{}{}", particle, multTag, BinningSuffix(binning));
}
// The signal loss divided by the event loss of the same class: drawn as a summary, read by nothing else in the chain.
inline std::string SignalLossOverEventLoss(std::string_view particle, std::string_view multTag,
                                           SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return std::format("h1{}SigLossOverEvLoss_{}{}", particle, multTag, BinningSuffix(binning));
}
inline const std::string kEventLoss{"hEventLoss"};
// Where they are in the corrections file.
inline std::vector<std::string> EfficiencyPath(const std::string& binningName) { return {binningName, "AccEff", "MultBin"}; }
inline std::vector<std::string> SignalLossPath(const std::string& binningName) { return {binningName, "SigLoss", "MultBin"}; }
inline std::vector<std::string> EventLossPath(const std::string& binningName) { return {binningName, "EvLoss"}; }
inline std::vector<std::string> SignalLossOverEventLossPath(const std::string& binningName) { return {binningName, "SigLoss", "OverEvLoss"}; }
// PhiFitTask -> CorrelationTask: the trigger matrices that CorrelationTask reads, of the four PhiFitTask writes.
inline const std::string kTriggerSignal{"h2TriggerSignal"};
inline const std::string kTriggerBkgRatio{"h2TriggerBkgRatio"};
// PurityTask -> CorrelationTask, by purity_key.
inline std::string Purity(std::string_view purityKey, std::string_view multTag, SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return std::format("h1{}Purity_{}{}", purityKey, multTag, BinningSuffix(binning));
}

inline std::string Spectrum(std::string_view particle, double dy, std::span<const double> multBinning, int multBin)
{
  return std::format("h1SpectrumPhi{}_dy{}_{}", particle, DyTag(dy), MultTag(multBinning, multBin));
}
// The spectrum with the extrapolated bin down to pT = 0 in front of the measured ones.
inline std::string SpectrumExtended(std::string_view spectrumName) { return std::format("{}_extended", spectrumName); }
inline std::string Trend(std::string_view particle, double dy) { return std::format("h1MultTrend_{}_dy{}", particle, DyTag(dy)); }
inline std::string TrendExtrapolated(std::string_view particle, double dy) { return std::format("h1MultTrendExtrap_{}_dy{}", particle, DyTag(dy)); }
inline std::string TrendExtrapOverMeas(std::string_view particle, double dy) { return std::format("Ratio_Extrap_Meas_{}_dy{}", particle, DyTag(dy)); }
inline std::string ParticleRatioMeasured(std::string_view num, std::string_view den, double dy)
{
  return std::format("Ratio_{}_{}_Meas_dy{}", num, den, DyTag(dy));
}
inline std::string ParticleRatioExtrapolated(std::string_view num, std::string_view den, double dy)
{
  return std::format("Ratio_{}_{}_Extrap_dy{}", num, den, DyTag(dy));
}

// ---------------------------------------------------------------------------
// Canvases: the summaries every task draws and the fits, by the names a reader looks them up with
// ---------------------------------------------------------------------------
// The spectra of a species and |dy|, one curve per multiplicity class, measured and extended down to pT = 0.
inline std::string SpectraCanvas(std::string_view particle, double dy) { return std::format("cSpectra_{}_dy{}", particle, DyTag(dy)); }
inline std::string SpectraExtrapCanvas(std::string_view particle, double dy) { return std::format("cSpectraExtrap_{}_dy{}", particle, DyTag(dy)); }
// The yield of a species against multiplicity, measured or extrapolated, every |dy| on one canvas; the extrapolated over
// the measured; the ratio of two species. In the species' Trends directory, the last in Ratios.
inline std::string TrendCanvas(std::string_view particle, bool extrapolated)
{
  return std::format("cTrend_{}_{}", extrapolated ? "Extrap" : "Meas", particle);
}
inline std::string TrendExtrapOverMeasCanvas(std::string_view particle) { return std::format("cRatio_ExtrapVsMeas_{}", particle); }
inline std::string ParticleRatioCanvas(std::string_view num, std::string_view den) { return std::format("canvasRatio_{}_{}_MultTrend", num, den); }

// The correction maps of a particle, one curve per multiplicity class (MCTask).
enum class CorrectionMap { Efficiency,
                           SignalLoss,
                           SignalLossOverEventLoss };
inline std::string CorrectionSummaryCanvas(std::string_view particle, CorrectionMap map, SpectrumBinning binning = SpectrumBinning::Analysis)
{
  constexpr std::string_view kMapNames[] = {"Efficiency", "SignalLoss", "SignalLossOverEventLoss"};
  return std::format("c_{}_{}{}", particle, kMapNames[static_cast<int>(map)], BinningSuffix(binning));
}
inline std::vector<std::string> CorrectionSummaryPath(const std::string& binningName, CorrectionMap map)
{
  switch (map) {
    case CorrectionMap::Efficiency:
      return {binningName, "AccEff", "Summary"};
    case CorrectionMap::SignalLoss:
      return {binningName, "SigLoss", "Summary"};
    case CorrectionMap::SignalLossOverEventLoss:
      return {binningName, "SigLoss", "Summary", "OverEvLoss"};
  }
  return {};
}

// The purity of a purity_key, one curve per multiplicity class, in SummaryPath (PurityTask).
inline std::string PuritySummaryCanvas(std::string_view purityKey, SpectrumBinning binning)
{
  return std::format("canvas{}Purity{}", purityKey, BinningSuffix(binning));
}

// The mass fit of one cell (CellTag): the phi's in PhiFitsPath, a purity key's in PurityFitsPath.
inline std::vector<std::string> PhiFitsPath(const std::string& binningName) { return {binningName, "Fits"}; }
inline std::vector<std::string> PurityFitsPath(const std::string& binningName, const std::string& purityKey) { return {binningName, "Fits", purityKey}; }
inline std::string PhiFitCanvas(std::string_view cell) { return std::format("cFit_Phi_{}", cell); }
inline std::string PurityFitCanvas(std::string_view purityKey, std::string_view cell, SpectrumBinning binning)
{
  return std::format("cFit_{}_{}{}", purityKey, cell, BinningSuffix(binning));
}

} // namespace OutputNames

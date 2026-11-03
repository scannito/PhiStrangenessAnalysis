#pragma once

#include "BinningUtils.h"

#include "TDirectory.h"

#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>
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
// Next to it: the |dy| limits the section was produced with (RootIO::WriteValueList), so that a reader can list them
// without parsing names.
inline const std::string kDeltaYLimits{"deltaY_limits"};

// The files between the fit tasks and CorrelationTask: the name that follows the output prefix.
// Written once here, since the writer and the reader would otherwise each spell it, and a
// configuration key for it would be one more copy to keep equal on both sides (it was one, for
// the purities, until 21 October 2026).
inline const std::string kPhiDataFile{"PhiDataHistograms.root"}; // PhiFitTask -> CorrelationTask
inline const std::string kPurityFile{"Purity.root"};             // PurityTask -> CorrelationTask, every particle
// The directory of each that holds what the other task reads is SummaryPath, below.
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
// 'path' at a binning: itself for the analysis one, its SourceBinning subdirectory for the source one.
inline std::vector<std::string> BinningPath(std::vector<std::string> path, SpectrumBinning binning)
{
  return binning == SpectrumBinning::Source ? SourceBinningPath(std::move(path)) : path;
}
// The directory, below the binning_name, of what PhiFitTask and PurityTask write for CorrelationTask to read
// (declared with kPhiDataFile and kPurityFile above).
inline std::vector<std::string> SummaryPath(const std::string& binningName, SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return BinningPath({binningName, "Summary"}, binning);
}

// A species directory of a section is one that holds per-species results, which is what
// tells it from the Ratios sibling. Recognised by what it contains and not by excluding the
// names of everything it is not: that exclusion would need a new entry the first time
// another sibling appears (systematics, QA), and whoever added it would have no reason to
// look here.
// A section is the directory of one of the two engines (a binning scheme is RootIO::IsSchemeDir).
inline bool IsSectionDir(TDirectory* dir) { return dir && (dir->GetName() == kExtract1D || dir->GetName() == kExtract2D); }

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
// MCTask -> CorrelationTask: the maps of a particle, of one class (MultTag) or integrated (kMultIntegrated), at the
// analysis binning or at the source one (SpectrumBinning::Source: the suffix on the name, SourceBinning/ on the path).
// The signal loss over the event loss is drawn as a summary and read by nothing else in the chain.
enum class CorrectionMap { Efficiency,
                           SignalLoss,
                           SignalLossOverEventLoss };
namespace Detail
{
// One row per map, the only place its names are spelled: the directory of the map, the subdirectory of its objects,
// the object's name, its summary canvas's, and what it is in words.
struct CorrectionMapNames {
  std::string_view dir, objectsDir, object, canvas, title;
};
inline CorrectionMapNames NamesOf(CorrectionMap map)
{
  switch (map) {
    case CorrectionMap::Efficiency:
      return {"AccEff", "MultBin", "Efficiency", "Efficiency", "efficiency"};
    case CorrectionMap::SignalLoss:
      return {"SigLoss", "MultBin", "SigLoss", "SignalLoss", "signal loss"};
    case CorrectionMap::SignalLossOverEventLoss:
      return {"SigLoss", "OverEvLoss", "SigLossOverEvLoss", "SignalLossOverEventLoss", "signal loss over event loss"};
  }
  throw std::runtime_error(std::format("[FATAL] OutputNames: CorrectionMap {} has no names.", static_cast<int>(map)));
}
} // namespace Detail

inline std::string_view CorrectionMapTitle(CorrectionMap map) { return Detail::NamesOf(map).title; }
inline std::string CorrectionMapName(std::string_view particle, CorrectionMap map, std::string_view multTag,
                                     SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return std::format("h1{}{}_{}{}", particle, Detail::NamesOf(map).object, multTag, BinningSuffix(binning));
}
inline std::vector<std::string> CorrectionMapPath(const std::string& binningName, CorrectionMap map,
                                                  SpectrumBinning binning = SpectrumBinning::Analysis)
{
  const Detail::CorrectionMapNames names = Detail::NamesOf(map);
  return BinningPath({binningName, std::string(names.dir), std::string(names.objectsDir)}, binning);
}
inline const std::string kEventLoss{"hEventLoss"};
inline std::vector<std::string> EventLossPath(const std::string& binningName) { return {binningName, "EvLoss"}; }
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
inline std::string TrendCanvas(std::string_view particle) { return std::format("cTrend_Meas_{}", particle); }
inline std::string TrendExtrapCanvas(std::string_view particle) { return std::format("cTrend_Extrap_{}", particle); }
inline std::string TrendExtrapOverMeasCanvas(std::string_view particle) { return std::format("cRatio_ExtrapVsMeas_{}", particle); }
inline std::string ParticleRatioCanvas(std::string_view num, std::string_view den) { return std::format("canvasRatio_{}_{}_MultTrend", num, den); }

// The correction maps of a particle, one curve per multiplicity class (MCTask), in the Summary directory of the map.
inline std::string CorrectionSummaryCanvas(std::string_view particle, CorrectionMap map, SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return std::format("c_{}_{}{}", particle, Detail::NamesOf(map).canvas, BinningSuffix(binning));
}
inline std::vector<std::string> CorrectionSummaryPath(const std::string& binningName, CorrectionMap map,
                                                      SpectrumBinning binning = SpectrumBinning::Analysis)
{
  std::vector<std::string> path{binningName, std::string(Detail::NamesOf(map).dir), "Summary"};
  if (map == CorrectionMap::SignalLossOverEventLoss)
    path.emplace_back("OverEvLoss");
  return BinningPath(std::move(path), binning);
}

// The purity of a purity_key, one curve per multiplicity class, in SummaryPath (PurityTask).
inline std::string PuritySummaryCanvas(std::string_view purityKey, SpectrumBinning binning)
{
  return std::format("canvas{}Purity{}", purityKey, BinningSuffix(binning));
}

// The mass fit of one cell (CellTag): the phi's in PhiFitsPath, a purity key's in PurityFitsPath.
inline std::vector<std::string> PhiFitsPath(const std::string& binningName) { return {binningName, "Fits"}; }
inline std::vector<std::string> PurityFitsPath(const std::string& binningName, const std::string& purityKey,
                                               SpectrumBinning binning = SpectrumBinning::Analysis)
{
  return BinningPath({binningName, "Fits", purityKey}, binning);
}
inline std::string PhiFitCanvas(std::string_view cell) { return std::format("cFit_Phi_{}", cell); }
inline std::string PurityFitCanvas(std::string_view purityKey, std::string_view cell, SpectrumBinning binning)
{
  return std::format("cFit_{}_{}{}", purityKey, cell, BinningSuffix(binning));
}

} // namespace OutputNames

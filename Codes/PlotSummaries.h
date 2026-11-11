#pragma once

#include "BinningUtils.h"
#include "CanvasManager.h"
#include "OutputNames.h"
#include "PlotSeries.h"
#include "RootIOHelpers.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TLegend.h"

#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <vector>

// The summary canvases of the tasks, drawn from a file already written (DESIGN_HISTORY, "Plots: deterministic
// components, composed"): a task calls them at the end of Terminate on the file it has just closed, and the same
// functions redraw them from any file of that kind, with no task. What is drawn is found by PlotSeries, by meaning;
// what a summary looks like is fixed here, the look the tasks' canvases have always had, and a variant is a
// composition of the same series (PlotDesigns). Each draws into the CanvasManager it is given, at the canvas names and
// paths of OutputNames. What a file holds (its particles, purity keys, species ratios) is read from the file's own
// tables, by the task as by the command, so that both draw from one source.
//
// What must be in the file is said per family. A family the task always writes is required: a missing object stops
// the summary with what is missing (a wrong particle name, a file of another kind). A family the task writes only when
// asked (the source binning, the extrapolation) is looked up once, and when it is there it is required whole. The
// signal loss over event loss alone may lack a class: MCTask writes none where the event loss is zero, and says so.
namespace PlotSummaries
{

namespace Detail
{
// The title of a correction summary, as the canvases have always been titled.
inline std::string CorrectionTitle(const std::string& particle, OutputNames::CorrectionMap map, OutputNames::SpectrumBinning binning)
{
  using OutputNames::CorrectionMap;
  const bool source = binning == OutputNames::SpectrumBinning::Source;
  switch (map) {
    case CorrectionMap::Efficiency:
      return source ? std::format("{} Efficiency (Source Binning)", particle) : std::format("{} Acceptance x Efficiency", particle);
    case CorrectionMap::SignalLoss:
      return std::format("{} Signal Loss{}", particle, source ? " (Source Binning)" : "");
    case CorrectionMap::SignalLossOverEventLoss:
      return std::format("{} Signal Loss / Event Loss{}", particle, source ? " (Source Binning)" : "");
  }
  throw std::runtime_error("[FATAL] PlotSummaries: unknown correction map.");
}

// The series, or none when the file does not have it: for what a task writes only when asked.
template <class Resolve>
std::optional<PlotSeries::Series> TryRead(Resolve resolve)
{
  try {
    return resolve();
  } catch (const PlotSeries::NotInFiles&) {
    return std::nullopt;
  }
}

// Which series of a family must be in the file: every one (what the task always writes), every one or none, by the
// first (what it writes only when asked), or each one only if it is there (the signal loss over event loss, by class).
enum class Presence { Required,
                      IfProduced,
                      EachOptional };

// One series per value (a multiplicity class, a |dy| limit) by 'resolve', in the colour 'colorOf' gives its index, as
// 'presence' asks.
template <class Value, class Resolve, class ColorOf>
std::vector<PlotSeries::Series> Each(const std::vector<Value>& values, Resolve resolve, ColorOf colorOf, Presence presence)
{
  std::vector<PlotSeries::Series> series;
  if (presence == Presence::IfProduced && !values.empty() && !TryRead([&] { return resolve(values.front()); }))
    return series;
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::optional<PlotSeries::Series> s = presence == Presence::EachOptional ? TryRead([&] { return resolve(values[i]); }) : resolve(values[i]);
    if (!s)
      continue;
    s->look.color = colorOf(static_cast<int>(i));
    series.push_back(std::move(*s));
  }
  return series;
}

// One series per multiplicity class of the file, in the colour of the class.
template <class Resolve>
std::vector<PlotSeries::Series> ByClass(const std::string& fileName, const std::string& scheme, Resolve resolve, Presence presence)
{
  return Each(PlotSeries::Intervals(fileName, scheme), resolve, CanvasManager::ClassColor, presence);
}

// One series per multiplicity class, in the colour of the class, then the union of the classes in black, as present as
// the classes are ('presence'); with a single class, the class alone, which is the union under the same name.
template <class Resolve>
std::vector<PlotSeries::Series> ByClassAndUnion(const std::string& fileName, const std::string& scheme, Resolve resolve, Presence presence)
{
  const std::vector<double> edges = PlotSeries::MultEdges(fileName, scheme);
  std::vector<PlotSeries::Interval> mults = PlotSeries::Intervals(edges);
  const int nClasses = static_cast<int>(mults.size());
  if (const std::optional<PlotSeries::Interval> u = PlotSeries::UnionOfClasses(edges))
    mults.push_back(*u);
  return Each(mults, resolve, [&](int i) { return i < nClasses ? CanvasManager::ClassColor(i) : CanvasManager::UnionColor(); }, presence);
}

// The series overlaid on a new summary canvas, at its name and path, in the look the summaries have always had: the
// first one sets the frame, ROOT's default option, the Y range adjusted when the manager writes the canvas.
inline std::vector<TH1*> DrawSummary(CanvasManager& canvases, const std::string& name, const std::string& title,
                                     const std::vector<std::string>& path, const std::vector<PlotSeries::Series>& series, bool logY = false)
{
  TCanvas* canvas = canvases.CreateCanvas(name, title, path);
  canvas->SetLogy(logY);
  canvases.SetAutoYRange(canvas);
  canvas->cd();
  return PlotSeries::Draw(series, "");
}
} // namespace Detail

// The summaries drawn on 'canvases' from 'fileName' (at 'where', resolved), kept with no task, in '<outDir>/<name>/',
// where <name> is the file's stem with the scheme and the section, so that two schemes or sections of one file, or two
// files, never share an output: each canvas as '<canvas name>.png' and '.pdf', and all of them in '<name>.root' at the
// paths the task gives them, with the command and the input file's provenance (PlotSeries::OpenPlotFile).
inline void Save(const CanvasManager& canvases, const std::string& fileName, const PlotSeries::Where& where, const std::string& outDir,
                 const std::string& recipe)
{
  std::string name = std::format("{}_{}", std::filesystem::path(fileName).stem().string(), where.scheme);
  if (!where.section.empty())
    name += "_" + where.section;
  const std::string dir = std::format("{}/{}", outDir, name);
  for (const std::string& format : PlotSeries::kPlotFormats)
    canvases.ExportAll(dir, format);
  std::unique_ptr<TFile> out = PlotSeries::OpenPlotFile(std::format("{}/{}.root", dir, name), recipe, "PlotSummaries::Save");
  canvases.WriteAll(out.get());
  PlotSeries::CopyInputProvenance(out.get(), fileName, where.scheme, "input");
  out->Close();
}

// The maps of one particle at one binning: one series per multiplicity class, then the union of the classes in
// black. Efficiency and signal loss at the analysis binning are required; the source binning is there only with
// 'rebinning_pt'; the signal loss over event loss may lack a class, or the union.
inline std::vector<PlotSeries::Series> CorrectionSeries(const std::string& fileName, const std::string& scheme, const std::string& particle,
                                                        OutputNames::CorrectionMap map, OutputNames::SpectrumBinning binning)
{
  using Detail::Presence;
  auto resolve = [&](const PlotSeries::Interval& mult) { return PlotSeries::CorrectionMap(fileName, particle, map, mult, binning, scheme); };
  const bool overEventLoss = map == OutputNames::CorrectionMap::SignalLossOverEventLoss;
  const Presence presence = overEventLoss ? Presence::EachOptional : binning == OutputNames::SpectrumBinning::Analysis ? Presence::Required : Presence::IfProduced;
  return Detail::ByClassAndUnion(fileName, scheme, resolve, presence);
}

// MCTask's summaries: for each particle the file records, each map and each binning it has, the classes overlaid on
// one canvas.
inline void Corrections(CanvasManager& canvases, const std::string& fileName, const std::string& scheme)
{
  using OutputNames::CorrectionMap;
  using OutputNames::SpectrumBinning;
  for (const std::string& particle : PlotSeries::CorrectionParticles(fileName, scheme))
    for (CorrectionMap map : {CorrectionMap::Efficiency, CorrectionMap::SignalLoss, CorrectionMap::SignalLossOverEventLoss})
      for (SpectrumBinning binning : {SpectrumBinning::Analysis, SpectrumBinning::Source}) {
        const std::vector<PlotSeries::Series> series = CorrectionSeries(fileName, scheme, particle, map, binning);
        if (!series.empty())
          Detail::DrawSummary(canvases, OutputNames::CorrectionSummaryCanvas(particle, map, binning), Detail::CorrectionTitle(particle, map, binning),
                              OutputNames::CorrectionSummaryPath(scheme, map, binning), series);
      }
}

// The purity of one key at one binning: one series per multiplicity class, then the union of the classes in black. The
// analysis binning required, the source one there only with 'rebinning_pt'.
inline std::vector<PlotSeries::Series> PuritySeries(const std::string& fileName, const std::string& scheme, const std::string& purityKey,
                                                    OutputNames::SpectrumBinning binning)
{
  auto resolve = [&](const PlotSeries::Interval& mult) { return PlotSeries::Purity(fileName, purityKey, mult, binning, scheme); };
  return Detail::ByClassAndUnion(fileName, scheme, resolve,
                                 binning == OutputNames::SpectrumBinning::Analysis ? Detail::Presence::Required : Detail::Presence::IfProduced);
}

// PurityTask's summaries: for each purity key the file records and each binning it has, the classes overlaid on one
// canvas.
inline void Purities(CanvasManager& canvases, const std::string& fileName, const std::string& scheme)
{
  using OutputNames::SpectrumBinning;
  for (const std::string& key : PlotSeries::PurityKeys(fileName, scheme))
    for (SpectrumBinning binning : {SpectrumBinning::Analysis, SpectrumBinning::Source}) {
      const std::vector<PlotSeries::Series> series = PuritySeries(fileName, scheme, key, binning);
      if (!series.empty())
        Detail::DrawSummary(canvases, OutputNames::PuritySummaryCanvas(key, binning),
                            std::format("{} Purity{}", key, binning == SpectrumBinning::Source ? " (source binning)" : ""),
                            OutputNames::SummaryPath(scheme, binning), series);
    }
}

// The correlation task's summaries of one section, for each species the section holds: its spectra per |dy| (the
// classes, on a log scale); where it was extrapolated, the same extended to pT = 0 with their fitted curves (the
// measured canvas starts at the first measured bin, so the region the extrapolation is about, a third of the yield, is
// the part it cannot show); its yield trends against multiplicity (the |dy| limits in the colours of the variants),
// measured, and extrapolated with the one over the other where it was extrapolated. Then the species ratios the section
// records, measured open and extrapolated filled, with their legend, titled by their label (the Y title of the ratio).
inline void Correlations(CanvasManager& canvases, const std::string& fileName, const std::string& scheme, const std::string& section)
{
  using PlotSeries::TrendKind;
  using Detail::Presence;
  const PlotSeries::Where where{scheme, section};
  const std::vector<double> deltaYLimits = PlotSeries::DeltaYLimits(fileName, where);

  // One series per |dy| limit, in the colour of its variant; what was extrapolated only when asked is IfProduced.
  auto byDeltaY = [&](auto resolve, Presence presence) { return Detail::Each(deltaYLimits, resolve, CanvasManager::VariantColor, presence); };

  for (const std::string& p : PlotSeries::SpeciesIn(fileName, where)) {
    const std::vector<std::string> spectraPath = OutputNames::SpectraPath(scheme, section, p);
    for (double dy : deltaYLimits) {
      const std::string dyText = BinningUtils::FormatDeltaY(dy);
      Detail::DrawSummary(canvases, OutputNames::SpectraCanvas(p, dy), std::format("Spectra {} |#Delta y| < {}", p, dyText), spectraPath,
                          Detail::ByClass(fileName, scheme, [&](const PlotSeries::Interval& mult) { return PlotSeries::Spectrum(fileName, p, dy, mult, where); },
                                          Presence::Required),
                          true);
      const std::vector<PlotSeries::Series> extended = Detail::ByClass(
        fileName, scheme, [&](const PlotSeries::Interval& mult) { return PlotSeries::SpectrumExtended(fileName, p, dy, mult, where); },
        Presence::IfProduced);
      if (!extended.empty())
        Detail::DrawSummary(canvases, OutputNames::SpectraExtrapCanvas(p, dy), std::format("Spectra with extrapolation {} |#Delta y| < {}", p, dyText),
                            spectraPath, extended, true);
    }

    const std::vector<std::string> trendsPath = OutputNames::TrendsPath(scheme, section, p);
    auto trend = [&](TrendKind kind) { return [&, kind](double dy) { return PlotSeries::Trend(fileName, p, dy, kind, where); }; };
    Detail::DrawSummary(canvases, OutputNames::TrendCanvas(p), "Measured Yield Trend", trendsPath, byDeltaY(trend(TrendKind::Measured), Presence::Required));
    if (const std::vector<PlotSeries::Series> extrap = byDeltaY(trend(TrendKind::Extrapolated), Presence::IfProduced); !extrap.empty()) {
      Detail::DrawSummary(canvases, OutputNames::TrendExtrapCanvas(p), "Extrapolated Yield Trend", trendsPath, extrap);
      Detail::DrawSummary(canvases, OutputNames::TrendExtrapOverMeasCanvas(p), "Extrap / Measured Ratio", trendsPath,
                          byDeltaY(trend(TrendKind::ExtrapOverMeas), Presence::Required));
    }
  }

  for (const auto& [num, den] : PlotSeries::SpeciesRatios(fileName, where)) {
    auto ofKind = [&](TrendKind kind) { return [&, kind](double dy) { return PlotSeries::SpeciesRatio(fileName, num, den, dy, kind, where); }; };
    const std::vector<PlotSeries::Series> measured = byDeltaY(ofKind(TrendKind::Measured), Presence::Required);
    const std::vector<PlotSeries::Series> extrapolated = byDeltaY(ofKind(TrendKind::Extrapolated), Presence::IfProduced);
    // Per |dy|, the measured ratio (open) and then the extrapolated one (filled), as the legend lists them.
    const auto [filled, open] = PlotSeries::kShapes[0];
    std::vector<PlotSeries::Series> series;
    std::vector<std::string> labels;
    for (std::size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
      const std::string dyText = BinningUtils::FormatDeltaY(deltaYLimits[yIdx]);
      series.push_back(measured[yIdx]);
      series.back().look.marker = open;
      labels.push_back(std::format("Meas. |#Delta y| < {}", dyText));
      if (!extrapolated.empty()) {
        series.push_back(extrapolated[yIdx]);
        series.back().look.marker = filled;
        labels.push_back(std::format("Extrap. |#Delta y| < {}", dyText));
      }
    }
    const std::vector<TH1*> drawn =
      Detail::DrawSummary(canvases, OutputNames::ParticleRatioCanvas(num, den),
                          std::format("Ratio Mult Trend {}", PlotSeries::SpeciesRatioLabel(fileName, num, den, where)),
                          OutputNames::RatiosPath(scheme, section), series);
    TLegend* legend = CanvasManager::CreateLegend(0.7, 0.7, 0.9, 0.9, 2);
    for (std::size_t i = 0; i < drawn.size(); ++i)
      legend->AddEntry(drawn[i], labels[i].c_str(), "p");
    legend->Draw("SAME");
  }
}

// Every summary of a file, whichever task wrote it (PlotSeries::KindOf), at 'where' (an empty scheme or section is the
// only one the file has; a section only for a spectra file). Returns where they were drawn from, resolved.
inline PlotSeries::Where OfFile(CanvasManager& canvases, const std::string& fileName, const PlotSeries::Where& where)
{
  const PlotSeries::FileKind kind = PlotSeries::KindOf(fileName, where.scheme);
  if (kind == PlotSeries::FileKind::Spectra) {
    const PlotSeries::Where resolved = PlotSeries::Resolve(fileName, where);
    Correlations(canvases, fileName, resolved.scheme, resolved.section);
    return resolved;
  }
  if (!where.section.empty())
    throw std::runtime_error(std::format("[FATAL] PlotSummaries: '{}' is not a spectra file; a section ('{}') means nothing in it.", fileName,
                                         where.section));
  const std::string scheme = PlotSeries::SchemeName(fileName, where.scheme);
  switch (kind) {
    case PlotSeries::FileKind::Corrections:
      Corrections(canvases, fileName, scheme);
      break;
    case PlotSeries::FileKind::Purity:
      Purities(canvases, fileName, scheme);
      break;
    case PlotSeries::FileKind::PhiFits:
      throw std::runtime_error(std::format("[FATAL] PlotSummaries: '{}' is the phi fit file, whose task draws no summary (its fits: "
                                           "PlotDesigns::FitCanvasGrid).",
                                           fileName));
    case PlotSeries::FileKind::Spectra:
      break; // drawn above, with its section
  }
  return {scheme, ""};
}

// The summaries of a file a task has just written and closed, drawn from it as the command draws them (OfFile) and
// written into it at their paths: the file is reopened in UPDATE, the summaries' directories being under the subtree
// the task cleared in Init. 'owner' names the caller in errors.
inline void WriteInto(const std::string& fileName, const std::string& owner, const PlotSeries::Where& where)
{
  CanvasManager summaries;
  OfFile(summaries, fileName, where);
  auto file = RootIO::OpenOrThrow(fileName, "UPDATE", owner);
  summaries.WriteAll(file.get());
  file->Close();
}

} // namespace PlotSummaries

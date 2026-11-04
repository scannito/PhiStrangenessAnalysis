#pragma once

#include "BinningUtils.h"
#include "CanvasManager.h"
#include "OutputNames.h"
#include "PlotSeries.h"
#include "RootIOHelpers.h"

#include "TCanvas.h"
#include "TLegend.h"

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// The summary canvases of the tasks, drawn from a file already written (DESIGN_NOTES section 1, "Plots: deterministic
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

// One series per multiplicity class of the file, in the colour of the class, by 'resolve' (the class's interval to its
// series): every class required, or, with 'eachOptional', those the file has.
template <class Resolve>
std::vector<PlotSeries::Series> ByClass(const std::string& fileName, const std::string& scheme, Resolve resolve, bool eachOptional = false)
{
  std::vector<PlotSeries::Series> series;
  const std::vector<PlotSeries::Interval> classes = PlotSeries::Intervals(fileName, scheme);
  for (std::size_t multBin = 0; multBin < classes.size(); ++multBin) {
    std::optional<PlotSeries::Series> s = eachOptional ? TryRead([&] { return resolve(classes[multBin]); }) : resolve(classes[multBin]);
    if (!s)
      continue;
    s->look.color = CanvasManager::ClassColor(static_cast<int>(multBin));
    series.push_back(std::move(*s));
  }
  return series;
}

// The classes of a family the task writes only when asked: none when the first class is not in the file, all of them
// otherwise.
template <class Resolve>
std::vector<PlotSeries::Series> ByClassIfProduced(const std::string& fileName, const std::string& scheme, Resolve resolve)
{
  const PlotSeries::Interval first = PlotSeries::Intervals(fileName, scheme).front();
  if (!TryRead([&] { return resolve(first); }))
    return {};
  return ByClass(fileName, scheme, resolve);
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

// The summaries of a file a task has just written and closed, written into it: 'draw' puts them on the CanvasManager
// it is given, from the file, and they go to their paths when the file is reopened. 'owner' names the caller in errors.
inline void WriteInto(const std::string& fileName, const std::string& owner, const std::function<void(CanvasManager&)>& draw)
{
  CanvasManager summaries;
  draw(summaries);
  auto file = RootIO::OpenOrThrow(fileName, "UPDATE", owner);
  summaries.WriteAll(file.get());
  file->Close();
}

// The summaries drawn on 'canvases' from 'fileName', kept with no task: each canvas as '<outDir>/<canvas name>.png' and
// '.pdf', and all of them in '<outDir>/<stem>.root' at the paths the task gives them, with the plot's provenance (the
// command) and the input file's. That .root replaces only one the plots wrote (PlotSeries::Detail::RequirePlotOutput).
inline void Save(const CanvasManager& canvases, const std::string& fileName, const std::string& scheme, const std::string& outDir,
                 const std::string& stem, const std::string& recipe)
{
  const std::string rootName = std::format("{}/{}.root", outDir, stem);
  PlotSeries::Detail::RequirePlotOutput(rootName, "PlotSummaries::Save");
  for (const char* format : {"png", "pdf"})
    canvases.ExportAll(outDir, format);
  auto out = RootIO::OpenOrThrow(rootName, "RECREATE", "PlotSummaries::Save");
  canvases.WriteAll(out.get());
  RootIO::WriteProvenance(out.get(), PlotSeries::kPlotOwner, RootIO::StartProvenance(recipe));
  RootIO::CopyProvenance(PlotSeries::Detail::Open(fileName).get(), scheme, out.get(), PlotSeries::kPlotOwner, "input", fileName);
  out->Close();
}

// The maps of one particle at one binning: one series per multiplicity class, then the multiplicity-integrated one in
// black. Efficiency and signal loss at the analysis binning are required; the source binning is there only with
// 'rebinning_pt'; the signal loss over event loss may lack a class, or the integrated map.
inline std::vector<PlotSeries::Series> CorrectionSeries(const std::string& fileName, const std::string& scheme, const std::string& particle,
                                                        OutputNames::CorrectionMap map, OutputNames::SpectrumBinning binning)
{
  auto resolve = [&](std::optional<PlotSeries::Interval> mult) { return PlotSeries::CorrectionMap(fileName, particle, map, mult, binning, scheme); };
  auto ofClass = [&](const PlotSeries::Interval& mult) { return resolve(mult); };
  const bool overEventLoss = map == OutputNames::CorrectionMap::SignalLossOverEventLoss;
  std::vector<PlotSeries::Series> series = overEventLoss                                       ? Detail::ByClass(fileName, scheme, ofClass, true)
                                           : binning == OutputNames::SpectrumBinning::Analysis ? Detail::ByClass(fileName, scheme, ofClass)
                                                                                               : Detail::ByClassIfProduced(fileName, scheme, ofClass);
  if (series.empty())
    return series;
  std::optional<PlotSeries::Series> integrated = overEventLoss ? Detail::TryRead([&] { return resolve(std::nullopt); }) : resolve(std::nullopt);
  if (integrated) {
    integrated->look.color = kBlack;
    series.push_back(std::move(*integrated));
  }
  return series;
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

// The purity of one key at one binning, one series per multiplicity class: the analysis binning required, the source
// one there only with 'rebinning_pt'.
inline std::vector<PlotSeries::Series> PuritySeries(const std::string& fileName, const std::string& scheme, const std::string& purityKey,
                                                    OutputNames::SpectrumBinning binning)
{
  auto ofClass = [&](const PlotSeries::Interval& mult) { return PlotSeries::Purity(fileName, purityKey, mult, binning, scheme); };
  return binning == OutputNames::SpectrumBinning::Analysis ? Detail::ByClass(fileName, scheme, ofClass)
                                                           : Detail::ByClassIfProduced(fileName, scheme, ofClass);
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
  const PlotSeries::Where where{scheme, section};
  const std::vector<double> deltaYLimits = PlotSeries::DeltaYLimits(fileName, where);

  // One series per |dy| limit, in the colour of its variant: all of them, or, with 'ifProduced', none when the first is
  // not in the file (what was not extrapolated).
  auto byDeltaY = [&](auto resolve, bool ifProduced) {
    std::vector<PlotSeries::Series> series;
    if (ifProduced && !Detail::TryRead([&] { return resolve(deltaYLimits.front()); }))
      return series;
    for (std::size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
      PlotSeries::Series s = resolve(deltaYLimits[yIdx]);
      s.look.color = CanvasManager::VariantColor(static_cast<int>(yIdx));
      series.push_back(std::move(s));
    }
    return series;
  };

  for (const std::string& p : PlotSeries::SpeciesIn(fileName, where)) {
    const std::vector<std::string> spectraPath = OutputNames::SpectraPath(scheme, section, p);
    for (double dy : deltaYLimits) {
      const std::string dyText = BinningUtils::FormatDeltaY(dy);
      Detail::DrawSummary(canvases, OutputNames::SpectraCanvas(p, dy), std::format("Spectra {} |#Delta y| < {}", p, dyText), spectraPath,
                          Detail::ByClass(fileName, scheme, [&](const PlotSeries::Interval& mult) { return PlotSeries::Spectrum(fileName, p, dy, mult, where); }),
                          true);
      const std::vector<PlotSeries::Series> extended = Detail::ByClassIfProduced(
        fileName, scheme, [&](const PlotSeries::Interval& mult) { return PlotSeries::SpectrumExtended(fileName, p, dy, mult, where); });
      if (!extended.empty())
        Detail::DrawSummary(canvases, OutputNames::SpectraExtrapCanvas(p, dy), std::format("Spectra with extrapolation {} |#Delta y| < {}", p, dyText),
                            spectraPath, extended, true);
    }

    const std::vector<std::string> trendsPath = OutputNames::TrendsPath(scheme, section, p);
    auto trend = [&](TrendKind kind) { return [&, kind](double dy) { return PlotSeries::Trend(fileName, p, dy, kind, where); }; };
    Detail::DrawSummary(canvases, OutputNames::TrendCanvas(p), "Measured Yield Trend", trendsPath, byDeltaY(trend(TrendKind::Measured), false));
    if (const std::vector<PlotSeries::Series> extrap = byDeltaY(trend(TrendKind::Extrapolated), true); !extrap.empty()) {
      Detail::DrawSummary(canvases, OutputNames::TrendExtrapCanvas(p), "Extrapolated Yield Trend", trendsPath, extrap);
      Detail::DrawSummary(canvases, OutputNames::TrendExtrapOverMeasCanvas(p), "Extrap / Measured Ratio", trendsPath,
                          byDeltaY(trend(TrendKind::ExtrapOverMeas), false));
    }
  }

  for (const auto& [num, den] : PlotSeries::SpeciesRatios(fileName, where)) {
    auto ofKind = [&](TrendKind kind) { return [&, kind](double dy) { return PlotSeries::SpeciesRatio(fileName, num, den, dy, kind, where); }; };
    const std::vector<PlotSeries::Series> measured = byDeltaY(ofKind(TrendKind::Measured), false);
    const std::vector<PlotSeries::Series> extrapolated = byDeltaY(ofKind(TrendKind::Extrapolated), true);
    // Per |dy|, the measured ratio and then the extrapolated one, as the legend lists them.
    std::vector<PlotSeries::Series> series;
    std::vector<std::string> labels;
    for (std::size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
      const std::string dyText = BinningUtils::FormatDeltaY(deltaYLimits[yIdx]);
      series.push_back(measured[yIdx]);
      series.back().look.marker = 24;
      labels.push_back(std::format("Meas. |#Delta y| < {}", dyText));
      if (!extrapolated.empty()) {
        series.push_back(extrapolated[yIdx]);
        series.back().look.marker = 20;
        labels.push_back(std::format("Extrap. |#Delta y| < {}", dyText));
      }
    }
    const std::vector<TH1*> drawn =
      Detail::DrawSummary(canvases, OutputNames::ParticleRatioCanvas(num, den), std::format("Ratio Mult Trend {}", measured.front().hist->GetYaxis()->GetTitle()),
                          OutputNames::RatiosPath(scheme, section), series);
    TLegend* legend = CanvasManager::CreateLegend(0.7, 0.7, 0.9, 0.9, 2);
    for (std::size_t i = 0; i < drawn.size(); ++i)
      legend->AddEntry(drawn[i], labels[i].c_str(), "p");
    legend->Draw("SAME");
  }
}

} // namespace PlotSummaries

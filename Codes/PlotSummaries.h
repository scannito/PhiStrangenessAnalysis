#pragma once

#include "CanvasManager.h"
#include "Logger.h"
#include "OutputNames.h"
#include "PlotSeries.h"

#include "TCanvas.h"

#include <format>
#include <optional>
#include <string>
#include <vector>

// The summary canvases of the tasks, drawn from a file already written (DESIGN_NOTES section 1, "Plots: deterministic
// components, composed"): a task calls them at the end of Terminate on the file it has just closed, and the same
// functions redraw them from any file of that kind, with no task. What is drawn is found by PlotSeries, by meaning;
// what a summary looks like is fixed here, and a variant is a composition of the same series (PlotDesigns). Each draws
// into the CanvasManager it is given, at the canvas names and paths of OutputNames, which writes them.
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

// One series per multiplicity class of the file, in the colour of the class, by 'resolve' (the class's interval to its
// series). A class whose object is not in the file is left out with a warning that names 'what'; none at all is an empty
// list, which is what a binning the task did not produce looks like (the source binning without 'rebinning_pt').
template <class Resolve>
std::vector<PlotSeries::Series> ByClass(const std::string& fileName, const std::string& scheme, const std::string& what, Resolve resolve)
{
  std::vector<PlotSeries::Series> series;
  std::vector<std::string> missing;
  const std::vector<PlotSeries::Interval> classes = PlotSeries::Intervals(fileName, scheme);
  for (std::size_t multBin = 0; multBin < classes.size(); ++multBin) {
    try {
      PlotSeries::Series s = resolve(classes[multBin]);
      s.look.color = CanvasManager::ClassColor(static_cast<int>(multBin));
      series.push_back(std::move(s));
    } catch (const PlotSeries::NotInFiles&) {
      missing.push_back(std::format("{}-{}", BinningUtils::FormatEdge(classes[multBin].low), BinningUtils::FormatEdge(classes[multBin].high)));
    }
  }
  if (!series.empty() && !missing.empty())
    Logger::Warn("PlotSummaries: {} of '{}' is missing for the class(es) {}: not drawn.", what, fileName, Logger::Join(missing, ", "));
  return series;
}

// The classes overlaid on a new summary canvas, at its name and path, in the look the summaries have always had: the
// first class sets the frame, ROOT's default option, the Y range adjusted when the manager writes the canvas.
inline void DrawSummary(CanvasManager& canvases, const std::string& name, const std::string& title, const std::vector<std::string>& path,
                        const std::vector<PlotSeries::Series>& series)
{
  TCanvas* canvas = canvases.CreateCanvas(name, title, path);
  canvases.SetAutoYRange(canvas);
  canvas->cd();
  PlotSeries::Draw(series, "");
}
} // namespace Detail

// The maps of one particle at one binning: one series per multiplicity class (Detail::ByClass; MCTask writes no signal
// loss over event loss where the event loss is zero), then the multiplicity-integrated one in black.
inline std::vector<PlotSeries::Series> CorrectionSeries(const std::string& fileName, const std::string& scheme, const std::string& particle,
                                                        OutputNames::CorrectionMap map, OutputNames::SpectrumBinning binning)
{
  std::vector<PlotSeries::Series> series =
    Detail::ByClass(fileName, scheme, std::format("the {} of {}", OutputNames::CorrectionMapTitle(map), particle),
                    [&](const PlotSeries::Interval& mult) { return PlotSeries::CorrectionMap(fileName, particle, map, mult, binning, scheme); });
  if (series.empty())
    return series;
  PlotSeries::Series integrated = PlotSeries::CorrectionMap(fileName, particle, map, std::nullopt, binning, scheme);
  integrated.look.color = kBlack;
  series.push_back(std::move(integrated));
  return series;
}

// MCTask's summaries: for each particle, each map and each binning it has, the classes overlaid on one canvas, the Y
// range adjusted when the manager writes it.
inline void Corrections(CanvasManager& canvases, const std::string& fileName, const std::string& scheme, const std::vector<std::string>& particles)
{
  using OutputNames::CorrectionMap;
  using OutputNames::SpectrumBinning;
  for (const std::string& particle : particles)
    for (CorrectionMap map : {CorrectionMap::Efficiency, CorrectionMap::SignalLoss, CorrectionMap::SignalLossOverEventLoss})
      for (SpectrumBinning binning : {SpectrumBinning::Analysis, SpectrumBinning::Source}) {
        const std::vector<PlotSeries::Series> series = CorrectionSeries(fileName, scheme, particle, map, binning);
        if (!series.empty())
          Detail::DrawSummary(canvases, OutputNames::CorrectionSummaryCanvas(particle, map, binning), Detail::CorrectionTitle(particle, map, binning),
                              OutputNames::CorrectionSummaryPath(scheme, map, binning), series);
      }
}

// The purity of one key at one binning, one series per multiplicity class (Detail::ByClass).
inline std::vector<PlotSeries::Series> PuritySeries(const std::string& fileName, const std::string& scheme, const std::string& purityKey,
                                                    OutputNames::SpectrumBinning binning)
{
  return Detail::ByClass(fileName, scheme, std::format("the purity of {}", purityKey),
                         [&](const PlotSeries::Interval& mult) { return PlotSeries::Purity(fileName, purityKey, mult, binning, scheme); });
}

// PurityTask's summaries: for each purity key and each binning it has, the classes overlaid on one canvas.
inline void Purities(CanvasManager& canvases, const std::string& fileName, const std::string& scheme, const std::vector<std::string>& purityKeys)
{
  using OutputNames::SpectrumBinning;
  for (const std::string& key : purityKeys)
    for (SpectrumBinning binning : {SpectrumBinning::Analysis, SpectrumBinning::Source}) {
      const std::vector<PlotSeries::Series> series = PuritySeries(fileName, scheme, key, binning);
      if (!series.empty())
        Detail::DrawSummary(canvases, OutputNames::PuritySummaryCanvas(key, binning),
                            std::format("{} Purity{}", key, binning == SpectrumBinning::Source ? " (source binning)" : ""),
                            OutputNames::SummaryPath(scheme, binning), series);
    }
}

} // namespace PlotSummaries

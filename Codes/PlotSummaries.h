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
} // namespace Detail

// The maps of one particle at one binning: one series per multiplicity class, in the colour of the class, then the
// multiplicity-integrated one in black. A class without its map is left out with a warning (MCTask writes no signal
// loss over event loss where the event loss is zero); no map at all is an empty list, which is what a binning the task
// did not produce looks like (the source binning without 'rebinning_pt').
inline std::vector<PlotSeries::Series> CorrectionSeries(const std::string& fileName, const std::string& scheme, const std::string& particle,
                                                        OutputNames::CorrectionMap map, OutputNames::SpectrumBinning binning)
{
  std::vector<PlotSeries::Series> series;
  std::vector<std::string> missing;
  const std::vector<PlotSeries::Interval> classes = PlotSeries::Intervals(fileName, scheme);
  for (std::size_t multBin = 0; multBin < classes.size(); ++multBin) {
    try {
      PlotSeries::Series s = PlotSeries::CorrectionMap(fileName, particle, map, classes[multBin], binning, scheme);
      s.look.color = CanvasManager::ClassColor(static_cast<int>(multBin));
      series.push_back(std::move(s));
    } catch (const PlotSeries::NotInFiles&) {
      missing.push_back(std::format("{}-{}", BinningUtils::FormatEdge(classes[multBin].low), BinningUtils::FormatEdge(classes[multBin].high)));
    }
  }
  if (series.empty())
    return series;
  if (!missing.empty())
    Logger::Warn("PlotSummaries: {} {} of '{}' has no map for the class(es) {}: not drawn.", particle, OutputNames::CorrectionMapTitle(map),
                 fileName, Logger::Join(missing, ", "));
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
        if (series.empty())
          continue;
        TCanvas* canvas = canvases.CreateCanvas(OutputNames::CorrectionSummaryCanvas(particle, map, binning),
                                                Detail::CorrectionTitle(particle, map, binning), OutputNames::CorrectionSummaryPath(scheme, map, binning));
        canvases.SetAutoYRange(canvas);
        canvas->cd();
        PlotSeries::Draw(series, "");
      }
}

} // namespace PlotSummaries

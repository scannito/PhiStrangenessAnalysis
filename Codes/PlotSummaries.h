#pragma once

#include "CanvasManager.h"
#include "Logger.h"
#include "OutputNames.h"
#include "PlotSeries.h"

#include "TCanvas.h"
#include "TF1.h"
#include "TLegend.h"

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
inline std::vector<TH1*> DrawSummary(CanvasManager& canvases, const std::string& name, const std::string& title,
                                     const std::vector<std::string>& path, const std::vector<PlotSeries::Series>& series, bool logY = false)
{
  TCanvas* canvas = canvases.CreateCanvas(name, title, path);
  canvas->SetLogy(logY);
  canvases.SetAutoYRange(canvas);
  canvas->cd();
  return PlotSeries::Draw(series, "");
}

// The |dy| limits of a section, as the correlation task recorded them next to its objects.
inline std::vector<double> DeltaYLimits(const std::string& fileName, const std::string& scheme, const std::string& section)
{
  for (const PlotSeries::Catalog::Scheme& s : PlotSeries::ReadCatalog(fileName).schemes)
    if (s.name == scheme)
      for (const PlotSeries::Catalog::Section& sec : s.sections)
        if (sec.name == section) {
          if (!sec.deltaYLimits)
            throw std::runtime_error(std::format("[FATAL] PlotSummaries: section '{}/{}' of '{}' does not record its |dy| limits ('{}'); "
                                                 "it was written before they were: re-run the correlation task.",
                                                 scheme, section, fileName, OutputNames::kDeltaYLimits));
          return *sec.deltaYLimits;
        }
  throw PlotSeries::NotInFiles(std::format("[FATAL] PlotSummaries: '{}' has no section '{}/{}'.", fileName, scheme, section));
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

// A ratio of two species' yields the correlation task was asked for (yield_ratios), with its label.
struct SpeciesRatioSpec {
  std::string num, den, label;
};

// The correlation task's summaries of one section, for each species: its spectra per |dy| (the classes, on a log
// scale), the same extended to pT = 0 with their fitted curves where it was extrapolated (the measured canvas starts at
// the first measured bin, so the region the extrapolation is about, a third of the yield, is the part it cannot show),
// its yield trends against
// multiplicity (the |dy| limits in the colours of the variants), measured, extrapolated and the one over the other;
// then the species ratios, measured open and extrapolated filled, with their legend.
inline void Correlations(CanvasManager& canvases, const std::string& fileName, const std::string& scheme, const std::string& section,
                         const std::vector<std::string>& species, const std::vector<SpeciesRatioSpec>& ratios)
{
  using PlotSeries::TrendKind;
  const PlotSeries::Where where{scheme, section};
  const std::vector<double> deltaYLimits = Detail::DeltaYLimits(fileName, scheme, section);
  // A series that may not have been produced (no extrapolation for this species, no extrapolated ratio): none.
  auto optional = [](auto resolve) -> std::optional<PlotSeries::Series> {
    try {
      return resolve();
    } catch (const PlotSeries::NotInFiles&) {
      return std::nullopt;
    }
  };

  for (const std::string& p : species) {
    const std::vector<std::string> spectraPath{scheme, section, p, OutputNames::kSpectraDir};
    const std::vector<std::string> trendsPath{scheme, section, p, OutputNames::kTrendsDir};
    for (double dy : deltaYLimits) {
      const std::string dyText = BinningUtils::FormatDeltaY(dy);
      Detail::DrawSummary(canvases, OutputNames::SpectraCanvas(p, dy), std::format("Spectra {} |#Delta y| < {}", p, dyText), spectraPath,
                          Detail::ByClass(fileName, scheme, std::format("the spectrum of {} at |dy| < {}", p, dyText),
                                          [&](const PlotSeries::Interval& mult) { return PlotSeries::Spectrum(fileName, p, dy, mult, where); }),
                          true);
      const std::vector<PlotSeries::Series> extended =
        Detail::ByClass(fileName, scheme, std::format("the extended spectrum of {} at |dy| < {}", p, dyText),
                        [&](const PlotSeries::Interval& mult) { return PlotSeries::SpectrumExtended(fileName, p, dy, mult, where); });
      if (extended.empty())
        continue;
      // Each fitted curve in the colour of its spectrum, so that it reads against its own points and not the next ones.
      for (TH1* drawn : Detail::DrawSummary(canvases, OutputNames::SpectraExtrapCanvas(p, dy),
                                            std::format("Spectra with extrapolation {} |#Delta y| < {}", p, dyText), spectraPath, extended, true))
        for (TObject* function : *drawn->GetListOfFunctions())
          if (auto* curve = dynamic_cast<TF1*>(function)) {
            curve->SetLineColor(drawn->GetMarkerColor());
            curve->SetLineWidth(2);
          }
    }

    auto trends = [&](TrendKind kind) {
      std::vector<PlotSeries::Series> series;
      for (std::size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx)
        if (std::optional<PlotSeries::Series> s = optional([&] { return PlotSeries::Trend(fileName, p, deltaYLimits[yIdx], kind, where); })) {
          s->look.color = CanvasManager::VariantColor(static_cast<int>(yIdx));
          series.push_back(std::move(*s));
        }
      return series;
    };
    Detail::DrawSummary(canvases, OutputNames::TrendCanvas(p), "Measured Yield Trend", trendsPath, trends(TrendKind::Measured));
    if (const std::vector<PlotSeries::Series> extrap = trends(TrendKind::Extrapolated); !extrap.empty()) {
      Detail::DrawSummary(canvases, OutputNames::TrendExtrapCanvas(p), "Extrapolated Yield Trend", trendsPath, extrap);
      Detail::DrawSummary(canvases, OutputNames::TrendExtrapOverMeasCanvas(p), "Extrap / Measured Ratio", trendsPath, trends(TrendKind::ExtrapOverMeas));
    }
  }

  for (const SpeciesRatioSpec& ratio : ratios) {
    std::vector<PlotSeries::Series> series;
    std::vector<std::string> labels;
    for (std::size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
      const double dy = deltaYLimits[yIdx];
      const int color = CanvasManager::VariantColor(static_cast<int>(yIdx));
      PlotSeries::Series meas = PlotSeries::SpeciesRatio(fileName, ratio.num, ratio.den, dy, TrendKind::Measured, where);
      meas.look = {.color = color, .marker = 24};
      series.push_back(std::move(meas));
      labels.push_back(std::format("Meas. |#Delta y| < {}", BinningUtils::FormatDeltaY(dy)));
      if (std::optional<PlotSeries::Series> extrap =
            optional([&] { return PlotSeries::SpeciesRatio(fileName, ratio.num, ratio.den, dy, TrendKind::Extrapolated, where); })) {
        extrap->look = {.color = color, .marker = 20};
        series.push_back(std::move(*extrap));
        labels.push_back(std::format("Extrap. |#Delta y| < {}", BinningUtils::FormatDeltaY(dy)));
      }
    }
    const std::vector<TH1*> drawn = Detail::DrawSummary(canvases, OutputNames::ParticleRatioCanvas(ratio.num, ratio.den),
                                                        std::format("Ratio Mult Trend {}", ratio.label), {scheme, section, OutputNames::kRatiosDir}, series);
    TLegend* legend = CanvasManager::CreateLegend(0.7, 0.7, 0.9, 0.9, 2);
    for (std::size_t i = 0; i < drawn.size(); ++i)
      legend->AddEntry(drawn[i], labels[i].c_str(), "p");
    legend->Draw("SAME");
  }
}

} // namespace PlotSummaries

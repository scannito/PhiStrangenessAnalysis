#pragma once

#include "CanvasManager.h"
#include "Logger.h"
#include "OutputNames.h"
#include "PlotSeries.h"
#include "RootIOHelpers.h"

#include "TCanvas.h"
#include "TH1.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLegendEntry.h"
#include "TPad.h"

#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The recurring designs of a plot, each one function over series PlotSeries found and checked: the six layouts the
// requests have asked for (DESIGN_NOTES section 1, "Plots: deterministic components, composed"). A design decides the
// layout; what is drawn comes in as series, and how it looks comes in as parameters with a default, which are the only
// things a composition sets. Nothing here reads a file or builds a name except FitCanvasGrid, which reopens canvases
// already written, by the names OutputNames gives them.
namespace PlotDesigns
{

// What every design draws the same way: the text in pixels (fonts of precision 3, so that it keeps its size across
// pads of different widths), the draw option, the legend, the ALICE label when one is asked for.
struct Frame {
  int width{800};
  int height{600};
  int font{43};
  int textPx{22};
  std::string drawOption{"E1"};
  // Legend corners in the pad's NDC; columns; drawn on every panel unless the panel says otherwise.
  double legendX1{0.55}, legendY1{0.62}, legendX2{0.92}, legendY2{0.90};
  int legendColumns{1};
  // The status and the system together, or no label: the collision system is not in the files.
  std::optional<std::pair<std::string, std::string>> alice;
};

// A legend that says what one attribute of the look means, by one fact of the series (the colour is the |dy|, the
// marker filled or open is extrapolated or measured): one entry per value of the fact, in the order the series bring them. It is
// derived from the series as drawn, so it cannot say what the plot does not show: a value drawn with two colours, or a
// colour standing for two values, stops the plot.
struct Key {
  enum class Shows { Color, Fill };
  std::optional<std::string> PlotSeries::Facts::* fact;
  Shows shows;
  double x1, y1, x2, y2; // NDC of the pad
  int columns{1};
  // The words of an entry, by the value of the fact, and the order of the entries: those named here first, in this
  // order; the others after, as their value, in the order the series bring them. A value no series has is refused.
  std::vector<std::pair<std::string, std::string>> text;
};

// One pad's worth: the series and how its axes read. Empty titles are the first series' own.
struct Panel {
  std::vector<PlotSeries::Series> series;
  std::string title;  // written in the pad's top left corner
  std::string xTitle;
  std::string yTitle;
  std::optional<std::pair<double, double>> yRange; // otherwise the series' own, with headroom
  bool logY{false};
  bool legend{true};       // the legend AutoLabels writes, one entry per series
  std::vector<Key> keys;   // legends of the look, each on its own corners
  std::vector<std::string> notes; // lines of text under the title
};

namespace Detail
{
inline void FormatAxes(TH1* h, const Frame& frame, double yTitleOffset = 1.6)
{
  for (TAxis* axis : {h->GetXaxis(), h->GetYaxis()}) {
    axis->SetLabelFont(frame.font);
    axis->SetLabelSize(frame.textPx);
    axis->SetTitleFont(frame.font);
    axis->SetTitleSize(frame.textPx + 2);
  }
  h->GetXaxis()->SetTitleOffset(1.2);
  h->GetYaxis()->SetTitleOffset(yTitleOffset);
}

// The Y range of the drawn copies: the given one, or the series' own with room above for the legend.
inline void ApplyYRange(const std::vector<TH1*>& drawn, const Panel& panel)
{
  if (drawn.empty())
    return;
  const auto [low, high] = panel.yRange ? *panel.yRange
                                        : CanvasManager::ComputeGlobalYRange(drawn, {.isLogY = panel.logY, .zeroBaseline = !panel.logY, .topMargin = 0.35});
  drawn.front()->SetMinimum(low);
  drawn.front()->SetMaximum(high);
}

inline void WriteText(double x, double y, const std::string& text, const Frame& frame, int align = 13, int extraPx = 0)
{
  TLatex latex;
  latex.SetNDC();
  latex.SetTextFont(frame.font);
  latex.SetTextSize(frame.textPx + extraPx);
  latex.SetTextAlign(align);
  latex.DrawLatex(x, y, text.c_str());
}

// The open markers of ROOT's table (TAttMarker): the outline of a filled one. The others are filled, or are lines (a
// cross, a star) which a Fill key cannot tell apart; those are refused there.
inline bool IsOpenMarker(int style)
{
  static const std::set<int> kOpen{4, 24, 25, 26, 27, 28, 30, 32, 35, 36, 37, 38, 40, 42, 44, 46};
  static const std::set<int> kFilled{8, 20, 21, 22, 23, 29, 33, 34, 39, 41, 43, 45, 47, 48, 49};
  if (!kOpen.contains(style) && !kFilled.contains(style))
    throw std::runtime_error(std::format("[FATAL] PlotDesigns: marker {} is neither open nor filled; a key of the filling cannot use it.", style));
  return kOpen.contains(style);
}

// Key's entries: the values of its fact, each with the colour or the filling of its series, checked one to one. A
// colour entry is drawn as its first series is; a filling entry as a circle, open or filled, in black, so that it is not
// read as one more colour or shape.
inline void DrawKey(TPad* pad, const std::vector<PlotSeries::Series>& series, const std::vector<TH1*>& drawn, const Key& key, const Frame& frame)
{
  std::vector<std::pair<std::string, std::size_t>> entries; // value, its first series
  std::map<std::string, int> attributeOf;
  std::map<int, std::string> valueOf;
  for (std::size_t i = 0; i < series.size(); ++i) {
    const std::optional<std::string>& value = series[i].facts.*key.fact;
    if (!value)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::DrawKey: series {} ('{}') has no value for the fact of the key.", i,
                                           series[i].origins.front().path));
    const int attribute = key.shows == Key::Shows::Color ? drawn[i]->GetMarkerColor() : IsOpenMarker(drawn[i]->GetMarkerStyle());
    const auto [a, newValue] = attributeOf.emplace(*value, attribute);
    const auto [v, newAttribute] = valueOf.emplace(attribute, *value);
    if (a->second != attribute || v->second != *value)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::DrawKey: the {} does not stand for one '{}' alone (series {}, '{}'): the "
                                           "key would mislabel the plot.", key.shows == Key::Shows::Color ? "colour" : "filling of the marker", *value, i,
                                           series[i].origins.front().path));
    if (newValue)
      entries.emplace_back(*value, i);
  }
  std::vector<std::pair<std::string, std::size_t>> ordered; // words, first series
  for (const auto& [value, words] : key.text) {
    const auto entry = std::ranges::find(entries, value, &std::pair<std::string, std::size_t>::first);
    if (entry == entries.end())
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::DrawKey: text given for '{}', which no series has.", value));
    ordered.emplace_back(words, entry->second);
  }
  for (const auto& [value, first] : entries)
    if (std::ranges::find(key.text, value, &std::pair<std::string, std::string>::first) == key.text.end())
      ordered.emplace_back(value, first);

  pad->cd();
  TLegend* legend = CanvasManager::CreateLegend(key.x1, key.y1, key.x2, key.y2, key.columns);
  legend->SetTextFont(frame.font);
  legend->SetTextSize(frame.textPx);
  for (const auto& [words, first] : ordered) {
    const char* label = words.c_str();
    if (key.shows == Key::Shows::Color) {
      legend->AddEntry(drawn[first], label, "lp");
      continue;
    }
    TLegendEntry* entry = legend->AddEntry(static_cast<TObject*>(nullptr), label, "p");
    entry->SetMarkerStyle(IsOpenMarker(drawn[first]->GetMarkerStyle()) ? 24 : 20);
    entry->SetMarkerSize(drawn[first]->GetMarkerSize());
    entry->SetMarkerColor(kBlack);
  }
  legend->Draw();
}

inline void DrawLegend(TPad* pad, const std::vector<PlotSeries::Series>& series, const std::vector<TH1*>& drawn, const Frame& frame)
{
  const PlotSeries::LegendText text = PlotSeries::AutoLabels(series);
  pad->cd();
  TLegend* legend = CanvasManager::CreateLegend(frame.legendX1, frame.legendY1, frame.legendX2, frame.legendY2, frame.legendColumns);
  legend->SetTextFont(frame.font);
  legend->SetTextSize(frame.textPx);
  if (!text.header.empty())
    legend->SetHeader(text.header.c_str());
  for (std::size_t i = 0; i < drawn.size(); ++i)
    legend->AddEntry(drawn[i], text.labels[i].c_str(), "lp");
  legend->Draw();
}

// A panel on 'pad': the series (PlotSeries::Draw: one warning for what they disagree on, non-finite bins left empty),
// the range, the titles, the legend and the panel's title.
inline std::vector<TH1*> DrawPanel(TPad* pad, const Panel& panel, const Frame& frame, double yTitleOffset = 1.6)
{
  pad->cd();
  pad->SetLogy(panel.logY);
  pad->SetTicks(1, 1);
  std::vector<TH1*> drawn = PlotSeries::Draw(panel.series, frame.drawOption.c_str());
  if (drawn.empty())
    return drawn;
  ApplyYRange(drawn, panel);
  FormatAxes(drawn.front(), frame, yTitleOffset);
  drawn.front()->SetTitle("");
  if (!panel.xTitle.empty())
    drawn.front()->GetXaxis()->SetTitle(panel.xTitle.c_str());
  if (!panel.yTitle.empty())
    drawn.front()->GetYaxis()->SetTitle(panel.yTitle.c_str());
  if (panel.legend)
    DrawLegend(pad, panel.series, drawn, frame);
  for (const Key& key : panel.keys)
    DrawKey(pad, panel.series, drawn, key, frame);
  // The title a little larger than the text, the notes one line each below it, in pixels so that pads of different
  // heights space them alike.
  const double x = pad->GetLeftMargin() + 0.04, line = 1.6 * frame.textPx / (pad->GetAbsHNDC() * pad->GetCanvas()->GetWh());
  double y = 1.0 - pad->GetTopMargin() - 0.03;
  if (!panel.title.empty()) {
    WriteText(x, y, panel.title, frame, 13, 2);
    y -= line;
  }
  for (const std::string& note : panel.notes) {
    WriteText(x, y, note, frame);
    y -= line;
  }
  pad->RedrawAxis();
  return drawn;
}

inline void DrawAlice(TPad* pad, const Frame& frame)
{
  if (frame.alice)
    CanvasManager::DrawAliceLabel(pad, pad->GetLeftMargin() + 0.04, 0.84, frame.alice->first, frame.alice->second);
}
} // namespace Detail

// -----------------------------------------------------------------------------
// The designs
// -----------------------------------------------------------------------------
// 1. Every series of one panel on one pad.
inline TCanvas* Overlay(CanvasManager& canvases, const std::string& name, const Panel& panel, const Frame& frame = {})
{
  TCanvas* canvas = canvases.CreateCanvas(name, panel.title, frame.width, frame.height);
  CanvasManager::SetPadMargins(canvas, {.left = 0.15, .right = 0.04, .top = 0.06, .bottom = 0.13});
  Detail::DrawPanel(canvas, panel, frame);
  Detail::DrawAlice(canvas, frame);
  return canvas;
}

// 2. Panels in a grid, row by row ('nCols' per row): the overlay of the classes of several particles and files.
inline TCanvas* Grid(CanvasManager& canvases, const std::string& name, const std::vector<Panel>& panels, int nCols,
                     const Frame& frame = {})
{
  const int nRows = (static_cast<int>(panels.size()) + nCols - 1) / nCols;
  TCanvas* canvas = canvases.CreateGridCanvas(name, name, nCols, nRows, frame.width, frame.height,
                                              CanvasManager::PadMargins{0.15, 0.03, 0.06, 0.13});
  for (std::size_t i = 0; i < panels.size(); ++i)
    Detail::DrawPanel(static_cast<TPad*>(canvas->GetPad(static_cast<int>(i) + 1)), panels[i], frame, 2.0);
  if (frame.alice)
    Detail::DrawAlice(static_cast<TPad*>(canvas->GetPad(1)), frame);
  return canvas;
}

// 3. Series against a reference, with their ratio to it below. The option of the division is stated, never guessed
// (PlotSeries::Ratio): "" for independent samples, "B" for a closure.
inline TCanvas* CompareWithRatio(CanvasManager& canvases, const std::string& name, const Panel& panel, std::size_t reference,
                                 const std::string& divideOption, std::optional<std::pair<double, double>> ratioRange = std::nullopt,
                                 const std::string& ratioTitle = "ratio", const Frame& frame = {})
{
  if (reference >= panel.series.size())
    throw std::runtime_error(std::format("[FATAL] PlotDesigns::CompareWithRatio: reference {} of {} series.", reference, panel.series.size()));
  CanvasManager::RatioCanvas rc = canvases.CreateRatioCanvas(name, panel.title, std::vector<std::string>{}, frame.width, frame.height);
  Detail::DrawPanel(rc.topPad, panel, frame);
  Detail::DrawAlice(rc.topPad, frame);

  Panel ratios{.yRange = ratioRange, .logY = false, .legend = false};
  for (std::size_t i = 0; i < panel.series.size(); ++i)
    if (i != reference) {
      PlotSeries::Series r = PlotSeries::Ratio(panel.series[i], panel.series[reference], divideOption);
      r.look = panel.series[i].look;
      if (!r.look.color)
        r.look.color = CanvasManager::ClassColor(static_cast<int>(i));
      ratios.series.push_back(std::move(r));
    }
  std::vector<TH1*> drawn = Detail::DrawPanel(rc.bottomPad, ratios, frame);
  if (!drawn.empty()) {
    drawn.front()->GetYaxis()->SetTitle(ratioTitle.c_str());
    CanvasManager::DrawHorizontalLine(rc.bottomPad, drawn.front()->GetXaxis()->GetXmin(), drawn.front()->GetXaxis()->GetXmax());
  }
  return rc.canvas;
}

// 4. One panel per pad, the pads joined with one Y axis and one range for all (the procedures side by side).
inline TCanvas* JoinedPads(CanvasManager& canvases, const std::string& name, std::vector<Panel> panels, const Frame& frame = {})
{
  // One range for every pad: the given one, or the union of the series' own.
  std::optional<std::pair<double, double>> shared;
  for (const Panel& panel : panels)
    if (panel.yRange)
      shared = panel.yRange;
  if (!shared) {
    std::vector<TH1*> all;
    for (const Panel& panel : panels)
      for (const PlotSeries::Series& s : panel.series)
        all.push_back(s.hist.get());
    shared = CanvasManager::ComputeGlobalYRange(all, {.zeroBaseline = false, .topMargin = 0.45});
  }
  CanvasManager::JoinedPads joined = canvases.CreateJoinedPads(name, name, static_cast<int>(panels.size()), frame.width, frame.height);
  for (std::size_t i = 0; i < panels.size(); ++i) {
    panels[i].yRange = shared;
    std::vector<TH1*> drawn = Detail::DrawPanel(joined.pads[i], panels[i], frame, 2.3);
    if (drawn.empty())
      continue;
    // The axis is drawn once, on the first pad; the labels where two pads meet would overlap.
    if (i != 0)
      drawn.front()->GetYaxis()->SetLabelSize(0);
    if (i != 0)
      drawn.front()->GetXaxis()->ChangeLabel(1, -1, 0);
    if (i + 1 != panels.size()) {
      drawn.front()->GetXaxis()->ChangeLabel(-1, -1, 0);
      drawn.front()->GetXaxis()->SetTitle("");
    }
  }
  if (frame.alice && !joined.pads.empty())
    Detail::DrawAlice(joined.pads.front(), frame);
  return joined.canvas;
}

// 5. Series against an external reference (a preliminary), with its systematic uncertainty as a band, and the ratio of
// each series to it below. Only the X axes can be checked against a foreign file (PlotSeries::Object).
inline TCanvas* WithReference(CanvasManager& canvases, const std::string& name, Panel panel, const PlotSeries::Series& reference,
                              const std::optional<PlotSeries::Series>& systematic, const std::string& divideOption,
                              std::optional<std::pair<double, double>> ratioRange = std::nullopt, const Frame& frame = {})
{
  panel.series.insert(panel.series.begin(), reference);
  if (panel.series.front().look.color.value_or(kBlack) == kBlack)
    panel.series.front().look.color = kBlack;
  TCanvas* canvas = CompareWithRatio(canvases, name, panel, 0, divideOption, ratioRange, "ratio to the reference", frame);
  if (systematic) {
    PlotSeries::RequireCombinable(*systematic, reference);
    auto* top = static_cast<TPad*>(canvas->GetPad(1));
    top->cd();
    auto band = std::unique_ptr<TH1>(static_cast<TH1*>(systematic->hist->Clone()));
    band->SetDirectory(0);
    band->SetFillColorAlpha(kGray + 1, 0.4);
    band->SetLineWidth(0);
    band->SetMarkerSize(0);
    band.release()->Draw("E2 SAME");
  }
  return canvas;
}

// 6a. The fit canvases already written, laid out in a grid ('nCols' per row), each labelled; by the paths OutputNames
// gives them (PhiFitsPath + PhiFitCanvas, PurityFitsPath + PurityFitCanvas).
inline TCanvas* FitCanvasGrid(CanvasManager& canvases, const std::string& name, const std::string& fileName,
                              const std::vector<std::pair<std::string, std::string>>& canvasPathAndLabel, int nCols,
                              const Frame& frame = {})
{
  auto file = RootIO::OpenOrThrow(fileName, "READ", "PlotDesigns::FitCanvasGrid");
  const int nRows = (static_cast<int>(canvasPathAndLabel.size()) + nCols - 1) / nCols;
  TCanvas* canvas = canvases.CreateGridCanvas(name, name, nCols, nRows, frame.width, frame.height, CanvasManager::PadMargins{0, 0, 0, 0});
  for (std::size_t i = 0; i < canvasPathAndLabel.size(); ++i) {
    const auto& [path, label] = canvasPathAndLabel[i];
    std::unique_ptr<TCanvas> fit(dynamic_cast<TCanvas*>(file->Get(path.c_str())));
    if (!fit)
      throw PlotSeries::NotInFiles(std::format("[FATAL] PlotDesigns::FitCanvasGrid: no canvas '{}' in '{}'.", path, fileName));
    canvas->cd(static_cast<int>(i) + 1);
    fit->DrawClonePad();
    if (!label.empty())
      Detail::WriteText(0.15, 0.95, label, frame);
  }
  return canvas;
}

// 6b. The series printed bin by bin: the values a plot draws, for a reader who wants the numbers.
inline std::string Table(const std::vector<PlotSeries::Series>& series)
{
  if (series.empty())
    return "";
  const PlotSeries::LegendText text = PlotSeries::AutoLabels(series);
  std::string out = text.header.empty() ? "" : std::format("{}\n", text.header);
  out += "bin  interval";
  for (const std::string& label : text.labels)
    out += std::format("  |  {}", label);
  out += "\n";
  const TAxis* axis = series.front().hist->GetXaxis();
  for (int bin = 1; bin <= axis->GetNbins(); ++bin) {
    out += std::format("{}  {}-{}", bin, BinningUtils::FormatEdge(axis->GetBinLowEdge(bin)), BinningUtils::FormatEdge(axis->GetBinUpEdge(bin)));
    for (const PlotSeries::Series& s : series)
      out += std::format("  |  {} +- {}", s.hist->GetBinContent(bin), s.hist->GetBinError(bin));
    out += "\n";
  }
  return out;
}

} // namespace PlotDesigns

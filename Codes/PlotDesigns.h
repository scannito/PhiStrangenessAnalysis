#pragma once

#include "BinningUtils.h"
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
#include "TROOT.h"

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

// The recurring designs of a plot, each one function over series PlotSeries found and checked: the six layouts the
// requests have asked for (DESIGN_HISTORY, "Plots: deterministic components, composed"). A design decides the
// layout; what is drawn comes in as series, and how it looks comes in as parameters with a default, which are the only
// things a composition sets. Each returns what it drew, the series it computed included (the ratios), for Save and
// Table. Nothing here reads a file or builds a name except FitCanvasGrid, which reopens canvases already written, by
// the names OutputNames gives them.
namespace PlotDesigns
{

// A box in the pad's NDC, and its columns: a legend's place.
struct Corners {
  double x1, y1, x2, y2;
  int columns{1};
};

// What every design draws the same way: the text in pixels (fonts of precision 3, so that it keeps its size across
// pads of different widths), the draw option, the legend, the ALICE label when one is asked for.
struct Frame {
  int width{800};
  int height{600};
  int font{43};
  int textPx{22};
  std::string drawOption{"E1"};
  Corners legend{0.55, 0.62, 0.92, 0.90}; // on every panel that has one
  // The status and the system together, or no label: the collision system is not in the files.
  std::optional<std::pair<std::string, std::string>> alice;
};

// A legend that says what one attribute of the look means, by one fact of the series (the colour is the |dy|, the
// marker filled or open is extrapolated or measured): one entry per value of the fact. It is derived from the series as
// drawn, so it cannot say what the plot does not show: a value drawn with two colours, or a colour standing for two
// values, stops the plot.
struct Key {
  enum class Shows { Color, Fill };
  PlotSeries::Fact fact;
  Shows shows;
  Corners where;
  // The words of an entry, by the value of the fact (as the series' facts have it), and the order of the entries:
  // those named here first, in this order; the others after, as their value, in the order the series bring them. A
  // value no series has is refused.
  std::vector<std::pair<std::string, std::string>> text;
};

// One pad's worth: the series and how its axes read. Empty titles are the first series' own.
struct Panel {
  std::vector<PlotSeries::Series> series;
  std::string title; // written in the pad's top left corner
  std::string xTitle;
  std::string yTitle;
  std::optional<std::pair<double, double>> yRange; // otherwise the series' own, by the two below
  bool zeroBaseline{true};                         // the automatic range starts at 0 (linear scale, positive data)
  double headroom{0.35};                           // room above the data, as a fraction of their span, for the text
  bool logY{false};
  bool legend{true};              // the legend AutoLabels writes, one entry per series
  std::vector<Key> keys;          // legends of the look, each in its own corners
  std::vector<std::string> notes; // lines of text under the title
  // A systematic uncertainty, drawn as a band behind the series; combinable with the first series.
  std::optional<PlotSeries::Series> band;
};

// The values of 'fact' among the series, distinct, in the order the series bring them: what a look by fact numbers and
// a key lists. A series without the fact stops ('what' names the caller): its look, or its entry, would say nothing.
inline std::vector<std::string> FactValues(const std::vector<const PlotSeries::Series*>& series, PlotSeries::Fact fact, std::string_view what)
{
  std::vector<std::string> values;
  for (const PlotSeries::Series* s : series) {
    const std::optional<std::string>& value = s->facts.*fact;
    if (!value)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::{}: '{}' has no value for the fact it is taken from.", what, s->origins.front().path));
    if (std::ranges::find(values, *value) == values.end())
      values.push_back(*value);
  }
  return values;
}

// Looks from facts: series that share the value of a fact share the attribute (ColourBy, ShapeBy, FillBy), what a Key
// then explains, so that the legend and the look agree by construction. Given the panels of a design, the values are
// numbered over all of them, so one value has one look in every pad, and a key drawn in one pad holds for the others.
// Each keeps what the others set (the shape its filling, the filling its shape), so the order does not matter; a look
// a series was given by hand is not overwritten, it stops.
namespace Detail
{
inline std::vector<PlotSeries::Series*> SeriesOf(std::vector<PlotSeries::Series>& series)
{
  std::vector<PlotSeries::Series*> all;
  for (PlotSeries::Series& s : series)
    all.push_back(&s);
  return all;
}
inline std::vector<PlotSeries::Series*> SeriesOf(std::vector<Panel>& panels)
{
  std::vector<PlotSeries::Series*> all;
  for (Panel& panel : panels)
    for (PlotSeries::Series& s : panel.series)
      all.push_back(&s);
  return all;
}

// The index of each series' value of 'fact', checked against how many values the look can say.
inline std::vector<std::size_t> ValueIndices(const std::vector<PlotSeries::Series*>& series, PlotSeries::Fact fact, std::string_view what,
                                             std::size_t maxValues)
{
  const std::vector<std::string> values = FactValues({series.begin(), series.end()}, fact, what);
  if (values.size() > maxValues)
    throw std::runtime_error(std::format("[FATAL] PlotDesigns::{}: {} values ({}), {} at most.", what, values.size(), Logger::Join(values, ", "), maxValues));
  std::vector<std::size_t> indices;
  for (const PlotSeries::Series* s : series)
    indices.push_back(static_cast<std::size_t>(std::ranges::find(values, *(s->facts.*fact)) - values.begin()));
  return indices;
}

// The filled shapes, in the order ShapeBy gives them, and the outline of each (TAttMarker).
inline constexpr std::pair<int, int> kShapes[] = {{20, 24}, {21, 25}, {33, 27}, {34, 28}, {29, 30}, {22, 26}, {23, 32}};
inline constexpr std::size_t kDiamond = 2; // kShapes[kDiamond]
// A diamond looks smaller than a circle or a square of the same size: drawn larger by this much, the 2.0 against 1.4 of
// the owner's ratio plots (26 October 2026).
inline constexpr double kDiamondScale = 2.0 / 1.4;

// The pair of kShapes a marker belongs to, and whether it is the open one; none for a marker outside them.
inline std::optional<std::pair<std::size_t, bool>> ShapeOf(int marker)
{
  for (std::size_t i = 0; i < std::size(kShapes); ++i)
    if (kShapes[i].first == marker || kShapes[i].second == marker)
      return std::pair{i, kShapes[i].second == marker};
  return std::nullopt;
}

inline void ColourBy(const std::vector<PlotSeries::Series*>& series, PlotSeries::Fact fact, const std::vector<int>& palette)
{
  const std::vector<std::size_t> indices =
    ValueIndices(series, fact, "ColourBy", palette.empty() ? std::numeric_limits<std::size_t>::max() : palette.size());
  for (std::size_t i = 0; i < series.size(); ++i) {
    if (series[i]->look.color)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::ColourBy: '{}' has a colour of its own.", series[i]->origins.front().path));
    series[i]->look.color = palette.empty() ? CanvasManager::VariantColor(static_cast<int>(indices[i])) : palette[indices[i]];
  }
}

inline void ShapeBy(const std::vector<PlotSeries::Series*>& series, PlotSeries::Fact fact)
{
  const std::vector<std::size_t> indices = ValueIndices(series, fact, "ShapeBy", std::size(kShapes));
  for (std::size_t i = 0; i < series.size(); ++i) {
    PlotSeries::Look& look = series[i]->look;
    const auto shape = look.marker ? ShapeOf(*look.marker) : std::nullopt;
    if (look.marker && !shape)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::ShapeBy: '{}' has a marker of its own.", series[i]->origins.front().path));
    look.marker = shape && shape->second ? kShapes[indices[i]].second : kShapes[indices[i]].first;
    if (indices[i] == kDiamond)
      look.markerSize = look.markerSize.value_or(CanvasManager::kMarkerSize) * kDiamondScale;
  }
}

inline void FillBy(const std::vector<PlotSeries::Series*>& series, PlotSeries::Fact fact)
{
  const std::vector<std::size_t> indices = ValueIndices(series, fact, "FillBy", 2);
  for (std::size_t i = 0; i < series.size(); ++i) {
    PlotSeries::Look& look = series[i]->look;
    const auto shape = ShapeOf(look.marker.value_or(kShapes[0].first));
    if (!shape)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::FillBy: '{}' has a marker of its own, with no filled and open pair.",
                                           series[i]->origins.front().path));
    look.marker = indices[i] == 0 ? kShapes[shape->first].first : kShapes[shape->first].second;
  }
}
} // namespace Detail

// Each value of 'fact' in a colour: 'palette' in order (as many as there are values), the variants' colours
// (CanvasManager::VariantColor) without one.
inline void ColourBy(std::vector<PlotSeries::Series>& series, PlotSeries::Fact fact, const std::vector<int>& palette = {})
{
  Detail::ColourBy(Detail::SeriesOf(series), fact, palette);
}
inline void ColourBy(std::vector<Panel>& panels, PlotSeries::Fact fact, const std::vector<int>& palette = {})
{
  Detail::ColourBy(Detail::SeriesOf(panels), fact, palette);
}
// Each value of 'fact' in a shape of its own (Detail::kShapes), a diamond drawn larger.
inline void ShapeBy(std::vector<PlotSeries::Series>& series, PlotSeries::Fact fact) { Detail::ShapeBy(Detail::SeriesOf(series), fact); }
inline void ShapeBy(std::vector<Panel>& panels, PlotSeries::Fact fact) { Detail::ShapeBy(Detail::SeriesOf(panels), fact); }
// Two values of 'fact', the first filled and the second open (a circle where no shape was given).
inline void FillBy(std::vector<PlotSeries::Series>& series, PlotSeries::Fact fact) { Detail::FillBy(Detail::SeriesOf(series), fact); }
inline void FillBy(std::vector<Panel>& panels, PlotSeries::Fact fact) { Detail::FillBy(Detail::SeriesOf(panels), fact); }

// What a design drew: the canvas and every series on it, those it computed included.
struct Drawn {
  TCanvas* canvas{nullptr};
  std::vector<PlotSeries::Series> series;
};

namespace Detail
{
inline void FormatAxes(TH1* h, const Frame& frame, double yTitleOffset)
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

inline void WriteText(double x, double y, const std::string& text, const Frame& frame, int align = 13, int extraPx = 0)
{
  TLatex latex;
  latex.SetNDC();
  latex.SetTextFont(frame.font);
  latex.SetTextSize(frame.textPx + extraPx);
  latex.SetTextAlign(align);
  latex.DrawLatex(x, y, text.c_str());
}

inline TLegend* NewLegend(const Corners& where, const Frame& frame)
{
  TLegend* legend = CanvasManager::CreateLegend(where.x1, where.y1, where.x2, where.y2, where.columns);
  legend->SetTextFont(frame.font);
  legend->SetTextSize(frame.textPx);
  return legend;
}

// Rows of a grid holding 'n' pads, 'nCols' per row.
inline int Rows(std::size_t n, int nCols)
{
  if (nCols <= 0)
    throw std::runtime_error(std::format("[FATAL] PlotDesigns: a grid of {} column(s).", nCols));
  return (static_cast<int>(n) + nCols - 1) / nCols;
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
// colour entry is drawn as the first series of its value is, its shape included (the shape may say the same value
// again, as in the owner's ratio plots); a filling entry as a circle, open or filled, in black, so that it is not read
// as one more colour or shape.
inline void DrawKey(TPad* pad, const std::vector<PlotSeries::Series>& series, const std::vector<TH1*>& drawn, const Key& key, const Frame& frame)
{
  std::vector<const PlotSeries::Series*> pointers;
  for (const PlotSeries::Series& s : series)
    pointers.push_back(&s);
  std::vector<std::pair<std::string, std::size_t>> entries; // value, its first series
  for (const std::string& value : FactValues(pointers, key.fact, "DrawKey"))
    entries.emplace_back(value, static_cast<std::size_t>(std::ranges::find_if(series, [&](const auto& s) { return s.facts.*key.fact == value; }) -
                                                         series.begin()));
  std::map<std::string, int> attributeOf;
  std::map<int, std::string> valueOf;
  for (std::size_t i = 0; i < series.size(); ++i) {
    const std::string& value = *(series[i].facts.*key.fact);
    const int attribute = key.shows == Key::Shows::Color ? drawn[i]->GetMarkerColor() : IsOpenMarker(drawn[i]->GetMarkerStyle());
    const auto [a, newValue] = attributeOf.emplace(value, attribute);
    const auto [v, newAttribute] = valueOf.emplace(attribute, value);
    if (a->second != attribute || v->second != value)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::DrawKey: the {} does not stand for one '{}' alone (series {}, '{}'): the "
                                           "key would mislabel the plot.", key.shows == Key::Shows::Color ? "colour" : "filling of the marker", value, i,
                                           series[i].origins.front().path));
  }
  std::vector<std::pair<std::string, std::size_t>> ordered; // words, first series
  for (const auto& [value, words] : key.text) {
    const auto entry = std::ranges::find(entries, value, &std::pair<std::string, std::size_t>::first);
    if (entry == entries.end())
      throw std::runtime_error(std::format("[FATAL] PlotDesigns::DrawKey: text given for '{}', which no series has.", value));
    ordered.emplace_back(words, entry->second);
  }
  // A value without words of its own is said by what tells it apart: the leading words (", "-separated) every value
  // shares are left out ("species ratio, measured" and "species ratio, extrapolated" read "measured", "extrapolated").
  const std::string& first = entries.front().first;
  std::size_t shared = 0;
  for (std::size_t next; entries.size() > 1 && (next = first.find(", ", shared)) != std::string::npos; shared = next + 2)
    if (!std::ranges::all_of(entries, [&](const auto& e) { return e.first.starts_with(std::string_view(first).substr(0, next + 2)); }))
      break;
  for (const auto& [value, first] : entries)
    if (std::ranges::find(key.text, value, &std::pair<std::string, std::string>::first) == key.text.end())
      ordered.emplace_back(value.substr(shared), first);

  pad->cd();
  TLegend* legend = NewLegend(key.where, frame);
  for (const auto& [words, first] : ordered) {
    if (key.shows == Key::Shows::Color) {
      legend->AddEntry(drawn[first], words.c_str(), "lp");
      continue;
    }
    TLegendEntry* entry = legend->AddEntry(static_cast<TObject*>(nullptr), words.c_str(), "p");
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
  TLegend* legend = NewLegend(frame.legend, frame);
  if (!text.header.empty())
    legend->SetHeader(text.header.c_str());
  for (std::size_t i = 0; i < drawn.size(); ++i)
    legend->AddEntry(drawn[i], text.labels[i].c_str(), "lp");
  legend->Draw();
}

// A panel on 'pad': the series (PlotSeries::Draw: one warning for what they disagree on, non-finite bins left empty),
// the band behind them (the series drawn again over it; a band has no point to leave out, so a non-finite bin in it is
// refused), the range, the titles, the legends, then the panel's title, its notes and the ALICE label one line below
// the other from the top left corner.
inline std::vector<TH1*> DrawPanel(TPad* pad, const Panel& panel, const Frame& frame, double yTitleOffset, bool alice)
{
  pad->cd();
  pad->SetLogy(panel.logY);
  pad->SetTicks(1, 1);
  std::vector<TH1*> drawn = PlotSeries::Draw(panel.series, frame.drawOption);
  if (drawn.empty())
    return drawn;
  std::vector<TH1*> ranged = drawn;
  if (panel.band) {
    PlotSeries::RequireCombinable(*panel.band, panel.series.front());
    if (panel.band->nonFiniteBins)
      throw std::runtime_error(std::format("[FATAL] PlotDesigns: the band '{}' has {} non-finite bin(s); a band cannot leave a bin out.",
                                           panel.band->origins.front().path, panel.band->nonFiniteBins));
    TH1* band = panel.band->hist->DrawCopy("E2 SAME");
    band->SetFillColorAlpha(kGray + 1, 0.4);
    band->SetLineWidth(0);
    band->SetMarkerSize(0);
    for (TH1* d : drawn)
      d->Draw(std::format("{} SAME", frame.drawOption).c_str());
    ranged.push_back(band);
  }
  const auto [low, high] = panel.yRange ? *panel.yRange
                                        : CanvasManager::ComputeGlobalYRange(ranged, {.isLogY = panel.logY, .zeroBaseline = panel.zeroBaseline && !panel.logY,
                                                                                      .topMargin = panel.headroom});
  drawn.front()->SetMinimum(low);
  drawn.front()->SetMaximum(high);
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
  // In pixels, so that pads of different heights space the lines alike; the title a little larger than the text.
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
  if (alice && frame.alice) {
    WriteText(x, y, CanvasManager::AliceLabelText(frame.alice->first), frame);
    WriteText(x, y - line, frame.alice->second, frame);
  }
  pad->RedrawAxis();
  return drawn;
}

// The series of several panels, in order: what a design drew.
inline std::vector<PlotSeries::Series> AllSeries(const std::vector<Panel>& panels)
{
  std::vector<PlotSeries::Series> all;
  for (const Panel& panel : panels) {
    all.insert(all.end(), panel.series.begin(), panel.series.end());
    if (panel.band)
      all.push_back(*panel.band);
  }
  return all;
}
} // namespace Detail

// -----------------------------------------------------------------------------
// The designs
// -----------------------------------------------------------------------------
// 1. Every series of one panel on one pad.
inline Drawn Overlay(CanvasManager& canvases, const std::string& name, const Panel& panel, const Frame& frame = {})
{
  TCanvas* canvas = canvases.CreateCanvas(name, panel.title, frame.width, frame.height, {.left = 0.15, .right = 0.04, .top = 0.06, .bottom = 0.13});
  Detail::DrawPanel(canvas, panel, frame, 1.6, true);
  return {canvas, Detail::AllSeries({panel})};
}

// 2. Panels in a grid, row by row ('nCols' per row): the overlay of the classes of several particles and files.
inline Drawn Grid(CanvasManager& canvases, const std::string& name, const std::vector<Panel>& panels, int nCols, const Frame& frame = {})
{
  TCanvas* canvas = canvases.CreateGridCanvas(name, name, nCols, Detail::Rows(panels.size(), nCols), frame.width, frame.height,
                                              CanvasManager::PadMargins{0.15, 0.03, 0.06, 0.13});
  for (std::size_t i = 0; i < panels.size(); ++i)
    Detail::DrawPanel(static_cast<TPad*>(canvas->GetPad(static_cast<int>(i) + 1)), panels[i], frame, 2.0, i == 0);
  return {canvas, Detail::AllSeries(panels)};
}

// 3. Series against a reference, with their ratio to it below, each ratio drawn as its numerator is above. The option
// of the division is stated, never guessed (PlotSeries::Ratio): "" for independent samples, "B" for a closure.
inline Drawn CompareWithRatio(CanvasManager& canvases, const std::string& name, const Panel& panel, std::size_t reference,
                              const std::string& divideOption, std::optional<std::pair<double, double>> ratioRange = std::nullopt,
                              const std::string& ratioTitle = "ratio", const Frame& frame = {})
{
  if (reference >= panel.series.size())
    throw std::runtime_error(std::format("[FATAL] PlotDesigns::CompareWithRatio: reference {} of {} series.", reference, panel.series.size()));
  CanvasManager::RatioCanvas rc = canvases.CreateRatioCanvas(name, panel.title, frame.width, frame.height);
  const std::vector<TH1*> top = Detail::DrawPanel(rc.topPad, panel, frame, 1.6, true);
  // The X axis is read on the ratio pad; on the top one its labels would fall into the joint.
  top.front()->GetXaxis()->SetLabelSize(0);
  top.front()->GetXaxis()->SetTitleSize(0);

  Panel ratios{.yRange = ratioRange, .zeroBaseline = false, .headroom = 0.1, .legend = false};
  for (std::size_t i = 0; i < panel.series.size(); ++i)
    if (i != reference) {
      PlotSeries::Series r = PlotSeries::Ratio(panel.series[i], panel.series[reference], divideOption);
      r.look = {.color = top[i]->GetMarkerColor(), .marker = top[i]->GetMarkerStyle(), .markerSize = top[i]->GetMarkerSize()};
      ratios.series.push_back(std::move(r));
    }
  const std::vector<TH1*> bottom = Detail::DrawPanel(rc.bottomPad, ratios, frame, 1.6, false);
  if (!bottom.empty()) {
    bottom.front()->GetYaxis()->SetTitle(ratioTitle.c_str());
    CanvasManager::DrawHorizontalLine(rc.bottomPad, bottom.front()->GetXaxis()->GetXmin(), bottom.front()->GetXaxis()->GetXmax());
  }
  return {rc.canvas, Detail::AllSeries({panel, ratios})};
}

// 4. One panel per pad, the pads joined with one Y axis and one range for all (the procedures side by side): the given
// one, or the union of the ranges each pad takes for itself. The panels' own yRange is not read here.
inline Drawn JoinedPads(CanvasManager& canvases, const std::string& name, const std::vector<Panel>& panels,
                        std::optional<std::pair<double, double>> yRange = std::nullopt, const Frame& frame = {})
{
  for (const Panel& panel : panels) {
    if (panel.yRange)
      throw std::runtime_error("[FATAL] PlotDesigns::JoinedPads: the range is one for every pad; give it to JoinedPads, not to a panel.");
    if (panel.logY != panels.front().logY)
      throw std::runtime_error("[FATAL] PlotDesigns::JoinedPads: one Y axis for every pad, so one scale: the panels mix log and linear.");
  }
  CanvasManager::JoinedPads joined = canvases.CreateJoinedPads(name, name, static_cast<int>(panels.size()), frame.width, frame.height);
  std::vector<TH1*> frames;
  for (std::size_t i = 0; i < panels.size(); ++i) {
    std::vector<TH1*> drawn = Detail::DrawPanel(joined.pads[i], panels[i], frame, 2.3, i == 0);
    if (drawn.empty())
      continue;
    frames.push_back(drawn.front());
    // The axis is drawn once, on the first pad; the labels where two pads meet would overlap.
    if (i != 0) {
      drawn.front()->GetYaxis()->SetLabelSize(0);
      drawn.front()->GetXaxis()->ChangeLabel(1, -1, 0);
    }
    if (i + 1 != panels.size()) {
      drawn.front()->GetXaxis()->ChangeLabel(-1, -1, 0);
      drawn.front()->GetXaxis()->SetTitle("");
    }
  }
  if (!yRange && !frames.empty()) {
    yRange = std::pair{frames.front()->GetMinimum(), frames.front()->GetMaximum()};
    for (TH1* f : frames)
      yRange = std::pair{std::min(yRange->first, f->GetMinimum()), std::max(yRange->second, f->GetMaximum())};
  }
  for (TH1* f : frames) {
    f->SetMinimum(yRange->first);
    f->SetMaximum(yRange->second);
  }
  return {joined.canvas, Detail::AllSeries(panels)};
}

// 5. Series against an external reference (a preliminary), with its systematic uncertainty as a band, and the ratio of
// each series to it below. Only the X axes can be checked against a foreign file (PlotSeries::Object).
inline Drawn WithReference(CanvasManager& canvases, const std::string& name, Panel panel, const PlotSeries::Series& reference,
                           const std::optional<PlotSeries::Series>& systematic, const std::string& divideOption,
                           std::optional<std::pair<double, double>> ratioRange = std::nullopt, const Frame& frame = {})
{
  panel.series.insert(panel.series.begin(), reference);
  if (!panel.series.front().look.color)
    panel.series.front().look.color = kBlack;
  panel.band = systematic;
  return CompareWithRatio(canvases, name, panel, 0, divideOption, ratioRange, "ratio", frame);
}

// 6a. The mass-fit canvases of some cells, already written, laid out in a grid ('nCols' per row), each labelled (by
// its cell tag unless a label is given): the phi's when no purity key is given, the key's otherwise, by the paths
// OutputNames gives them. A cell is named by its intervals, as CellTag writes them.
struct FitCell {
  PlotSeries::Interval mult, pt;
  std::string label;
};

inline Drawn FitCanvasGrid(CanvasManager& canvases, const std::string& name, const std::string& fileName, const std::string& scheme,
                           const std::optional<std::string>& purityKey, const std::vector<FitCell>& cells, int nCols,
                           OutputNames::SpectrumBinning binning = OutputNames::SpectrumBinning::Analysis, const Frame& frame = {})
{
  auto file = RootIO::OpenOrThrow(fileName, "READ", "PlotDesigns::FitCanvasGrid");
  TCanvas* canvas = canvases.CreateGridCanvas(name, name, nCols, Detail::Rows(cells.size(), nCols), frame.width, frame.height,
                                              CanvasManager::PadMargins{0, 0, 0, 0});
  for (std::size_t i = 0; i < cells.size(); ++i) {
    const FitCell& cell = cells[i];
    const std::string tag = OutputNames::CellTag(std::array{cell.mult.low, cell.mult.high}, 0, std::array{cell.pt.low, cell.pt.high}, 0);
    const std::string path = purityKey ? RootIO::ObjectPath(OutputNames::PurityFitsPath(scheme, *purityKey, binning),
                                                            OutputNames::PurityFitCanvas(*purityKey, tag, binning))
                                       : RootIO::ObjectPath(OutputNames::PhiFitsPath(scheme), OutputNames::PhiFitCanvas(tag));
    std::unique_ptr<TCanvas> fit = PlotSeries::Detail::Get<TCanvas>(file.get(), fileName, path);
    canvas->cd(static_cast<int>(i) + 1);
    fit->DrawClonePad();
    Detail::WriteText(0.15, 0.95, cell.label.empty() ? tag : cell.label, frame);
  }
  return {canvas, {}};
}

// 6b. The series printed bin by bin: the values a plot draws, for a reader who wants the numbers. One column per series
// against one X axis, so the series must share it.
inline std::string Table(const std::vector<PlotSeries::Series>& series)
{
  if (series.empty())
    return "";
  const TAxis* axis = series.front().hist->GetXaxis();
  for (std::size_t i = 1; i < series.size(); ++i)
    BinningUtils::RequireSameAxis(axis, series[i].hist->GetXaxis(), series.front().origins.front().path, series[i].origins.front().path);
  const PlotSeries::LegendText text = PlotSeries::AutoLabels(series);
  std::string out = text.header.empty() ? "" : std::format("{}\n", text.header);
  out += "bin  interval";
  for (const std::string& label : text.labels)
    out += std::format("  |  {}", label);
  out += "\n";
  for (int bin = 1; bin <= axis->GetNbins(); ++bin) {
    out += std::format("{}  {}-{}", bin, BinningUtils::FormatEdge(axis->GetBinLowEdge(bin)), BinningUtils::FormatEdge(axis->GetBinUpEdge(bin)));
    for (const PlotSeries::Series& s : series)
      out += std::format("  |  {} +- {}", s.hist->GetBinContent(bin), s.hist->GetBinError(bin));
    out += "\n";
  }
  return out;
}

// What a design drew, kept: PlotSeries::Save with the series the design returned.
inline void Save(const Drawn& drawn, const std::string& outBase, const std::string& recipe, const std::map<std::string, std::string>& facts = {})
{
  PlotSeries::Save(drawn.canvas, outBase, drawn.series, recipe, facts);
}

// The body of a composition's main: batch mode, no window, and the exit code a caller acts on: 0 done, 1 refused or
// failed, 3 not in the files (PlotSeries::NotInFiles).
inline int Run(const std::function<void()>& body)
{
  gROOT->SetBatch(kTRUE);
  try {
    body();
    return 0;
  } catch (const PlotSeries::NotInFiles& e) {
    Logger::Error("{}", e.what());
    return 3;
  } catch (const std::exception& e) {
    Logger::Error("{}", e.what());
    return 1;
  }
}

} // namespace PlotDesigns

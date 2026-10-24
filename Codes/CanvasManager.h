#pragma once

#include "Logger.h"
#include "RootIOHelpers.h"

#include "TCanvas.h"
#include "TClass.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TKey.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TPad.h"
#include "TROOT.h"
#include "TStyle.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// =============================================================================
// CanvasManager: centralized lifecycle, layout factory, ALICE styling, and IO
// for ROOT canvases.
//
// Solves four problems that grew across the analysis tasks:
// 1. Proliferation of hardcoded std::unique_ptr<TCanvas> members in data structures
//    (e.g. LoadedMC, LoadedPurity), which required touching multiple structs and
//    lambdas whenever a new diagnostic or ratio plot was introduced.
// 2. ROOT global canvas list collisions: TCanvas registers itself globally in
//    gROOT->GetListOfCanvases() regardless of TDirectory, causing segfaults or
//    warnings on duplicate names across tasks or loops.
// 3. Lack of standardized layout factories (split ratio pads 70/30 with seamless
//    borders, font-size compensation on smaller pads, multibin grid pads).
// 4. Repeated writeCanvas boilerplate across tasks, and missing unified export to
//    multi-page PDF reports or disk images.
// =============================================================================

// These two live outside the class on purpose: a nested struct with default member
// initializers cannot be used as a default argument (`= PadMargins{}`) of a member
// function of its enclosing class - a real compiler rejects it, Cling lets it pass.
// CanvasManager re-exports them below, so CanvasManager::PadMargins and
// CanvasManager::YRangeOptions keep working unchanged for every caller.
namespace CanvasInfo
{
struct PadMargins {
  double left{0.14};
  double right{0.05};
  double top{0.08};
  double bottom{0.12};
};

// Options for multi-histogram automatic Y-range determination
struct YRangeOptions {
  bool isLogY{false};
  bool zeroBaseline{false};  // If true and min >= 0, sets Y min to 0.0 (e.g. counts/spectra)
  bool includeErrors{true};  // Includes bin error bars in min/max evaluation
  double topMargin{0.25};    // Headroom above maximum, as a fraction of the span (of its decades in log scale)
  double bottomMargin{0.10}; // Footroom below minimum, the same way
};
} // namespace CanvasInfo

class CanvasManager
{
 public:
  using PadMargins = CanvasInfo::PadMargins;
  using YRangeOptions = CanvasInfo::YRangeOptions;

  // Holds the components of a dual-pad canvas (e.g. distribution on top, ratio on bottom)
  struct RatioCanvas {
    TCanvas* canvas{nullptr};
    TPad* topPad{nullptr};
    TPad* bottomPad{nullptr};
    double fontScale{1.0}; // Compensates font sizes in bottom pad (pad height ratio)
  };

  CanvasManager(bool showStats = false)
  {
    SetOptStat(showStats);
  }
  ~CanvasManager() = default;

  CanvasManager(const CanvasManager&) = delete;
  CanvasManager& operator=(const CanvasManager&) = delete;
  CanvasManager(CanvasManager&&) noexcept = default;
  CanvasManager& operator=(CanvasManager&&) noexcept = default;

  // ---------------------------------------------------------------------------
  // 1. Registry & Lifecycle Management
  // ---------------------------------------------------------------------------

  // Creates a standard single-pad canvas, unregistering any existing colliding object
  // from ROOT's global canvas list.
  TCanvas* CreateCanvas(const std::string& name, const std::string& title = "",
                        int width = 800, int height = 600,
                        const PadMargins& margins = PadMargins{})
  {
    return CreateCanvas(name, title, std::vector<std::string>{}, width, height, margins);
  }

  // Creates a standard single-pad canvas with an associated target directory path in ROOT files.
  TCanvas* CreateCanvas(const std::string& name, const std::string& title,
                        const std::vector<std::string>& targetPath,
                        int width = 800, int height = 600,
                        const PadMargins& margins = PadMargins{})
  {
    CleanGlobalCollision(name);
    const std::string cTitle = title.empty() ? name : title;
    auto c = std::make_unique<TCanvas>(name.c_str(), cTitle.c_str(), width, height);
    SetPadMargins(c.get(), margins);
    if (!targetPath.empty()) {
      canvasPaths[name] = targetPath;
    }
    return RecordCanvas(std::move(c));
  }

  // Registers an existing unique_ptr<TCanvas>, taking ownership.
  TCanvas* RegisterCanvas(std::unique_ptr<TCanvas> canvas, const std::vector<std::string>& targetPath = {})
  {
    if (!canvas)
      return nullptr;
    if (!targetPath.empty()) {
      canvasPaths[canvas->GetName()] = targetPath;
    }
    return RecordCanvas(std::move(canvas));
  }

  // Sets or updates the target directory path in ROOT files for a managed canvas.
  void SetCanvasPath(const std::string& name, const std::vector<std::string>& targetPath)
  {
    canvasPaths[name] = targetPath;
  }

  std::vector<std::string> GetCanvasPath(const std::string& name) const
  {
    auto it = canvasPaths.find(name);
    return it != canvasPaths.end() ? it->second : std::vector<std::string>{};
  }

  // Lookups
  TCanvas* GetCanvas(const std::string& name) const
  {
    auto it = canvases.find(name);
    return it != canvases.end() ? it->second.get() : nullptr;
  }

  TCanvas* RequireCanvas(const std::string& name) const
  {
    TCanvas* c = GetCanvas(name);
    if (!c) {
      throw std::runtime_error(std::format("[FATAL] CanvasManager: Canvas '{}' not found in registry", name));
    }
    return c;
  }

  bool HasCanvas(const std::string& name) const
  {
    return canvases.contains(name);
  }

  // Releases ownership to caller, removing from manager. [[nodiscard]] because
  // discarding the result destroys the canvas, which is a different intention:
  // that one is DestroyCanvas, and should be said by name.
  [[nodiscard]] std::unique_ptr<TCanvas> ReleaseCanvas(const std::string& name)
  {
    canvasPaths.erase(name);
    autoYRanges.erase(name);
    auto it = canvases.find(name);
    if (it == canvases.end())
      return nullptr;
    auto c = std::move(it->second);
    canvases.erase(it);
    std::erase(canvasOrder, name);
    return c;
  }

  // Removes a canvas from the registry and destroys it, together with every
  // primitive drawn on it with kCanDelete. Unknown names are a no-op.
  void DestroyCanvas(const std::string& name)
  {
    canvasPaths.erase(name);
    autoYRanges.erase(name);
    canvases.erase(name);
    std::erase(canvasOrder, name);
  }

  const std::map<std::string, std::unique_ptr<TCanvas>>& GetAllCanvases() const { return canvases; }
  const std::vector<std::string>& GetCanvasOrder() const { return canvasOrder; }

  void Clear()
  {
    canvases.clear();
    canvasOrder.clear();
    canvasPaths.clear();
    autoYRanges.clear();
  }

  // Declares, next to the creation of a canvas, that its Y axis is to be fitted to
  // the histograms drawn on it. The adjustment is applied by the manager itself,
  // every time it writes or exports the canvas - so after the last histogram has
  // been drawn, which for a summary canvas filled over a loop is the only moment
  // the range is known.
  //
  // It takes the canvas and not a name on purpose: the name then appears once, where
  // the canvas is created. Adjusting at write time by a name rebuilt elsewhere was
  // the alternative, and a rebuilt name that drifts is skipped without a word.
  // Log scale is read from the pad when the adjustment runs, so SetLogy may come
  // before or after this call.
  void SetAutoYRange(TCanvas* canvas, const YRangeOptions& opts = YRangeOptions{})
  {
    if (!canvas || GetCanvas(canvas->GetName()) != canvas) {
      throw std::runtime_error(std::format("[FATAL] CanvasManager::SetAutoYRange: canvas '{}' is not managed by this manager",
                                           canvas ? canvas->GetName() : "nullptr"));
    }
    autoYRanges[canvas->GetName()] = opts;
  }

  // ---------------------------------------------------------------------------
  // 2. Layout Factory (Ratio pads, Grids, Margins)
  // ---------------------------------------------------------------------------

  // Creates a HEP-standard 2-pad ratio canvas (top: spectrum/fit, bottom: ratio/pull).
  // Pads are joined without gap (top bottom-margin = bottom top-margin = 0.02).
  RatioCanvas CreateRatioCanvas(const std::string& name, const std::string& title = "",
                                int width = 800, int height = 800,
                                double splitRatio = 0.30)
  {
    return CreateRatioCanvas(name, title, std::vector<std::string>{}, width, height, splitRatio);
  }

  // Creates a HEP-standard 2-pad ratio canvas with an associated target directory path.
  RatioCanvas CreateRatioCanvas(const std::string& name, const std::string& title,
                                const std::vector<std::string>& targetPath,
                                int width = 800, int height = 800,
                                double splitRatio = 0.30)
  {
    if (splitRatio <= 0.0 || splitRatio >= 1.0) {
      throw std::runtime_error("[FATAL] CanvasManager: splitRatio must be between 0.0 and 1.0");
    }

    CleanGlobalCollision(name);
    const std::string cTitle = title.empty() ? name : title;
    auto c = std::make_unique<TCanvas>(name.c_str(), cTitle.c_str(), width, height);
    c->cd();

    // Top Pad (Distribution / Fit / Numerator & Denominator)
    TPad* topPad = new TPad((name + "_top").c_str(), "top", 0.0, splitRatio, 1.0, 1.0);
    topPad->SetLeftMargin(0.14);
    topPad->SetRightMargin(0.05);
    topPad->SetTopMargin(0.08);
    topPad->SetBottomMargin(0.02); // Seamless junction with bottom pad
    topPad->Draw();

    // Bottom Pad (Ratio / Residuals / Pulls)
    TPad* bottomPad = new TPad((name + "_bottom").c_str(), "bottom", 0.0, 0.0, 1.0, splitRatio);
    bottomPad->SetLeftMargin(0.14);
    bottomPad->SetRightMargin(0.05);
    bottomPad->SetTopMargin(0.02); // Seamless junction with top pad
    bottomPad->SetBottomMargin(0.35); // Ample space for X axis labels and title
    bottomPad->Draw();

    // In ROOT fractional coordinates, text on the smaller bottom pad appears
    // shrunken unless multiplied by the height ratio:
    double fontScale = (1.0 - splitRatio) / splitRatio;

    if (!targetPath.empty()) {
      canvasPaths[name] = targetPath;
    }
    TCanvas* raw = RecordCanvas(std::move(c));
    return RatioCanvas{raw, topPad, bottomPad, fontScale};
  }

  // Creates a canvas divided into nCols x nRows subpads with uniform margins
  TCanvas* CreateGridCanvas(const std::string& name, const std::string& title,
                            int nCols, int nRows,
                            int width = 1200, int height = 900,
                            const PadMargins& margins = PadMargins{0.12, 0.04, 0.06, 0.10})
  {
    return CreateGridCanvas(name, title, std::vector<std::string>{}, nCols, nRows, width, height, margins);
  }

  // Creates a grid canvas with an associated target directory path.
  TCanvas* CreateGridCanvas(const std::string& name, const std::string& title,
                            const std::vector<std::string>& targetPath,
                            int nCols, int nRows,
                            int width = 1200, int height = 900,
                            const PadMargins& margins = PadMargins{0.12, 0.04, 0.06, 0.10})
  {
    if (nCols <= 0 || nRows <= 0) {
      throw std::runtime_error("[FATAL] CanvasManager: nCols and nRows must be positive");
    }
    CleanGlobalCollision(name);
    const std::string cTitle = title.empty() ? name : title;
    auto c = std::make_unique<TCanvas>(name.c_str(), cTitle.c_str(), width, height);
    c->Divide(nCols, nRows, 0.001, 0.001);

    for (int i = 1; i <= nCols * nRows; ++i) {
      if (TPad* p = static_cast<TPad*>(c->GetPad(i))) {
        SetPadMargins(p, margins);
      }
    }

    if (!targetPath.empty()) {
      canvasPaths[name] = targetPath;
    }
    return RecordCanvas(std::move(c));
  }

  static void SetPadMargins(TPad* pad, const PadMargins& margins)
  {
    if (!pad)
      return;
    pad->SetLeftMargin(margins.left);
    pad->SetRightMargin(margins.right);
    pad->SetTopMargin(margins.top);
    pad->SetBottomMargin(margins.bottom);
  }

  static void SetLog(TPad* pad, bool logX = false, bool logY = false, bool logZ = false)
  {
    if (!pad)
      return;
    pad->SetLogx(logX ? 1 : 0);
    pad->SetLogy(logY ? 1 : 0);
    pad->SetLogz(logZ ? 1 : 0);
  }

  static void SetGrid(TPad* pad, bool gridX = true, bool gridY = true)
  {
    if (!pad)
      return;
    pad->SetGridx(gridX ? 1 : 0);
    pad->SetGridy(gridY ? 1 : 0);
  }

  // Hides X axis title and tick labels on the top pad so they do not collide with the ratio pad below
  template <typename T>
  static void FormatTopPadAxes(T* plot, double titleSize = 0.045, double labelSize = 0.040, double titleOffset = 1.25)
  {
    if (!plot)
      return;
    plot->GetXaxis()->SetLabelSize(0.0);
    plot->GetXaxis()->SetTitleSize(0.0);
    plot->GetYaxis()->SetTitleSize(titleSize);
    plot->GetYaxis()->SetLabelSize(labelSize);
    plot->GetYaxis()->SetTitleOffset(titleOffset);
  }

  // Formats ratio histogram or frame in the bottom pad with adjusted font scaling and tick divisions
  template <typename T>
  static void FormatBottomPadAxes(T* plot, const std::string& yTitle = "Ratio",
                                  double yMin = 0.5, double yMax = 1.5,
                                  double fontScale = 2.33, double baseTextSize = 0.045,
                                  int nDivisions = 505)
  {
    if (!plot)
      return;
    plot->SetMinimum(yMin);
    plot->SetMaximum(yMax);
    plot->GetYaxis()->SetTitle(yTitle.c_str());
    plot->GetYaxis()->SetNdivisions(nDivisions);

    plot->GetXaxis()->SetTitleSize(baseTextSize * fontScale);
    plot->GetXaxis()->SetLabelSize(baseTextSize * fontScale);
    plot->GetXaxis()->SetTitleOffset(0.9);

    plot->GetYaxis()->SetTitleSize(baseTextSize * fontScale);
    plot->GetYaxis()->SetLabelSize(baseTextSize * fontScale);
    plot->GetYaxis()->SetTitleOffset(1.0 / fontScale);
  }

  // Draws a horizontal line (e.g. unity line y = 1.0 or zero reference y = 0.0) on a pad
  static TLine* DrawHorizontalLine(TPad* pad, double xMin, double xMax, double yVal = 1.0,
                                   int style = kDashed, int color = kGray + 2, int width = 2)
  {
    if (!pad)
      return nullptr;
    pad->cd();
    TLine* line = new TLine(xMin, yVal, xMax, yVal);
    line->SetBit(kCanDelete);
    line->SetLineStyle(style);
    line->SetLineColor(color);
    line->SetLineWidth(width);
    line->Draw("SAME");
    return line;
  }

  // Draws a pair of horizontal boundary lines for a range [yLow, yHigh] (e.g. +/- 2 sigma bands)
  static void DrawHorizontalRange(TPad* pad, double xMin, double xMax, double yLow, double yHigh,
                                  int style = kDashed, int color = kBlack, int width = 2)
  {
    DrawHorizontalLine(pad, xMin, xMax, yLow, style, color, width);
    DrawHorizontalLine(pad, xMin, xMax, yHigh, style, color, width);
  }

  // Draws a vertical reference line on a pad with managed primitive lifetime (SetBit(kCanDelete))
  static TLine* DrawVerticalLine(TPad* pad, double xVal, double yMin, double yMax,
                                 int style = kDashed, int color = kBlack, int width = 2)
  {
    if (!pad)
      return nullptr;
    pad->cd();
    TLine* line = new TLine(xVal, yMin, xVal, yMax);
    line->SetBit(kCanDelete);
    line->SetLineStyle(style);
    line->SetLineColor(color);
    line->SetLineWidth(width);
    line->Draw("SAME");
    return line;
  }

  // Draws a pair of vertical boundary lines for a range [xLow, xHigh]
  static void DrawVerticalRange(TPad* pad, double xLow, double xHigh, double yMin, double yMax,
                                int style = kDashed, int color = kBlack, int width = 2)
  {
    DrawVerticalLine(pad, xLow, yMin, yMax, style, color, width);
    DrawVerticalLine(pad, xHigh, yMin, yMax, style, color, width);
  }

  // ---------------------------------------------------------------------------
  // Statistics Box Management (TPaveStats)
  // ---------------------------------------------------------------------------

  // Sets ROOT's global histogram statistics display (TPaveStats).
  // In CanvasManager, stats are disabled by default (optStat = 0).
  static void SetOptStat(bool enable = false, int optStat = 1111)
  {
    if (gStyle) {
      gStyle->SetOptStat(enable ? optStat : 0);
    }
  }

  // Toggles the statistics box for a specific histogram
  static void SetStats(TH1* h, bool enable = false)
  {
    if (h) {
      h->SetStats(enable);
    }
  }

  // Removes an already drawn stats box ("stats" primitive) from a pad
  static void RemoveStats(TPad* pad)
  {
    if (!pad)
      return;
    if (TObject* stats = pad->FindObject("stats")) {
      pad->GetListOfPrimitives()->Remove(stats);
      delete stats;
      pad->Modified();
    }
  }

  // Instance method to enable/disable stats box
  void EnableStats(bool enable = true, int optStat = 1111)
  {
    SetOptStat(enable, optStat);
  }

  // ---------------------------------------------------------------------------
  // Multi-Histogram Automatic Y-Axis Range Determination
  // ---------------------------------------------------------------------------

  // Computes the global minimum and maximum across a collection of 1D histograms,
  // taking into account error bars, non-empty bins, linear vs log scale, and headroom.
  // Multi-dimensional histograms (TH2, TH3) are skipped because their Y-axis represents
  // a phase-space coordinate rather than bin content.
  static std::pair<double, double> ComputeGlobalYRange(std::span<TH1* const> hists,
                                                       const YRangeOptions& opts = YRangeOptions{})
  {
    double globalMin = std::numeric_limits<double>::infinity();
    double globalMax = -std::numeric_limits<double>::infinity();
    double posMin = std::numeric_limits<double>::infinity();
    bool foundAny = false;

    for (const auto* h : hists) {
      if (!h || h->GetDimension() != 1)
        continue;

      const int nBins = h->GetNbinsX();
      for (int b = 1; b <= nBins; ++b) {
        const double c = h->GetBinContent(b);
        const double e = opts.includeErrors ? h->GetBinError(b) : 0.0;

        // Skip completely unpopulated bins
        if (c == 0.0 && e == 0.0)
          continue;

        const double valLow = c - e;
        const double valHigh = c + e;

        if (valLow < globalMin)
          globalMin = valLow;
        if (valHigh > globalMax)
          globalMax = valHigh;

        if (c > 0.0 && c < posMin)
          posMin = c;
        if (valLow > 0.0 && valLow < posMin)
          posMin = valLow;

        foundAny = true;
      }
    }

    if (!foundAny) {
      return opts.isLogY ? std::pair<double, double>{0.1, 1.0} : std::pair<double, double>{0.0, 1.0};
    }

    if (opts.isLogY) {
      if (posMin == std::numeric_limits<double>::infinity() || posMin <= 0.0) {
        posMin = 1e-4;
      }
      if (globalMax <= 0.0) {
        globalMax = 1.0;
      }
      // The margins as fractions of the decades the data span, as they are of the span in linear scale: a fixed factor
      // (5 above, 0.5 below, until 24 October 2026) left the peak of a pi TPC purity fit, spanning seven decades, under
      // the label. At least one decade, so that a single value or a flat set still gets room.
      const double decades = std::max(globalMax / posMin, 10.0);
      double yMin = posMin / std::pow(decades, opts.bottomMargin);
      double yMax = globalMax * std::pow(decades, opts.topMargin);
      if (yMin <= 0.0)
        yMin = 1e-4;
      if (yMax <= yMin)
        yMax = yMin * 10.0;
      return {yMin, yMax};
    }

    // Linear scale calculation
    double yMin = globalMin;
    double yMax = globalMax;

    if (yMin == yMax) {
      const double delta = (yMax != 0.0) ? std::abs(yMax) * 0.2 : 1.0;
      yMin -= delta;
      yMax += delta;
    } else {
      const double span = yMax - yMin;
      yMax += opts.topMargin * span;
      if (opts.zeroBaseline && yMin >= 0.0) {
        yMin = 0.0;
      } else {
        yMin -= opts.bottomMargin * span;
      }
    }

    return {yMin, yMax};
  }

  // Adjusts the Y-axis range for a collection of 1D histograms based on their global min and max.
  // Accepts any contiguous container (std::vector, std::array, std::span) via std::span<TH1* const>.
  static void AdjustYRange(std::span<TH1* const> hists,
                           const YRangeOptions& opts = YRangeOptions{})
  {
    auto [yMin, yMax] = ComputeGlobalYRange(hists, opts);
    for (auto* h : hists) {
      if (h && h->GetDimension() == 1) {
        h->GetYaxis()->SetRangeUser(yMin, yMax);
      }
    }
  }

  // Overload for initializer_list: CanvasManager::AdjustYRange({h1, h2, h3})
  static void AdjustYRange(std::initializer_list<TH1*> hists,
                           const YRangeOptions& opts = YRangeOptions{})
  {
    AdjustYRange(std::span<TH1* const>(hists.begin(), hists.size()), opts);
  }

  // Convenience overloads with simple boolean flags
  static void AdjustYRange(std::span<TH1* const> hists,
                           bool isLogY, bool zeroBaseline = false)
  {
    YRangeOptions opts;
    opts.isLogY = isLogY;
    opts.zeroBaseline = zeroBaseline;
    AdjustYRange(hists, opts);
  }

  static void AdjustYRange(std::initializer_list<TH1*> hists,
                           bool isLogY, bool zeroBaseline = false)
  {
    YRangeOptions opts;
    opts.isLogY = isLogY;
    opts.zeroBaseline = zeroBaseline;
    AdjustYRange(std::span<TH1* const>(hists.begin(), hists.size()), opts);
  }

  // Inspects all 1D histograms currently drawn on pad, computes their global Y-range,
  // and applies it to all of them. Automatically detects log scale from pad->GetLogy().
  static void AdjustPadYRange(TPad* pad, const YRangeOptions& opts = YRangeOptions{})
  {
    if (!pad)
      return;
    std::vector<TH1*> hists;
    TIter next(pad->GetListOfPrimitives());
    TObject* obj = nullptr;
    while ((obj = next())) {
      if (obj->InheritsFrom(TH1::Class())) {
        auto* h = static_cast<TH1*>(obj);
        if (h->GetDimension() == 1) {
          hists.push_back(h);
        }
      }
    }
    YRangeOptions effectiveOpts = opts;
    if (pad->GetLogy() != 0) {
      effectiveOpts.isLogY = true;
    }
    AdjustYRange(hists, effectiveOpts);
    pad->Modified();
    pad->Update();
  }

  static void AdjustPadYRange(TPad* pad, bool isLogY, bool zeroBaseline = false)
  {
    YRangeOptions opts;
    opts.isLogY = isLogY;
    opts.zeroBaseline = zeroBaseline;
    AdjustPadYRange(pad, opts);
  }

  // The static AdjustYRange / AdjustPadYRange above can also be called on an instance;
  // an instance overload with the same signature is not allowed (and would recurse).
  void AdjustCanvasYRange(const std::string& canvasName,
                         const YRangeOptions& opts = YRangeOptions{}) const
  {
    TCanvas* c = RequireCanvas(canvasName);
    AdjustPadYRange(c, opts);
  }

  // ---------------------------------------------------------------------------
  // 3. ALICE Visual Standards & Legends
  // ---------------------------------------------------------------------------

  // Draws standard ALICE text header:
  //   ALICE <status>
  //   <collisionSystem>
  //
  // On the pad passed, like the other Draw* helpers here, and not on gPad: which
  // pad is current after a drawing call is an accident of that call, and on a
  // ratio canvas the wrong guess puts the label on the pulls.
  static void DrawAliceLabel(TPad* pad, double x = 0.18, double y = 0.84,
                             const std::string& status = "Work in progress",
                             const std::string& collisionSystem = "pp, #sqrt{#it{s}} = 13.6 TeV",
                             double textSize = 0.045)
  {
    if (!pad)
      return;
    pad->cd();
    TLatex latex;
    latex.SetNDC();
    latex.SetTextFont(42);
    latex.SetTextSize(textSize);

    std::string aliceLine = status.empty() ? "#bf{ALICE}" : std::format("#bf{{ALICE}} {}", status);
    latex.DrawLatex(x, y, aliceLine.c_str());

    if (!collisionSystem.empty()) {
      latex.DrawLatex(x, y - textSize * 1.3, collisionSystem.c_str());
    }
  }

  // Creates a transparent borderless legend adhering to ALICE font conventions
  static TLegend* CreateLegend(double x1 = 0.65, double y1 = 0.65, double x2 = 0.88, double y2 = 0.88,
                               int nColumns = 1, double textSize = 0.04)
  {
    TLegend* leg = new TLegend(x1, y1, x2, y2);
    leg->SetBit(kCanDelete);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextFont(42);
    leg->SetTextSize(textSize);
    leg->SetNColumns(nColumns);
    return leg;
  }

  // Applies HEP/ALICE standard visual styling to 1D histograms
  static void SetHistogramStyle(TH1* h, int color, int markerStyle = 20,
                                double markerSize = 1.5, int lineWidth = 2,
                                int fillStyle = 3001)
  {
    if (!h)
      return;
    h->SetMarkerStyle(markerStyle);
    h->SetMarkerColor(color);
    h->SetMarkerSize(markerSize);
    h->SetLineColor(color);
    h->SetLineWidth(lineWidth);
    h->SetFillStyle(fillStyle);
    h->SetFillColor(color);
    h->GetYaxis()->SetTitleSize(0.045);
    h->GetYaxis()->SetTitleOffset(1.0);
    h->GetYaxis()->SetLabelSize(0.045);
  }

  // ---------------------------------------------------------------------------
  // 4. IO & Export Integration
  // ---------------------------------------------------------------------------

  // Static helper to write any canvas into targetDir/pathNodes
  static void WriteCanvas(TCanvas* canvas, TDirectory* baseDir, const std::vector<std::string>& pathNodes)
  {
    if (!canvas || !baseDir)
      return;
    if (pathNodes.empty()) {
      WriteCanvas(canvas, baseDir);
      return;
    }
    if (TDirectory* dir = RootIO::GetOrCreatePath(baseDir, pathNodes, false)) {
      dir->cd();
      canvas->Write(nullptr, TObject::kOverwrite);
    }
  }

  // Static helper to write a canvas directly into a target TDirectory
  static void WriteCanvas(TCanvas* canvas, TDirectory* targetDir)
  {
    if (!canvas || !targetDir)
      return;
    targetDir->cd();
    canvas->Write(nullptr, TObject::kOverwrite);
  }

  // Writes a managed canvas by name using its registered targetPath (or baseDir if none)
  void WriteCanvas(const std::string& name, TDirectory* baseDir) const
  {
    TCanvas* c = RequireCanvas(name);
    ApplyAutoYRange(name);
    auto it = canvasPaths.find(name);
    if (it != canvasPaths.end() && !it->second.empty()) {
      WriteCanvas(c, baseDir, it->second);
    } else {
      WriteCanvas(c, baseDir);
    }
  }

  // Writes a managed canvas by name into an explicit path, overriding any registered path
  void WriteCanvas(const std::string& name, TDirectory* baseDir, const std::vector<std::string>& pathNodes) const
  {
    TCanvas* c = RequireCanvas(name);
    ApplyAutoYRange(name);
    WriteCanvas(c, baseDir, pathNodes);
  }

  // Writes a managed canvas (to its registered path, or to baseDir), then destroys it.
  // For canvases drawn once per cell and never read back, such as the fits: kept
  // registered until a WriteAll, every one of them would stay in memory - and in
  // gROOT's global canvas list - for the whole task. WriteAll is for the other kind,
  // the summary canvases that accumulate over a loop and are complete only at the end.
  void WriteAndDestroy(const std::string& name, TDirectory* baseDir)
  {
    WriteCanvas(name, baseDir);
    DestroyCanvas(name);
  }

  // Writes all managed canvases, using each canvas's registered targetPath (or baseDir if none)
  void WriteAll(TDirectory* baseDir) const
  {
    if (!baseDir)
      return;
    for (const auto& name : canvasOrder) {
      if (auto it = canvases.find(name); it != canvases.end() && it->second) {
        ApplyAutoYRange(name);
        auto pit = canvasPaths.find(name);
        if (pit != canvasPaths.end() && !pit->second.empty()) {
          WriteCanvas(it->second.get(), baseDir, pit->second);
        } else {
          WriteCanvas(it->second.get(), baseDir);
        }
      }
    }
  }

  // Writes all managed canvases into the specified directory path
  void WriteAll(TDirectory* baseDir, const std::vector<std::string>& pathNodes) const
  {
    if (!baseDir)
      return;
    if (TDirectory* dir = RootIO::GetOrCreatePath(baseDir, pathNodes, false)) {
      dir->cd();
      for (const auto& [name, c] : canvases) {
        if (c) {
          ApplyAutoYRange(name);
          c->Write(nullptr, TObject::kOverwrite);
        }
      }
    }
  }

  // Saves a single canvas to disk (e.g. .pdf, .png, .svg), creating parent directories if needed
  static void ExportCanvas(TCanvas* canvas, const std::string& filePath)
  {
    if (!canvas)
      return;
    std::filesystem::path p(filePath);
    if (p.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(p.parent_path(), ec);
      if (ec) {
        throw std::runtime_error(std::format("[FATAL] CanvasManager: Cannot create directory '{}': {}",
                                             p.parent_path().string(), ec.message()));
      }
    }
    canvas->SaveAs(filePath.c_str());
  }

  void ExportCanvas(const std::string& name, const std::string& filePath) const
  {
    TCanvas* c = RequireCanvas(name);
    ApplyAutoYRange(name);
    ExportCanvas(c, filePath);
  }

  // Exports all managed canvases to individual image/PDF files in outputDir
  void ExportAll(const std::string& outputDir, const std::string& extension = "pdf") const
  {
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
      throw std::runtime_error(std::format("[FATAL] CanvasManager: Cannot create directory '{}': {}",
                                           outputDir, ec.message()));
    }
    for (const auto& [name, c] : canvases) {
      if (c) {
        ApplyAutoYRange(name);
        std::string path = (std::filesystem::path(outputDir) / (name + "." + extension)).string();
        c->SaveAs(path.c_str());
      }
    }
  }

  // Exports a sequence of canvases into a single multi-page PDF document
  static void ExportMultiPagePdf(const std::string& pdfPath, const std::vector<TCanvas*>& canvasList)
  {
    std::vector<TCanvas*> valid;
    for (auto* c : canvasList) {
      if (c)
        valid.push_back(c);
    }
    if (valid.empty())
      return;

    std::filesystem::path p(pdfPath);
    if (p.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(p.parent_path(), ec);
      if (ec) {
        throw std::runtime_error(std::format("[FATAL] CanvasManager: Cannot create directory '{}': {}",
                                             p.parent_path().string(), ec.message()));
      }
    }

    if (valid.size() == 1) {
      valid.front()->Print(pdfPath.c_str());
      return;
    }

    // ROOT syntax for multi-page PDF generation:
    // first page with '[' begins document, last page with ']' closes it.
    valid.front()->Print((pdfPath + "[").c_str());
    for (auto* c : valid) {
      c->Print(pdfPath.c_str());
    }
    valid.back()->Print((pdfPath + "]").c_str());
  }

  // Exports managed canvases (either specified subset or all in insertion order) to a multi-page PDF
  void ExportMultiPagePdf(const std::string& pdfPath, const std::vector<std::string>& canvasNames = {}) const
  {
    std::vector<TCanvas*> list;
    if (canvasNames.empty()) {
      for (const auto& name : canvasOrder) {
        if (auto it = canvases.find(name); it != canvases.end() && it->second) {
          ApplyAutoYRange(name);
          list.push_back(it->second.get());
        }
      }
    } else {
      for (const auto& name : canvasNames) {
        list.push_back(RequireCanvas(name));
        ApplyAutoYRange(name);
      }
    }
    ExportMultiPagePdf(pdfPath, list);
  }

  // Scans a TFile or sub-directory and exports all stored TCanvas objects into a single multi-page PDF
  static void ExportFileCanvasesToPdf(TFile* file, const std::string& pdfPath, const std::string& subDir = "")
  {
    if (!file || file->IsZombie())
      throw std::runtime_error("[FATAL] CanvasManager: File is null or zombie");

    TDirectory* targetDir = file;
    if (!subDir.empty()) {
      targetDir = file->GetDirectory(subDir.c_str());
      if (!targetDir) {
        throw std::runtime_error(std::format("[FATAL] CanvasManager: Directory '{}' not found in file '{}'",
                                             subDir, file->GetName()));
      }
    }

    std::vector<std::unique_ptr<TCanvas>> loadedCanvases;
    std::vector<TCanvas*> canvasPtrs;

    TIter next(targetDir->GetListOfKeys());
    TKey* key = nullptr;
    while ((key = static_cast<TKey*>(next()))) {
      TClass* cl = gROOT->GetClass(key->GetClassName());
      if (cl && cl->InheritsFrom("TCanvas")) {
        if (TCanvas* c = dynamic_cast<TCanvas*>(key->ReadObj())) {
          loadedCanvases.emplace_back(c);
          canvasPtrs.push_back(c);
        }
      }
    }

    if (canvasPtrs.empty()) {
      Logger::Warn("CanvasManager: No canvases found in '{}'", targetDir->GetName());
      return;
    }

    ExportMultiPagePdf(pdfPath, canvasPtrs);
  }

 private:
  std::map<std::string, std::unique_ptr<TCanvas>> canvases;
  std::vector<std::string> canvasOrder;
  std::map<std::string, std::vector<std::string>> canvasPaths;
  std::map<std::string, YRangeOptions> autoYRanges; // see SetAutoYRange

  // Idempotent: the range is computed from the bin contents, never from a previous
  // minimum or maximum, so a canvas written and then exported is adjusted to the same
  // range twice.
  void ApplyAutoYRange(const std::string& name) const
  {
    if (auto it = autoYRanges.find(name); it != autoYRanges.end())
      AdjustCanvasYRange(name, it->second);
  }

  // Protects against ROOT's global canvas list collisions (see CLAUDE.md).
  // If the canvas is already owned by this manager, it is safely destroyed and replaced.
  // If an external canvas with the same name exists in gROOT, throws an error to prevent
  // double-free corruption and memory collisions.
  void CleanGlobalCollision(const std::string& name)
  {
    auto it = canvases.find(name);
    if (it != canvases.end()) {
      it->second.reset(); // Destroys canvas and automatically unregisters from gROOT
    } else if (gROOT && gROOT->GetListOfCanvases()->FindObject(name.c_str())) {
      throw std::runtime_error(std::format(
        "[FATAL] CanvasManager: A TCanvas named '{}' already exists in gROOT but is not owned "
        "by this manager. Canvas names must be unique to prevent collisions and memory corruption.",
        name));
    }
  }

  TCanvas* RecordCanvas(std::unique_ptr<TCanvas> canvas)
  {
    if (!canvas)
      return nullptr;
    const std::string name = canvas->GetName();
    TCanvas* raw = canvas.get();
    if (std::ranges::find(canvasOrder, name) == canvasOrder.end()) {
      canvasOrder.push_back(name);
    }
    canvases[name] = std::move(canvas);
    return raw;
  }
};

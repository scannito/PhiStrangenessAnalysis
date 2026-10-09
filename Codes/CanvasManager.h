#pragma once

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
#include <filesystem>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
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

class CanvasManager
{
 public:
  struct PadMargins {
    double left{0.14};
    double right{0.05};
    double top{0.08};
    double bottom{0.12};
  };

  // Holds the components of a dual-pad canvas (e.g. distribution on top, ratio on bottom)
  struct RatioCanvas {
    TCanvas* canvas{nullptr};
    TPad* topPad{nullptr};
    TPad* bottomPad{nullptr};
    double fontScale{1.0}; // Compensates font sizes in bottom pad (pad height ratio)
  };

  CanvasManager() = default;
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
    CleanGlobalCollision(name);
    const std::string cTitle = title.empty() ? name : title;
    auto c = std::make_unique<TCanvas>(name.c_str(), cTitle.c_str(), width, height);
    SetPadMargins(c.get(), margins);
    return RecordCanvas(std::move(c));
  }

  // Registers an existing unique_ptr<TCanvas>, taking ownership.
  TCanvas* RegisterCanvas(std::unique_ptr<TCanvas> canvas)
  {
    return RecordCanvas(std::move(canvas));
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

  // Releases ownership to caller, removing from manager
  std::unique_ptr<TCanvas> ReleaseCanvas(const std::string& name)
  {
    auto it = canvases.find(name);
    if (it == canvases.end())
      return nullptr;
    auto c = std::move(it->second);
    canvases.erase(it);
    std::erase(canvasOrder, name);
    return c;
  }

  const std::map<std::string, std::unique_ptr<TCanvas>>& GetAllCanvases() const { return canvases; }
  const std::vector<std::string>& GetCanvasOrder() const { return canvasOrder; }

  void Clear()
  {
    canvases.clear();
    canvasOrder.clear();
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

    TCanvas* raw = RecordCanvas(std::move(c));
    return RatioCanvas{raw, topPad, bottomPad, fontScale};
  }

  // Creates a canvas divided into nCols x nRows subpads with uniform margins
  TCanvas* CreateGridCanvas(const std::string& name, const std::string& title,
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
  static void FormatTopHist(TH1* h)
  {
    if (!h)
      return;
    h->GetXaxis()->SetLabelSize(0.0);
    h->GetXaxis()->SetTitleSize(0.0);
  }

  // Formats ratio histogram in the bottom pad with adjusted font scaling and tick divisions
  static void FormatRatioHist(TH1* h, const std::string& yTitle = "Ratio",
                              double yMin = 0.5, double yMax = 1.5,
                              double fontScale = 2.33, double baseTextSize = 0.045)
  {
    if (!h)
      return;
    h->GetYaxis()->SetTitle(yTitle.c_str());
    h->GetYaxis()->SetRangeUser(yMin, yMax);
    h->GetYaxis()->SetNdivisions(505);

    h->GetXaxis()->SetTitleSize(baseTextSize * fontScale);
    h->GetXaxis()->SetLabelSize(baseTextSize * fontScale);
    h->GetXaxis()->SetTitleOffset(0.9);

    h->GetYaxis()->SetTitleSize(baseTextSize * fontScale);
    h->GetYaxis()->SetLabelSize(baseTextSize * fontScale);
    h->GetYaxis()->SetTitleOffset(1.0 / fontScale);
  }

  // Draws a horizontal reference line (e.g. unity line y = 1.0 or y = 0.0) on a pad
  static TLine* DrawReferenceLine(TPad* pad, double xMin, double xMax, double yVal = 1.0,
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

  // ---------------------------------------------------------------------------
  // 3. ALICE Visual Standards & Legends
  // ---------------------------------------------------------------------------

  // Draws standard ALICE text header:
  //   ALICE <status>
  //   <collisionSystem>
  static void DrawAliceLabel(double x = 0.18, double y = 0.84,
                             const std::string& status = "Work in progress",
                             const std::string& collisionSystem = "pp, #sqrt{#it{s}} = 13.6 TeV",
                             double textSize = 0.045)
  {
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

  // ---------------------------------------------------------------------------
  // 4. IO & Export Integration
  // ---------------------------------------------------------------------------

  // Static helper to write any canvas into targetDir/pathNodes
  static void WriteCanvas(TCanvas* canvas, TDirectory* baseDir, const std::vector<std::string>& pathNodes)
  {
    if (!canvas || !baseDir)
      return;
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

  // Writes a managed canvas by name
  void WriteCanvas(const std::string& name, TDirectory* baseDir, const std::vector<std::string>& pathNodes) const
  {
    WriteCanvas(RequireCanvas(name), baseDir, pathNodes);
  }

  // Writes all managed canvases into the specified directory path
  void WriteAll(TDirectory* baseDir, const std::vector<std::string>& pathNodes) const
  {
    if (!baseDir)
      return;
    if (TDirectory* dir = RootIO::GetOrCreatePath(baseDir, pathNodes, false)) {
      dir->cd();
      for (const auto& [_, c] : canvases) {
        if (c)
          c->Write(nullptr, TObject::kOverwrite);
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
    ExportCanvas(RequireCanvas(name), filePath);
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
          list.push_back(it->second.get());
        }
      }
    } else {
      for (const auto& name : canvasNames) {
        list.push_back(RequireCanvas(name));
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
      std::cerr << "[WARNING] CanvasManager: No canvases found in '" << targetDir->GetName() << "'" << std::endl;
      return;
    }

    ExportMultiPagePdf(pdfPath, canvasPtrs);
  }

 private:
  std::map<std::string, std::unique_ptr<TCanvas>> canvases;
  std::vector<std::string> canvasOrder;

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

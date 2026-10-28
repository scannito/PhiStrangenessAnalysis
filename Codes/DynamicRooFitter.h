#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "CanvasManager.h"
#include "FitConfigManager.h"
#include "HistogramUtils.h"
#include "Logger.h"

#include "RooAbsPdf.h"
#include "RooAddPdf.h"
#include "RooArgList.h"
#include "RooBernstein.h"
#include "RooChebychev.h"
#include "RooConstVar.h"
#include "RooCrystalBall.h"
#include "RooDataHist.h"
#include "RooDataSet.h"
#include "RooExponential.h"
#include "RooFFTConvPdf.h"
#include "RooFitResult.h"
#include "RooFormulaVar.h"
#include "RooGaussian.h"
#include "RooGenericPdf.h"
#include "RooHist.h"
#include "RooMsgService.h"
#include "RooPlot.h"
#include "RooPolynomial.h"
#include "RooProdPdf.h"
#include "RooProduct.h"
#include "RooRealVar.h"
#include "RooVoigtian.h"
#include "TH1.h"
#include "TLegend.h"
#include "TObject.h"
#include "TPad.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <initializer_list>
#include <iostream>
#include <array>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

class DynamicRooFitter
{
 public:
  DynamicRooFitter(TH1* h, const FitConfig& cfg) : h1Data(h), config(cfg)
  {
    // Suppress verbose RooFit INFO/DEBUG messages during batch fitting
    RooMsgService::instance().setGlobalKillBelow(RooFit::WARNING);

    obs = Own<RooRealVar>(cfg.obs.name.c_str(), cfg.obs.title.c_str(), cfg.obs.min, cfg.obs.max);
    // The data before the model: the import moves the observable's range outwards onto the histogram's bin edges
    // (0.995-1.1 became 0.9945-1.101 for the phi), and a model built before that keeps what it took from the old range.
    // The FFT grid of RelBWKK stayed on 0.995-1.1 and the signal came out normalised 22% high in every point (pp 2026,
    // phi mult 15-20%, pT 1.4-2: nsig 1.31e6 instead of 1.10e6, the curve 35% above the peak).
    data = std::make_unique<RooDataHist>("dataHist", "Data", *obs, RooFit::Import(*h1Data));

    // Dynamic model parsing for both signal and background
    sigPdf = ParseModelString(cfg.model.sigModel, "sig");
    bkgPdf = ParseModelString(cfg.model.bkgModel, "bkg");

    nsig = CreateVar("nsig");
    nbkg = CreateVar("nbkg");

    model = Own<RooAddPdf>("model", "Total Model", RooArgList(*sigPdf, *bkgPdf), RooArgList(*nsig, *nbkg));

    // The configured values of every parameter, which every start of DoFit begins from: taken here, where they are
    // the configuration's, so that a second DoFit on this fitter could not begin from the first one's result.
    parameters.reset(model->getParameters(RooArgSet(*obs)));
    configured.reset(parameters->snapshot());
  }

  // Tear down the owned RooFit objects in strict reverse creation order:
  // composites (RooAddPdf, RooProdPdf) die before the RooRealVars they use as
  // servers. ~vector() alone would NOT guarantee this — the destruction order
  // of vector elements is unspecified, and it genuinely differs between
  // implementations (libc++ destroys back-to-front, libstdc++ front-to-back).
  // pop_back() always destroys the last element, so this is portable.
  ~DynamicRooFitter()
  {
    configured.reset(); // the sets and the data refer to the owned variables: gone before them
    data.reset();
    parameters.reset();
    while (!ownedObjects.empty())
      ownedObjects.pop_back();
  }

  // Fits the model from several starts and keeps the lowest minimum (RooFitKnowledge/ROADMAP.md, phase 3). A single
  // fit from the configuration was measured to stop far from the minimum with every flag clean (DESIGN_HISTORY.md,
  // "Several starts per fit": on the 22 October production, a median 2.4e5 of NLL above it for pi_tpc). Every start
  // begins from the configured values and from the cell itself, so a cell's result depends on nothing that ran
  // before it:
  //   "config"  the configured values, fitted as DoFit fitted before there were several starts (fitTo without
  //             offset): the winner is never worse than that fit, and where it wins the result is that fit's;
  //   "yields"  each free yield at half the cell's counts (from one configuration for every cell they start up to
  //             10000 times off);
  //   "staged"  the yields as above, and a first fit with the parameters the model declared with Stage held (the
  //             highest Chebychev term at 0, so a background one order lower; the DSCB tails at their configured
  //             values), then released. Only for a model that declares any.
  // The derived starts are an Offset("bin") fit and a refit from its result. Offset("bin") because the likelihood of
  // these cells is of order 1e7 to 1e10 while the minimum is resolved to 1e-3: in RooFitKnowledge note 09 (section
  // 9.9) it gave the yields of Offset(true) with the edm test passing in 96% of the cells instead of 88%.
  // The winner is the lowest NLL, the likelihood itself without offset (an offset fit reports its minimum relative to
  // its own reference), among the starts where it is finite; a later start wins only when lower by more than 0.01.
  // Nothing else decides it: invalid evaluations count Minuit's path, not the point it ends at, and the covariance
  // is the estimate of the errors, not the place of the minimum. Both stay flags on the winner.
  int DoFit()
  {
    RooDataHist& dataHist = *data;
    const double counts = dataHist.sumEntries();
    const std::unique_ptr<RooAbsReal> nll(model->createNLL(dataHist, RooFit::Extended(true)));
    std::vector<Start> starts;

    // One start: the parameters back to their configured values, 'fits' run from there, and the result they return
    // recorded with the NLL at its minimum (where the last fit left the parameters).
    auto start = [&](std::string label, auto fits) {
      parameters->assign(*configured);
      std::unique_ptr<RooFitResult> result = fits();
      FitDiagnostics d = DiagnosticsOf(*result, counts);
      d.minNll = nll->getVal();
      d.start = std::move(label);
      starts.push_back({std::move(result), std::move(d)});
    };

    start("config", [&] { return Fit(dataHist, false); });
    start("yields", [&] {
      ScaleYields(counts);
      return FitAndRefit(dataHist);
    });
    if (!staged.empty())
      start("staged", [&] {
        ScaleYields(counts);
        FitWithStagedHeld(dataHist);
        return FitAndRefit(dataHist);
      });

    KeepLowest(starts);
    return fitResult->status();
  }

  // What the last fit was: the winning start's facts, and the NLL the "config" start reached next to its.
  FitDiagnostics Diagnostics() const
  {
    RequireFit();
    return diagnostics;
  }

  // Structure to hold the results of the yield and purity calculations
  struct FitResults {
    std::pair<double, double> signalAndError{0.0, 0.0};
    std::pair<double, double> bkgInSignalRegionAndError{0.0, 0.0};
    std::pair<double, double> purityAndError{0.0, 0.0};
    std::pair<double, double> bkgInSidebandAndError{0.0, 0.0};
    std::pair<double, double> bkgRatioAndError{0.0, 0.0};
  };

  FitResults ExtractYieldsAndPurity()
  {
    RequireFit();

    FitResults results;

    // Retrieve integration limits dynamically based on the configuration logic
    auto [minRange, maxRange] = CalculateIntegrationLimits();

    std::string modeLog = std::holds_alternative<FixedRange>(config.integration.signalWindow) ? "FIXED" : "N-SIGMA";
    Logger::Debug("Integrating in {} range: [{:g} , {:g}]", modeLog, minRange, maxRange);

    obs->setRange("signalRegion", minRange, maxRange);

    // --- 1. SIGNAL YIELD & ERROR ---
    std::unique_ptr<RooAbsReal> sigInt(sigPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("signalRegion")));
    RooProduct signalYield("signalYield", "Signal Yield", RooArgList(*nsig, *sigInt));
    results.signalAndError = {signalYield.getVal(), signalYield.getPropagatedError(*fitResult, RooArgSet(*obs))};

    // --- 2. BACKGROUND YIELD & ERROR ---
    std::unique_ptr<RooAbsReal> bkgInt(bkgPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("signalRegion")));
    RooProduct bkgYield("bkgYield", "Background Yield", RooArgList(*nbkg, *bkgInt));
    results.bkgInSignalRegionAndError = {bkgYield.getVal(), bkgYield.getPropagatedError(*fitResult, RooArgSet(*obs))};

    // --- 3. OPTIONAL PURITY & ERROR (S / S+B) ---
    if (config.integration.calculatePurity) {
      RooFormulaVar purity("purity", "Signal Purity", "@0 / (@0 + @1)", RooArgList(signalYield, bkgYield));
      results.purityAndError = {purity.getVal(), purity.getPropagatedError(*fitResult, RooArgSet(*obs))};
    }

    // --- 4. OPTIONAL BACKGROUND IN SIDEBAND YIELD & ERROR ---
    if (config.integration.sideband) {
      double sbMin = config.integration.sideband->range.first;
      double sbMax = config.integration.sideband->range.second;
      obs->setRange("sidebandRegion", sbMin, sbMax);

      if (config.integration.sideband->fromFit) {
        std::unique_ptr<RooAbsReal> bkgIntSB(bkgPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("sidebandRegion")));
        RooProduct bkgYieldSB("bkgYieldSB", "Bkg in Sideband", RooArgList(*nbkg, *bkgIntSB));
        results.bkgInSidebandAndError = {bkgYieldSB.getVal(), bkgYieldSB.getPropagatedError(*fitResult, RooArgSet(*obs))};

        RooFormulaVar bkgRatio("bkgRatio", "Bkg(signal)/Bkg(sideband)", "@0 / @1", RooArgList(*bkgInt, *bkgIntSB));
        results.bkgRatioAndError = {bkgRatio.getVal(), bkgRatio.getPropagatedError(*fitResult, RooArgSet(*obs))};
      } else {
        // Direct integration from histogram bins
        results.bkgInSidebandAndError = HistogramUtils::IntegralAndErrorPair(h1Data, sbMin, sbMax);

        const double num = results.bkgInSignalRegionAndError.first;
        const double den = results.bkgInSidebandAndError.first;
        if (den > 0.0) {
          const double ratio = num / den;
          const double relNum = (num != 0.0) ? results.bkgInSignalRegionAndError.second / num : 0.0;
          const double relDen = results.bkgInSidebandAndError.second / den;
          results.bkgRatioAndError = {ratio, ratio * std::sqrt(relNum * relNum + relDen * relDen)};
        }
      }
    }

    // Memory cleanup for integrals (RooProducts and RooFormulaVar die naturally at the end of the method)

    return results;
  }

  // Draws the fit into pads the caller owns. The fitter knows what to draw - the
  // components, the integration and sideband windows, the names inside the RooPlot -
  // and nothing about where it ends up: creating the canvas, choosing whether it has
  // a pull pad, labelling it, writing it and deciding how long it lives belong to the
  // task, which owns the output file. The title is the task's too, since only the task knows which cell this is: it
  // must say so, and must not be empty (RooFit::Title("") gives RooFit's default, "A RooPlot of ...").
  // So is the scale (logY).
  void DrawFit(TPad* pad, const std::string& title, bool logY) { DrawFitOnPads(pad, nullptr, 1.0, title, logY); }

  // Same, with the pulls on the bottom pad of a ratio canvas.
  void DrawFit(const CanvasManager::RatioCanvas& ratio, const std::string& title, bool logY)
  {
    DrawFitOnPads(ratio.topPad, ratio.bottomPad, ratio.fontScale, title, logY);
  }

 private:
  TH1* h1Data;
  FitConfig config;

  RooRealVar* obs{nullptr}; // The X-axis observable

  RooAbsPdf* sigPdf{nullptr}; // Pointer to the signal part
  RooAbsPdf* bkgPdf{nullptr}; // Pointer to the background part
  RooAbsPdf* model{nullptr};  // The total model (Signal + Background)

  RooRealVar* nsig{nullptr};
  RooRealVar* nbkg{nullptr};

  // A parameter held at 'at' for the first fit of the "staged" start.
  struct Held {
    RooRealVar* var;
    double at;
  };
  std::vector<Held> staged;
  std::vector<std::string> upperIsShape; // declared where the model is built (the DSCB), passed on with the diagnostics

  std::unique_ptr<RooArgSet> parameters; // every parameter of the model
  std::unique_ptr<RooArgSet> configured; // their configured values, which every start begins from
  std::unique_ptr<RooDataHist> data;     // h1Data, imported once, before the model is built (see the constructor)

  // The winning start's result, kept alive to use its covariance matrix later, and its facts
  std::unique_ptr<RooFitResult> fitResult;
  FitDiagnostics diagnostics;

  // One start of DoFit: its result, and its facts with the NLL it reached.
  struct Start {
    std::unique_ptr<RooFitResult> result;
    FitDiagnostics diagnostics;
  };

  // One fit of the model to 'data', from wherever the parameters are; it leaves them at its minimum.
  //
  // No NumCPU here, on purpose. It was NumCPU(4), and it did nothing: with the default
  // 'cpu' evaluation backend RooFit ignores it (RooAbsPdf.cxx says so), and the two ways
  // to make a parallel setting count are worse - EvalBackend("legacy") made these fits
  // 50 to 100 times slower and changed the result, RooFit::Parallelize throws in this
  // ROOT build (no multiprocessing). Measured on all 960 cells of the real K0S, Xi and
  // pion spectra: same time, same results to 12 digits, with and without it. The numbers
  // are in DESIGN_HISTORY.md, "The fitter is built once per configuration".
  std::unique_ptr<RooFitResult> Fit(RooDataHist& data, bool offset)
  {
    if (!offset)
      return std::unique_ptr<RooFitResult>(model->fitTo(data, RooFit::Optimize(1), RooFit::Extended(1), RooFit::Save(1), RooFit::PrintLevel(-1)));
    return std::unique_ptr<RooFitResult>(model->fitTo(data, RooFit::Optimize(1), RooFit::Extended(1), RooFit::Save(1),
                                                      RooFit::PrintLevel(-1), RooFit::Offset("bin")));
  }

  // An Offset("bin") fit, whose result is dropped, and the refit from its minimum, whose result is returned.
  std::unique_ptr<RooFitResult> FitAndRefit(RooDataHist& data)
  {
    Fit(data, true);
    return Fit(data, true);
  }

  void ScaleYields(double counts)
  {
    for (RooRealVar* y : {nsig, nbkg})
      if (!y->isConstant())
        y->setVal(std::clamp(0.5 * counts, y->getMin(), y->getMax()));
  }

  // The staged parameters held, one fit, then released: its result is dropped, the parameters stay at its minimum.
  void FitWithStagedHeld(RooDataHist& data)
  {
    for (const Held& h : staged) {
      h.var->setVal(h.at);
      h.var->setConstant(true);
    }
    Fit(data, true);
    for (const Held& h : staged)
      h.var->setConstant(false);
  }

  // The rule is DoFit's. The parameters are set back to the winner's minimum because the last fit run may not be the
  // winner's, and ExtractYieldsAndPurity reads them.
  void KeepLowest(std::vector<Start>& starts)
  {
    Start* winner = &starts.front();
    for (Start& st : starts) {
      const double value = st.diagnostics.minNll;
      if (std::isfinite(value) && (!std::isfinite(winner->diagnostics.minNll) || value < winner->diagnostics.minNll - 0.01))
        winner = &st;
    }
    parameters->assign(winner->result->floatParsFinal());
    fitResult = std::move(winner->result);
    diagnostics = std::move(winner->diagnostics);
    diagnostics.nllConfig = starts.front().diagnostics.minNll;
  }

  // Ownership of all dynamically created RooFit args (vars + pdfs).
  // Insertion order = creation order (leaves first, composites last).
  // The destructor depends on this invariant to tear down in reverse.
  std::vector<std::unique_ptr<RooAbsArg>> ownedObjects;

  // The facts of one fit result and of the data it saw (the NLL and the start are DoFit's). "At a limit" as in note
  // 09 of RooFitKnowledge: within 1e-5 of the range from either end. The distance there was bimodal, below 1e-6 or
  // above 1e-4, so the threshold does not split a population. Except at the lower end of a yield: its range is counts,
  // 0 to 1e7-1e11, and 1e-5 of it called every yield below 1e2-1e6 counts "at its limit" (with the yields open to 1e11,
  // all 70 Omega and Xi fits of the analysis binning on the 22 October production). A yield is at its lower limit below
  // one count: in the model study's scans the yields within 1e-5 of their range at the lower end were 0 or 1e-7 to 1
  // count (2537: those Minuit pushed onto the bound, where its sine mapping leaves them at a distance that grows with
  // the range), then a gap from 1 to 10 counts (34), then from 10 counts up (real small yields). The cell's counts were
  // tried as the scale first, and cut through the first group (0.003 counts in a cell of 25000). Which end is recorded,
  // and which parameters their model calls a shape at the upper end: whether that is a problem is
  // FitQualityRecorder::Reasons' to say.
  FitDiagnostics DiagnosticsOf(const RooFitResult& result, double counts) const
  {
    FitDiagnostics d;
    d.status = result.status();
    d.covQual = result.covQual();
    d.edm = result.edm();
    d.numInvalidNLL = result.numInvalidNLL();
    d.counts = counts;
    for (const RooAbsArg* arg : result.floatParsFinal()) {
      const auto* v = dynamic_cast<const RooRealVar*>(arg);
      if (!v || !v->hasMin() || !v->hasMax())
        continue;
      const std::string_view name = v->GetName();
      const double tolerance = 1e-5 * (v->getMax() - v->getMin());
      const double lowTolerance = (name == nsig->GetName() || name == nbkg->GetName()) ? 1.0 : tolerance;
      if (v->getVal() - v->getMin() < lowTolerance)
        d.atLowerLimit.emplace_back(name);
      if (v->getMax() - v->getVal() < tolerance)
        d.atUpperLimit.emplace_back(name);
    }
    d.upperIsShape = upperIsShape;
    return d;
  }

  // Everything that reads the result asks here: there is none before DoFit.
  void RequireFit() const
  {
    if (!fitResult)
      throw std::runtime_error("[FATAL] DynamicRooFitter: call DoFit() first.");
  }

  void DrawFitOnPads(TPad* mainPad, TPad* pullPad, double fontScale, const std::string& title, bool logY)
  {
    RequireFit();
    if (!mainPad) {
      throw std::runtime_error("[FATAL] DynamicRooFitter::DrawFit: no pad to draw on.");
    }

    auto [minRange, maxRange] = CalculateIntegrationLimits();

    mainPad->cd();
    mainPad->SetLogy(logY);

    RooDataHist& dataHist = *data;

    // Create a RooPlot frame using the observable's range
    RooPlot* frame = obs->frame(RooFit::Title(title.c_str()));
    frame->SetBit(kCanDelete);

    dataHist.plotOn(frame, RooFit::Name("Data_Plot"), RooFit::MarkerStyle(20));
    model->plotOn(frame, RooFit::Name("Model_Plot"), RooFit::LineColor(kBlue), RooFit::LineWidth(2));
    model->plotOn(frame, RooFit::Components(*sigPdf), RooFit::Name("Sig_Plot"), RooFit::LineColor(kRed),
                  RooFit::LineWidth(2), RooFit::Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, RooFit::Components(*bkgPdf), RooFit::Name("Bkg_Plot"), RooFit::LineColor(kGreen + 2),
                  RooFit::LineWidth(2), RooFit::Normalization(1.0, RooAbsReal::RelativeExpected));

    if (pullPad) {
      CanvasManager::FormatTopPadAxes(frame);
    }

    // The y range from the data in the fit range alone. RooPlot's own includes the curves, and in log scale the signal's
    // tails, drawn down to 1e-3 of the peak, left the data in the top quarter of the pad (the phi fits, then drawn in log
    // scale, 24 October 2026); h1Data itself covers the whole online axis, outside the fit range too. The histogram is
    // made on the dataset's own variable, which has the binning plotOn draws: obs's is 100 bins, which merged two K0S bins
    // of 0.5 MeV into one and doubled the maximum. Half the span above the data, for the legend and the label; the
    // linear axis from zero unless an error bar goes below it.
    const auto& binnedObs = static_cast<const RooRealVar&>(*dataHist.get()->find(*obs));
    const std::unique_ptr<TH1> inRange(dataHist.createHistogram("yRange", binnedObs));
    inRange->SetDirectory(0);
    const auto [yMin, yMax] =
      CanvasManager::ComputeGlobalYRange(std::array{inRange.get()}, {.isLogY = logY, .zeroBaseline = true, .topMargin = 0.5});
    frame->SetMinimum(yMin);
    frame->SetMaximum(yMax);

    frame->Draw();

    // Integration and Sideband boundary lines
    CanvasManager::DrawVerticalRange(mainPad, minRange, maxRange, yMin, yMax);
    if (config.integration.sideband) {
      CanvasManager::DrawVerticalRange(mainPad, config.integration.sideband->range.first,
                                       config.integration.sideband->range.second, yMin, yMax);
    }

    // The legend stays here, unlike the ALICE label: its entries are looked up by the
    // names given to the RooPlot objects above, which only this class knows.
    TLegend* leg = CanvasManager::CreateLegend(0.65, 0.65, 0.88, 0.88);
    for (const auto& [name, label, opt] : {std::tuple{"Data_Plot", "Data", "pe"},
                                           {"Model_Plot", "Total Fit", "l"},
                                           {"Sig_Plot", "Signal", "l"},
                                           {"Bkg_Plot", "Background", "l"}}) {
      if (auto* obj = frame->findObject(name))
        leg->AddEntry(obj, label, opt);
    }
    leg->Draw("SAME");

    // Pull plot on bottom pad
    if (pullPad) {
      pullPad->cd();
      pullPad->SetGridy(1);

      if (RooHist* hpull = frame->pullHist("Data_Plot", "Model_Plot")) {
        RooPlot* pullFrame = obs->frame();
        pullFrame->SetTitle(""); // no title on the pulls; SetTitle keeps an empty one, RooFit::Title("") would not
        pullFrame->SetBit(kCanDelete);
        pullFrame->addPlotable(hpull, "P");

        CanvasManager::FormatBottomPadAxes(pullFrame, "Pull (#sigma)", -5.0, 5.0, fontScale);
        pullFrame->Draw();

        CanvasManager::DrawHorizontalLine(pullPad, obs->getMin(), obs->getMax(), 0.0, kDashed, kGray + 2, 2);
        CanvasManager::DrawHorizontalRange(pullPad, obs->getMin(), obs->getMax(), -2.0, 2.0, kDotted, kRed - 7, 1);
      }
    }
  }

  // Helper: creates, stores ownership, returns a non-owning raw pointer for RooFit APIs
  template <typename T, typename... Args>
  T* Own(Args&&... args)
  {
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = ptr.get();
    ownedObjects.push_back(std::move(ptr));
    return raw;
  }

  // The parameters a model holds for the first fit of the "staged" start (DoFit), each at the value given: declared
  // here where the model is built, the one place that knows its parameters. A constant one is skipped: the
  // configuration fixed it, and it stays fixed.
  void Stage(std::initializer_list<Held> held)
  {
    for (const Held& h : held)
      if (!h.var->isConstant())
        staged.push_back(h);
  }

  // Creates ROOT variables(RooRealVar) starting from the configuration.
  // This function centralizes error checking and memory management.
  RooRealVar* CreateVar(const std::string& name)
  {
    // Safety check: if you request a parameter that you forgot to include in the JSON,
    // the program stops immediately with a clear error, preventing a cryptic ROOT crash later.
    if (config.model.params.count(name) == 0) {
      throw std::runtime_error(std::format("[FATAL] Missing parameter in JSON: {}", name));
    }

    // Extract the [val, min, max, isConstant] struct from the map
    const MathParam& p = config.model.params.at(name);

    // No setConstant() here, and none is needed: which RooRealVar
    // constructor is called IS the choice (DoFit holds the staged parameters for one
    // fit and releases them, which leaves this choice as it was). Three arguments builds a constant - that
    // overload exists for exactly this and calls setConstant(true) itself - while
    // adding min and max builds a free parameter with those limits. So a scalar in the
    // JSON ("width_sig": 0.00426) is fixed and an array is fitted, which is the whole
    // meaning of MathParam::isConstant.
    if (p.isConstant) {
      return Own<RooRealVar>(name.c_str(), name.c_str(), p.val);
    } else {
      return Own<RooRealVar>(name.c_str(), name.c_str(), p.val, p.min, p.max);
    }
  }

  // Finds a RooRealVar in a RooArgSet, checking the primary name and any optional fallback names.
  RooRealVar* FindVar(const RooArgSet& vars, const std::string& name,
                      std::initializer_list<std::string_view> fallbacks = {}) const
  {
    if (auto* v = static_cast<RooRealVar*>(vars.find(name.c_str())))
      return v;

    std::string tried = name;
    for (const auto& alt : fallbacks) {
      if (auto* v = static_cast<RooRealVar*>(vars.find(std::string(alt).c_str())))
        return v;
      tried += ", ";
      tried += alt;
    }

    throw std::runtime_error(std::format(
      "[FATAL] DynamicRooFitter: Variable '{}' not found in model{}",
      name, fallbacks.size() ? " (tried: [" + tried + "])" : ""));
  }

  // The K+K- threshold, 2*m_K: the phase-space backgrounds are built on (M - 2*m_K)
  // and are real only above it. Derived rather than written out, so this file holds no
  // second copy of the kaon mass - it used to, as 0.987354, and it disagreed with
  // AnalysisConstants in the sixth decimal because the constant had a digit missing.
  static double TwoKaonThreshold() { return 2.0 * AnalysisConstants::GetMass("K"); }

  // The order n of "<family><n>" (n >= 1, no leading zero), or 0 when modelType is not one of that family.
  static int PolynomialOrder(const std::string& modelType, const std::string& family)
  {
    std::smatch match;
    return std::regex_match(modelType, match, std::regex(family + "([1-9][0-9]*)")) ? std::stoi(match[1]) : 0;
  }

  // The values a parameter can take: a constant has no range (CreateVar builds it without one, limits +-inf).
  static double Lowest(const RooRealVar& v) { return v.isConstant() ? v.getVal() : v.getMin(); }
  static double Highest(const RooRealVar& v) { return v.isConstant() ? v.getVal() : v.getMax(); }

  // The P-wave Breit-Wigner of RelBWKK, convolved by FFT with the resolution. With q the kaon momentum in the pair frame,
  // Gamma(m) = Gamma0 (M/m) (q/q0)^3 (1 + (q0 R)^2)/(1 + (q R)^2) and dN/dm ~ m M Gamma(m) / ((m^2 - M^2)^2 + M^2 Gamma(m)^2).
  //
  // The FFT samples the Breit-Wigner on a cache grid that extends past the observable's range by a buffer, and both set
  // how exact the convolution is: they are derived from the declared limits of the resolution, not left to RooFit's
  // defaults (100 bins of 1.05 MeV for the phi, about one sigma; a buffer of 10% of the range, about one sigma at its
  // upper limit, where the convolution wraps around). The buffer reaches below the KK threshold, so q^2 is clamped at 0
  // there and the Breit-Wigner is 0 instead of NaN. The kaon mass is AnalysisConstants', as for the threshold.
  // Measured with this grid on a Breit-Wigner of constant width against RooVoigtian (24 October 2026, sigma 0.8-2 MeV,
  // its limits 0.1-10 MeV): the fraction in [1.0, 1.05] agrees within 2e-6, and the shape is the Voigtian's moved by
  // 25 keV to lower mass, within 3e-5 once moved. mean_sig takes the shift; no yield does.
  RooAbsPdf* BuildRelBWKK(const std::string& name, RooRealVar& m, RooRealVar& w, RooRealVar& s, RooRealVar& r)
  {
    if (Lowest(m) <= TwoKaonThreshold()) {
      throw std::runtime_error(std::format("[FATAL] DynamicRooFitter: RelBWKK needs the lower limit of '{}' above the KK "
                                           "threshold {} GeV/c^2 (it is {}): below it the pole momentum is imaginary.",
                                           m.GetName(), TwoKaonThreshold(), Lowest(m)));
    }
    if (Lowest(s) <= 0.0) {
      throw std::runtime_error(std::format("[FATAL] DynamicRooFitter: RelBWKK needs a positive lower limit for the "
                                           "resolution '{}' (it is {}): the FFT grid is derived from it.",
                                           s.GetName(), Lowest(s)));
    }
    const double kaonMass2 = std::pow(AnalysisConstants::GetMass("K"), 2);
    const std::string q2 = std::format("TMath::Max(@0*@0/4-{},0.)", kaonMass2);
    const std::string q02 = std::format("(@1*@1/4-{})", kaonMass2);
    const std::string width = std::format("(@2*(@1/@0)*pow({}/{},1.5)*(1+{}*@3*@3)/(1+{}*@3*@3))", q2, q02, q02, q2);
    auto* bw = Own<RooGenericPdf>((name + "_bw").c_str(), "P-wave Breit-Wigner",
                                  std::format("@0*@1*{0}/(pow(@0*@0-@1*@1,2)+@1*@1*{0}*{0})", width).c_str(),
                                  RooArgList(*obs, m, w, r));
    auto* resolution = Own<RooGaussian>((name + "_res").c_str(), "resolution", *obs, RooFit::RooConst(0.0), s);

    // Four cache bins per sigma at its lowest, and a buffer of five sigmas at its highest on each side: the limits of
    // sigma_sig set the grid, so a cell that overrides them also changes its precision (and its cost: a lower limit of
    // 1e-5 would mean some 40000 bins). On obs's range as the data import left it, which the constructor makes final
    // before any model is built.
    const double range = obs->getMax() - obs->getMin();
    obs->setBins(static_cast<int>(std::ceil(4.0 * range / Lowest(s))), "cache");
    auto* conv = Own<RooFFTConvPdf>(name.c_str(), "RelBWKK", *obs, *bw, *resolution);
    conv->setBufferFraction(5.0 * Highest(s) / range);
    return conv;
  }

  // A model that takes sqrt() or a non-integer power of (M - threshold) is NaN below
  // the threshold, and RooFit does not report that as an error: it reports a fit that
  // did not converge, or converged on nonsense, with no mention of the reason.
  //
  // The observable range is the normalisation domain of every PDF in the model, so it
  // is the range that decides whether the evaluation ever goes below the threshold -
  // not the integration range, which is a sub-interval of it. Hence the check is on
  // the observable, and it runs when the model is built rather than when it is
  // evaluated, so the message names the model and the two numbers.
  void RequireThresholdBelowRange(const std::string& modelType) const
  {
    if (obs->getMin() < TwoKaonThreshold()) {
      throw std::runtime_error(std::format(
        "[FATAL] DynamicRooFitter: background model '{}' is only defined above the K+K- "
        "threshold {}, but the observable starts at {}. Below the threshold it evaluates "
        "to NaN and the fit fails without saying why. Raise 'observable.min' in the fit "
        "configuration, or choose a background with no threshold (Chebychev<n>, Bernstein<n>, Exponential).",
        modelType, TwoKaonThreshold(), obs->getMin()));
    }
  }

  // Centralize pdf component creation (Factory)
  RooAbsPdf* CreatePdfComponent(const std::string& modelType, const std::string& suffix)
  {
    std::string name = modelType + suffix;
    RooAbsPdf* pdf{nullptr};

    // Signal models
    if (modelType == "DSCB") {
      RooRealVar* m = CreateVar("mean" + suffix);
      RooRealVar* s = CreateVar("sigma" + suffix);
      RooRealVar* a1 = CreateVar("alpha1" + suffix);
      RooRealVar* n1 = CreateVar("n1" + suffix);
      RooRealVar* a2 = CreateVar("alpha2" + suffix);
      RooRealVar* n2 = CreateVar("n2" + suffix);
      Stage({{a1, a1->getVal()}, {n1, n1->getVal()}, {a2, a2->getVal()}, {n2, n2->getVal()}}); // the tails as configured
      // A tail n at its upper limit is a tail as steep as an exponential, a shape the model can describe, and not a fit
      // that ran out of room; at its lower limit, the heaviest tail allowed, it is (the owner's choice, 24 October 2026).
      // Declared here, judged in FitQualityRecorder::Reasons.
      upperIsShape.insert(upperIsShape.end(), {n1->GetName(), n2->GetName()});
      pdf = Own<RooCrystalBall>(name.c_str(), "DSCB", *obs, *m, *s, *a1, *n1, *a2, *n2);
    } else if (modelType == "Gaussian") {
      RooRealVar* m = CreateVar("mean" + suffix);
      RooRealVar* s = CreateVar("sigma" + suffix);
      pdf = Own<RooGaussian>(name.c_str(), "Gaussian", *obs, *m, *s);
    } else if (modelType == "Voigtian") {
      RooRealVar* m = CreateVar("mean" + suffix);
      RooRealVar* w = CreateVar("width" + suffix); // Natural width (Gamma)
      RooRealVar* s = CreateVar("sigma" + suffix); // Resolution (Gaussian part)
      pdf = Own<RooVoigtian>(name.c_str(), "Voigtian", *obs, *m, *w, *s);
    } else if (modelType == "RelBWKK") {
      // The phi -> K+K- line shape: a P-wave relativistic Breit-Wigner, whose width grows with the kaon momentum, convolved
      // with the Gaussian resolution. The Voigtian's width is constant, so its peak is symmetric; near the KK threshold the
      // data are not: in pp 2026 (mult 15-20%, from 0.8 GeV/c) chi2/ndf 4-9 with this shape against 26-48 with the
      // Voigtian, the same with either background. The same parameter names as the Voigtian, with the same meanings, plus
      // the Blatt-Weisskopf radius. Only for a pair of kaons: their mass is in the formula.
      RooRealVar* m = CreateVar("mean" + suffix);
      RooRealVar* w = CreateVar("width" + suffix);  // Gamma at the pole
      RooRealVar* s = CreateVar("sigma" + suffix);  // resolution
      RooRealVar* r = CreateVar("radius" + suffix); // Blatt-Weisskopf radius (GeV^-1)
      pdf = BuildRelBWKK(name, *m, *w, *s, *r);
    }

    // Background models
    else if (const int order = PolynomialOrder(modelType, "Chebychev")) {
      // Chebychev<n>: c1..cn, the constant term implicit (the yield sets the scale).
      std::vector<RooRealVar*> c;
      RooArgList coefficients;
      for (int i = 1; i <= order; ++i)
        coefficients.add(*c.emplace_back(CreateVar(std::format("c{}{}", i, suffix))));
      if (order > 1)
        Stage({{c.back(), 0.0}}); // one order lower first
      pdf = Own<RooChebychev>(name.c_str(), modelType.c_str(), *obs, coefficients);
    } else if (const int degree = PolynomialOrder(modelType, "Bernstein")) {
      // Bernstein<n>: b0..bn, the n+1 coefficients of degree n, one of them redundant with the yield. The configuration
      // fixes which one, since that choice shapes the curve (near the KK threshold the phi background is small, so
      // there the first is a poor anchor), and it must fix one above zero: with all free, or the fixed one at zero, a
      // common factor of the free ones leaves the curve unchanged, the fit is degenerate and nothing says so.
      // All of them non-negative, which keeps the curve positive: the reason to choose Bernstein over Chebychev. No
      // staged start: a Bernstein polynomial with its last coefficient at 0 is still of degree n, pinned to 0 at the
      // upper edge, not the polynomial one degree lower.
      std::vector<RooRealVar*> b;
      RooArgList coefficients;
      for (int i = 0; i <= degree; ++i)
        coefficients.add(*b.emplace_back(CreateVar(std::format("b{}{}", i, suffix))));
      if (std::ranges::none_of(b, [](const RooRealVar* v) { return v->isConstant() && v->getVal() > 0.0; })) {
        throw std::runtime_error(std::format("[FATAL] DynamicRooFitter: {} needs one of b0{}..b{}{} fixed above zero "
                                             "(a positive number instead of [value, min, max]): one coefficient is "
                                             "redundant with the yield.",
                                             modelType, suffix, degree, suffix));
      }
      for (const RooRealVar* v : b) {
        if (Lowest(*v) < 0.0) {
          throw std::runtime_error(std::format("[FATAL] DynamicRooFitter: {} needs '{}' non-negative (lowest value {}): "
                                               "a negative coefficient lets the background go below zero.",
                                               modelType, v->GetName(), Lowest(*v)));
        }
      }
      pdf = Own<RooBernstein>(name.c_str(), modelType.c_str(), *obs, coefficients);
    } else if (modelType == "Exponential") {
      RooRealVar* slope = CreateVar("slope" + suffix);
      pdf = Own<RooExponential>(name.c_str(), "Exponential", *obs, *slope);
    } else if (modelType == "BkgSourav1") {
      // Custom Phase-Space Background for Phi -> K+ K-
      // Formula: 1.0 + c1*M + c2*sqrt(M - 2*m_K)
      RequireThresholdBelowRange(modelType);
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      pdf = Own<RooGenericPdf>(name.c_str(), "Sourav Background",
                               std::format("1.0 + @1*@0 + @2*sqrt(@0 - {})", TwoKaonThreshold()).c_str(),
                               RooArgList(*obs, *c1, *c2));
    } else if (modelType == "BkgSourav2") {
      // Custom Phase-Space Background for Phi -> K+ K-
      // Formula: 1.0 + c1*M + c2*M^2 + c3*sqrt(M - 2*m_K)
      RequireThresholdBelowRange(modelType);
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      RooRealVar* c3 = CreateVar("c3" + suffix);
      pdf = Own<RooGenericPdf>(name.c_str(), "Sourav Background",
                               std::format("1.0 + @1*@0 + @2*@0*@0 + @3*sqrt(@0 - {})", TwoKaonThreshold()).c_str(),
                               RooArgList(*obs, *c1, *c2, *c3));
    } else if (modelType == "BkgMattia") {
      // Formula: (M - 2*m_K)^n * exp(c1*(M - 2*m_K) + c2*(M - 2*m_K)^2 + c3*(M - 2*m_K)^3)
      //
      // Needs 'n', 'c1', 'c2', 'c3' in the fit configuration; CreateVar names the
      // one that is missing. Note that 'c1'..'c3' also exist for BkgSourav2, where
      // they multiply powers of M and not of (M - 2*m_K): the names are shared, the
      // meanings are not, so this model belongs in its own block rather than as a
      // one-key override of a Sourav one.
      RequireThresholdBelowRange(modelType);
      RooRealVar* n = CreateVar("n" + suffix);
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      RooRealVar* c3 = CreateVar("c3" + suffix);
      const double thr = TwoKaonThreshold();
      pdf = Own<RooGenericPdf>(name.c_str(), "Mattia Background",
                               std::format("pow(@0 - {}, @1) * exp(@2*(@0 - {}) + @3*pow(@0 - {}, 2) + @4*pow(@0 - {}, 3))",
                                           thr, thr, thr, thr)
                                 .c_str(),
                               RooArgList(*obs, *n, *c1, *c2, *c3));
    }

    else {
      throw std::runtime_error(std::format("[FATAL] Unknown model type: {}", modelType));
    }

    return pdf;
  }

  // Centralized method to parse the model string and create the corresponding RooAbsPdf
  RooAbsPdf* ParseModelString(const std::string& modelStr, const std::string& roleName)
  {
    std::string cleanStr = modelStr;
    cleanStr.erase(std::remove(cleanStr.begin(), cleanStr.end(), ' '), cleanStr.end()); // Remove spaces for easier parsing

    // 1. Summation handling (+)
    if (cleanStr.find('+') != std::string::npos) {
      std::stringstream ss(cleanStr);
      std::string token;
      RooArgList pdfList;
      RooArgList fracList;
      int i = 0;

      // Add each component pdf to the list
      while (std::getline(ss, token, '+')) {
        std::string suffix = std::format("_{}_{}", roleName, i);
        pdfList.add(*CreatePdfComponent(token, suffix));
        i++;
      }

      // Create fraction parameters: for N pdfs, we need N-1 fractions (the last one is 1 - sum of others)
      for (int j = 0; j < i - 1; ++j) {
        std::string fracName = std::format("frac_{}_{}", roleName, j);
        fracList.add(*CreateVar(fracName));
      }

      return Own<RooAddPdf>(roleName.c_str(), ("Sum of " + roleName).c_str(), pdfList, fracList);
    }

    // 2. Product handling (*)
    else if (cleanStr.find('*') != std::string::npos) {
      std::stringstream ss(cleanStr);
      std::string token;
      RooArgList pdfList;
      int i = 0;

      while (std::getline(ss, token, '*')) {
        std::string suffix = std::format("_{}_{}", roleName, i++);
        pdfList.add(*CreatePdfComponent(token, suffix));
      }

      // RooProdPdf does not need explicit fraction parameters, it multiplies the components directly
      return Own<RooProdPdf>(roleName.c_str(), ("Product of " + roleName).c_str(), pdfList);
    }

    // 3. Single model handling
    else {
      return CreatePdfComponent(cleanStr, "_" + roleName);
    }
  }

  // Helper method to centralize the logic for integration limits
  std::pair<double, double> CalculateIntegrationLimits() const
  {
    double minRange = 0.0;
    double maxRange = 0.0;

    // 1. Determine base limits based on the configured mode
    if (const auto* fixed = std::get_if<FixedRange>(&config.integration.signalWindow)) {
      minRange = fixed->min;
      maxRange = fixed->max;
    } else {
      const auto& nSigma = std::get<NSigma>(config.integration.signalWindow);
      std::unique_ptr<RooArgSet> vars(model->getVariables());
      double m = FindVar(*vars, "mean_sig", {"mean_sig_0", "mean"})->getVal();
      double s = FindVar(*vars, "sigma_sig", {"sigma_sig_0", "sigma"})->getVal();

      minRange = m - (nSigma.n * s);
      maxRange = m + (nSigma.n * s);
    }

    // 2. Adjust limits to align with histogram bin edges if requested
    if (config.integration.snapToBin) {
      int lowEdgeBin = h1Data->GetXaxis()->FindFixBin(minRange);
      int upEdgeBin = h1Data->GetXaxis()->FindFixBin(maxRange);

      minRange = h1Data->GetXaxis()->GetBinLowEdge(lowEdgeBin);
      maxRange = h1Data->GetXaxis()->GetBinLowEdge(upEdgeBin + 1);
    }

    return {minRange, maxRange};
  }
};

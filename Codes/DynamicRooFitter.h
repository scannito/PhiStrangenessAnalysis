#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisUtils.h"
#include "FitConfigManager.h"

#include "RooAbsPdf.h"
#include "RooAddPdf.h"
#include "RooArgList.h"
#include "RooChebychev.h"
#include "RooCrystalBall.h"
#include "RooDataHist.h"
#include "RooDataSet.h"
#include "RooExponential.h"
#include "RooFitResult.h"
#include "RooFormulaVar.h"
#include "RooGaussian.h"
#include "RooGenericPdf.h"
#include "RooPlot.h"
#include "RooPolynomial.h"
#include "RooProdPdf.h"
#include "RooProduct.h"
#include "RooRealVar.h"
#include "RooVoigtian.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TObject.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class DynamicRooFitter
{
 public:
  DynamicRooFitter(TH1* h, const FitConfig& cfg) : h1Data(h), config(cfg)
  {
    obs = Own<RooRealVar>(cfg.obs.name.c_str(), cfg.obs.title.c_str(), cfg.obs.min, cfg.obs.max);

    // Dynamic model parsing for both signal and background
    sigPdf = ParseModelString(cfg.model.sigModel, "sig");
    bkgPdf = ParseModelString(cfg.model.bkgModel, "bkg");

    RooRealVar* nsig = CreateVar("nsig");
    RooRealVar* nbkg = CreateVar("nbkg");

    model = Own<RooAddPdf>("model", "Total Model", RooArgList(*sigPdf, *bkgPdf), RooArgList(*nsig, *nbkg));
  }

  // Tear down the owned RooFit objects in strict reverse creation order:
  // composites (RooAddPdf, RooProdPdf) die before the RooRealVars they use as
  // servers. ~vector() alone would NOT guarantee this — the destruction order
  // of vector elements is unspecified, and it genuinely differs between
  // implementations (libc++ destroys back-to-front, libstdc++ front-to-back).
  // pop_back() always destroys the last element, so this is portable.
  ~DynamicRooFitter()
  {
    while (!ownedObjects.empty())
      ownedObjects.pop_back();
  }

  int DoFit()
  {

    RooDataHist dataHist("dataHist", "Data", *obs, RooFit::Import(*h1Data));

    // Save the result to the class member instead of a local variable
    // reset() releases any previous fitResult and takes ownership of the new one
    fitResult.reset(model->fitTo(dataHist, RooFit::Optimize(1), RooFit::Extended(1), RooFit::Save(1), RooFit::PrintLevel(-1), RooFit::NumCPU(4)));

    return fitResult->status();
  }

  // Structure to hold the results of the yield and purity calculations
  struct FitResults {
    std::pair<double, double> signalAndError{0.0, 0.0};
    std::pair<double, double> backgroundAndError{0.0, 0.0};
    std::pair<double, double> purityAndError{0.0, 0.0};
    std::pair<double, double> bkgInSidebandAndError{0.0, 0.0};
    std::pair<double, double> bkgRatioAndError{0.0, 0.0};
  };

  FitResults ExtractYieldsAndPurity()
  {
    if (!fitResult)
      throw std::runtime_error("[FATAL ERROR] Call DoFit() first!");

    FitResults results;

    // Retrieve integration limits dynamically based on the configuration logic
    auto [minRange, maxRange] = CalculateIntegrationLimits();

    std::string modeLog = config.integration.useFixedRange ? "FIXED" : "N-SIGMA";
    std::cout << "[INFO] Integrating in " << modeLog << " range: [" << minRange << " , " << maxRange << "]" << std::endl;

    obs->setRange("signalRegion", minRange, maxRange);

    std::unique_ptr<RooArgSet> modelVars(model->getVariables());
    RooRealVar* nsigVar = static_cast<RooRealVar*>(modelVars->find("nsig"));
    RooRealVar* nbkgVar = static_cast<RooRealVar*>(modelVars->find("nbkg"));

    // --- 1. SIGNAL YIELD & ERROR ---
    std::unique_ptr<RooAbsReal> sigInt(sigPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("signalRegion")));
    RooProduct signalYield("signalYield", "Signal Yield", RooArgList(*nsigVar, *sigInt));
    results.signalAndError = {signalYield.getVal(), signalYield.getPropagatedError(*fitResult, RooArgSet(*obs))};

    // --- 2. BACKGROUND YIELD & ERROR ---
    std::unique_ptr<RooAbsReal> bkgInt(bkgPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("signalRegion")));
    RooProduct bkgYield("bkgYield", "Background Yield", RooArgList(*nbkgVar, *bkgInt));
    results.backgroundAndError = {bkgYield.getVal(), bkgYield.getPropagatedError(*fitResult, RooArgSet(*obs))};

    // --- 3. OPTIONAL PURITY & ERROR (S / S+B) ---
    if (config.integration.calculatePurity) {
      RooFormulaVar purity("purity", "Signal Purity", "@0 / (@0 + @1)", RooArgList(signalYield, bkgYield));
      results.purityAndError = {purity.getVal(), purity.getPropagatedError(*fitResult, RooArgSet(*obs))};
    }

    // --- 4. OPTIONAL BACKGROUND IN SIDEBAND YIELD & ERROR ---
    if (config.integration.calculateSideband) {
      double sbMin = config.integration.sidebandRange.first;
      double sbMax = config.integration.sidebandRange.second;
      obs->setRange("sidebandRegion", sbMin, sbMax);

      if (config.integration.sidebandFromFit) {
        std::unique_ptr<RooAbsReal> bkgIntSB(bkgPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("sidebandRegion")));
        RooProduct bkgYieldSB("bkgYieldSB", "Bkg in Sideband", RooArgList(*nbkgVar, *bkgIntSB));
        results.bkgInSidebandAndError = {bkgYieldSB.getVal(), bkgYieldSB.getPropagatedError(*fitResult, RooArgSet(*obs))};

        RooFormulaVar bkgRatio("bkgRatio", "Bkg(signal)/Bkg(sideband)", "@0 / @1", RooArgList(*bkgInt, *bkgIntSB));
        results.bkgRatioAndError = {bkgRatio.getVal(), bkgRatio.getPropagatedError(*fitResult, RooArgSet(*obs))};
      } else {
        // Direct integration from histogram bins
        results.bkgInSidebandAndError = AnalysisUtils::IntegralAndErrorPair(h1Data, sbMin, sbMax);

        const double num = results.backgroundAndError.first;
        const double den = results.bkgInSidebandAndError.first;
        if (den > 0.0) {
          const double ratio = num / den;
          const double relNum = (num != 0.0) ? results.backgroundAndError.second / num : 0.0;
          const double relDen = results.bkgInSidebandAndError.second / den;
          results.bkgRatioAndError = {ratio, ratio * std::sqrt(relNum * relNum + relDen * relDen)};
        }
      }
    }

    // Memory cleanup for integrals (RooProducts and RooFormulaVar die naturally at the end of the method)

    return results;
  }

  void SaveFitCanvas(TDirectory* dirOutput, const std::string& canvasName)
  {
    if (!fitResult) {
      throw std::runtime_error("[FATAL ERROR] You must call DoFit() before drawing the canvas!");
    }

    auto [minRange, maxRange] = CalculateIntegrationLimits();

    if (dirOutput)
      dirOutput->cd();

    std::unique_ptr<TCanvas> cFit = std::make_unique<TCanvas>(canvasName.c_str(), "Fit Canvas", 800, 800);
    cFit->SetLogy();

    RooDataHist dataHist("dataHist", "Data", *obs, RooFit::Import(*h1Data));

    // Create a RooPlot frame using the observable's range
    std::unique_ptr<RooPlot> frame(obs->frame(RooFit::Title("")));
    dataHist.plotOn(frame.get(), RooFit::Name("Data_Plot"), RooFit::MarkerStyle(20));
    model->plotOn(frame.get(), RooFit::Name("Model_Plot"), RooFit::LineColor(kBlue), RooFit::LineWidth(2));
    model->plotOn(frame.get(), RooFit::Components(*sigPdf), RooFit::Name("Sig_Plot"), RooFit::LineColor(kRed),
                  RooFit::LineWidth(2), RooFit::Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame.get(), RooFit::Components(*bkgPdf), RooFit::Name("Bkg_Plot"), RooFit::LineColor(kGreen + 2),
                  RooFit::LineWidth(2), RooFit::Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();

    // Retrieve the Y-axis limits directly from the RooPlot frame
    double yMin = frame->GetMinimum();
    double yMax = frame->GetMaximum();

    std::unique_ptr<TLine> line1 = std::make_unique<TLine>(minRange, yMin, minRange, yMax);
    line1->SetLineColor(kBlack);
    line1->SetLineStyle(kDashed);
    line1->SetLineWidth(2);
    line1->Draw("SAME");

    std::unique_ptr<TLine> line2 = std::make_unique<TLine>(maxRange, yMin, maxRange, yMax);
    line2->SetLineColor(kBlack);
    line2->SetLineStyle(kDashed);
    line2->SetLineWidth(2);
    line2->Draw("SAME");

    // Optionally draw sideband lines if calculated
    std::unique_ptr<TLine> line3, line4;
    if (config.integration.calculateSideband) {
      double sbMin = config.integration.sidebandRange.first;
      double sbMax = config.integration.sidebandRange.second;
      line3 = std::make_unique<TLine>(sbMin, yMin, sbMin, yMax);
      line4 = std::make_unique<TLine>(sbMax, yMin, sbMax, yMax);
      line3->SetLineColor(kBlack);
      line3->SetLineStyle(kDashed);
      line3->SetLineWidth(2);
      line4->SetLineColor(kBlack);
      line4->SetLineStyle(kDashed);
      line4->SetLineWidth(2);
      line3->Draw("SAME");
      line4->Draw("SAME");
    }

    if (dirOutput) {
      dirOutput->cd();
      cFit->Write(nullptr, TObject::kOverwrite);
    }
  }

 private:
  TH1* h1Data;
  FitConfig config;

  RooRealVar* obs{nullptr}; // The X-axis observable

  RooAbsPdf* sigPdf{nullptr}; // Pointer to the signal part
  RooAbsPdf* bkgPdf{nullptr}; // Pointer to the background part
  RooAbsPdf* model{nullptr};  // The total model (Signal + Background)

  // Keep the fit result alive to use its covariance matrix later
  std::unique_ptr<RooFitResult> fitResult;

  // Ownership of all dynamically created RooFit args (vars + pdfs).
  // Insertion order = creation order (leaves first, composites last).
  // The destructor depends on this invariant to tear down in reverse.
  std::vector<std::unique_ptr<RooAbsArg>> ownedObjects;

  // Helper: creates, stores ownership, returns a non-owning raw pointer for RooFit APIs
  template <typename T, typename... Args>
  T* Own(Args&&... args)
  {
    auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = ptr.get();
    ownedObjects.push_back(std::move(ptr));
    return raw;
  }

  // Creates ROOT variables(RooRealVar) starting from the configuration.
  // This function centralizes error checking and memory management.
  RooRealVar* CreateVar(const std::string& name)
  {
    // Safety check: if you request a parameter that you forgot to include in the JSON,
    // the program stops immediately with a clear error, preventing a cryptic ROOT crash later.
    if (config.model.params.count(name) == 0) {
      throw std::runtime_error("[FATAL ERROR] Missing parameter in JSON: " + name);
    }

    // Extract the [val, min, max, isConstant] struct from the map
    const MathParam& p = config.model.params.at(name);

    // No setConstant() anywhere in this class, and none is needed: which RooRealVar
    // constructor is called IS the choice. Three arguments builds a constant - that
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

  // The K+K- threshold, 2*m_K: the phase-space backgrounds are built on (M - 2*m_K)
  // and are real only above it. Derived rather than written out, so this file holds no
  // second copy of the kaon mass - it used to, as 0.987354, and it disagreed with
  // AnalysisConstants in the sixth decimal because the constant had a digit missing.
  static double TwoKaonThreshold() { return 2.0 * AnalysisConstants::GetMass("K"); }

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
        "configuration, or choose a background with no threshold (Chebychev*, Exponential).",
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
    }

    // Background models
    else if (modelType == "Chebychev1") {
      RooRealVar* c1 = CreateVar("c1" + suffix);
      pdf = Own<RooChebychev>(name.c_str(), "Chebychev1", *obs, RooArgList(*c1));
    } else if (modelType == "Chebychev2") {
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      pdf = Own<RooChebychev>(name.c_str(), "Chebychev2", *obs, RooArgList(*c1, *c2));
    } else if (modelType == "Chebychev3") {
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      RooRealVar* c3 = CreateVar("c3" + suffix);
      pdf = Own<RooChebychev>(name.c_str(), "Chebychev3", *obs, RooArgList(*c1, *c2, *c3));
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
      throw std::runtime_error("[FATAL ERROR] Unknown model type: " + modelType);
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
        std::string suffix = "_" + roleName + "_" + std::to_string(i);
        pdfList.add(*CreatePdfComponent(token, suffix));
        i++;
      }

      // Create fraction parameters: for N pdfs, we need N-1 fractions (the last one is 1 - sum of others)
      for (int j = 0; j < i - 1; ++j) {
        std::string fracName = "frac_" + roleName + "_" + std::to_string(j);
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
        std::string suffix = "_" + roleName + "_" + std::to_string(i++);
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
    if (config.integration.useFixedRange) {
      minRange = config.integration.range.first;
      maxRange = config.integration.range.second;
    } else {
      std::unique_ptr<RooArgSet> vars(model->getVariables());
      RooRealVar* meanVar = static_cast<RooRealVar*>(vars->find("mean"));
      RooRealVar* sigmaVar = static_cast<RooRealVar*>(vars->find("sigma"));

      if (!meanVar || !sigmaVar) {
        throw std::runtime_error("[FATAL ERROR] Dynamic N-Sigma requested, but 'mean' or 'sigma' missing in model!");
      }

      double m = meanVar->getVal();
      double s = sigmaVar->getVal();
      double n = config.integration.nSigma;

      minRange = m - (n * s);
      maxRange = m + (n * s);
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

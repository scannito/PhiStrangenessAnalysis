#pragma once

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
#include <iostream>
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
    obs = new RooRealVar(cfg.obs.name.c_str(), cfg.obs.title.c_str(), cfg.obs.min, cfg.obs.max);
    garbageCollector.push_back(obs);

    /*if (cfg.model.sigModel == "DSCB") {
      RooRealVar* m = CreateVar("mean");
      RooRealVar* s = CreateVar("sigma");
      RooRealVar* a1 = CreateVar("alpha1");
      RooRealVar* n1 = CreateVar("n1");
      RooRealVar* a2 = CreateVar("alpha2");
      RooRealVar* n2 = CreateVar("n2");
      sigPdf = new RooCrystalBall("sigPdf", "Signal", *obs, *m, *s, *a1, *n1, *a2, *n2);
    } else if (cfg.model.sigModel == "Gaussian") {
      RooRealVar* m = CreateVar("mean");
      RooRealVar* s = CreateVar("sigma");
      sigPdf = new RooGaussian("sigPdf", "Signal", *obs, *m, *s);
    } else if (config.model.sigModel == "Voigtian") {
      RooRealVar* m = CreateVar("mean");
      RooRealVar* w = CreateVar("width"); // Natural width (Gamma)
      RooRealVar* s = CreateVar("sigma"); // Resolution (Gaussian part)
      sigPdf = new RooVoigtian("sigPdf", "Signal", *obs, *m, *w, *s);
    } else {
      throw std::runtime_error("[FATAL ERROR] Unknown signal model: " + cfg.model.sigModel);
    }
    garbageCollector.push_back(sigPdf);

    if (cfg.model.bkgModel == "Chebychev1") {
      RooRealVar* c1 = CreateVar("c1");
      bkgPdf = new RooChebychev("bkgPdf", "Background", *obs, RooArgList(*c1));
    } else if (cfg.model.bkgModel == "Chebychev2") {
      RooRealVar* c1 = CreateVar("c1");
      RooRealVar* c2 = CreateVar("c2");
      bkgPdf = new RooChebychev("bkgPdf", "Background", *obs, RooArgList(*c1, *c2));
    } else if (cfg.model.bkgModel == "Chebychev3") {
      RooRealVar* c1 = CreateVar("c1");
      RooRealVar* c2 = CreateVar("c2");
      RooRealVar* c3 = CreateVar("c3");
      bkgPdf = new RooChebychev("bkgPdf", "Background", *obs, RooArgList(*c1, *c2, *c3));
    } else if (cfg.model.bkgModel == "Exponential") {
      RooRealVar* slope = CreateVar("slope");
      bkgPdf = new RooExponential("bkgPdf", "Background", *obs, *slope);
    } else if (config.model.bkgModel == "BkgSourav1") {
      // Custom Phase-Space Background for Phi -> K+ K-
      // Formula: 1.0 + c1*M + c2*sqrt(M - 2*m_K)
      // m_K = 0.493677 GeV/c^2  -> 2*m_K = 0.987354 GeV/c^2
      RooRealVar* c1 = CreateVar("c1");
      RooRealVar* c2 = CreateVar("c2");
      bkgPdf = new RooGenericPdf("bkgPdf", "Sourav Background", "1.0 + @1*@0 + @2*sqrt(@0 - 0.987354)", RooArgList(*obs, *c1, *c2));
    } else if (config.model.bkgModel == "BkgSourav2") {
      // Custom Phase-Space Background for Phi -> K+ K-
      // Formula: c0 + c1*M + c2*sqrt(M - 2*m_K)
      // m_K = 0.493677 GeV/c^2  -> 2*m_K = 0.987354 GeV/c^2
      RooRealVar* c1 = CreateVar("c1");
      RooRealVar* c2 = CreateVar("c2");
      RooRealVar* c3 = CreateVar("c3");
      bkgPdf = new RooGenericPdf("bkgPdf", "Sourav Background", "1.0 + @1*@0 +@2*@0*@0 + @3*sqrt(@0 - 0.987354)", RooArgList(*obs, *c1, *c2, *c3));
    } else {
      throw std::runtime_error("[FATAL ERROR] Unknown background model: " + cfg.model.bkgModel);
    }
    garbageCollector.push_back(bkgPdf);*/

    // Dynamic model parsing for both signal and background
    sigPdf = ParseModelString(cfg.model.sigModel, "sig");
    bkgPdf = ParseModelString(cfg.model.bkgModel, "bkg");

    RooRealVar* nsig = CreateVar("nsig");
    RooRealVar* nbkg = CreateVar("nbkg");

    model = new RooAddPdf("model", "Total Model", RooArgList(*sigPdf, *bkgPdf), RooArgList(*nsig, *nbkg));
    garbageCollector.push_back(model);
  }

  ~DynamicRooFitter()
  {
    // Safely delete all dynamically allocated RooFit objects
    // to prevent RAM saturation during extensive analysis loops.
    for (TObject* obj : garbageCollector) {
      delete obj;
    }

    if (fitResult)
      delete fitResult;
  }

  int DoFit()
  {
    // Clean up any previous fit result to prevent memory leaks
    if (fitResult) {
      delete fitResult;
      fitResult = nullptr;
    }

    RooDataHist dataHist("dataHist", "Data", *obs, RooFit::Import(*h1Data));

    // Save the result to the class member instead of a local variable
    fitResult = model->fitTo(dataHist, RooFit::Optimize(1), RooFit::Extended(1), RooFit::Save(1), RooFit::PrintLevel(-1), RooFit::NumCPU(4));

    return fitResult->status();
  }

  // Structure to hold the results of the yield and purity calculations
  struct FitResults {
    std::pair<double, double> signal{0.0, 0.0};
    std::pair<double, double> background{0.0, 0.0};
    std::pair<double, double> purity{0.0, 0.0};
    std::pair<double, double> bkgInSideband{0.0, 0.0};
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

    // --- 1. SIGNAL YIELD & ERROR ---
    RooAbsReal* sigInt = sigPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("signalRegion"));
    RooRealVar* nsigVar = (RooRealVar*)model->getVariables()->find("nsig");
    RooProduct signalYield("signalYield", "Signal Yield", RooArgList(*nsigVar, *sigInt));
    results.signal = {signalYield.getVal(), signalYield.getPropagatedError(*fitResult, RooArgSet(*obs))};

    // --- 2. BACKGROUND YIELD & ERROR ---
    RooAbsReal* bkgInt = bkgPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("signalRegion"));
    RooRealVar* nbkgVar = (RooRealVar*)model->getVariables()->find("nbkg");
    RooProduct bkgYield("bkgYield", "Background Yield", RooArgList(*nbkgVar, *bkgInt));
    results.background = {bkgYield.getVal(), bkgYield.getPropagatedError(*fitResult, RooArgSet(*obs))};

    // --- 3. OPTIONAL PURITY & ERROR (S / S+B) ---
    if (config.integration.calculatePurity) {
      RooFormulaVar purity("purity", "Signal Purity", "@0 / (@0 + @1)", RooArgList(signalYield, bkgYield));
      results.purity = {purity.getVal(), purity.getPropagatedError(*fitResult, RooArgSet(*obs))};
    }

    // --- 4. OPTIONAL BACKGROUND IN SIDEBAND YIELD & ERROR ---
    if (config.integration.calculateSideband) {
      double sbMin = config.integration.sidebandRange.first;
      double sbMax = config.integration.sidebandRange.second;
      obs->setRange("sidebandRegion", sbMin, sbMax);

      if (config.integration.sidebandFromFit) {
        RooAbsReal* bkgIntSB = bkgPdf->createIntegral(*obs, RooFit::NormSet(*obs), RooFit::Range("sidebandRegion"));
        RooProduct bkgYieldSB("bkgYieldSB", "Bkg in Sideband", RooArgList(*nbkgVar, *bkgIntSB));
        results.bkgInSideband = {bkgYieldSB.getVal(), bkgYieldSB.getPropagatedError(*fitResult, RooArgSet(*obs))};
        delete bkgIntSB;
      } else {
        // Direct integration from histogram bins
        results.bkgInSideband = AnalysisUtils::IntegralAndErrorPair(h1Data, sbMin, sbMax);
      }
    }

    // Memory cleanup for integrals (RooProducts and RooFormulaVar die naturally at the end of the method)
    delete sigInt;
    delete bkgInt;

    return results;
  }

  void SaveFitCanvas(TFile* fileOutput, const std::string& canvasName)
  {
    if (!fitResult) {
      throw std::runtime_error("[FATAL ERROR] You must call DoFit() before drawing the canvas!");
    }

    auto [minRange, maxRange] = CalculateIntegrationLimits();

    TCanvas* cFit = new TCanvas(canvasName.c_str(), "Fit Canvas", 800, 800);
    cFit->SetLogy();

    RooDataHist dataHist("dataHist", "Data", *obs, RooFit::Import(*h1Data));

    // Create a RooPlot frame using the observable's range
    RooPlot* frame = obs->frame(RooFit::Title(""));
    dataHist.plotOn(frame, RooFit::Name("Data_Plot"), RooFit::MarkerStyle(20));
    model->plotOn(frame, RooFit::Name("Model_Plot"), RooFit::LineColor(kBlue), RooFit::LineWidth(2));
    model->plotOn(frame, RooFit::Components(*sigPdf), RooFit::Name("Sig_Plot"), RooFit::LineColor(kRed),
                  RooFit::LineWidth(2), RooFit::Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, RooFit::Components(*bkgPdf), RooFit::Name("Bkg_Plot"), RooFit::LineColor(kGreen + 2),
                  RooFit::LineWidth(2), RooFit::Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();

    // Retrieve the Y-axis limits directly from the RooPlot frame
    double yMin = frame->GetMinimum();
    double yMax = frame->GetMaximum();

    TLine* line1 = new TLine(minRange, yMin, minRange, yMax);
    line1->SetLineColor(kBlack);
    line1->SetLineStyle(kDashed);
    line1->SetLineWidth(2);
    line1->Draw("SAME");

    TLine* line2 = new TLine(maxRange, yMin, maxRange, yMax);
    line2->SetLineColor(kBlack);
    line2->SetLineStyle(kDashed);
    line2->SetLineWidth(2);
    line2->Draw("SAME");

    // Optionally draw sideband lines if calculated
    TLine *line3{nullptr}, *line4{nullptr};
    if (config.integration.calculateSideband) {
      double sbMin = config.integration.sidebandRange.first;
      double sbMax = config.integration.sidebandRange.second;
      line3 = new TLine(sbMin, yMin, sbMin, yMax);
      line4 = new TLine(sbMax, yMin, sbMax, yMax);
      line3->SetLineColor(kBlack);
      line3->SetLineStyle(kDashed);
      line3->SetLineWidth(2);
      line4->SetLineColor(kBlack);
      line4->SetLineStyle(kDashed);
      line4->SetLineWidth(2);
      line3->Draw("SAME");
      line4->Draw("SAME");
    }

    fileOutput->cd();
    cFit->Write();

    // Memory cleanup to prevent RAM leaks inside the analysis loop
    delete line1;
    delete line2;
    if (line3)
      delete line3;
    if (line4)
      delete line4;
    // delete leg;
    delete frame;
    delete cFit;
  }

 private:
  TH1* h1Data;
  const FitConfig& config;

  RooRealVar* obs; // The X-axis observable

  RooAbsPdf* sigPdf; // Pointer to the signal part
  RooAbsPdf* bkgPdf; // Pointer to the background part
  RooAbsPdf* model;  // The total model (Signal + Background)

  // Keep the fit result alive to use its covariance matrix later
  RooFitResult* fitResult{nullptr};

  // Garbage collector to clean up RAM when the object is destroyed
  std::vector<TObject*> garbageCollector;

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
    const FitParam& p = config.model.params.at(name);

    RooRealVar* var{nullptr};

    if (p.isConstant) {
      var = new RooRealVar(name.c_str(), name.c_str(), p.val);
    } else {
      var = new RooRealVar(name.c_str(), name.c_str(), p.val, p.min, p.max);
    }

    // Push the pointer into our "trash bin" (garbage collector) for cleanup at the end of the fit
    garbageCollector.push_back(var);

    return var;
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
      pdf = new RooCrystalBall(name.c_str(), "DSCB", *obs, *m, *s, *a1, *n1, *a2, *n2);
    } else if (modelType == "Gaussian") {
      RooRealVar* m = CreateVar("mean" + suffix);
      RooRealVar* s = CreateVar("sigma" + suffix);
      pdf = new RooGaussian(name.c_str(), "Gaussian", *obs, *m, *s);
    } else if (modelType == "Voigtian") {
      RooRealVar* m = CreateVar("mean" + suffix);
      RooRealVar* w = CreateVar("width" + suffix); // Natural width (Gamma)
      RooRealVar* s = CreateVar("sigma" + suffix); // Resolution (Gaussian part)
      pdf = new RooVoigtian(name.c_str(), "Voigtian", *obs, *m, *w, *s);
    }

    // Background models
    else if (modelType == "Chebychev1") {
      RooRealVar* c1 = CreateVar("c1" + suffix);
      pdf = new RooChebychev(name.c_str(), "Chebychev1", *obs, RooArgList(*c1));
    } else if (modelType == "Chebychev2") {
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      pdf = new RooChebychev(name.c_str(), "Chebychev2", *obs, RooArgList(*c1, *c2));
    } else if (modelType == "Chebychev3") {
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      RooRealVar* c3 = CreateVar("c3" + suffix);
      pdf = new RooChebychev(name.c_str(), "Chebychev3", *obs, RooArgList(*c1, *c2, *c3));
    } else if (modelType == "Exponential") {
      RooRealVar* slope = CreateVar("slope" + suffix);
      pdf = new RooExponential(name.c_str(), "Exponential", *obs, *slope);
    } else if (modelType == "BkgSourav1") {
      // Custom Phase-Space Background for Phi -> K+ K-
      // Formula: 1.0 + c1*M + c2*sqrt(M - 2*m_K)
      // m_K = 0.493677 GeV/c^2  -> 2*m_K = 0.987354 GeV/c^2
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      pdf = new RooGenericPdf(name.c_str(), "Sourav Background", "1.0 + @1*@0 + @2*sqrt(@0 - 0.987354)", RooArgList(*obs, *c1, *c2));
    } else if (modelType == "BkgSourav2") {
      // Custom Phase-Space Background for Phi -> K+ K-
      // Formula: c0 + c1*M + c2*sqrt(M - 2*m_K)
      // m_K = 0.493677 GeV/c^2  -> 2*m_K = 0.987354 GeV/c^2
      RooRealVar* c1 = CreateVar("c1" + suffix);
      RooRealVar* c2 = CreateVar("c2" + suffix);
      RooRealVar* c3 = CreateVar("c3" + suffix);
      pdf = new RooGenericPdf(name.c_str(), "Sourav Background", "1.0 + @1*@0 +@2*@0*@0 + @3*sqrt(@0 - 0.987354)", RooArgList(*obs, *c1, *c2, *c3));
    }

    else {
      throw std::runtime_error("[FATAL ERROR] Unknown model type: " + modelType);
    }

    garbageCollector.push_back(pdf);
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

      RooAbsPdf* sumPdf = new RooAddPdf(roleName.c_str(), ("Sum of " + roleName).c_str(), pdfList, fracList);
      garbageCollector.push_back(sumPdf);
      return sumPdf;
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
      RooAbsPdf* prodPdf = new RooProdPdf(roleName.c_str(), ("Product of " + roleName).c_str(), pdfList);
      garbageCollector.push_back(prodPdf);
      return prodPdf;
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
      RooRealVar* meanVar = (RooRealVar*)model->getVariables()->find("mean");
      RooRealVar* sigmaVar = (RooRealVar*)model->getVariables()->find("sigma");

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

/*template <PartType partType>
class DynamicRooFitter
{
public:
    DynamicRooFitter(TH1 *h, std::vector<int> indices, TFile *fileOutput)
        : h1(h)
    {
        // RooRealVar obs("obs", "Observable", h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());

        if constexpr (partType == kK0S)
        {
            RooRealVar obs("obs", "Observable", 0.45, 0.55);
            RooDataHist dataHist("dataHist", "Data Histogram", obs, Import(*h1));

            // const std::vector<double> params{1., 1., 5., 5., 0.49, 0.003, -1.};
            // const std::vector<double> lowLimits{1., 1., 1., 1., 0.48, 0.001, -1.7};
            // const std::vector<double> upLimits{2., 2., 10., 10., 0.5, 0.01, 10};

            // const std::vector<double> params{1.0, 1.0, 5., 5., 0.497, 0.005, -0.5, 0.1};
            // const std::vector<double> lowLimits{0.1, 0.1, 0.1, 0.1, 0.48, 0.001, -1.0, -1.0};
            // const std::vector<double> upLimits{10., 10., 50., 50., 0.51, 0.02, 1.0, 1.0;

            const std::vector<double> params{1.2, 1.2, 2.0, 2.0, 0.497, 0.004, -0.8, 0.1};
            const std::vector<double> lowLimits{0.5, 0.5, 1.1, 1.1, 0.48, 0.001, -1.0, -1.0};
            const std::vector<double> upLimits{4.0, 4.0, 30., 30., 0.51, 0.008, 1.0, 1.0};

            RooRealVar alpha1CB("alpha1CB", "alpha1CB", params.at(0), lowLimits.at(0), upLimits.at(0));
            RooRealVar alpha2CB("alpha2CB", "alpha2CB", params.at(1), lowLimits.at(1), upLimits.at(1));
            RooRealVar n1CB("n1CB", "n1CB", params.at(2), lowLimits.at(2), upLimits.at(2));
            RooRealVar n2CB("n2CB", "n2CB", params.at(3), lowLimits.at(3), upLimits.at(3));
            RooRealVar meanCB("meanCB", "meanCB", params.at(4), lowLimits.at(4), upLimits.at(4));
            RooRealVar sigmaCB("sigmaCB", "sigmaCB", params.at(5), lowLimits.at(5), upLimits.at(5));
            RooCrystalBall dsCrystalBall("dsCrystalBall", "DoubleSidedCrystalBall1", obs, meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB);

            // RooRealVar a1("a1", "a1", params.at(6), lowLimits.at(6), upLimits.at(6));
            // RooPolynomial bkgDSCB("bkgDSCB", "bkgDSCB", obs, RooArgList(a1));

            RooRealVar c1("c1", "Chebyshev param 1", params.at(6), lowLimits.at(6), upLimits.at(6));
            RooRealVar c2("c2", "Chebyshev param 2", params.at(7), lowLimits.at(7), upLimits.at(7));
            RooChebychev bkgCheby("bkgCheby", "Background Chebyshev", obs, RooArgList(c1));
            // RooChebychev bkgCheby("bkgCheby", "Background Chebyshev", obs, RooArgList(c1, c2));

            RooRealVar nsig("nsig", "nsig", 100000, 0, 100000000);
            RooRealVar nbkg("nbkg", "nbkg", 50000, 0, 25000000);

            // RooAddPdf model("model", "model", RooArgList(dsCrystalBall, bkgDSCB), RooArgList(nsig, nbkg));
            RooAddPdf model("model", "model", RooArgList(dsCrystalBall, bkgCheby), RooArgList(nsig, nbkg));

            RooFitResult *result = model.fitTo(dataHist, Optimize(1), Extended(1), Save(1), NumCPU(4));

            std::cout << "\n\n========================================================" << std::endl;
            std::cout << "=== RISULTATI FIT PER IL BIN: " << indices.at(0) << ", " << indices.at(1) << " ===" << std::endl;
            std::cout << "========================================================" << std::endl;
            result->Print("v");

            int lowEdge = h1->GetXaxis()->FindFixBin(meanCB.getVal() - 3. * sigmaCB.getVal());
            int upEdge = h1->GetXaxis()->FindFixBin(meanCB.getVal() + 3. * sigmaCB.getVal());

            // double lowFitK0S = h1->GetXaxis()->GetBinLowEdge(lowEdge);
            // double upFitK0S = h1->GetXaxis()->GetBinLowEdge(upEdge + 1);
            double lowFitK0S = 0.47;
            double upFitK0S = 0.53;
            obs.setRange("signalRegion", lowFitK0S, upFitK0S);

            RooAbsReal *signalIntegral = dsCrystalBall.createIntegral(obs, NormSet(obs), Range("signalRegion"));
            RooProduct signalYield("signalYield", "signalYield", RooArgList(nsig, *signalIntegral));
            signalIntegralAndError = std::make_pair(signalYield.getVal(), signalYield.getPropagatedError(*result, RooArgSet(obs)));

            // RooAbsReal *bkgIntegral = bkgDSCB.createIntegral(obs, NormSet(obs), Range("signalRegion"));
            RooAbsReal *bkgIntegral = bkgCheby.createIntegral(obs, NormSet(obs), Range("signalRegion"));
            RooProduct bkgYield("bkgYield", "bkgYield", RooArgList(nbkg, *bkgIntegral));
            bkgIntegralAndError = std::make_pair(bkgYield.getVal(), bkgYield.getPropagatedError(*result, RooArgSet(obs)));

            RooFormulaVar purity("purity", "Signal Purity", "@0 / (@0 + @1)", RooArgList(signalYield, bkgYield));
            purityAndError = std::make_pair(purity.getVal(), purity.getPropagatedError(*result, RooArgSet(obs)));

            RooPlot *frame = obs.frame(Name(Form("Frame_%d_%d", indices.at(0), indices.at(1))));
            dataHist.plotOn(frame);
            model.plotOn(frame);
            model.plotOn(frame, Components(dsCrystalBall), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
            // model.plotOn(frame, Components(bkgDSCB), LineColor(kGreen + 2), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
            model.plotOn(frame, Components(bkgCheby), LineColor(kGreen + 2), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));

            TCanvas *c = new TCanvas(h1->GetName(), h1->GetName(), 800, 600);
            c->SetLogy();
            frame->Draw();

            TLine *line1 = new TLine(lowFitK0S, frame->GetMinimum(), lowFitK0S, frame->GetMaximum());
            line1->SetLineColor(kBlack);
            line1->SetLineStyle(kDashed);
            line1->SetLineWidth(2);
            line1->Draw("same");

            TLine *line2 = new TLine(upFitK0S, frame->GetMinimum(), upFitK0S, frame->GetMaximum());
            line2->SetLineColor(kBlack);
            line2->SetLineStyle(kDashed);
            line2->SetLineWidth(2);
            line2->Draw("same");

            TDirectory *savedDir = gDirectory;

            fileOutput->cd();
            c->Write();

            savedDir->cd();

            delete c;
            delete frame;
            delete signalIntegral;
            delete bkgIntegral;
            delete result;
        }
        else if constexpr (partType == kPi)
        {
            RooRealVar obs("obs", "Observable", -10., 10.);
            RooDataHist dataHist("dataHist", "Data Histogram", obs, Import(*h1));

            // const std::vector<double> params{1., 1., 10., 10., 0., 1., 8., 1.};
            // const std::vector<double> lowLimits{1., 1., 1., 1., -1., 0.001, 6., 0.001};
            // const std::vector<double> upLimits{5., 5., 10., 10., 1.5, 2.5, 10., 3.};

            const std::vector<double> params{1.2, 1.2, 5.0, 5.0, 0.0, 1.0, 5.0, 1.5};
            const std::vector<double> lowLimits{0.5, 0.5, 1.1, 1.1, -1.0, 0.5, 4.0, 0.5};
            const std::vector<double> upLimits{5.0, 5.0, 100., 100., 1.0, 2.5, 8.0, 5.0};

            RooRealVar alpha1CB("alpha1CB", "alpha1CB", params.at(0), lowLimits.at(0), upLimits.at(0));
            RooRealVar alpha2CB("alpha2CB", "alpha2CB", params.at(1), lowLimits.at(1), upLimits.at(1));
            RooRealVar n1CB("n1CB", "n1CB", params.at(2), lowLimits.at(2), upLimits.at(2));
            RooRealVar n2CB("n2CB", "n2CB", params.at(3), lowLimits.at(3), upLimits.at(3));
            RooRealVar meanCB("meanCB", "meanCB", params.at(4), lowLimits.at(4), upLimits.at(4));
            RooRealVar sigmaCB("sigmaCB", "sigmaCB", params.at(5), lowLimits.at(5), upLimits.at(5));
            RooCrystalBall dsCrystalBall("dsCrystalBall", "DoubleSidedCrystalBall1", obs, meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB);

            RooRealVar meanG2("meanG2", "meanG2", params.at(6), lowLimits.at(6), upLimits.at(6));
            RooRealVar sigmaG2("sigmaG2", "sigmaG2", params.at(7), lowLimits.at(7), upLimits.at(7));
            RooGaussian gauss("gauss2", "gauss2", obs, meanG2, sigmaG2);

            RooRealVar nsig("nsig", "nsig", 100000, 0, 10000000000);
            RooRealVar nbkg("nbkg", "nbkg", 50000, 0, 2500000000);

            RooAddPdf model("model", "model", RooArgList(dsCrystalBall, gauss), RooArgList(nsig, nbkg));

            RooFitResult *result = model.fitTo(dataHist, Optimize(1), Extended(1), Save(1), NumCPU(4));

            int lowEdge = h1->GetXaxis()->FindFixBin(meanCB.getVal() - 3. * sigmaCB.getVal());
            int upEdge = h1->GetXaxis()->FindFixBin(meanCB.getVal() + 3. * sigmaCB.getVal());

            double lowFitPi = h1->GetXaxis()->GetBinLowEdge(lowEdge);
            double upFitPi = h1->GetXaxis()->GetBinLowEdge(upEdge + 1);
            obs.setRange("signalRegion", lowFitPi, upFitPi);

            RooAbsReal *signalIntegral = dsCrystalBall.createIntegral(obs, NormSet(obs), Range("signalRegion"));
            RooProduct signalYield("signalYield", "signalYield", RooArgList(nsig, *signalIntegral));
            signalIntegralAndError = std::make_pair(signalYield.getVal(), signalYield.getPropagatedError(*result, RooArgSet(obs)));

            RooAbsReal *bkgIntegral = gauss.createIntegral(obs, NormSet(obs), Range("signalRegion"));
            RooProduct bkgYield("bkgYield", "bkgYield", RooArgList(nbkg, *bkgIntegral));
            bkgIntegralAndError = std::make_pair(bkgYield.getVal(), bkgYield.getPropagatedError(*result, RooArgSet(obs)));

            RooFormulaVar purity("purity", "Signal Purity", "@0 / (@0 + @1)", RooArgList(signalYield, bkgYield));
            purityAndError = std::make_pair(purity.getVal(), purity.getPropagatedError(*result, RooArgSet(obs)));

            RooPlot *frame = obs.frame(Name(Form("Frame_%d_%d", indices.at(0), indices.at(1))));
            dataHist.plotOn(frame);
            model.plotOn(frame);
            model.plotOn(frame, Components(dsCrystalBall), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
            model.plotOn(frame, Components(gauss), LineColor(kGreen + 2), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));

            TCanvas *c = new TCanvas(h1->GetName(), h1->GetName(), 800, 600);
            c->SetLogy();
            frame->Draw();

            TLine *line1 = new TLine(lowFitPi, frame->GetMinimum(), lowFitPi, frame->GetMaximum());
            line1->SetLineColor(kBlack);
            line1->SetLineStyle(kDashed);
            line1->SetLineWidth(2);
            line1->Draw("same");

            TLine *line2 = new TLine(upFitPi, frame->GetMinimum(), upFitPi, frame->GetMaximum());
            line2->SetLineColor(kBlack);
            line2->SetLineStyle(kDashed);
            line2->SetLineWidth(2);
            line2->Draw("same");

            TDirectory *savedDir = gDirectory;

            fileOutput->cd();
            c->Write();

            savedDir->cd();

            delete c;
            delete frame;
            delete signalIntegral;
            delete bkgIntegral;
            delete result;
        }
    }

    std::pair<double, double> getSignalAndError() const { return signalIntegralAndError; }
    std::pair<double, double> getBkgIntegralAndError() const { return bkgIntegralAndError; }
    std::pair<double, double> getPurityAndError() const { return purityAndError; }

    void constructPuritySpectrum(TH1 *hPuritySpectrum, int binIndex)
    {
        hPuritySpectrum->SetBinContent(binIndex, purityAndError.first);
        hPuritySpectrum->SetBinError(binIndex, purityAndError.second);
    }

private:
    TH1 *h1 = nullptr;

    std::pair<double, double> signalIntegralAndError{0.0, 0.0};
    std::pair<double, double> bkgIntegralAndError{0.0, 0.0};
    std::pair<double, double> purityAndError{0.0, 0.0};
};*/

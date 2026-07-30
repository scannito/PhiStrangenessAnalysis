#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "CorrelationCalculator.h"
#include "ExtrapConfigManager.h"
#include "ExtrapolationModelFactory.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"
#include "SpectrumExtrapolator.h"
#include "YieldMean.h"

#include "TCanvas.h"
#include "TDirectory.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH1F.h"
#include "TH2D.h"
#include "THnSparse.h"
#include "TLegend.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Shared logic between CorrelationTask and CorrelationWPDGTask.
// Init() and Run() stay pure virtual: the data source and correlation
// extraction strategy differ enough between derived tasks that forcing
// a shared implementation would just mean flags to switch behavior off.
// Everything else (corrections loading, trend bookkeeping, spectra/extrap
// generation, and Terminate) is identical or near-identical and lives here.
class CorrelationTaskBase : public IAnalysisTask
{
 public:
  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override = 0;
  void Run() override = 0;

  void Terminate() override
  {
    std::cout << "[INFO] " << GetName() << ": TERMINATING AND CLEANING UP..." << std::endl;

    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    std::vector<std::string> baseLogicalPath = {globalCfgs.binningName, dirName};

    // =========================================================================
    // 1. Write Yield Trends, Ratios Extrap/Meas, and Canvases PER SPECIES
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const std::string& pName = assocParticles[pIdx].name;

      // Create a dedicated subdirectory for each particle (e.g., Extract1D/K0S)
      std::vector<std::string> particlePath = baseLogicalPath;
      particlePath.push_back(pName);
      TDirectory* particleDir = AnalysisUtils::GetOrCreatePath(fileOutputSpectra.get(), particlePath);

      if (!particleDir)
        continue;
      particleDir->cd();

      // --- Save measured trends ---
      std::string cNameMeas = std::format("cTrend_Meas_{}", pName);
      std::unique_ptr<TCanvas> cTrendMeas = std::make_unique<TCanvas>(cNameMeas.c_str(), "Measured Yield Trend", 800, 600);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        particleDir->cd();
        h1MultTrends[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

        cTrendMeas->cd();
        AnalysisUtils::SetHistogramStyle(h1MultTrends[pIdx][yIdx].get(), globalCfgs.GetMultTrendColor(yIdx));
        h1MultTrends[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
      }
      particleDir->cd();
      cTrendMeas->Write(nullptr, TObject::kOverwrite);

      // --- Save extrapolated trends ---
      bool hasExtrapTrend = !h1MultTrendsExtrap.empty() && !h1MultTrendsExtrap[pIdx].empty();
      if (hasExtrapTrend) {
        std::string cNameExtrap = std::format("cTrend_Extrap_{}", pName);
        std::unique_ptr<TCanvas> cTrendExtrap = std::make_unique<TCanvas>(cNameExtrap.c_str(), "Extrapolated Yield Trend", 800, 600);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          particleDir->cd();
          h1MultTrendsExtrap[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

          cTrendExtrap->cd();
          AnalysisUtils::SetHistogramStyle(h1MultTrendsExtrap[pIdx][yIdx].get(), globalCfgs.GetMultTrendColor(yIdx));
          h1MultTrendsExtrap[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
        }
        particleDir->cd();
        cTrendExtrap->Write(nullptr, TObject::kOverwrite);
      }

      // --- Save Extrapolated/Measured ratios for this species ---
      if (hasExtrapTrend) {
        std::string canvasName = std::format("cRatio_ExtrapVsMeas_{}", pName);
        std::unique_ptr<TCanvas> cRatioExtrapMeas = std::make_unique<TCanvas>(canvasName.c_str(), "Extrap / Measured Ratio", 800, 600);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          double dyLimit = deltaYLimits[yIdx];
          auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

          std::string ratioName = std::format("Ratio_Extrap_Meas_{}_dy{}", pName, dyNameStr);
          std::string title = Form("Extrap/Measured Contribution %s |#Delta y| < %.2f; Multiplicity percentile (%%); Yield_{Extrap} / Yield_{Meas}", pName.c_str(), dyLimit);

          std::unique_ptr<TH1> hRatioExtrapMeas = AnalysisUtils::MakeRatioHist(h1MultTrendsExtrap[pIdx][yIdx].get(), h1MultTrends[pIdx][yIdx].get(),
                                                                               ratioName, title, globalCfgs.GetMultTrendColor(yIdx), 1.0, 1.0);

          /*TH1* hRatioExtrapMeas = static_cast<TH1*>(h1MultTrendsExtrap[pIdx][yIdx]->Clone(ratioName.c_str()));
          hRatioExtrapMeas->SetTitle(title.c_str());
          hRatioExtrapMeas->SetDirectory(0);
          hRatioExtrapMeas->Divide(h1MultTrendsExtrap[pIdx][yIdx], h1MultTrends[pIdx][yIdx], 1.0, 1.0);*/

          cRatioExtrapMeas->cd();
          AnalysisUtils::SetHistogramStyle(hRatioExtrapMeas.get(), globalCfgs.GetMultTrendColor(yIdx));
          hRatioExtrapMeas->DrawCopy(yIdx == 0 ? "" : "SAME");
        }

        particleDir->cd();
        cRatioExtrapMeas->Write(nullptr, TObject::kOverwrite);
      }

      // --- Save accumulated spectra Canvases for this species ---
      for (auto& canvas : spectraCanvases[pIdx]) {
        if (canvas) {
          particleDir->cd();
          canvas->Write(nullptr, TObject::kOverwrite);
        }
      }
    }
    spectraCanvases.clear();

    // =========================================================================
    // 2. Write Yield Ratios across species (into a dedicated "Ratios" directory)
    // =========================================================================
    if (!requestedRatios.empty()) {
      // Create or access the "Ratios" directory inside Extract1D/2D
      std::vector<std::string> ratioPath = baseLogicalPath;
      ratioPath.push_back("Ratios");
      TDirectory* ratioDir = AnalysisUtils::GetOrCreatePath(fileOutputSpectra.get(), ratioPath);

      if (ratioDir) {
        ratioDir->cd();

        // Helper lambda function to find particle index by name
        auto getParticleIndex = [&](const std::string& name) -> int {
          for (size_t i = 0; i < assocParticles.size(); ++i) {
            if (assocParticles[i].name == name)
              return i;
          }
          return -1;
        };

        for (const auto& ratioCfg : requestedRatios) {
          int idxNum = getParticleIndex(ratioCfg.num);
          int idxDen = getParticleIndex(ratioCfg.den);

          if (idxNum == -1 || idxDen == -1) {
            std::cerr << "[WARNING] Cannot compute ratio " << ratioCfg.num << "/" << ratioCfg.den
                      << " because one or both particles are not configured." << std::endl;
            continue; // Skip this specific ratio and move to the next
          }

          const auto& num = assocParticles[idxNum];
          const auto& den = assocParticles[idxDen];

          double numScale = GetYieldScaleFactor(num.name);
          double denScale = GetYieldScaleFactor(den.name);

          bool numHasExtrap = doExtrapolationPerParticle.count(num.name) && doExtrapolationPerParticle.at(num.name);
          bool denHasExtrap = doExtrapolationPerParticle.count(den.name) && doExtrapolationPerParticle.at(den.name);
          bool doExtrapRatio = applyExtrapolation && (numHasExtrap || denHasExtrap);

          std::string canvasName = std::format("canvasRatio_{}_{}_MultTrend", num.name, den.name);
          std::unique_ptr<TCanvas> canvasRatio = std::make_unique<TCanvas>(canvasName.c_str(), ("Ratio Mult Trend " + ratioCfg.label).c_str(), 800, 600);
          canvasRatio->cd();

          // std::unique_ptr<TLegend> legend = std::make_unique<TLegend>(0.7, 0.7, 0.9, 0.9);
          TLegend* legend = new TLegend(0.7, 0.7, 0.9, 0.9);
          legend->SetNColumns(2);
          legend->SetLineWidth(0);

          for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
            double dyLimit = deltaYLimits[yIdx];
            auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

            std::string ratioMeasName = std::format("Ratio_{}_{}_Meas_dy{}", num.name, den.name, dyNameStr);
            std::string ratioMeasTitle = "Measured Ratio; Multiplicity percentile (%);" + ratioCfg.label;
            std::unique_ptr<TH1> hRatioMeas = AnalysisUtils::MakeRatioHist(h1MultTrends[idxNum][yIdx].get(), h1MultTrends[idxDen][yIdx].get(),
                                                                           ratioMeasName, ratioMeasTitle, globalCfgs.GetMultTrendColor(yIdx), numScale, denScale);

            /*TH1* hRatioMeas = static_cast<TH1*>(h1MultTrends[idxNum][yIdx]->Clone(ratioMeasName.c_str()));
            hRatioMeas->SetTitle(("Ratio;Multiplicity Percentile (%);" + ratioCfg.label).c_str());
            hRatioMeas->SetDirectory(0);
            hRatioMeas->Divide(h1MultTrends[idxNum][yIdx], h1MultTrends[idxDen][yIdx], numScale, denScale);*/
            AnalysisUtils::SetHistogramStyle(hRatioMeas.get(), globalCfgs.GetMultTrendColor(yIdx));
            hRatioMeas->SetMarkerStyle(24);

            std::unique_ptr<TH1> hRatioExtrap{nullptr};
            if (doExtrapRatio) {
              TH1* hNumTrend = numHasExtrap ? h1MultTrendsExtrap[idxNum][yIdx].get() : h1MultTrends[idxNum][yIdx].get();
              TH1* hDenTrend = denHasExtrap ? h1MultTrendsExtrap[idxDen][yIdx].get() : h1MultTrends[idxDen][yIdx].get();

              std::string ratioExtrapName = std::format("Ratio_{}_{}_Extrap_dy{}", num.name, den.name, dyNameStr);
              std::string ratioExtrapTitle = "Extrapolated Ratio; Multiplicity percentile (%);" + ratioCfg.label;
              hRatioExtrap = AnalysisUtils::MakeRatioHist(hNumTrend, hDenTrend, ratioExtrapName, ratioExtrapTitle, globalCfgs.GetMultTrendColor(yIdx), numScale, denScale);

              /*hRatioExtrap = static_cast<TH1*>(hNumTrend->Clone(ratioExtrapName.c_str()));
              hRatioExtrap->SetDirectory(0);
              hRatioExtrap->Divide(hNumTrend, hDenTrend, numScale, denScale);*/
              AnalysisUtils::SetHistogramStyle(hRatioExtrap.get(), globalCfgs.GetMultTrendColor(yIdx));
              hRatioExtrap->SetMarkerStyle(20);
            }

            double globalMax = hRatioMeas->GetMaximum();
            double globalMin = hRatioMeas->GetMinimum();
            if (hRatioExtrap) {
              globalMax = std::max(globalMax, hRatioExtrap->GetMaximum());
              globalMin = std::min(globalMin, hRatioExtrap->GetMinimum());
            }
            hRatioMeas->GetYaxis()->SetRangeUser(globalMin * 0.9, globalMax * 1.2);

            // hRatioMeas->DrawCopy(yIdx == 0 ? "" : "SAME");
            // if (hRatioExtrap)
            // hRatioExtrap->DrawCopy("SAME");

            TH1* cloneMeas = hRatioMeas->DrawCopy(yIdx == 0 ? "" : "SAME");
            legend->AddEntry(cloneMeas, std::format("Meas. |#Delta y| < {}", dyTitleStr).c_str(), "p");
            if (hRatioExtrap) {
              TH1* cloneExtrap = hRatioExtrap->DrawCopy("SAME");
              legend->AddEntry(cloneExtrap, std::format("Extrap. |#Delta y| < {}", dyTitleStr).c_str(), "p");
            }
          }

          legend->Draw("SAME");
          ratioDir->cd();
          canvasRatio->Write(nullptr, TObject::kOverwrite);
        }
      }
    } else {
      std::cout << "[INFO] " << GetName() << ": No yield ratios requested. Skipping." << std::endl;
    }

    // =========================================================================
    // 3. Close Files
    // =========================================================================
    if (fileOutputSpectra) {
      fileOutputSpectra->Close();
    }

    for (auto& file : filesPhiAssocDataOutput) {
      file->Close();
    }

    for (auto& file : filesPhiAssocQAOutput) {
      file->Close();
    }

    std::cout << "[INFO] " << GetName() << ": DONE." << std::endl;
  }

 protected:
  AnalysisSettings globalCfgs;

  std::string basePathData, basePathDataME;

  bool applyME{false}, applyEfficiency{false}, applyExtrapolation{false};
  bool useIntegratedEfficiency{false}, useProjectionCache{false}, use2DMENormalization{false};
  bool useLegacyExtrapolation{false};

  std::unique_ptr<TH1> hEventLoss;

  std::vector<AssocParticleConfig> assocParticles;
  std::vector<LoadedAssocData> loadedDataCollection;

  std::map<std::string, LoadedCorrections> correctionCollection;

  CorrelationCalculator::AxisTarget projectionAxis{CorrelationCalculator::AxisTarget::DeltaY_Y};

  std::map<std::string, bool> doExtrapolationPerParticle;

  std::vector<std::vector<std::vector<std::unique_ptr<TH1>>>> h1PhiAssocNoPtPhi;

  std::vector<double> deltaYLimits{1.0, 0.5, 0.1};

  std::vector<std::vector<std::unique_ptr<TH1>>> h1MultTrends;
  std::vector<std::vector<std::unique_ptr<TH1>>> h1MultTrendsExtrap;

  std::unique_ptr<ExtrapConfigManager> extrapConfigManager;

  std::unique_ptr<TFile> fileOutputSpectra;
  std::vector<std::unique_ptr<TFile>> filesPhiAssocDataOutput;
  std::vector<std::unique_ptr<TFile>> filesPhiAssocQAOutput;

  std::vector<std::vector<std::unique_ptr<TCanvas>>> spectraCanvases;

  std::vector<YieldRatioConfig> requestedRatios;

  // -------------------------------------------------------------------------
  // Hooks for derived-class-specific behavior in Terminate()
  // -------------------------------------------------------------------------
  // Hook for GenerateSpectraAndTrends: returns a purity correction histogram
  // for this particle/multBin, or nullptr if none should be applied.
  // CorrelationTask overrides this; CorrelationWPDGTask leaves the default.
  virtual TH1* GetPurityHist(const std::string& /*particleName*/, int /*multBin*/) { return nullptr; }

  // Trigger yield / background ratio for a given (multBin, ptPhiBin), used by
  // the shared RunLegacy(). Every derived task MUST provide GetTriggerSignal;
  // GetTriggerBkgRatio defaults to "no background subtraction" (WPDG's case).
  virtual double GetTriggerSignal(int multBin, int ptPhiBin) = 0;
  virtual double GetTriggerBkgRatio(int /*multBin*/, int /*ptPhiBin*/) { return 0.0; }

  // -------------------------------------------------------------------------
  // Shared JSON parsing for flags common to both derived Init() implementations.
  // Call this first thing from each derived Init().
  // -------------------------------------------------------------------------
  void InitCommonFlags(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings)
  {
    globalCfgs = globalSettings;

    /*applyME = taskConfig["apply_mixed_events"].GetBool();
    applyEfficiency = taskConfig["apply_efficiency"].GetBool();
    applyExtrapolation = taskConfig["apply_extrapolation"].GetBool();
    useIntegratedEfficiency = taskConfig["use_integrated_efficiency"].GetBool();
    useProjectionCache = taskConfig["use_projection_cache"].GetBool();
    use2DMENormalization = taskConfig["use_2d_me_normalization"].GetBool();*/

    applyME = RequireBool(taskConfig, "apply_mixed_events", GetName());
    applyEfficiency = RequireBool(taskConfig, "apply_efficiency", GetName());
    applyExtrapolation = RequireBool(taskConfig, "apply_extrapolation", GetName());
    useIntegratedEfficiency = RequireBool(taskConfig, "use_integrated_efficiency", GetName());
    useProjectionCache = RequireBool(taskConfig, "use_projection_cache", GetName());
    use2DMENormalization = RequireBool(taskConfig, "use_2d_me_normalization", GetName());

    if (taskConfig.HasMember("use_legacy_extrapolation") && taskConfig["use_legacy_extrapolation"].IsBool()) {
      useLegacyExtrapolation = taskConfig["use_legacy_extrapolation"].GetBool();
    }

    if (taskConfig.HasMember("projection_axis") && taskConfig["projection_axis"].IsString()) {
      std::string pAxis = taskConfig["projection_axis"].GetString();
      if (pAxis == "DeltaPhi" || pAxis == "delta_phi") {
        projectionAxis = CorrelationCalculator::AxisTarget::DeltaPhi_X;
        std::cout << "[INFO] " << GetName() << ": Physics projection axis set to Delta Phi (X)." << std::endl;
      } else if (pAxis == "DeltaY" || pAxis == "delta_y") {
        projectionAxis = CorrelationCalculator::AxisTarget::DeltaY_Y;
        std::cout << "[INFO] " << GetName() << ": Physics projection axis set to Delta Y (Y)." << std::endl;
      } else {
        std::cerr << "[WARNING] " << GetName() << ": Unknown projection_axis '" << pAxis << "'. Defaulting to DeltaY." << std::endl;
      }
    }

    if (taskConfig.HasMember("yield_ratios") && taskConfig["yield_ratios"].IsArray()) {
      for (const auto& r : taskConfig["yield_ratios"].GetArray()) {
        YieldRatioConfig ratio;
        ratio.num = r["numerator"].GetString();
        ratio.den = r["denominator"].GetString();
        if (r.HasMember("label") && r["label"].IsString()) {
          ratio.label = r["label"].GetString();
        } else {
          ratio.label = ratio.num + "/" + ratio.den;
        }
        requestedRatios.push_back(ratio);
      }
    }

    if (taskConfig.HasMember("delta_y_limits")) {
      deltaYLimits.clear();
      for (const auto& v : taskConfig["delta_y_limits"].GetArray())
        deltaYLimits.push_back(v.GetDouble());
    }
  }

  // -------------------------------------------------------------------------
  // Shared parsing of "associated_particles". Requires globalCfgs already set.
  // -------------------------------------------------------------------------
  void InitAssocParticles(const rapidjson::Value& taskConfig)
  {
    assocParticles.clear();
    for (const auto& sp : taskConfig["associated_particles"].GetArray()) {
      std::string name = sp["name"].GetString();
      std::string dirName = sp["dir_name"].GetString();
      const auto& binning = globalCfgs.GetPtBinning(name);
      assocParticles.emplace_back(name, dirName, static_cast<int>(binning.size()) - 1, binning, AnalysisConstants::GetMass(name));
    }
  }

  // -------------------------------------------------------------------------
  // Shared efficiency loading + extrapolation config setup.
  // Requires assocParticles already populated.
  // -------------------------------------------------------------------------
  void InitCorrectionsAndExtrapolation(const rapidjson::Value& taskConfig)
  {
    if (applyEfficiency)
      LoadCorrections(taskConfig);

    if (applyExtrapolation) {
      if (!taskConfig.HasMember("extrapolation_config_file"))
        throw std::runtime_error("[FATAL ERROR] " + GetName() + ": 'extrapolation_config_file' missing in JSON!");
      std::string extrapFile = taskConfig["extrapolation_config_file"].GetString();
      // extrapConfigManager = new ExtrapConfigManager(extrapFile);
      extrapConfigManager = std::make_unique<ExtrapConfigManager>(extrapFile);
      std::cout << "[INFO] " << GetName() << ": Extrapolation configuration loaded successfully." << std::endl;

      for (const auto& p : assocParticles) {
        bool hasCfg = extrapConfigManager->HasConfig(p.name);
        doExtrapolationPerParticle[p.name] = hasCfg;
        if (!hasCfg)
          std::cout << "[INFO] " << GetName() << ": No extrapolation config found for '" << p.name << "'. Skipping." << std::endl;
      }
    }
  }

  // -------------------------------------------------------------------------
  // Shared efficiency / signal-loss / event-loss loading.
  // -------------------------------------------------------------------------
  void LoadCorrections(const rapidjson::Value& taskConfig)
  {
    /*if (!taskConfig.HasMember("input_efficiency_file")) {
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_efficiency_file' missing in JSON!");
    }

    std::string inputEffFile = taskConfig["input_efficiency_file"].GetString();
    TFile* fileEffInput = new TFile(inputEffFile.c_str(), "READ");
    if (!fileEffInput || fileEffInput->IsZombie()) {
      throw std::runtime_error("[FATAL] CorrelationTask: Efficiency requested but Corrections.root not found at: " + inputEffFile);
    }*/

    std::string inputEffFile = RequireString(taskConfig, "input_efficiency_file", GetName());
    std::unique_ptr<TFile> fileEffInput = OpenOrThrow(inputEffFile, "READ", "CorrelationTaskBase::LoadCorrections");

    // Mirror the exact layout MCTask writes: {binningName}/AccEff/MultBin,
    // {binningName}/SigLoss/MultBin, {binningName}/EvLoss
    TDirectory* accEffDir = AnalysisUtils::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "AccEff", "MultBin"}, true);
    TDirectory* sigLossDir = AnalysisUtils::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "SigLoss", "MultBin"}, true);
    TDirectory* evLossDir = AnalysisUtils::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "EvLoss"}, true);

    std::vector<std::string> activeCorrections;
    if (taskConfig.HasMember("active_corrections") && taskConfig["active_corrections"].IsArray()) {
      for (const auto& val : taskConfig["active_corrections"].GetArray()) {
        activeCorrections.push_back(val.GetString());
      }
    } else {
      std::cout << "[INFO] 'active_corrections' missing or not an array. Applying NO corrections!" << std::endl;
    }

    // Lambda to quickly check if a specific correction was requested in the JSON
    auto isActive = [&](const std::string& name) {
      return std::find(activeCorrections.begin(), activeCorrections.end(), name) != activeCorrections.end();
    };

    bool doAccEfficiency = isActive("acceptance_efficiency");
    bool doSigLoss = isActive("signal_loss");

    // Read TO WHICH PARTICLES they should be applied
    std::vector<std::string> effParts;
    if (taskConfig.HasMember("apply_efficiency_to") && taskConfig["apply_efficiency_to"].IsArray()) {
      for (const auto& val : taskConfig["apply_efficiency_to"].GetArray())
        effParts.push_back(val.GetString());
    } else {
      // Secure fallback: if the user doesn't specify it in the JSON, apply to Phi + associated particles in the analysis
      effParts.push_back("Phi");
      for (const auto& p : assocParticles)
        effParts.push_back(p.name);
      std::cout << "[INFO] 'apply_efficiency_to' not found. Defaulting to all active particles." << std::endl;
    }

    // Event Loss is loaded if requested
    if (isActive("event_loss")) {
      if (!evLossDir)
        throw std::runtime_error("[FATAL] 'event_loss' requested but directory '" + globalCfgs.binningName + "/EvLoss' not found!");

      if (auto hTemp = static_cast<TH1*>(evLossDir->Get("hEventLoss"))) {
        hEventLoss.reset(static_cast<TH1*>(hTemp->Clone("hEventLoss")));
        hEventLoss->SetDirectory(0);
      } else
        throw std::runtime_error("[FATAL] 'event_loss' requested but 'hEventLoss' not found!");
    }

    // LAMBDA HELPER 1: Load the histogram and throw an error if missing
    auto fetchHist = [&](TDirectory* dir, const std::string& name, bool apply, const std::vector<double>& targetBinning) -> std::unique_ptr<TH1F> {
      if (!apply)
        return nullptr;
      if (!dir)
        throw std::runtime_error("[FATAL] Missing directory containing: " + name);

      std::unique_ptr<TH1F> h = GetUniqueOrThrow<TH1F>(dir, name, "CorrelationTaskBase::LoadCorrections");

      return AnalysisUtils::RebinToTargetBinning(std::move(h), targetBinning, "CorrelationTaskBase::LoadCorrections");
    };

    // LAMBDA HELPER 2: Clone and multiply (if both are needed) safely
    auto combineHists = [](const TH1F* hEff, const TH1F* hLoss, const std::string& name) -> std::unique_ptr<TH1F> {
      if (!hEff && !hLoss)
        return nullptr;

      std::unique_ptr<TH1F> hRes(static_cast<TH1F*>(hEff ? hEff->Clone(name.c_str()) : hLoss->Clone(name.c_str())));
      if (hEff && hLoss)
        hRes->Multiply(hLoss);
      hRes->SetDirectory(0);

      return hRes;
    };

    correctionCollection.clear();

    for (const std::string& name : effParts) {
      LoadedCorrections corr;
      corr.name = name;
      corr.h1Corrections.resize(globalCfgs.nBinMult);
      corr.h1CorrectionsEffMultInt.resize(globalCfgs.nBinMult);

      std::vector<double> targetBinning;
      if (name == "Phi") {
        targetBinning = globalCfgs.GetPtBinning("Phi");
      } else {
        auto it = std::find_if(assocParticles.begin(), assocParticles.end(), [&](const AssocParticleConfig& p) { return p.name == name; });
        if (it == assocParticles.end())
          throw std::runtime_error("[FATAL] Unknown particle name in 'apply_efficiency_to': " + name);
        targetBinning = it->binning;
      }

      // Naming convention for histograms based on particle name
      std::string effHistBase = "h1" + name + "Efficiency";
      std::string sigLossHistBase = "h1" + name + "SigLoss";

      // Fetch integrated histograms ONLY if useIntegratedEfficiency is true
      std::unique_ptr<TH1F> hEffInt = fetchHist(accEffDir, effHistBase + "_multIntegrated", doAccEfficiency && useIntegratedEfficiency, targetBinning);
      // Note: Signal loss is typically not integrated, but we fetch it if requested for consistency
      // std::unique_ptr<TH1F> hLossInt = fetchHist(sigLossDir, sigLossHistBase + "_multIntegrated", doSigLoss && useIntegratedEfficiency, targetBinning);

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string iStr = std::to_string(i);

        // Fetch binned histograms ONLY if useIntegratedEfficiency is false
        std::unique_ptr<TH1F> hEffBin = fetchHist(accEffDir, effHistBase + "_multBin" + iStr, doAccEfficiency && !useIntegratedEfficiency, targetBinning);
        // Note: Signal loss is typically not integrated, so we fetch the binned version if signal loss correction is requested,
        // regardless of the useIntegratedEfficiency flag
        std::unique_ptr<TH1F> hLossBin = fetchHist(sigLossDir, sigLossHistBase + "_multBin" + iStr, doSigLoss, targetBinning);

        corr.h1Corrections[i] = combineHists(hEffBin.get(), hLossBin.get(), "h1Corrected_" + name + "_multBin" + iStr);
        corr.h1CorrectionsEffMultInt[i] = combineHists(hEffInt.get(), hLossBin.get(), "h1Corrected_" + name + "_multInt" + iStr);
      }

      correctionCollection[name] = std::move(corr);
    }

    std::cout << "[INFO] CorrelationTask: Efficiencies loaded successfully." << std::endl;
  }

  // -------------------------------------------------------------------------
  // Shared trend/spectra-canvas bookkeeping setup.
  // -------------------------------------------------------------------------
  void SetupTrendHistograms()
  {
    // 1. Initialize structure for pT bin accumulation
    h1PhiAssocNoPtPhi.resize(assocParticles.size());
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      h1PhiAssocNoPtPhi[pIdx].resize(globalCfgs.nBinMult);
      for (int i{0}; i < globalCfgs.nBinMult; ++i) {
        h1PhiAssocNoPtPhi[pIdx][i].resize(assocParticles[pIdx].nBinPt);
      }
    }

    // 2. Initialize matrix for Mult Trends: [ParticleIndex][DeltaYIndex]
    h1MultTrends.resize(assocParticles.size());
    if (applyExtrapolation) {
      h1MultTrendsExtrap.resize(assocParticles.size());
    }

    spectraCanvases.resize(assocParticles.size());

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& p = assocParticles[pIdx];
      bool doExtrapForThis = applyExtrapolation && doExtrapolationPerParticle.count(p.name) && doExtrapolationPerParticle.at(p.name);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];
        auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

        std::string hName = std::format("h1MultTrend_{}_dy{}", p.name, dyNameStr);
        std::string hTitle = std::format("Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

        std::unique_ptr<TH1F> hTrend = std::make_unique<TH1F>(hName.c_str(), hTitle.c_str(), globalCfgs.nBinMult, globalCfgs.binsMult.data());
        hTrend->SetDirectory(0);
        h1MultTrends[pIdx].push_back(std::move(hTrend));

        if (doExtrapForThis) {
          std::string hExtrapName = std::format("h1MultTrendExtrap_{}_dy{}", p.name, dyNameStr);
          std::string hExtrapTitle = std::format("Extrapolated Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

          std::unique_ptr<TH1F> hTrendExtrap = std::make_unique<TH1F>(hExtrapName.c_str(), hExtrapTitle.c_str(), globalCfgs.nBinMult, globalCfgs.binsMult.data());
          hTrendExtrap->SetDirectory(0);
          h1MultTrendsExtrap[pIdx].push_back(std::move(hTrendExtrap));
        }

        std::string cSpecName = std::format("cSpectra_{}_dy{}", p.name, dyNameStr);
        std::string cSpecTitle = std::format("Spectra {} |#Delta y| < {}", p.name, dyTitleStr);

        std::unique_ptr<TCanvas> cSpec = std::make_unique<TCanvas>(cSpecName.c_str(), cSpecTitle.c_str(), 800, 600);
        cSpec->SetLogy();
        spectraCanvases[pIdx].push_back(std::move(cSpec));
      }
    }
  }

  // -------------------------------------------------------------------------
  // Extrapolation function
  // -------------------------------------------------------------------------
  ExtrapolationResult ExtrapolateSpectrum(TH1* hSpec, const AssocParticleConfig& config, int multBin, TDirectory* targetDir)
  {
    // 1. Fetch configuration for THIS particle and THIS multBin from JSON
    ExtrapConfig eCfg = extrapConfigManager->GetConfig(config.name, multBin);
    eCfg.mass = config.mass; // Inject physical mass

    // 2. Generate configured TF1 model
    std::unique_ptr<TF1> extrapModel = ExtrapolationModelFactory::CreateModel(eCfg, hSpec->GetMaximum());

    /*{
      // std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}", hSpec->GetName(), extrapFunction);
      std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}_multBin{}", hSpec->GetName(), eCfg.model, multBin);
      std::unique_ptr<TCanvas> cDebug = std::make_unique<TCanvas>(debugCanvasName.c_str(), "Debug Guesses", 800, 600);
      // cDebug->SetLogy();

      hSpec->SetMarkerStyle(20);
      hSpec->SetMarkerColor(kBlack);
      hSpec->SetLineColor(kBlack);
      hSpec->DrawCopy();

      extrapModel->SetRange(0.0, 8.0);
      extrapModel->SetLineColor(kRed);
      extrapModel->SetLineWidth(2);
      extrapModel->DrawCopy("SAME");

      targetDir->cd();
      cDebug->Write(nullptr, TObject::kOverwrite);
    }*/

    // 3. Perform the extrapolation using either legacy or new method
    ExtrapolationResult res;
    if (useLegacyExtrapolation) {
      res = CalculateYieldAndMeanLegacy(hSpec, extrapModel.get(),
                                        eCfg.domainRange.first, eCfg.domainRange.second,
                                        0.01, 0.1, "0QI", "../Logs/logExtrapolation.root",
                                        eCfg.fitRange.first, eCfg.fitRange.second, config.name);
    } else {
      SpectrumExtrapolator extrapolator(hSpec, extrapModel.get());
      extrapolator.SetFitRange(eCfg.fitRange.first, eCfg.fitRange.second);

      res = extrapolator.CalculateYieldAndMean();
    }

    // 4. Debug output
    {
      double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");
      double firstDataPt = config.binning[0];
      double fitLowPtIntegral = 0.0;
      if (firstDataPt > 0.0) {
        fitLowPtIntegral = extrapModel->Integral(0.0, firstDataPt);
      }

      std::cout << "\n[DEBUG EXTRAP] " << std::endl;
      std::cout << "  -> Raw Data Integral (ROOT): " << rawIntegral << std::endl;
      std::cout << "  -> Total Extrapolated (extY): " << res.yield << std::endl;
      std::cout << "  -> Fit Integral [0.0 - " << firstDataPt << "]: " << fitLowPtIntegral << std::endl;
    }

    // 5. Create extended binning dynamically
    std::vector<double> extBinning;
    extBinning.push_back(0.0); // Lower limit for extrapolation
    for (double edge : config.binning) {
      extBinning.push_back(edge);
    }

    std::string extName = std::string(hSpec->GetName()) + "_extended";
    std::unique_ptr<TH1> hSpecExt = std::make_unique<TH1D>(extName.c_str(), extName.c_str(), extBinning.size() - 1, extBinning.data());
    hSpecExt->SetDirectory(0);

    // 6. Copy measured contents (shifted by 1 bin to the right)
    for (int b = 1; b <= hSpec->GetNbinsX(); ++b) {
      hSpecExt->SetBinContent(b + 1, hSpec->GetBinContent(b));
      hSpecExt->SetBinError(b + 1, hSpec->GetBinError(b));
    }

    // Leave the first bin empty (visual placeholder for the fit curve)
    hSpecExt->SetBinContent(1, 0.0);
    hSpecExt->SetBinError(1, 0.0);

    AnalysisUtils::SetHistogramStyle(hSpecExt.get(), globalCfgs.GetSpectraColor(multBin));
    hSpecExt->GetListOfFunctions()->Add(extrapModel->Clone());

    targetDir->cd();
    hSpecExt->Write(nullptr, TObject::kOverwrite);
  }

  // -------------------------------------------------------------------------
  // Shared spectra construction, normalization, and extrapolation.
  // -------------------------------------------------------------------------
  void GenerateSpectraAndTrends(int multBin, double totalTriggerSignalPerMult)
  {
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    TDirectory* targetSpectraDir = AnalysisUtils::GetOrCreatePath(fileOutputSpectra.get(), {globalCfgs.binningName, dirName});

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& config = assocParticles[pIdx];
      bool doExtrapForThis = applyExtrapolation && doExtrapolationPerParticle.count(config.name) && doExtrapolationPerParticle.at(config.name);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];

        auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

        std::string spectraName = std::format("h1SpectrumPhi{}_dy{}_multBin{}", config.name, dyNameStr, multBin);

        std::vector<TH1*> views;
        views.reserve(h1PhiAssocNoPtPhi[pIdx][multBin].size());
        for (const auto& h : h1PhiAssocNoPtPhi[pIdx][multBin])
          views.push_back(h.get());

        // Construct the 1D spectrum from the accumulated Pt bins
        std::unique_ptr<TH1> h1Spectrum = AnalysisUtils::ConstructSpectrum(views, config.binning, spectraName, dyLimit);

        double effectiveTriggers = totalTriggerSignalPerMult;

        // Normalize by the event loss correction if it is required
        if (applyEfficiency && hEventLoss) {
          double eventLossCorr = hEventLoss->GetBinContent(multBin + 1);
          if (eventLossCorr > 0.0) {
            effectiveTriggers /= eventLossCorr;
          }
        }

        // Normalize by the total number of triggers AND the Delta Y phase space
        if (totalTriggerSignalPerMult > 0.0) {
          h1Spectrum->Scale(1.0 / (effectiveTriggers * 2.0 * dyLimit));
        }

        // Purity correction hook: no-op unless overridden (CorrelationTask does).
        if (TH1* hPurity = GetPurityHist(config.name, multBin)) {
          h1Spectrum->Multiply(hPurity);
        }

        AnalysisUtils::SetHistogramStyle(h1Spectrum.get(), globalCfgs.GetSpectraColor(multBin));

        // Draw and Write (Only draw the nominal Delta Y limit on the summary canvas)
        spectraCanvases[pIdx][yIdx]->cd();
        h1Spectrum->DrawCopy(multBin == 0 ? "" : "SAME");

        targetSpectraDir->cd();
        h1Spectrum->Write(nullptr, TObject::kOverwrite);

        AnalysisUtils::ConstructMultTrend(h1MultTrends[pIdx][yIdx].get(), h1Spectrum.get(), multBin);

        // Extrapolate and Fill Trend
        if (doExtrapForThis) {
          ExtrapolationResult res = ExtrapolateSpectrum(h1Spectrum.get(), config, multBin, targetSpectraDir);
          AnalysisUtils::ConstructMultTrend(h1MultTrendsExtrap[pIdx][yIdx].get(), res, multBin);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Shared "simple" run: one pass over mult/ptPhi/pt-assoc bins, no L2 cache.
  // Relies on GetTriggerSignal/GetTriggerBkgRatio hooks for the data-source-
  // specific part. This is what CorrelationWPDGTask uses directly as its Run(),
  // and what CorrelationTask::RunLegacy() is when apply_purity/QA aren't needed
  // beyond what the hooks already provide.
  // -------------------------------------------------------------------------
  void RunLegacy()
  {
    std::cout << "[INFO] " << GetName() << ": RUNNING CORRELATIONS (Legacy)..." << std::endl;
    CorrelationCalculator corrCalculator(applyME, useProjectionCache, false, false);

    const std::vector<std::unique_ptr<TH1F>>* phiCorrs{nullptr};
    std::vector<const std::vector<std::unique_ptr<TH1F>>*> assocCorrs(assocParticles.size(), nullptr);

    if (applyEfficiency && !correctionCollection.empty()) {
      auto itPhi = correctionCollection.find("Phi");
      if (itPhi != correctionCollection.end())
        phiCorrs = useIntegratedEfficiency ? &itPhi->second.h1CorrectionsEffMultInt : &itPhi->second.h1Corrections;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        auto it = correctionCollection.find(assocParticles[pIdx].name);
        if (it != correctionCollection.end())
          assocCorrs[pIdx] = useIntegratedEfficiency ? &it->second.h1CorrectionsEffMultInt : &it->second.h1Corrections;
      }
    }

    const int nBinPtPhi = globalCfgs.GetNBinPt("Phi");
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    std::vector<std::string> logicalPath{globalCfgs.binningName, dirName};

    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      AnalysisUtils::AxisToCut axisToCutMult{0, i + 1, i + 1};
      double totalTriggerSignalPerMult = 0.0;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i].get() : nullptr;

      for (int j = 0; j < nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        double triggerSignal = GetTriggerSignal(i, j);
        double triggerBkgRatio = GetTriggerBkgRatio(i, j);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i].get() : nullptr;

          TDirectory* targetDir = AnalysisUtils::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, useProjectionCache);

          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string histNameBase = "h1Phi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

            std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(data, axesToCut, totalEff, triggerBkgRatio, histNameBase + suffix, targetDir, nullptr, projectionAxis);

            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_pt" + config.name + "Bin" + std::to_string(k);
              h1PhiAssocNoPtPhi[pIdx][i][k].reset(static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str())));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal.get());
            }
          }
        }
      }

      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    }
  }

  static double GetYieldScaleFactor(const std::string& name)
  {
    return (name == "K0S") ? 2.0 : 1.0;
  }
};

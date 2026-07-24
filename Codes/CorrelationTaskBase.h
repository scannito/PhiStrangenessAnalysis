#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "CorrelationCalculator.h"
#include "ExtrapConfigManager.h"
#include "ExtrapolationModelFactory.h"
#include "IAnalysisTask.h"
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

    // =========================================================================
    // 1. Write the Yield Ratio (only if exactly 2 associated particles)
    // =========================================================================
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    std::vector<std::string> logicalPath = {globalCfgs.binningName, dirName};
    TDirectory* targetSpectraDir = AnalysisUtils::GetOrCreatePath(fileOutputSpectra, logicalPath);
    if (targetSpectraDir)
      targetSpectraDir->cd();

    if (assocParticles.size() == 2) {
      const auto& num = assocParticles[0];
      const auto& den = assocParticles[1];

      double numScale = GetYieldScaleFactor(num.name);
      double denScale = GetYieldScaleFactor(den.name);
      std::string axisLabel = !yieldRatioLabel.empty() ? yieldRatioLabel : num.name + "/" + den.name;

      bool numHasExtrap = doExtrapolationPerParticle.count(num.name) && doExtrapolationPerParticle.at(num.name);
      bool denHasExtrap = doExtrapolationPerParticle.count(den.name) && doExtrapolationPerParticle.at(den.name);
      bool doExtrapRatio = applyExtrapolation && (numHasExtrap || denHasExtrap);

      std::string canvasName = std::format("canvasRatio_{}_{}_MultTrend", num.name, den.name);
      TCanvas* canvasRatio = new TCanvas(canvasName.c_str(), "Ratio Mult Trend", 800, 600);
      canvasRatio->cd();

      TLegend* legend = new TLegend(0.7, 0.7, 0.9, 0.9);
      legend->SetNColumns(2);
      legend->SetLineWidth(0);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];
        std::string dyTitleStr = Form("%.2f", dyLimit);
        std::string dyNameStr = dyTitleStr;
        std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

        std::string ratioMeasName = std::format("Ratio_{}_{}_Meas_dy{}", num.name, den.name, dyNameStr);
        TH1* hRatioMeas = static_cast<TH1*>(h1MultTrends[0][yIdx]->Clone(ratioMeasName.c_str()));
        hRatioMeas->SetTitle(("Ratio;Multiplicity Percentile (%);" + axisLabel).c_str());
        hRatioMeas->SetDirectory(0);
        hRatioMeas->Divide(h1MultTrends[0][yIdx], h1MultTrends[1][yIdx], numScale, denScale);
        AnalysisUtils::SetHistogramStyle(hRatioMeas, globalCfgs.GetMultTrendColor(yIdx));
        hRatioMeas->SetMarkerStyle(24);

        TH1* hRatioExtrap{nullptr};
        if (doExtrapRatio) {
          TH1* hNumTrend = numHasExtrap ? h1MultTrendsExtrap[0][yIdx] : h1MultTrends[0][yIdx];
          TH1* hDenTrend = denHasExtrap ? h1MultTrendsExtrap[1][yIdx] : h1MultTrends[1][yIdx];

          std::string ratioExtrapName = std::format("Ratio_{}_{}_Extrap_dy{}", num.name, den.name, dyNameStr);
          hRatioExtrap = static_cast<TH1*>(hNumTrend->Clone(ratioExtrapName.c_str()));
          hRatioExtrap->SetDirectory(0);
          hRatioExtrap->Divide(hNumTrend, hDenTrend, numScale, denScale);
          AnalysisUtils::SetHistogramStyle(hRatioExtrap, globalCfgs.GetMultTrendColor(yIdx));
          hRatioExtrap->SetMarkerStyle(20);
        }

        double globalMax = hRatioMeas->GetMaximum();
        double globalMin = hRatioMeas->GetMinimum();
        if (hRatioExtrap) {
          globalMax = std::max(globalMax, hRatioExtrap->GetMaximum());
          globalMin = std::min(globalMin, hRatioExtrap->GetMinimum());
        }
        hRatioMeas->GetYaxis()->SetRangeUser(globalMin * 0.9, globalMax * 1.2);

        hRatioMeas->Draw(yIdx == 0 ? "" : "SAME");
        if (hRatioExtrap)
          hRatioExtrap->Draw("SAME");

        legend->AddEntry(hRatioMeas, std::format("Meas. |#Delta y| < {}", dyTitleStr).c_str(), "p");
        if (hRatioExtrap)
          legend->AddEntry(hRatioExtrap, std::format("Extrap. |#Delta y| < {}", dyTitleStr).c_str(), "p");
      }

      legend->Draw("SAME");
      targetSpectraDir->cd();
      canvasRatio->Write(nullptr, TObject::kOverwrite);

      delete legend;
      delete canvasRatio;
    } else {
      std::cout << "[INFO] CorrelationTask: Skipping yield ratio (need exactly 2 associated particles, found "
                << assocParticles.size() << ")." << std::endl;
    }

    // =========================================================================
    // 2. Write Yield Trends (Measured and Extrapolated), per species
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      std::string cNameMeas = std::format("cTrend_Meas_{}", assocParticles[pIdx].name);
      TCanvas* cTrendMeas = new TCanvas(cNameMeas.c_str(), "Measured Yield Trend", 800, 600);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        h1MultTrends[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

        // Draw on the Summary Canvas
        cTrendMeas->cd();
        AnalysisUtils::SetHistogramStyle(h1MultTrends[pIdx][yIdx], globalCfgs.GetMultTrendColor(yIdx));
        h1MultTrends[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
      }
      targetSpectraDir->cd();
      cTrendMeas->Write(nullptr, TObject::kOverwrite);
      delete cTrendMeas;

      bool hasExtrapTrend = !h1MultTrendsExtrap.empty() && !h1MultTrendsExtrap[pIdx].empty();

      if (hasExtrapTrend) {
        std::string cNameExtrap = std::format("cTrend_Extrap_{}", assocParticles[pIdx].name);
        TCanvas* cTrendExtrap = new TCanvas(cNameExtrap.c_str(), "Extrapolated Yield Trend", 800, 600);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          h1MultTrendsExtrap[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

          cTrendExtrap->cd();
          AnalysisUtils::SetHistogramStyle(h1MultTrendsExtrap[pIdx][yIdx], globalCfgs.GetMultTrendColor(yIdx));
          h1MultTrendsExtrap[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
        }
        targetSpectraDir->cd();
        cTrendExtrap->Write(nullptr, TObject::kOverwrite);
        delete cTrendExtrap;
      }
    }

    // =========================================================================
    // 3. Write Extrapolated vs Measured Ratios, per species (independent block)
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      bool hasExtrapTrend = !h1MultTrendsExtrap.empty() && !h1MultTrendsExtrap[pIdx].empty();
      if (!hasExtrapTrend)
        continue;

      std::string canvasName = std::format("cRatio_ExtrapVsMeas_{}", assocParticles[pIdx].name);
      TCanvas* cRatioExtrapMeas = new TCanvas(canvasName.c_str(), "Extrap / Measured Ratio", 800, 600);
      cRatioExtrapMeas->cd();

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];
        // std::string dyStr = std::format("{:.2f}", dyLimit);
        std::string dyStr = Form("%.2f", dyLimit);
        std::replace(dyStr.begin(), dyStr.end(), '.', '_');

        std::string ratioName = std::format("Ratio_Extrap_Meas_{}_dy{}", assocParticles[pIdx].name, dyStr);
        // std::string title = std::format("Extrap/Measured Contribution {} |#Delta y| < {:.2f}", assocParticles[pIdx].name, dyLimit);
        std::string title = Form("Extrap/Measured Contribution %s |#Delta y| < %.2f", assocParticles[pIdx].name.c_str(), dyLimit);

        // Clone the extrapolated trend to safely divide it
        TH1* hRatioExtrapMeas = static_cast<TH1*>(h1MultTrendsExtrap[pIdx][yIdx]->Clone(ratioName.c_str()));
        hRatioExtrapMeas->SetTitle(title.c_str());
        hRatioExtrapMeas->SetDirectory(0);

        // Perform division: Yield_{Extrap} / Yield_{Measured}
        hRatioExtrapMeas->Divide(h1MultTrendsExtrap[pIdx][yIdx], h1MultTrends[pIdx][yIdx], 1.0, 1.0);

        // Set Y-axis title for clarity
        hRatioExtrapMeas->GetYaxis()->SetTitle("Yield_{Extrap} / Yield_{Meas}");

        AnalysisUtils::SetHistogramStyle(hRatioExtrapMeas, globalCfgs.GetMultTrendColor(yIdx));
        hRatioExtrapMeas->DrawCopy(yIdx == 0 ? "" : "SAME");

        delete hRatioExtrapMeas;
      }

      targetSpectraDir->cd();
      cRatioExtrapMeas->Write(nullptr, TObject::kOverwrite);
      delete cRatioExtrapMeas;
    }

    // =========================================================================
    // 4. Write Canvases
    // =========================================================================
    for (const auto& canvasVec : spectraCanvases) {
      for (auto* canvas : canvasVec) {
        targetSpectraDir->cd();
        canvas->Write(nullptr, TObject::kOverwrite);
        delete canvas;
      }
    }
    spectraCanvases.clear();

    // =========================================================================
    // 5. Close Files
    // =========================================================================
    fileOutputSpectra->Close();
    delete fileOutputSpectra;

    for (auto& file : filesPhiAssocDataOutput) {
      file->Close();
      delete file;
    }

    for (auto& file : filesPhiAssocQAOutput) {
      file->Close();
      delete file;
    }

    // =========================================================================
    // 6. Memory Cleanup for RAM-resident objects
    // =========================================================================

    // Clean up Extrapolation configuration
    if (extrapConfigManager) {
      delete extrapConfigManager;
    }

    if (hEventLoss)
      delete hEventLoss;

    // Clean up Trends
    for (auto& particleTrends : h1MultTrends) {
      for (auto* hTrend : particleTrends) {
        if (hTrend)
          delete hTrend;
      }
    }

    if (applyExtrapolation) {
      for (auto& particleTrends : h1MultTrendsExtrap) {
        for (auto* hTrend : particleTrends) {
          if (hTrend)
            delete hTrend;
        }
      }
    }

    // Clean up Associated Particle accumulation histograms
    for (auto& pIdxArr : h1PhiAssocNoPtPhi) {
      for (auto& mIdxArr : pIdxArr) {
        for (auto& h1 : mIdxArr) {
          if (h1)
            delete h1;
        }
      }
    }

    // Clean up Efficiency histograms
    for (auto& [name, corr] : correctionCollection) {
      for (auto& h1 : corr.h1Corrections) {
        if (h1)
          delete h1;
      }
      for (auto& h1 : corr.h1CorrectionsEffMultInt) {
        if (h1)
          delete h1;
      }
    }

    for (auto& data : loadedDataCollection) {
      if (data.h5DataSignal)
        delete data.h5DataSignal;
      // Sideband/ME-sideband are populated only by tasks that subtract background
      // (e.g. CorrelationTask); they stay nullptr — and this is a safe no-op — for
      // tasks that don't use them (e.g. CorrelationWPDGTask).
      if (data.h5DataSideband)
        delete data.h5DataSideband;
      if (data.h5DataMESignal)
        delete data.h5DataMESignal;
      if (data.h5DataMESideband)
        delete data.h5DataMESideband;
    }

    CleanupExtraMembers(); // hook: e.g. h2TriggerSignal/BkgRatio + purityCollection, or h3PhiData

    std::cout << "[INFO] " << GetName() << ": DONE." << std::endl;
  }

 protected:
  AnalysisSettings globalCfgs;

  std::string basePathData, basePathDataME;

  bool applyME{false}, applyEfficiency{false}, applyExtrapolation{false};
  bool useIntegratedEfficiency{false}, useProjectionCache{false}, use2DMENormalization{false};
  bool useLegacyExtrapolation{false};

  TH1* hEventLoss{nullptr};

  std::vector<AssocParticleConfig> assocParticles;
  std::vector<LoadedAssocData> loadedDataCollection;

  std::map<std::string, LoadedCorrections> correctionCollection;

  CorrelationCalculator::AxisTarget projectionAxis{CorrelationCalculator::AxisTarget::DeltaY_Y};

  std::map<std::string, bool> doExtrapolationPerParticle;

  std::vector<std::vector<std::vector<TH1*>>> h1PhiAssocNoPtPhi;

  std::vector<double> deltaYLimits{1.0, 0.5, 0.1};

  std::vector<std::vector<TH1*>> h1MultTrends;
  std::vector<std::vector<TH1*>> h1MultTrendsExtrap;

  ExtrapConfigManager* extrapConfigManager{nullptr};

  TFile* fileOutputSpectra{nullptr};
  std::vector<TFile*> filesPhiAssocDataOutput, filesPhiAssocQAOutput;
  std::vector<std::vector<TCanvas*>> spectraCanvases;

  std::string yieldRatioLabel{""};

  // -------------------------------------------------------------------------
  // Hooks for derived-class-specific behavior in Terminate()
  // -------------------------------------------------------------------------
  // RAM cleanup for members only some derived tasks have
  // (e.g. h2TriggerSignal/BkgRatio + purityCollection in CorrelationTask,
  //  h3PhiData/h2PhiData in CorrelationWPDGTask).
  virtual void CleanupExtraMembers() {}

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

    applyME = taskConfig["apply_mixed_events"].GetBool();
    applyEfficiency = taskConfig["apply_efficiency"].GetBool();
    applyExtrapolation = taskConfig["apply_extrapolation"].GetBool();
    useIntegratedEfficiency = taskConfig["use_integrated_efficiency"].GetBool();
    useProjectionCache = taskConfig["use_projection_cache"].GetBool();
    use2DMENormalization = taskConfig["use_2d_me_normalization"].GetBool();

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

    if (taskConfig.HasMember("yield_ratio_label") && taskConfig["yield_ratio_label"].IsString()) {
      yieldRatioLabel = taskConfig["yield_ratio_label"].GetString();
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
      extrapConfigManager = new ExtrapConfigManager(extrapFile);
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
    if (!taskConfig.HasMember("input_efficiency_file")) {
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_efficiency_file' missing in JSON!");
    }

    std::string inputEffFile = taskConfig["input_efficiency_file"].GetString();
    TFile* fileEffInput = new TFile(inputEffFile.c_str(), "READ");
    if (!fileEffInput || fileEffInput->IsZombie()) {
      throw std::runtime_error("[FATAL] CorrelationTask: Efficiency requested but Corrections.root not found at: " + inputEffFile);
    }

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
      if (auto hTemp = static_cast<TH1*>(fileEffInput->Get("hEventLoss"))) {
        hEventLoss = static_cast<TH1*>(hTemp->Clone("hEventLoss"));
        hEventLoss->SetDirectory(0);
      } else
        throw std::runtime_error("[FATAL] 'event_loss' requested but 'hEventLoss' not found!");
    }

    // LAMBDA HELPER 1: Load the histogram and throw an error if missing
    auto fetchHist = [&](const std::string& name, bool apply, const std::vector<double>& targetBinning) -> TH1F* {
      if (!apply)
        return nullptr;
      auto h = static_cast<TH1F*>(fileEffInput->Get(name.c_str()));
      if (!h)
        throw std::runtime_error("[FATAL] Missing required histogram: " + name);

      return AnalysisUtils::RebinToTargetBinning(h, targetBinning, "CorrelationTaskBase::LoadCorrections");
    };

    // LAMBDA HELPER 2: Clone and multiply (if both are needed) safely
    auto combineHists = [](TH1F* hEff, TH1F* hLoss, const std::string& name) -> TH1F* {
      if (!hEff && !hLoss)
        return nullptr;
      TH1F* hRes = static_cast<TH1F*>(hEff ? hEff->Clone(name.c_str()) : hLoss->Clone(name.c_str()));
      if (hEff && hLoss)
        hRes->Multiply(hLoss);
      hRes->SetDirectory(0);
      return hRes;
    };

    correctionCollection.clear();

    for (const std::string& name : effParts) {
      LoadedCorrections corr;
      corr.name = name;
      corr.h1Corrections.resize(globalCfgs.nBinMult, nullptr);
      corr.h1CorrectionsEffMultInt.resize(globalCfgs.nBinMult, nullptr);

      std::vector<double> targetBinning;
      if (name == "Phi") {
        targetBinning = globalCfgs.GetPtBinning("Phi");
      } else {
        auto it = std::find_if(assocParticles.begin(), assocParticles.end(), [&](const AssocParticleConfig& p) { return p.name == name; });
        if (it == assocParticles.end())
          throw std::runtime_error("[FATAL] Unknown particle name in 'apply_efficiency_to': " + name);
        targetBinning = it->binningPt;
      }

      // Naming convention for histograms based on particle name
      std::string effHistBase = "h1" + name + "Efficiency";
      std::string sigLossHistBase = "h1" + name + "SigLoss";

      // Fetch integrated histograms ONLY if useIntegratedEfficiency is true
      TH1F* hEffInt = fetchHist(effHistBase + "_multIntegrated", doAccEfficiency && useIntegratedEfficiency, targetBinning);
      // Note: Signal loss is typically not integrated, but we fetch it if requested for consistency
      // TH1F* hLossInt = fetchHist(sigLossHistBase + "_multIntegrated", doSigLoss && useIntegratedEfficiency);

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string iStr = std::to_string(i);

        // Fetch binned histograms ONLY if useIntegratedEfficiency is false
        TH1F* hEffBin = fetchHist(effHistBase + "_multBin" + iStr, doAccEfficiency && !useIntegratedEfficiency, targetBinning);
        // Note: Signal loss is typically not integrated, so we fetch the binned version if signal loss correction is requested,
        // regardless of the useIntegratedEfficiency flag
        TH1F* hLossBin = fetchHist(sigLossHistBase + "_multBin" + iStr, doSigLoss, targetBinning);

        corr.h1Corrections[i] = combineHists(hEffBin, hLossBin, "h1Corrected_" + name + "_multBin" + iStr);
        corr.h1CorrectionsEffMultInt[i] = combineHists(hEffInt, hLossBin, "h1Corrected_" + name + "_multInt" + iStr);
      }

      correctionCollection[name] = std::move(corr);
    }

    fileEffInput->Close();
    delete fileEffInput;

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
        h1PhiAssocNoPtPhi[pIdx][i].resize(assocParticles[pIdx].nBinPt, nullptr);
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

        // std::string dyTitleStr = std::format("{:.2f}", dyLimit);
        std::string dyTitleStr = Form("%.2f", dyLimit);
        std::string dyNameStr = dyTitleStr;
        std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

        std::string hName = std::format("h1MultTrend_{}_dy{}", p.name, dyNameStr);
        std::string hTitle = std::format("Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

        TH1* hTrend = new TH1F(hName.c_str(), hTitle.c_str(), globalCfgs.nBinMult, globalCfgs.binsMult.data());
        hTrend->SetDirectory(0);
        h1MultTrends[pIdx].push_back(hTrend);

        if (doExtrapForThis) {
          std::string hExtrapName = std::format("h1MultTrendExtrap_{}_dy{}", p.name, dyNameStr);
          std::string hExtrapTitle = std::format("Extrapolated Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

          TH1* hTrendExtrap = new TH1F(hExtrapName.c_str(), hExtrapTitle.c_str(), globalCfgs.nBinMult, globalCfgs.binsMult.data());
          hTrendExtrap->SetDirectory(0);
          h1MultTrendsExtrap[pIdx].push_back(hTrendExtrap);
        }

        std::string cSpecName = std::format("cSpectra_{}_dy{}", p.name, dyNameStr);
        std::string cSpecTitle = std::format("Spectra {} |#Delta y| < {}", p.name, dyTitleStr);

        TCanvas* cSpec = new TCanvas(cSpecName.c_str(), cSpecTitle.c_str(), 800, 600);
        cSpec->SetLogy();
        spectraCanvases[pIdx].push_back(cSpec);
      }
    }
  }

  // -------------------------------------------------------------------------
  // Shared spectra construction, normalization, and extrapolation.
  // -------------------------------------------------------------------------
  void GenerateSpectraAndTrends(int multBin, double totalTriggerSignalPerMult)
  {
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    TDirectory* targetSpectraDir = AnalysisUtils::GetOrCreatePath(fileOutputSpectra, {globalCfgs.binningName, dirName});

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& config = assocParticles[pIdx];
      bool doExtrapForThis = applyExtrapolation && doExtrapolationPerParticle.count(config.name) && doExtrapolationPerParticle.at(config.name);

      // Lambda function to extrapolate spectra
      auto extrapolateSpectrum = [&](TH1* hSpec, double& extY, double& extE) {
        double maxVal = hSpec->GetMaximum();

        // 1. Fetch configuration for THIS particle and THIS multBin from JSON
        ExtrapConfig eCfg = extrapConfigManager->GetConfig(config.name, multBin);
        eCfg.mass = config.mass; // Inject physical mass

        // 2. Generate configured TF1 model
        std::unique_ptr<TF1> extrapModel = ExtrapolationModelFactory::CreateModel(eCfg, maxVal);

        {
          // std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}", hSpec->GetName(), extrapFunction);
          std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}_multBin{}", hSpec->GetName(), eCfg.model, multBin);
          TCanvas* cDebug = new TCanvas(debugCanvasName.c_str(), "Debug Guesses", 800, 600);
          // cDebug->SetLogy();

          hSpec->SetMarkerStyle(20);
          hSpec->SetMarkerColor(kBlack);
          hSpec->SetLineColor(kBlack);
          hSpec->Draw();

          extrapModel->SetRange(0.0, 8.0);
          extrapModel->SetLineColor(kRed);
          extrapModel->SetLineWidth(2);
          extrapModel->Draw("SAME");

          targetSpectraDir->cd();
          cDebug->Write(nullptr, TObject::kOverwrite);
          delete cDebug;
        }

        if (useLegacyExtrapolation) {
          TH1* hYieldResult = YieldMean(
            hSpec, extrapModel.get(),
            eCfg.domainRange.first, eCfg.domainRange.second,
            0.01, 0.1, "0QI", "../Logs/logExtrapolation.root",
            eCfg.fitRange.first, eCfg.fitRange.second, config.name);

          if (hYieldResult) {
            extY = hYieldResult->GetBinContent(1); // 1 = kYield
            extE = hYieldResult->GetBinContent(2); // 2 = kYieldStat
            delete hYieldResult;
          } else {
            std::cerr << "[ERROR] YieldMean failed for " << config.name << " bin " << multBin << std::endl;
            extY = 0.0;
            extE = 0.0;
          }
        } else {
          SpectrumExtrapolator extrapolator(hSpec, extrapModel.get());

          extrapolator.SetFitRange(eCfg.fitRange.first, eCfg.fitRange.second);

          auto res = extrapolator.CalculateYieldAndMean();
          extY = res.yield;
          extE = res.yieldStatErr;
        }

        {
          // --- DEBUG ---
          double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");

          double firstDataPt = config.binning[0];
          double fitLowPtIntegral = 0.0;
          if (firstDataPt > 0.0) {
            fitLowPtIntegral = extrapModel->Integral(0.0, firstDataPt);
          }

          std::cout << "\n[DEBUG EXTRAP] " << std::endl;
          std::cout << "  -> Raw Data Integral (ROOT): " << rawIntegral << std::endl;
          std::cout << "  -> Total Extrapolated (extY): " << extY << std::endl;
          std::cout << "  -> Fit Integral [0.0 - " << firstDataPt << "]: " << fitLowPtIntegral << std::endl;
          // ---------------------
        }

        // Create extended binning dynamically
        std::vector<double> extBinning;
        extBinning.push_back(0.0); // Lower limit for extrapolation
        for (double edge : config.binning) {
          extBinning.push_back(edge);
        }

        std::string extName = std::string(hSpec->GetName()) + "_extended";
        TH1D* hSpecExt = new TH1D(extName.c_str(), extName.c_str(), extBinning.size() - 1, extBinning.data());
        hSpecExt->SetDirectory(0);

        // Copy measured contents (shifted by 1 bin to the right)
        for (int b = 1; b <= hSpec->GetNbinsX(); ++b) {
          hSpecExt->SetBinContent(b + 1, hSpec->GetBinContent(b));
          hSpecExt->SetBinError(b + 1, hSpec->GetBinError(b));
        }

        // Leave the first bin empty (visual placeholder for the fit curve)
        hSpecExt->SetBinContent(1, 0.0);
        hSpecExt->SetBinError(1, 0.0);

        AnalysisUtils::SetHistogramStyle(hSpecExt, globalCfgs.GetSpectraColor(multBin));
        hSpecExt->GetListOfFunctions()->Add(extrapModel->Clone());

        targetSpectraDir->cd();
        hSpecExt->Write(nullptr, TObject::kOverwrite);
        delete hSpecExt;
      };

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];

        // std::string dyTitleStr = std::format("{:.2f}", dyLimit);
        std::string dyTitleStr = Form("%.2f", dyLimit);
        std::string dyNameStr = dyTitleStr;
        std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

        std::string spectraName = std::format("h1SpectrumPhi{}_dy{}_multBin{}", config.name, dyNameStr, multBin);

        // Construct the 1D spectrum from the accumulated Pt bins
        TH1* h1Spectrum = AnalysisUtils::constructSpectrum(h1PhiAssocNoPtPhi[pIdx][multBin], config.binning, spectraName, dyLimit);

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

        AnalysisUtils::SetHistogramStyle(h1Spectrum, globalCfgs.GetSpectraColor(multBin));

        // Draw and Write (Only draw the nominal Delta Y limit on the summary canvas)
        spectraCanvases[pIdx][yIdx]->cd();
        h1Spectrum->DrawCopy(multBin == 0 ? "" : "SAME");

        targetSpectraDir->cd();
        h1Spectrum->Write(nullptr, TObject::kOverwrite);

        AnalysisUtils::constructMultTrend(h1MultTrends[pIdx][yIdx], h1Spectrum, multBin, false);

        // Extrapolate and Fill Trend
        if (doExtrapForThis) {
          double extY = 0.0, extE = 0.0;
          extrapolateSpectrum(h1Spectrum, extY, extE);
          AnalysisUtils::constructMultTrend(h1MultTrendsExtrap[pIdx][yIdx], h1Spectrum, multBin, true, extY, extE);
        }

        delete h1Spectrum;
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

    using EffArray = std::vector<TH1F*>;
    const EffArray* phiCorrs{nullptr};
    std::vector<const EffArray*> assocCorrs(assocParticles.size(), nullptr);

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
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

      for (int j = 0; j < nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        double triggerSignal = GetTriggerSignal(i, j);
        double triggerBkgRatio = GetTriggerBkgRatio(i, j);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i] : nullptr;

          TDirectory* targetDir = AnalysisUtils::GetOrCreatePath(filesPhiAssocDataOutput[pIdx], logicalPath, useProjectionCache);

          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string histNameBase = "h1Phi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

            TH1* h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
              data, axesToCut, totalEff, triggerBkgRatio, histNameBase + suffix, targetDir, nullptr, projectionAxis);

            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_pt" + config.name + "Bin" + std::to_string(k);
              h1PhiAssocNoPtPhi[pIdx][i][k] = static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str()));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal);
            }
            delete h1FinalSignal;
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

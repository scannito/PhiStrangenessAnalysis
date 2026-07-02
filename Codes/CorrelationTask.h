#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
// #include "CorrelationCalculator.h"
#include "CorrelationCalculator2.h"
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

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class CorrelationTask : public IAnalysisTask
{
 public:
  enum class RunMode { Legacy = 0,
                       Optimized };

  std::string GetName() const override { return "correlation_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] CorrelationTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings; // Store the global settings for later use

    // 1. Inherit global flags
    applyME = taskConfig["apply_mixed_events"].GetBool();
    applyEfficiency = taskConfig["apply_efficiency"].GetBool();
    applyPurity = taskConfig["apply_purity"].GetBool();
    applyExtrapolation = taskConfig["apply_extrapolation"].GetBool();
    useIntegratedEfficiency = taskConfig["use_integrated_efficiency"].GetBool();
    useProjectionCache = taskConfig["use_projection_cache"].GetBool();

    if (taskConfig.HasMember("use_signal_cache") && taskConfig["use_signal_cache"].IsBool()) {
      useSignalCache = taskConfig["use_signal_cache"].GetBool();
    }
    if (taskConfig.HasMember("do_more_qa") && taskConfig["do_more_qa"].IsBool()) {
      doMoreQA = taskConfig["do_more_qa"].GetBool();
    }

    use2DMENormalization = taskConfig["use_2d_me_normalization"].GetBool();

    if (taskConfig.HasMember("use_legacy_extrapolation") && taskConfig["use_legacy_extrapolation"].IsBool()) {
      useLegacyExtrapolation = taskConfig["use_legacy_extrapolation"].GetBool();
    }

    if (taskConfig.HasMember("projection_axis") && taskConfig["projection_axis"].IsString()) {
      std::string pAxis = taskConfig["projection_axis"].GetString();
      if (pAxis == "DeltaPhi" || pAxis == "delta_phi") {
        projectionAxis = CorrelationCalculator::AxisTarget::DeltaPhi_X;
        std::cout << "[INFO] CorrelationTask: Physics projection axis set to Delta Phi (X)." << std::endl;
      } else if (pAxis == "DeltaY" || pAxis == "delta_y") {
        projectionAxis = CorrelationCalculator::AxisTarget::DeltaY_Y;
        std::cout << "[INFO] CorrelationTask: Physics projection axis set to Delta Y (Y)." << std::endl;
      } else {
        std::cerr << "[WARNING] CorrelationTask: Unknown projection_axis '" << pAxis << "'. Defaulting to DeltaY." << std::endl;
      }
    }

    if (taskConfig.HasMember("run_mode") && taskConfig["run_mode"].IsInt()) {
      runMode = static_cast<RunMode>(taskConfig["run_mode"].GetInt());
    }

    // Prefix handling: search specific key -> Search fallback -> Search legacy
    auto getPrefix = [](const rapidjson::Value& config, const std::string& specificKey, const std::string& fallbackKey) -> std::string {
      // 1. Search for the highly specific prefix (e.g., "purity_prefix")
      if (config.HasMember(specificKey.c_str()) && config[specificKey.c_str()].IsString()) {
        return config[specificKey.c_str()].GetString();
      }
      // 2. Search for the fallback prefix (e.g., "input_prefix")
      if (config.HasMember(fallbackKey.c_str()) && config[fallbackKey.c_str()].IsString()) {
        return config[fallbackKey.c_str()].GetString();
      }
      // 3. Final fallback for legacy JSON configurations
      if (config.HasMember("input_output_prefix") && config["input_output_prefix"].IsString()) {
        return config["input_output_prefix"].GetString();
      }
      return "";
    };

    purityPrefix = getPrefix(taskConfig, "purity_prefix", "input_prefix");
    triggerPrefix = getPrefix(taskConfig, "trigger_prefix", "input_prefix");
    outputPrefix = getPrefix(taskConfig, "output_prefix", "output_prefix"); // Output has no fallback other than legacy

    // 2. Load Trigger information from PhiFitTask
    if (!taskConfig.HasMember("input_dir_proj")) {
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_dir_proj' missing in JSON!");
    }
    std::string basePathProj = taskConfig["input_dir_proj"].GetString();
    std::string phiDataName = basePathProj + triggerPrefix + "PhiDataHistograms.root";

    TFile* filePhiDataInput = new TFile(phiDataName.c_str(), "READ");
    if (!filePhiDataInput || filePhiDataInput->IsZombie()) {
      throw std::runtime_error("[FATAL] CorrelationTask: Missing PhiFitTask output file: " + phiDataName);
    }

    h2TriggerSignal = static_cast<TH2D*>(filePhiDataInput->Get("h2TriggerSignal"));
    h2TriggerBkgRatio = static_cast<TH2D*>(filePhiDataInput->Get("h2TriggerBkgRatio"));
    if (!h2TriggerSignal || !h2TriggerBkgRatio) {
      throw std::runtime_error("[FATAL] CorrelationTask: Missing trigger stats in " + phiDataName);
    }
    h2TriggerSignal->SetDirectory(0);
    h2TriggerBkgRatio->SetDirectory(0);
    filePhiDataInput->Close();
    delete filePhiDataInput;

    // 3. Load Associated Data (THnSparse or Projected TH1 depending on useProjectionCache)
    assocParticles = {
      {"K0S", "phiK0S", globalCfgs.nBinPtK0S, 1, globalCfgs.binspTK0S, AnalysisConstants::k0sMass},
      {"Pi", "phiPi", globalCfgs.nBinPtPi, 2, globalCfgs.binspTPi, AnalysisConstants::piMass}};

    if (!useProjectionCache) {
      std::cout << "[INFO] CorrelationTask: Cache DISABLED. Loading heavy THnSparse data..." << std::endl;

      if (!taskConfig.HasMember("input_data_file") || !taskConfig.HasMember("base_path_data")) {
        throw std::runtime_error("[FATAL ERROR] CorrelationTask: Missing input_data_file or base_path_data in JSON!");
      }
      std::string inputFile = taskConfig["input_data_file"].GetString();
      basePathData = taskConfig["base_path_data"].GetString();

      TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
      if (!fileDataInput || fileDataInput->IsZombie()) {
        throw std::runtime_error("[FATAL] CorrelationTask: Cannot open Data input file: " + inputFile);
      }

      TFile* fileDataMEInput{nullptr};
      if (applyME) {
        if (!taskConfig.HasMember("input_me_file") || !taskConfig.HasMember("base_path_me")) {
          throw std::runtime_error("[FATAL ERROR] CorrelationTask: ME files or paths missing in JSON despite applyME=true!");
        }
        std::string inputMEFile = taskConfig["input_me_file"].GetString();
        basePathDataME = taskConfig["base_path_me"].GetString();

        fileDataMEInput = new TFile(inputMEFile.c_str(), "READ");
        if (!fileDataMEInput || fileDataMEInput->IsZombie()) {
          throw std::runtime_error("[FATAL] CorrelationTask: Cannot open ME input file: " + inputMEFile);
        }
      }

      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
        data.h5DataSignal = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSignal").c_str()));
        data.h5DataSideband = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSideband").c_str()));

        if (applyME) {
          std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;
          data.h5DataMESignal = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESignal").c_str()));
          data.h5DataMESideband = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESideband").c_str()));
        }

        loadedDataCollection.push_back(data);
      }

      fileDataInput->Close();
      delete fileDataInput;
      if (applyME) {
        fileDataMEInput->Close();
        delete fileDataMEInput;
      }
    } else {
      std::cout << "[INFO] CorrelationTask: Cache ENABLED. Skipping THnSparse loading." << std::endl;

      // Create empty LoadedAssocData entries for each particle since we won't be using the THnSparse pointers in cache mode
      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(data);
      }
    }

    // 4. Load Efficiencies, Purities and Extrapolation Setup
    if (applyEfficiency)
      LoadCorrections(taskConfig);
    if (applyPurity)
      LoadPurities(taskConfig);

    if (applyExtrapolation) {
      if (!taskConfig.HasMember("extrapolation_config_file")) {
        throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'extrapolation_config_file' missing in JSON!");
      }
      std::string extrapFile = taskConfig["extrapolation_config_file"].GetString();
      extrapConfigManager = new ExtrapConfigManager(extrapFile);
      std::cout << "[INFO] CorrelationTask: Extrapolation configuration loaded successfully." << std::endl;
    }

    // 5. Initialize output
    std::string projMode = useProjectionCache ? "READ" : "UPDATE";

    std::string basePathFinal = taskConfig["output_dir_final"].GetString();
    std::string phiSpectraName = basePathFinal + outputPrefix + "PhiAssocSpectra.root";
    fileOutputSpectra = new TFile(phiSpectraName.c_str(), "UPDATE");

    for (const auto& p : assocParticles) {
      // Avoid overwriting for better caching
      std::string fName = basePathProj + outputPrefix + "Phi" + p.name + "DataHistograms.root";
      TFile* fProj = new TFile(fName.c_str(), projMode.c_str());
      if (useProjectionCache && (!fProj || fProj->IsZombie())) {
        throw std::runtime_error("[FATAL] Missing cache file: " + fName + ". Please run with 'use_projection_cache': false first!");
      }
      filesPhiAssocDataOutput.push_back(fProj);

      if (doMoreQA) {
        std::string fQAName = basePathProj + outputPrefix + "Phi" + p.name + "QAHistograms.root";
        TFile* fQA = new TFile(fQAName.c_str(), "RECREATE");
        filesPhiAssocQAOutput.push_back(fQA);
      }
    }

    if (taskConfig.HasMember("delta_y_limits")) {
      deltaYLimits.clear();
      for (const auto& v : taskConfig["delta_y_limits"].GetArray())
        deltaYLimits.push_back(v.GetDouble());
    }

    SetupTrendHistograms(); // Helper function to keep Init clean
  }

  void Run() override
  {
    switch (runMode) {
      case RunMode::Legacy:
        RunLegacy();
        break;
      case RunMode::Optimized:
        RunOptimized();
        break;
      default:
        throw std::runtime_error("[FATAL ERROR] CorrelationTask: Unknown run mode!");
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] CorrelationTask: TERMINATING AND CLEANING UP..." << std::endl;

    // =========================================================================
    // 1. Write the Ratios and Trends
    // =========================================================================
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    TDirectory* targetSpectraDir = fileOutputSpectra->GetDirectory(dirName.c_str());
    if (!targetSpectraDir) {
      targetSpectraDir = fileOutputSpectra->mkdir(dirName.c_str());
    }
    targetSpectraDir->cd();

    TCanvas* canvasRatioMultTrend = new TCanvas("canvasRatioMultTrend", "Ratio Mult Trend", 800, 600);
    canvasRatioMultTrend->cd();

    TLegend* legend = new TLegend(0.7, 0.7, 0.9, 0.9);
    legend->SetNColumns(2);
    legend->SetLineWidth(0);

    for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
      double dyLimit = deltaYLimits[yIdx];

      // std::string dyTitleStr = std::format("{:.2f}", dyLimit);
      std::string dyTitleStr = Form("%.2f", dyLimit);
      std::string dyNameStr = dyTitleStr;
      std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

      // Assuming 0 is K0S and 1 is Pi based on assocParticles order
      std::string ratioMeasName = std::format("Ratio_K0S_Pi_Meas_dy{}", dyNameStr);
      TH1* hRatioMeas = static_cast<TH1*>(h1MultTrends[0][yIdx]->Clone(ratioMeasName.c_str()));
      hRatioMeas->SetTitle("Ratio;Multiplicity Percentile (%);2K_{S}^{0}/(#pi^{+}+#pi^{-})");
      hRatioMeas->SetDirectory(0);

      hRatioMeas->Divide(h1MultTrends[0][yIdx], h1MultTrends[1][yIdx], 2.0, 1.0);

      AnalysisUtils::SetHistogramStyle(hRatioMeas, globalCfgs.GetMultTrendColor(yIdx));
      hRatioMeas->SetMarkerStyle(24);
      // hRatioMeas->DrawCopy(yIdx == 0 ? "" : "SAME");
      // delete hRatio;

      TH1* hRatioExtrap{nullptr};
      if (applyExtrapolation) {
        std::string ratioExtrapName = std::format("Ratio_K0S_Pi_Extrap_dy{}", dyNameStr);
        hRatioExtrap = static_cast<TH1*>(h1MultTrendsExtrap[0][yIdx]->Clone(ratioExtrapName.c_str()));
        hRatioExtrap->SetDirectory(0);

        hRatioExtrap->Divide(h1MultTrendsExtrap[0][yIdx], h1MultTrendsExtrap[1][yIdx], 2.0, 1.0);

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
    canvasRatioMultTrend->Write(nullptr, TObject::kOverwrite);

    // Write Yield Trends (Measured and Extrapolated)
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

      if (applyExtrapolation) {
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

    // Write Extrapolated vs Measured Ratios
    if (applyExtrapolation) {
      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
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
    }

    // =========================================================================
    // 2. Write Canvases
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
    // 3. Close Files
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
    // 4. Memory Cleanup for RAM-resident objects
    // =========================================================================

    // Clean up Extrapolation configuration
    if (extrapConfigManager) {
      delete extrapConfigManager;
      extrapConfigManager = nullptr;
    }

    // Clean up Trigger histograms passed from the Fit Task
    if (h2TriggerSignal)
      delete h2TriggerSignal;
    if (h2TriggerBkgRatio)
      delete h2TriggerBkgRatio;

    delete canvasRatioMultTrend;

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
    for (auto& corr : correctionCollection) {
      for (auto& h1 : corr.h1Corrections) {
        if (h1)
          delete h1;
      }
    }

    // Clean up Purity histograms
    for (auto& pur : purityCollection) {
      for (auto& h1 : pur.h1Purity) {
        if (h1)
          delete h1;
      }
    }

    // Clean up THnSparse data
    for (auto& data : loadedDataCollection) {
      if (data.h5DataSignal)
        delete data.h5DataSignal;
      if (data.h5DataSideband)
        delete data.h5DataSideband;
      if (data.h5DataMESignal)
        delete data.h5DataMESignal;
      if (data.h5DataMESideband)
        delete data.h5DataMESideband;
    }

    std::cout << "[INFO] CorrelationTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  RunMode runMode{RunMode::Legacy};

  std::string basePathData, basePathDataME;

  bool applyME{false}, applyEfficiency{false}, applyPurity{false}, applyExtrapolation{false};
  bool useIntegratedEfficiency{false}, useProjectionCache{false}, use2DMENormalization{false};
  bool useSignalCache{false}, doMoreQA{false}, useLegacyExtrapolation{false};

  std::string purityPrefix{""};
  std::string triggerPrefix{""};
  std::string outputPrefix{""};

  TH2D* h2TriggerSignal{nullptr};
  TH2D* h2TriggerBkgRatio{nullptr};

  TH1* hEventLoss{nullptr};

  std::vector<AssocParticleConfig> assocParticles;
  std::vector<LoadedAssocData> loadedDataCollection;
  std::vector<LoadedCorrections> correctionCollection;
  std::vector<LoadedPurity> purityCollection;

  CorrelationCalculator::AxisTarget projectionAxis{CorrelationCalculator::AxisTarget::DeltaY_Y};

  // All accumulators and file vectors from the original task
  std::vector<std::vector<std::vector<TH1*>>> h1PhiAssocNoPtPhi;
  std::vector<double> deltaYLimits{1.0, 0.5, 0.1};
  std::vector<std::vector<TH1*>> h1MultTrends;
  std::vector<std::vector<TH1*>> h1MultTrendsExtrap;

  // std::string extrapFunction{"LevyTsallis"};
  // std::pair<double, double> extrapFitRange{0.4, 6.0};
  ExtrapConfigManager* extrapConfigManager{nullptr};

  TFile* fileOutputSpectra{nullptr};
  std::vector<TFile*> filesPhiAssocDataOutput, filesPhiAssocQAOutput;
  std::vector<std::vector<TCanvas*>> spectraCanvases;

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
      std::cerr << "[WARNING] 'active_corrections' missing or not an array. Applying NO corrections!" << std::endl;
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
    auto fetchHist = [&](const std::string& name, bool apply) -> TH1F* {
      if (!apply)
        return nullptr;
      auto h = static_cast<TH1F*>(fileEffInput->Get(name.c_str()));
      if (!h)
        throw std::runtime_error("[FATAL] Missing required histogram: " + name);
      return h;
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

    // Particle definitions for corrections mapping
    std::vector<ParticleConfig<2>> allParticles = {
      {"Phi", {"h1PhiEfficiency", "h1PhiSigLoss"}},
      {"K0S", {"h1K0SEfficiency", "h1K0SSigLoss"}},
      {"Pi", {"h1PiEfficiency", "h1PiSigLoss"}}};

    correctionCollection.resize(allParticles.size());

    for (size_t idx = 0; idx < allParticles.size(); ++idx) {
      const auto& p = allParticles[idx];
      correctionCollection[idx].name = p.name;
      correctionCollection[idx].h1Corrections.resize(globalCfgs.nBinMult, nullptr);
      correctionCollection[idx].h1CorrectionsEffMultInt.resize(globalCfgs.nBinMult, nullptr);

      // We use effParts to check if we should apply efficiency to this particle
      if (std::find(effParts.begin(), effParts.end(), p.name) == effParts.end())
        continue;

      // Fetch integrated histograms ONLY if useIntegratedEfficiency is true
      TH1F* hEffInt = fetchHist(p.titles[0] + "_multIntegrated", doAccEfficiency && useIntegratedEfficiency);
      // Note: Signal loss is typically not integrated, but we fetch it if requested for consistency
      // TH1F* hLossInt = fetchHist(p.titles[1] + "_multIntegrated", doSigLoss && useIntegratedEfficiency);

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string iStr = std::to_string(i);

        // Fetch binned histograms ONLY if useIntegratedEfficiency is false
        TH1F* hEffBin = fetchHist(p.titles[0] + "_multBin" + iStr, doAccEfficiency && !useIntegratedEfficiency);
        // Note: Signal loss is typically not integrated, so we fetch the binned version if signal loss correction is requested,
        // regardless of the useIntegratedEfficiency flag
        TH1F* hLossBin = fetchHist(p.titles[1] + "_multBin" + iStr, doSigLoss);

        correctionCollection[idx].h1Corrections[i] = combineHists(hEffBin, hLossBin, "h1Corrected_" + p.name + "_multBin" + iStr);
        correctionCollection[idx].h1CorrectionsEffMultInt[i] = combineHists(hEffInt, hLossBin, "h1Corrected_" + p.name + "_multInt_clone" + iStr);
      }
    }

    /*for (const auto& p : allParticles) {
      LoadedCorrections corrections;
      corrections.name = p.name;

      std::string effNameIntegrated = p.titles[0] + "_multIntegrated";
      std::string lossNameIntegrated = p.titles[1] + "_multIntegrated";

      TH1F* h1EffIntegrated = static_cast<TH1F*>(fileEffInput->Get(effNameIntegrated.c_str()));
      TH1F* h1LossIntegrated = static_cast<TH1F*>(fileEffInput->Get(lossNameIntegrated.c_str()));

      if (!h1EffIntegrated || !h1LossIntegrated) {
        throw std::runtime_error("[FATAL] CorrelationTask: Missing integrated correction histograms for " + p.name);
      }

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string effName = p.titles[0] + "_multBin" + std::to_string(i);
        std::string lossName = p.titles[1] + "_multBin" + std::to_string(i);

        TH1F* h1Eff = static_cast<TH1F*>(fileEffInput->Get(effName.c_str()));
        TH1F* h1Loss = static_cast<TH1F*>(fileEffInput->Get(lossName.c_str()));

        if (!h1Eff || !h1Loss) {
          throw std::runtime_error("[FATAL] CorrelationTask: Missing correction histograms for " + p.name + " in mult bin " + std::to_string(i));
        }

        corrections.h1Corrections[i] = static_cast<TH1F*>(h1Eff->Clone());
        // corrections.h1Corrections[i]->Multiply(h1Eff, h1Loss);
        corrections.h1Corrections[i]->SetDirectory(0);

        corrections.h1CorrectionsEffMultInt[i] = static_cast<TH1F*>(h1EffIntegrated->Clone());
        // corrections.h1CorrectionsEffMultInt[i]->Multiply(h1EffIntegrated, h1Loss);
        corrections.h1CorrectionsEffMultInt[i]->SetDirectory(0);
      }

      correctionCollection.push_back(corrections);
    }*/

    fileEffInput->Close();
    delete fileEffInput;

    std::cout << "[INFO] CorrelationTask: Efficiencies loaded successfully." << std::endl;
  }

  void LoadPurities(const rapidjson::Value& taskConfig)
  {
    if (!taskConfig.HasMember("input_dir_purity")) {
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_dir_purity' missing in JSON!");
    }

    std::string purityDir = taskConfig["input_dir_purity"].GetString();

    std::vector<std::pair<std::string, std::string>> purityConfig = {
      {"k0s", purityDir + purityPrefix + "K0SPurity.root"},
      {"pi_tpc", purityDir + purityPrefix + "PiPurity.root"}};

    for (const auto& p : purityConfig) {
      LoadedPurity purity;
      purity.name = p.first;
      purity.h1Purity.resize(globalCfgs.nBinMult, nullptr);

      TFile* filePurity = new TFile(p.second.c_str(), "READ");
      if (!filePurity || filePurity->IsZombie()) {
        throw std::runtime_error("[FATAL] CorrelationTask: Cannot open purity file: " + p.second);
      }

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string hName = "h1" + p.first + "Purity_multBin" + std::to_string(i);
        TH1* h1Pur = static_cast<TH1*>(filePurity->Get(hName.c_str()));
        if (!h1Pur) {
          throw std::runtime_error("[FATAL] CorrelationTask: Missing purity histogram: " + hName + " in " + p.second);
        }

        purity.h1Purity[i] = static_cast<TH1*>(h1Pur->Clone());
        purity.h1Purity[i]->SetDirectory(0);
      }
      purityCollection.push_back(purity);

      filePurity->Close();
      delete filePurity;
    }
    std::cout << "[INFO] CorrelationTask: Purities loaded successfully." << std::endl;
  }

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

        if (applyExtrapolation) {
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

  void GenerateSpectraAndTrends(int multBin, double totalTriggerSignalPerMult)
  {
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    TDirectory* targetSpectraDir = fileOutputSpectra->GetDirectory(dirName.c_str());
    if (!targetSpectraDir) {
      targetSpectraDir = fileOutputSpectra->mkdir(dirName.c_str());
    }

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& config = assocParticles[pIdx];
      const auto& purity = purityCollection[pIdx];

      // Lambda function to extrapolate spectra
      auto extrapolateSpectrum = [&](TH1* hSpec, double& extY, double& extE) {
        double maxVal = hSpec->GetMaximum();
        /*double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");

        std::unique_ptr<TF1> extrapModel;

        // Dynamically select the model based on JSON
        if (extrapFunction == "LevyTsallis") {
          extrapModel = ExtrapolationModelFactory::CreateLevyTsallis(config.mass, 5.0, 0.4, maxVal * 1.5);
          // extrapModel = ExtrapolationModelFactory::CreateLevyTsallis(config.mass, 5.0, 0.7, rawIntegral);
        } else if (extrapFunction == "BlastWave") {
          extrapModel = ExtrapolationModelFactory::CreateBlastWave(config.mass, 0.7, 0.4, 25.0, rawIntegral);
        } else if (extrapFunction == "BoseEinstein") {
          extrapModel = ExtrapolationModelFactory::CreateBoseEinstein(config.mass, 0.7, rawIntegral);
        } else if (extrapFunction == "MtExponential") {
          extrapModel = ExtrapolationModelFactory::CreateMtExponential(config.mass, 0.7, rawIntegral);
        } else if (extrapFunction == "PtExponential") {
          extrapModel = ExtrapolationModelFactory::CreatePtExponential(0.7, rawIntegral);
        } else {
          throw std::runtime_error("[FATAL] CorrelationTask: Unknown extrap_function: " + extrapFunction);
        }*/

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

        // Apply Purity if enabled
        if (applyPurity) {
          h1Spectrum->Multiply(purityCollection[pIdx].h1Purity[multBin]);
        }

        AnalysisUtils::SetHistogramStyle(h1Spectrum, globalCfgs.GetSpectraColor(multBin));

        // Draw and Write (Only draw the nominal Delta Y limit on the summary canvas)
        spectraCanvases[pIdx][yIdx]->cd();
        h1Spectrum->DrawCopy(multBin == 0 ? "" : "SAME");

        targetSpectraDir->cd();
        h1Spectrum->Write(nullptr, TObject::kOverwrite);

        AnalysisUtils::constructMultTrend(h1MultTrends[pIdx][yIdx], h1Spectrum, multBin, false);

        // Extrapolate and Fill Trend
        if (applyExtrapolation /* && multBin == 0 && yIdx == 0*/) {
          double extY = 0.0, extE = 0.0;
          extrapolateSpectrum(h1Spectrum, extY, extE);
          AnalysisUtils::constructMultTrend(h1MultTrendsExtrap[pIdx][yIdx], h1Spectrum, multBin, true, extY, extE);
        }

        delete h1Spectrum;
      }
    }
  }

  void RunLegacy()
  {
    std::cout << "[INFO] CorrelationTask: RUNNING CORRELATIONS..." << std::endl;
    CorrelationCalculator corrCalculator(applyME, useProjectionCache, false, doMoreQA);

    // auto startRun = std::chrono::high_resolution_clock::now();
    // double totalExtractTime = 0.0;

    // Setup Efficiency Pointers Once (Loop Hoisting)
    // using EffArray = std::array<TH1F*, AnalysisConstants::nBinMult>;
    using EffArray = std::vector<TH1F*>;
    const EffArray* phiCorrs{nullptr};
    std::vector<const EffArray*> assocCorrs(assocParticles.size(), nullptr);

    if (applyEfficiency && !correctionCollection.empty()) {
      phiCorrs = useIntegratedEfficiency ? &correctionCollection[0].h1CorrectionsEffMultInt : &correctionCollection[0].h1Corrections;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        size_t effIdx = assocParticles[pIdx].effIndex;
        if (correctionCollection.size() > effIdx) {
          assocCorrs[pIdx] = useIntegratedEfficiency ? &correctionCollection[effIdx].h1CorrectionsEffMultInt : &correctionCollection[effIdx].h1Corrections;
        }
      }
    }

    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      AnalysisUtils::AxisToCut axisToCutMult{0, i + 1, i + 1};
      double totalTriggerSignalPerMult = 0.0;

      // Determine which efficiency to use for this multiplicity bin
      // auto phiCorrs = useIntegratedEfficiency ? correctionCollection[0].h1CorrectionsEffMultInt : correctionCollection[0].h1Corrections;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

      for (int j = 0; j < globalCfgs.nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        // Read the values generated by PhiFitTask
        double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
        double triggerBkgRatio = h2TriggerBkgRatio->GetBinContent(i + 1, j + 1);

        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];

          // auto assocCorrs = useIntegratedEfficiency ? correctionCollection[config.effIndex].h1CorrectionsEffMultInt : correctionCollection[config.effIndex].h1Corrections;
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i] : nullptr;

          std::string dirName{"Extract1D"};

          TDirectory* targetDir{nullptr};
          if (filesPhiAssocDataOutput[pIdx]) {
            targetDir = filesPhiAssocDataOutput[pIdx]->GetDirectory(dirName.c_str());

            if (!targetDir && !useProjectionCache) {
              targetDir = filesPhiAssocDataOutput[pIdx]->mkdir(dirName.c_str());
            }
          }

          TDirectory* currentQADir = doMoreQA ? filesPhiAssocQAOutput[pIdx] : nullptr;

          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};

            // Prepare inputs for the calculator
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            // double assocEff = (applyEfficiency && assocCorrs[i]) ? assocCorrs[i]->GetBinContent(k + 1) : 1.0;
            // double totalEff = phiEff * assocEff;
            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string histNameBase = "h1Phi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

            // auto startExtract = std::chrono::high_resolution_clock::now();

            // Call the calculator: project, scale, apply ME, subtract, and save intermediate files
            TH1* h1FinalSignal = corrCalculator.ExtractCorrectedSignal(data, axesToCut, totalEff, triggerBkgRatio,
                                                                       histNameBase + suffix, targetDir, currentQADir);

            // auto endExtract = std::chrono::high_resolution_clock::now();
            // totalExtractTime += std::chrono::duration<double>(endExtract - startExtract).count();

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
      } // End Pt Phi

      // Spectra Creation and Extrapolations (Identical to your original code)
      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    }

    /*auto endRun = std::chrono::high_resolution_clock::now();
    double totalRunTime = std::chrono::duration<double>(endRun - startRun).count();

    std::cout << "\n================= TIMING SUMMARY =================" << std::endl;
    std::cout << "Total Run() time          : " << totalRunTime << " s" << std::endl;
    std::cout << "ExtractCorrectedSignal    : " << totalExtractTime << " s ("
              << (totalExtractTime / totalRunTime) * 100.0 << " %)" << std::endl;
    std::cout << "Other operations (Spectra): " << (totalRunTime - totalExtractTime) << " s" << std::endl;
    std::cout << "====================================================" << std::endl;*/
  }

  void RunOptimized()
  {
    std::cout << "[INFO] CorrelationTask: RUNNING OPTIMIZED CORRELATIONS AND QA..." << std::endl;

    // =========================================================================
    // 0. EFFICIENCY POINTERS SETUP
    // Hoisted to the top so both the Cache L2 branch and the calculation branch
    // can access them to properly normalize the trigger signals.
    // =========================================================================
    // using EffArray = std::array<TH1F*, AnalysisConstants::nBinMult>;
    using EffArray = std::vector<TH1F*>;
    const EffArray* phiCorrs{nullptr};
    std::vector<const EffArray*> assocCorrs(assocParticles.size(), nullptr);

    if (applyEfficiency && !correctionCollection.empty()) {
      phiCorrs = useIntegratedEfficiency ? &correctionCollection[0].h1CorrectionsEffMultInt : &correctionCollection[0].h1Corrections;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        size_t effIdx = assocParticles[pIdx].effIndex;
        if (correctionCollection.size() > effIdx) {
          assocCorrs[pIdx] = useIntegratedEfficiency ? &correctionCollection[effIdx].h1CorrectionsEffMultInt : &correctionCollection[effIdx].h1Corrections;
        }
      }
    }

    // =========================================================================
    // MODE A: SUPER CACHE MANAGEMENT (LEVEL 2)
    // If active, completely bypasses the CorrelationCalculator and heavy loops.
    // =========================================================================
    if (useSignalCache) {
      std::cout << "[INFO] CorrelationTask: Super Cache (Signal L2) is ON. Skipping Calculator." << std::endl;

      std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        // Recalculate total triggers to accurately normalize the final spectra
        // taking into account the efficiency!
        double totalTriggerSignalPerMult = 0.0;
        TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

        for (int j = 0; j < globalCfgs.nBinPtPhi; j++) {
          double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
          double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
          totalTriggerSignalPerMult += triggerSignal / phiEff;
        }

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];

          TDirectory* sourceDir = filesPhiAssocDataOutput[pIdx]->GetDirectory(dirName.c_str());
          if (!sourceDir) {
            throw std::runtime_error("[FATAL] Cache L2 missing: Directory " + dirName + " not found!");
          }

          for (int k = 0; k < config.nBinPt; k++) {
            // Reconstruct the exact name used during the initial save
            std::string cacheName = "h1Phi" + config.name + "Final_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);

            // Direct read from the previously processed file (opened in READ mode)
            h1PhiAssocNoPtPhi[pIdx][i][k] = static_cast<TH1*>(sourceDir->Get(cacheName.c_str()));

            if (!h1PhiAssocNoPtPhi[pIdx][i][k]) {
              throw std::runtime_error("[FATAL] Cache L2 missing for: " + cacheName + ". Run with use_signal_cache: false first!");
            }
            // Disconnect from file memory to prevent double-free issues
            h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
          }
        }
        // Proceed directly to spectra generation and extrapolation fitting
        GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
      }
      return; // EXIT IMMEDIATELY. Task is complete for L2 Cache mode.
    }

    // =========================================================================
    // MODE B: STANDARD CALCULATION (If L2 Cache is disabled)
    // The CorrelationCalculator is invoked here (managing L1 Cache internally)
    // =========================================================================
    /*// Global QA Accumulators Initialization
    std::vector<TH1*> h1QA_FullyIntegrated(assocParticles.size(), nullptr);
    std::vector<std::vector<TH1*>> h1QA_ByMult(assocParticles.size(), std::vector<TH1*>(globalCfgs.nBinMult, nullptr));
    std::vector<std::vector<TH1*>> h1QA_ByPtPhi(assocParticles.size(), std::vector<TH1*>(globalCfgs.nBinPtPhi, nullptr));
    std::vector<std::vector<std::vector<TH1*>>> h1QA_ByMult_ByPtPhi(assocParticles.size(), std::vector<std::vector<TH1*>>(
                                                                                             globalCfgs.nBinMult, std::vector<TH1*>(globalCfgs.nBinPtPhi, nullptr)));*/
    // Pass doMoreQA to the constructor to enable/disable 2D QA dumps
    CorrelationCalculator corrCalculator(applyME, useProjectionCache, use2DMENormalization, doMoreQA);

    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";

    // --- MULTIPLICITY LOOP (i) ---
    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      AnalysisUtils::AxisToCut axisToCutMult{0, i + 1, i + 1};
      double totalTriggerSignalPerMult = 0.0;

      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

      // --- TRIGGER PT LOOP (j) ---
      for (int j = 0; j < globalCfgs.nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
        double triggerBkgRatio = h2TriggerBkgRatio->GetBinContent(i + 1, j + 1);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;

        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i] : nullptr;

          TDirectory* targetDir{nullptr};
          if (filesPhiAssocDataOutput[pIdx]) {
            targetDir = filesPhiAssocDataOutput[pIdx]->GetDirectory(dirName.c_str());

            if (!targetDir && !useProjectionCache) {
              targetDir = filesPhiAssocDataOutput[pIdx]->mkdir(dirName.c_str());
            }
          }

          // Retrieve the appropriate QA directory for this particle
          TDirectory* currentQADir = doMoreQA ? filesPhiAssocQAOutput[pIdx] : nullptr;

          // --- ASSOCIATED PT LOOP (k) ---
          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string baseNameStd = "hPhi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptBin" + std::to_string(k);

            // -----------------------------------------------------------------
            // A) STANDARD EXTRACTION (For Spectra & Yields)
            // -----------------------------------------------------------------
            TH1* h1FinalSignal = corrCalculator.ExtractCorrectedSignal(data, axesToCut, totalEff, triggerBkgRatio,
                                                                       baseNameStd + suffix, targetDir, currentQADir, projectionAxis);

            // Accumulate into the standard pT arrays (Summing over PtPhi dimension)
            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);
              h1PhiAssocNoPtPhi[pIdx][i][k] = static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str()));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal);
            }
            delete h1FinalSignal;

            /*// -----------------------------------------------------------------
            // B) QA EXTRACTION (Delta Phi 1D)
            // -----------------------------------------------------------------
            std::string tmpNameQA = "tmpQA_" + config.name + suffix;

            TH1* h1BinQADPhi = corrCalculator.ExtractDeltaPhi(
              data, axesToCut, totalEff, triggerBkgRatio,
              tmpNameQA, nullptr, currentQADir, true, -1.0, 1.0);

            if (h1BinQADPhi) {
              if (!h1QA_FullyIntegrated[pIdx]) {
                std::string name = "h1Phi" + config.name + "_QA_FullyIntegrated";
                h1QA_FullyIntegrated[pIdx] = static_cast<TH1*>(h1BinQADPhi->Clone(name.c_str()));
                h1QA_FullyIntegrated[pIdx]->SetDirectory(0);
              } else {
                h1QA_FullyIntegrated[pIdx]->Add(h1BinQADPhi);
              }

              if (!h1QA_ByMult[pIdx][i]) {
                std::string name = "h1Phi" + config.name + "_QA_MultBin" + std::to_string(i);
                h1QA_ByMult[pIdx][i] = static_cast<TH1*>(h1BinQADPhi->Clone(name.c_str()));
                h1QA_ByMult[pIdx][i]->SetDirectory(0);
              } else {
                h1QA_ByMult[pIdx][i]->Add(h1BinQADPhi);
              }

              if (!h1QA_ByPtPhi[pIdx][j]) {
                std::string name = "h1Phi" + config.name + "_QA_PtPhiBin" + std::to_string(j);
                h1QA_ByPtPhi[pIdx][j] = static_cast<TH1*>(h1BinQADPhi->Clone(name.c_str()));
                h1QA_ByPtPhi[pIdx][j]->SetDirectory(0);
              } else {
                h1QA_ByPtPhi[pIdx][j]->Add(h1BinQADPhi);
              }

              if (!h1QA_ByMult_ByPtPhi[pIdx][i][j]) {
                std::string name = "h1Phi" + config.name + "_QA_MultBin" + std::to_string(i) + "_PtPhiBin" + std::to_string(j);
                h1QA_ByMult_ByPtPhi[pIdx][i][j] = static_cast<TH1*>(h1BinQADPhi->Clone(name.c_str()));
                h1QA_ByMult_ByPtPhi[pIdx][i][j]->SetDirectory(0);
              } else {
                h1QA_ByMult_ByPtPhi[pIdx][i][j]->Add(h1BinQADPhi);
              }

              delete h1BinQADPhi;
            }*/
          } // End k
        } // End pIdx
      } // End j

      // =======================================================================
      // BATCH WRITE: SAVING AGGREGATED SIGNALS FOR L2 CACHE
      // Written only once per multiplicity bin after trigger aggregation
      // =======================================================================
      std::cout << "[INFO] Multiplicity bin " << i << " completed. Committing results to Level 2 Cache..." << std::endl;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        TDirectory* targetDir = filesPhiAssocDataOutput[pIdx]->GetDirectory(dirName.c_str());
        if (targetDir)
          targetDir->cd();
        for (int k = 0; k < assocParticles[pIdx].nBinPt; k++) {
          if (h1PhiAssocNoPtPhi[pIdx][i][k]) {
            std::string saveName = "h1Phi" + assocParticles[pIdx].name + "Final_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);
            h1PhiAssocNoPtPhi[pIdx][i][k]->SetName(saveName.c_str());
            h1PhiAssocNoPtPhi[pIdx][i][k]->Write(nullptr, TObject::kOverwrite);
          }
        }
      }

      // Generate spectra for this multiplicity bin using the accumulated standard data
      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    } // End i

    /*// =========================================================================
    // FINAL SAVING OF TOPOLOGICAL QA MATRICES
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      targetDir->cd();

      if (h1QA_FullyIntegrated[pIdx]) {
        h1QA_FullyIntegrated[pIdx]->Write(nullptr, TObject::kOverwrite);
        delete h1QA_FullyIntegrated[pIdx];
      }

      for (int i = 0; i < globalCfgs.nBinMult; ++i) {
        if (h1QA_ByMult[pIdx][i]) {
          h1QA_ByMult[pIdx][i]->Write(nullptr, TObject::kOverwrite);
          delete h1QA_ByMult[pIdx][i];
        }
      }
      for (int j = 0; j < globalCfgs.nBinPtPhi; ++j) {
        if (h1QA_ByPtPhi[pIdx][j]) {
          h1QA_ByPtPhi[pIdx][j]->Write(nullptr, TObject::kOverwrite);
          delete h1QA_ByPtPhi[pIdx][j];
        }
      }
      for (int i = 0; i < globalCfgs.nBinMult; ++i) {
        for (int j = 0; j < globalCfgs.nBinPtPhi; ++j) {
          if (h1QA_ByMult_ByPtPhi[pIdx][i][j]) {
            h1QA_ByMult_ByPtPhi[pIdx][i][j]->Write(nullptr, TObject::kOverwrite);
            delete h1QA_ByMult_ByPtPhi[pIdx][i][j];
          }
        }
      }
    }*/
  }
};

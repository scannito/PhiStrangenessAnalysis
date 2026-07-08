#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "CorrelationCalculator.h"
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
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class CorrelationWPDGTask : public IAnalysisTask
{
 public:
  std::string GetName() const override { return "correlation_wpdg_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] CorrelationWPDGTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings; // Store the global settings for later use

    // 1. Inherit global flags
    applyME = taskConfig["apply_mixed_events"].GetBool();
    applyEfficiency = taskConfig["apply_efficiency"].GetBool();
    applyExtrapolation = taskConfig["apply_extrapolation"].GetBool();
    useIntegratedEfficiency = taskConfig["use_integrated_efficiency"].GetBool();
    useProjectionCache = taskConfig["use_projection_cache"].GetBool();
    isPureGen = taskConfig["is_pure_gen"].GetBool();

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

    if (taskConfig.HasMember("yield_ratio_label") && taskConfig["yield_ratio_label"].IsString()) {
      yieldRatioLabel = taskConfig["yield_ratio_label"].GetString();
    }

    // 2. Load Data
    if (!taskConfig.HasMember("input_data_file") || !taskConfig.HasMember("base_path_data")) {
      throw std::runtime_error("[FATAL ERROR] CorrelationWPDGTask: Missing input_data_file or base_path_data in JSON!");
    }
    std::string inputFile = taskConfig["input_data_file"].GetString();
    basePathData = taskConfig["base_path_data"].GetString();

    TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
    if (!fileDataInput || fileDataInput->IsZombie()) {
      throw std::runtime_error("[FATAL] CorrelationWPDGTask: Cannot open Data input file: " + inputFile);
    }

    // Temporary
    if (!isPureGen) {
      h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiData").c_str()));
      if (!h3PhiData)
        throw std::runtime_error("[FATAL] CorrelationWPDGTask: Missing h3PhiData!");
      h3PhiData->SetDirectory(0);
    } else {
      // TFile* tempFile = new TFile("../DataFile/pp/DeltaY/MC/AnalysisResultsMC_136.root", "READ");
      // std::string genHistName = basePathData + "phi/h3PhiMCGen";
      // h3PhiData = static_cast<TH3F*>(tempFile->Get(genHistName.c_str()));
      h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiMCGen").c_str()));
      if (!h3PhiData)
        throw std::runtime_error("[FATAL] CorrelationWPDGTask: Missing h3PhiMCGen in temp file for pure gen test!");
      h3PhiData->SetDirectory(0);
      // tempFile->Close();
      // delete tempFile;
    }

    /*assocParticles = {
      {"K0S", "phiK0S", globalCfgs.nBinPtK0S, 1, globalCfgs.binspTK0S, AnalysisConstants::k0sMass},
      {"Pi", "phiPi", globalCfgs.nBinPtPi, 2, globalCfgs.binspTPi, AnalysisConstants::piMass}};*/

    assocParticles.clear();
    for (const auto& sp : taskConfig["associated_particles"].GetArray()) {
      std::string name = sp["name"].GetString();
      std::string dirName = sp["dir_name"].GetString();

      const auto& binning = globalCfgs.GetPtBinning(name);

      assocParticles.emplace_back(name, dirName, static_cast<int>(binning.size()) - 1, binning, AnalysisConstants::GetMass(name));
    }

    if (!useProjectionCache) {
      std::cout << "[INFO] CorrelationWPDGTask: Cache DISABLED. Loading heavy THnSparse data..." << std::endl;

      TFile* fileDataMEInput{nullptr};
      if (applyME) {
        if (!taskConfig.HasMember("input_me_file") || !taskConfig.HasMember("base_path_me")) {
          throw std::runtime_error("[FATAL ERROR] CorrelationWPDGTask: ME files or paths missing in JSON despite applyME=true!");
        }
        std::string inputMEFile = taskConfig["input_me_file"].GetString();
        basePathDataME = taskConfig["base_path_me"].GetString();

        fileDataMEInput = new TFile(inputMEFile.c_str(), "READ");
        if (!fileDataMEInput || fileDataMEInput->IsZombie()) {
          throw std::runtime_error("[FATAL] CorrelationWPDGTask: Cannot open ME input file: " + inputMEFile);
        }
      }

      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
        data.h5DataSignal = static_cast<THnSparseF*>(fileDataInput->Get((baseData + (isPureGen ? "ClosureMCGen" : "DataSignal")).c_str()));

        if (applyME) {
          std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;
          data.h5DataMESignal = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + (isPureGen ? "ClosureMCGenME" : "DataMESignal")).c_str()));
        }

        loadedDataCollection.push_back(data);
      }

      if (applyME) {
        fileDataMEInput->Close();
        delete fileDataMEInput;
      }
    } else {
      std::cout << "[INFO] CorrelationWPDGTask: Cache ENABLED. Skipping THnSparse loading." << std::endl;

      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(data);
      }
    }

    fileDataInput->Close();
    delete fileDataInput;

    // 3. Load Efficiencies and Extrapolation Setup
    if (applyEfficiency)
      LoadCorrections(taskConfig);

    if (applyExtrapolation) {
      if (!taskConfig.HasMember("extrapolation_config_file")) {
        throw std::runtime_error("[FATAL ERROR] CorrelationWPDGTask: 'extrapolation_config_file' missing in JSON!");
      }
      std::string extrapFile = taskConfig["extrapolation_config_file"].GetString();
      extrapConfigManager = new ExtrapConfigManager(extrapFile);
      std::cout << "[INFO] CorrelationWPDGTask: Extrapolation configuration loaded successfully." << std::endl;

      for (const auto& p : assocParticles) {
        bool hasCfg = extrapConfigManager->HasConfig(p.name);
        doExtrapolationPerParticle[p.name] = hasCfg;
        if (!hasCfg) {
          std::cout << "[INFO] CorrelationWPDGTask: No extrapolation config found for '"
                    << p.name << "'. Skipping extrapolation for this species." << std::endl;
        }
      }
    }

    // 4. Initialize output
    std::string projMode = useProjectionCache ? "READ" : "UPDATE";

    std::string prefix = taskConfig.HasMember("input_output_prefix") ? taskConfig["input_output_prefix"].GetString() : "";

    std::string basePathProj = taskConfig["output_dir_proj"].GetString();

    std::string basePathFinal = taskConfig["output_dir_final"].GetString();
    std::string phiSpectraName = basePathFinal + prefix + "PhiAssocSpectra.root";
    fileOutputSpectra = new TFile(phiSpectraName.c_str(), "RECREATE");

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + prefix + "Phi" + p.name + "DataHistograms.root";

      TFile* fProj = new TFile(fName.c_str(), projMode.c_str());
      if (useProjectionCache && (!fProj || fProj->IsZombie())) {
        throw std::runtime_error("[FATAL] Missing cache file: " + fName + ". Please run with 'use_projection_cache': false first!");
      }
      filesPhiAssocDataOutput.push_back(fProj);
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
    std::cout << "[INFO] CorrelationWPDGTask: RUNNING CORRELATIONS..." << std::endl;
    CorrelationCalculator corrCalculator(applyME, useProjectionCache, false, false);

    TH2* h2PhiData{nullptr};
    if (isPureGen) {
      h2PhiData = static_cast<TH2D*>(h3PhiData->Project3D("yx"));
      h2PhiData->SetDirectory(0);
    }

    // Setup Efficiency Pointers Once (Loop Hoisting)
    // using EffArray = std::array<TH1F*, AnalysisConstants::nBinMult>;
    using EffArray = std::vector<TH1F*>;
    const EffArray* phiCorrs{nullptr};
    std::vector<const EffArray*> assocCorrs(assocParticles.size(), nullptr);

    /*if (applyEfficiency) {
      phiCorrs = useIntegratedEfficiency ? &correctionCollection[0].h1CorrectionsEffMultInt : &correctionCollection[0].h1Corrections;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        size_t effIdx = assocParticles[pIdx].effIndex;
        assocCorrs[pIdx] = useIntegratedEfficiency ? &correctionCollection[effIdx].h1CorrectionsEffMultInt : &correctionCollection[effIdx].h1Corrections;
      }
    }*/

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

      // Determine which efficiency to use for this multiplicity bin
      // auto phiCorrs = useIntegratedEfficiency ? correctionCollection[0].h1CorrectionsEffMultInt : correctionCollection[0].h1Corrections;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

      for (int j = 0; j < nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        double triggerSignal{0.0};

        if (!isPureGen) {
          std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
          TH1* h1PhiData = static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1));
          h1PhiData->SetDirectory(0);
          triggerSignal = h1PhiData->Integral();
          delete h1PhiData;
        } else {
          triggerSignal = h2PhiData->GetBinContent(i + 1, j + 1);
        }

        double phiEff = applyEfficiency ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];

          // auto assocCorrs = useIntegratedEfficiency ? correctionCollection[config.effIndex].h1CorrectionsEffMultInt : correctionCollection[config.effIndex].h1Corrections;
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i] : nullptr;

          TDirectory* targetDir = AnalysisUtils::GetOrCreatePath(filesPhiAssocDataOutput[pIdx], logicalPath, useProjectionCache);

          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};

            // Prepare inputs for the calculator
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            std::string histNameBase = "h1Phi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

            double assocEff = applyEfficiency ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            // Call the calculator: project, scale, apply ME, subtract, and save intermediate files
            TH1* h1FinalSignal = corrCalculator.ExtractCorrectedSignal(data, axesToCut, totalEff, 0.0, histNameBase + suffix, targetDir, nullptr);

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

    if (isPureGen && h2PhiData) {
      delete h2PhiData;
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] CorrelationWPDGTask: TERMINATING AND CLEANING UP..." << std::endl;

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
      std::cout << "[INFO] CorrelationWPDGTask: Skipping yield ratio (need exactly 2 associated particles, found "
                << assocParticles.size() << ")." << std::endl;
    }

    /*TCanvas* canvasRatioMultTrend = new TCanvas("canvasRatioMultTrend", "Ratio Mult Trend", 800, 600);
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

      std::string ratioName = std::format("Ratio_K0S_Pi_dy{}", dyNameStr);
      std::string ratioTitle = std::format("Ratio K0S / Pi |#Delta y| < {}", dyTitleStr);

      // Assuming 0 is K0S and 1 is Pi based on assocParticles order
      std::string ratioMeasName = std::format("Ratio_K0S_Pi_Meas_dy{}", dyNameStr);
      TH1* hRatioMeas = static_cast<TH1*>(h1MultTrends[0][yIdx]->Clone(ratioMeasName.c_str()));
      hRatioMeas->SetTitle("Ratio;Multiplicity Percentile (%);2K_{S}^{0}/(#pi^{+}+#pi^{-})");
      hRatioMeas->SetDirectory(0);

      hRatioMeas->Divide(h1MultTrends[0][yIdx], h1MultTrends[1][yIdx], 2.0, 1.0);

      AnalysisUtils::SetHistogramStyle(hRatioMeas, globalCfgs.GetMultTrendColor(yIdx));
      hRatioMeas->SetMarkerStyle(24);

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

    canvasRatioMultTrend->Write();*/

    // =========================================================================
    // 2. Write Yield Trends (Measured and Extrapolated), per species
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      std::string cNameMeas = std::format("cTrend_Meas_{}", assocParticles[pIdx].name);
      TCanvas* cTrendMeas = new TCanvas(cNameMeas.c_str(), "Measured Yield Trend", 800, 600);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        h1MultTrends[pIdx][yIdx]->Write();

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
          h1MultTrendsExtrap[pIdx][yIdx]->Write();

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

    // =========================================================================
    // 6. Memory Cleanup for RAM-resident objects
    // =========================================================================

    // Clean up Extrapolation configuration
    if (extrapConfigManager) {
      delete extrapConfigManager;
    }

    if (h3PhiData) {
      delete h3PhiData;
    }

    if (hEventLoss) {
      delete hEventLoss;
    }

    // delete canvasRatioMultTrend;

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

    // Clean up THnSparse data
    for (auto& data : loadedDataCollection) {
      if (data.h5DataSignal)
        delete data.h5DataSignal;
      if (data.h5DataMESignal)
        delete data.h5DataMESignal;
    }

    std::cout << "[INFO] CorrelationWPDGTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::string basePathData, basePathDataME;

  bool applyME{false}, applyEfficiency{false}, applyExtrapolation{false};
  bool useIntegratedEfficiency{false}, useProjectionCache{false}, use2DMENormalization{false};
  bool useSignalCache{false}, doMoreQA{false}, isPureGen{false}, useLegacyExtrapolation{false};

  TH3F* h3PhiData{nullptr};

  TH1* hEventLoss{nullptr};

  std::vector<AssocParticleConfig> assocParticles;
  std::vector<LoadedAssocData> loadedDataCollection;

  // std::vector<LoadedCorrections> correctionCollection;
  std::map<std::string, LoadedCorrections> correctionCollection;

  CorrelationCalculator::AxisTarget projectionAxis{CorrelationCalculator::AxisTarget::DeltaY_Y};

  std::map<std::string, bool> doExtrapolationPerParticle;

  // All accumulators and file vectors from the original task
  std::vector<std::vector<std::vector<TH1*>>> h1PhiAssocNoPtPhi;
  std::vector<double> deltaYLimits{1.0, 0.5, 0.1};
  std::vector<std::vector<TH1*>> h1MultTrends;
  std::vector<std::vector<TH1*>> h1MultTrendsExtrap;

  // std::string extrapFunction{"LevyTsallis"};
  // std::pair<double, double> extrapFitRange{0.4, 6.0};
  ExtrapConfigManager* extrapConfigManager{nullptr};

  TFile* fileOutputSpectra{nullptr};
  std::vector<TFile*> filesPhiAssocDataOutput;
  std::vector<std::vector<TCanvas*>> spectraCanvases;

  std::string yieldRatioLabel{""};

  void LoadCorrections(const rapidjson::Value& taskConfig)
  {
    if (!taskConfig.HasMember("input_efficiency_file")) {
      throw std::runtime_error("[FATAL ERROR] CorrelationWPDGTask: 'input_efficiency_file' missing in JSON!");
    }

    std::string inputEffFile = taskConfig["input_efficiency_file"].GetString();
    TFile* fileEffInput = new TFile(inputEffFile.c_str(), "READ");
    if (!fileEffInput || fileEffInput->IsZombie()) {
      throw std::runtime_error("[FATAL] CorrelationWPDGTask: Efficiency requested but Corrections.root not found at: " + inputEffFile);
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

    /*// Particle definitions for corrections mapping
    std::vector<ParticleConfig<2>> particles = {
      {"Phi", {"h1PhiEfficiency", "h1PhiSigLoss"}},
      {"K0S", {"h1K0SEfficiency", "h1K0SSigLoss"}},
      {"Pi", {"h1PiEfficiency", "h1PiSigLoss"}}};

    for (const auto& p : particles) {
      LoadedCorrections corrections;
      corrections.name = p.name;

      std::string effNameIntegrated = p.titles[0] + "_multIntegrated";
      std::string lossNameIntegrated = p.titles[1] + "_multIntegrated";

      TH1F* h1EffIntegrated = static_cast<TH1F*>(fileEffInput->Get(effNameIntegrated.c_str()));
      TH1F* h1LossIntegrated = static_cast<TH1F*>(fileEffInput->Get(lossNameIntegrated.c_str()));

      if (!h1EffIntegrated || !h1LossIntegrated) {
        throw std::runtime_error("[FATAL] CorrelationWPDGTask: Missing integrated correction histograms for " + p.name);
      }

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string effName = p.titles[0] + "_multBin" + std::to_string(i);
        std::string lossName = p.titles[1] + "_multBin" + std::to_string(i);

        TH1F* h1Eff = static_cast<TH1F*>(fileEffInput->Get(effName.c_str()));
        TH1F* h1Loss = static_cast<TH1F*>(fileEffInput->Get(lossName.c_str()));

        if (!h1Eff || !h1Loss) {
          throw std::runtime_error("[FATAL] CorrelationWPDGTask: Missing correction histograms for " + p.name + " in mult bin " + std::to_string(i));
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

    correctionCollection.clear();

    for (const std::string& name : effParts) {
      LoadedCorrections corr;
      corr.name = name;
      corr.h1Corrections.resize(globalCfgs.nBinMult, nullptr);
      corr.h1CorrectionsEffMultInt.resize(globalCfgs.nBinMult, nullptr);

      // Naming convention for histograms based on particle name
      std::string effHistBase = "h1" + name + "Efficiency";
      std::string sigLossHistBase = "h1" + name + "SigLoss";

      // Fetch integrated histograms ONLY if useIntegratedEfficiency is true
      TH1F* hEffInt = fetchHist(effHistBase + "_multIntegrated", doAccEfficiency && useIntegratedEfficiency);
      // Note: Signal loss is typically not integrated, but we fetch it if requested for consistency
      // TH1F* hLossInt = fetchHist(sigLossHistBase + "_multIntegrated", doSigLoss && useIntegratedEfficiency);

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string iStr = std::to_string(i);

        // Fetch binned histograms ONLY if useIntegratedEfficiency is false
        TH1F* hEffBin = fetchHist(effHistBase + "_multBin" + iStr, doAccEfficiency && !useIntegratedEfficiency);
        // Note: Signal loss is typically not integrated, so we fetch the binned version if signal loss correction is requested,
        // regardless of the useIntegratedEfficiency flag
        TH1F* hLossBin = fetchHist(sigLossHistBase + "_multBin" + iStr, doSigLoss);

        corr.h1Corrections[i] = combineHists(hEffBin, hLossBin, "h1Corrected_" + name + "_multBin" + iStr);
        corr.h1CorrectionsEffMultInt[i] = combineHists(hEffInt, hLossBin, "h1Corrected_" + name + "_multInt" + iStr);
      }

      correctionCollection[name] = std::move(corr);
    }

    fileEffInput->Close();
    delete fileEffInput;

    std::cout << "[INFO] CorrelationWPDGTask: Efficiencies loaded successfully." << std::endl;
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
          throw std::runtime_error("[FATAL] CorrelationWPDGTask: Unknown extrap_function: " + extrapFunction);
        }*/

        ExtrapConfig eCfg = extrapConfigManager->GetConfig(config.name, multBin);
        eCfg.mass = config.mass;

        std::unique_ptr<TF1> extrapModel = ExtrapolationModelFactory::CreateModel(eCfg, maxVal);

        {
          std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}_multBin{}", hSpec->GetName(), eCfg.model, multBin);
          TCanvas* cDebug = new TCanvas(debugCanvasName.c_str(), "Debug Guesses", 800, 600);

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
        if (totalTriggerSignalPerMult > 0) {
          h1Spectrum->Scale(1.0 / (effectiveTriggers * 2.0 * dyLimit));
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

  static double GetYieldScaleFactor(const std::string& name)
  {
    return (name == "K0S") ? 2.0 : 1.0;
  }
};

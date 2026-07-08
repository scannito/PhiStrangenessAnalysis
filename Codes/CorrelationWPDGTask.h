#pragma once

#include "CorrelationTaskBase.h"

class CorrelationWPDGTask : public CorrelationTaskBase
{
 public:
  std::string GetName() const override { return "correlation_wpdg_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] CorrelationWPDGTask: INITIALIZING..." << std::endl;

    InitCommonFlags(taskConfig, globalSettings);

    isPureGen = taskConfig["is_pure_gen"].GetBool();

    // 2. Load Data
    if (!taskConfig.HasMember("input_data_file") || !taskConfig.HasMember("base_path_data"))
      throw std::runtime_error("[FATAL ERROR] CorrelationWPDGTask: Missing input_data_file or base_path_data in JSON!");
    std::string inputFile = taskConfig["input_data_file"].GetString();
    basePathData = taskConfig["base_path_data"].GetString();

    TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
    if (!fileDataInput || fileDataInput->IsZombie())
      throw std::runtime_error("[FATAL] CorrelationWPDGTask: Cannot open Data input file: " + inputFile);

    if (!isPureGen) {
      h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiData").c_str()));
      if (!h3PhiData)
        throw std::runtime_error("[FATAL] CorrelationWPDGTask: Missing h3PhiData!");
      h3PhiData->SetDirectory(0);
    } else {
      h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiMCGen").c_str()));
      if (!h3PhiData)
        throw std::runtime_error("[FATAL] CorrelationWPDGTask: Missing h3PhiMCGen for pure gen test!");
      h3PhiData->SetDirectory(0);

      // Pre-compute the 2D projection once, up front, so GetTriggerSignal()
      // during Run() is a pure lookup with no per-call state management.
      h2PhiData = static_cast<TH2D*>(h3PhiData->Project3D("yx"));
      h2PhiData->SetDirectory(0);
    }

    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      std::cout << "[INFO] CorrelationWPDGTask: Cache DISABLED. Loading heavy THnSparse data..." << std::endl;

      TFile* fileDataMEInput{nullptr};
      if (applyME) {
        if (!taskConfig.HasMember("input_me_file") || !taskConfig.HasMember("base_path_me"))
          throw std::runtime_error("[FATAL ERROR] CorrelationWPDGTask: ME files or paths missing in JSON despite applyME=true!");
        std::string inputMEFile = taskConfig["input_me_file"].GetString();
        basePathDataME = taskConfig["base_path_me"].GetString();

        fileDataMEInput = new TFile(inputMEFile.c_str(), "READ");
        if (!fileDataMEInput || fileDataMEInput->IsZombie())
          throw std::runtime_error("[FATAL] CorrelationWPDGTask: Cannot open ME input file: " + inputMEFile);
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

    InitCorrectionsAndExtrapolation(taskConfig);

    // 4. Output
    std::string prefix = taskConfig.HasMember("input_output_prefix") ? taskConfig["input_output_prefix"].GetString() : "";

    std::string basePathProj = taskConfig["output_dir_proj"].GetString();
    std::string basePathFinal = taskConfig["output_dir_final"].GetString();
    std::string phiSpectraName = basePathFinal + prefix + "PhiAssocSpectra.root";
    fileOutputSpectra = new TFile(phiSpectraName.c_str(), "RECREATE");

    std::string projMode = useProjectionCache ? "READ" : "RECREATE";

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + prefix + "Phi" + p.name + "DataHistograms.root";
      TFile* fProj = new TFile(fName.c_str(), projMode.c_str());
      if (useProjectionCache && (!fProj || fProj->IsZombie()))
        throw std::runtime_error("[FATAL] Missing cache file: " + fName + ". Run with 'use_projection_cache': false first!");
      filesPhiAssocDataOutput.push_back(fProj);
    }

    SetupTrendHistograms();
  }

  void Run() override
  {
    // WPDG uses the shared, simple pass for now — no L2 cache needed here yet.
    RunLegacy();
  }

 protected:
  void CleanupExtraMembers() override
  {
    if (h3PhiData)
      delete h3PhiData;
    if (h2PhiData)
      delete h2PhiData;
  }

  double GetTriggerSignal(int multBin, int ptPhiBin) override
  {
    if (!isPureGen) {
      std::string phiHistName = "h1PhiData_multBin" + std::to_string(multBin) + "_ptBin" + std::to_string(ptPhiBin);
      TH1* h1PhiData = static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), multBin + 1, multBin + 1, ptPhiBin + 1, ptPhiBin + 1));
      h1PhiData->SetDirectory(0);
      double integral = h1PhiData->Integral();
      delete h1PhiData;
      return integral;
    }
    return h2PhiData->GetBinContent(multBin + 1, ptPhiBin + 1);
  }
  // GetTriggerBkgRatio not overridden: base default (0.0) is correct for WPDG.

 private:
  bool isPureGen{false};

  TH3F* h3PhiData{nullptr};
  TH2* h2PhiData{nullptr};
};

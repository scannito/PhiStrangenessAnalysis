#pragma once

#include "CorrelationTaskBase.h"

class CorrelationTask : public CorrelationTaskBase
{
 public:
  enum class RunMode { Legacy = 0,
                       Optimized };

  std::string GetName() const override { return "correlation_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] CorrelationTask: INITIALIZING..." << std::endl;

    InitCommonFlags(taskConfig, globalSettings);

    applyPurity = taskConfig["apply_purity"].GetBool();

    if (taskConfig.HasMember("use_signal_cache") && taskConfig["use_signal_cache"].IsBool())
      useSignalCache = taskConfig["use_signal_cache"].GetBool();
    if (taskConfig.HasMember("do_more_qa") && taskConfig["do_more_qa"].IsBool())
      doMoreQA = taskConfig["do_more_qa"].GetBool();
    if (taskConfig.HasMember("run_mode") && taskConfig["run_mode"].IsInt())
      runMode = static_cast<RunMode>(taskConfig["run_mode"].GetInt());

    // Prefix handling: search specific key -> Search fallback -> Search legacy
    auto getPrefix = [](const rapidjson::Value& config, const std::string& specificKey, const std::string& fallbackKey) -> std::string {
      // 1. Search for the highly specific prefix (e.g., "purity_prefix")
      if (config.HasMember(specificKey.c_str()) && config[specificKey.c_str()].IsString())
        return config[specificKey.c_str()].GetString();
      // 2. Search for the fallback prefix (e.g., "input_prefix")
      if (config.HasMember(fallbackKey.c_str()) && config[fallbackKey.c_str()].IsString())
        return config[fallbackKey.c_str()].GetString();
      // 3. Final fallback for legacy JSON configurations
      if (config.HasMember("input_output_prefix") && config["input_output_prefix"].IsString())
        return config["input_output_prefix"].GetString();
      return "";
    };

    purityPrefix = getPrefix(taskConfig, "purity_prefix", "input_prefix");
    triggerPrefix = getPrefix(taskConfig, "trigger_prefix", "input_prefix");
    outputPrefix = getPrefix(taskConfig, "output_prefix", "output_prefix");

    // 2. Load Trigger information from PhiFitTask
    if (!taskConfig.HasMember("input_dir_proj"))
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_dir_proj' missing in JSON!");
    std::string basePathProj = taskConfig["input_dir_proj"].GetString();
    std::string phiDataName = basePathProj + triggerPrefix + "PhiDataHistograms.root";

    TFile* filePhiDataInput = new TFile(phiDataName.c_str(), "READ");
    if (!filePhiDataInput || filePhiDataInput->IsZombie())
      throw std::runtime_error("[FATAL] CorrelationTask: Missing PhiFitTask output file: " + phiDataName);

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};
    std::string folderPath = AnalysisUtils::VectorToPath(summaryPath);

    h2TriggerSignal = static_cast<TH2D*>(filePhiDataInput->Get((folderPath + "h2TriggerSignal").c_str()));
    h2TriggerBkgRatio = static_cast<TH2D*>(filePhiDataInput->Get((folderPath + "h2TriggerBkgRatio").c_str()));
    if (!h2TriggerSignal || !h2TriggerBkgRatio)
      throw std::runtime_error("[FATAL] CorrelationTask: Missing trigger stats in " + phiDataName);
    h2TriggerSignal->SetDirectory(0);
    h2TriggerBkgRatio->SetDirectory(0);
    filePhiDataInput->Close();
    delete filePhiDataInput;

    // 3. Associated particles + data loading
    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      std::cout << "[INFO] CorrelationTask: Cache DISABLED. Loading heavy THnSparse data..." << std::endl;

      if (!taskConfig.HasMember("input_data_file") || !taskConfig.HasMember("base_path_data"))
        throw std::runtime_error("[FATAL ERROR] CorrelationTask: Missing input_data_file or base_path_data in JSON!");
      std::string inputFile = taskConfig["input_data_file"].GetString();
      basePathData = taskConfig["base_path_data"].GetString();

      TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
      if (!fileDataInput || fileDataInput->IsZombie())
        throw std::runtime_error("[FATAL] CorrelationTask: Cannot open Data input file: " + inputFile);

      TFile* fileDataMEInput{nullptr};
      if (applyME) {
        if (!taskConfig.HasMember("input_me_file") || !taskConfig.HasMember("base_path_me"))
          throw std::runtime_error("[FATAL ERROR] CorrelationTask: ME files or paths missing in JSON despite applyME=true!");
        std::string inputMEFile = taskConfig["input_me_file"].GetString();
        basePathDataME = taskConfig["base_path_me"].GetString();

        fileDataMEInput = new TFile(inputMEFile.c_str(), "READ");
        if (!fileDataMEInput || fileDataMEInput->IsZombie())
          throw std::runtime_error("[FATAL] CorrelationTask: Cannot open ME input file: " + inputMEFile);
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
      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(data);
      }
    }

    // 4. Corrections / Purity / Extrapolation
    InitCorrectionsAndExtrapolation(taskConfig);
    if (applyPurity)
      LoadPurities(taskConfig);

    // 5. Output
    std::string projMode = useProjectionCache ? "READ" : "UPDATE";
    std::string basePathFinal = taskConfig["output_dir_final"].GetString();
    std::string phiSpectraName = basePathFinal + outputPrefix + "PhiAssocSpectra.root";
    fileOutputSpectra = new TFile(phiSpectraName.c_str(), "UPDATE");

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + outputPrefix + "Phi" + p.name + "DataHistograms.root";
      TFile* fProj = new TFile(fName.c_str(), projMode.c_str());
      if (useProjectionCache && (!fProj || fProj->IsZombie()))
        throw std::runtime_error("[FATAL] Missing cache file: " + fName + ". Run with 'use_projection_cache': false first!");
      filesPhiAssocDataOutput.push_back(fProj);

      if (doMoreQA) {
        std::string fQAName = basePathProj + outputPrefix + "Phi" + p.name + "QAHistograms.root";
        TFile* fQA = new TFile(fQAName.c_str(), "RECREATE");
        filesPhiAssocQAOutput.push_back(fQA);
      }
    }

    SetupTrendHistograms();
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

 protected:
  void CleanupExtraMembers() override
  {
    if (h2TriggerSignal)
      delete h2TriggerSignal;
    if (h2TriggerBkgRatio)
      delete h2TriggerBkgRatio;

    for (auto& [name, pur] : purityCollection)
      for (auto& h1 : pur.h1Purity)
        if (h1)
          delete h1;
  }

  TH1* GetPurityHist(const std::string& particleName, int multBin) override
  {
    if (!applyPurity)
      return nullptr;
    auto it = purityCollection.find(particleName);
    if (it == purityCollection.end())
      throw std::runtime_error("[FATAL] CorrelationTask: Missing purity data for particle '" + particleName + "'");
    return it->second.h1Purity[multBin];
  }

  double GetTriggerSignal(int multBin, int ptPhiBin) override
  {
    return h2TriggerSignal->GetBinContent(multBin + 1, ptPhiBin + 1);
  }

  double GetTriggerBkgRatio(int multBin, int ptPhiBin) override
  {
    return h2TriggerBkgRatio->GetBinContent(multBin + 1, ptPhiBin + 1);
  }

 private:
  RunMode runMode{RunMode::Legacy};

  bool applyPurity{false};
  bool useSignalCache{false}, doMoreQA{false};

  std::string purityPrefix{""}, triggerPrefix{""}, outputPrefix{""};

  TH2D* h2TriggerSignal{nullptr};
  TH2D* h2TriggerBkgRatio{nullptr};

  std::map<std::string, LoadedPurity> purityCollection;

  void LoadPurities(const rapidjson::Value& taskConfig)
  {
    if (!taskConfig.HasMember("input_dir_purity"))
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_dir_purity' missing in JSON!");
    if (!taskConfig.HasMember("purity_sources") || !taskConfig["purity_sources"].IsArray())
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'purity_sources' missing or invalid in JSON!");

    std::string purityDir = taskConfig["input_dir_purity"].GetString();
    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};
    std::string folderPath = AnalysisUtils::VectorToPath(summaryPath);

    purityCollection.clear();

    for (const auto& src : taskConfig["purity_sources"].GetArray()) {
      std::string name = src["name"].GetString();
      std::string purityKey = src["purity_key"].GetString();
      std::string fileSuffix = src["file_suffix"].GetString();
      std::string purityFilePath = purityDir + purityPrefix + fileSuffix;

      LoadedPurity purity;
      purity.name = name;
      purity.h1Purity.resize(globalCfgs.nBinMult, nullptr);

      TFile* filePurity = new TFile(purityFilePath.c_str(), "READ");
      if (!filePurity || filePurity->IsZombie())
        throw std::runtime_error("[FATAL] CorrelationTask: Cannot open purity file: " + purityFilePath);

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        std::string hName = folderPath + "h1" + purityKey + "Purity_multBin" + std::to_string(i);
        TH1* h1Pur = static_cast<TH1*>(filePurity->Get(hName.c_str()));
        if (!h1Pur)
          throw std::runtime_error("[FATAL] CorrelationTask: Missing purity histogram: " + hName + " in " + purityFilePath);

        purity.h1Purity[i] = static_cast<TH1*>(h1Pur->Clone());
        purity.h1Purity[i]->SetDirectory(0);
      }

      filePurity->Close();
      delete filePurity;
      purityCollection[name] = std::move(purity);
    }
    std::cout << "[INFO] CorrelationTask: Purities loaded successfully." << std::endl;
  }

  void RunOptimized()
  {
    std::cout << "[INFO] CorrelationTask: RUNNING OPTIMIZED CORRELATIONS AND QA..." << std::endl;

    // =========================================================================
    // 0. EFFICIENCY POINTERS SETUP
    // Hoisted to the top so both the Cache L2 branch and the calculation branch
    // can access them to properly normalize the trigger signals.
    // =========================================================================
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

    // =========================================================================
    // MODE A: SUPER CACHE MANAGEMENT (LEVEL 2)
    // If active, completely bypasses the CorrelationCalculator and heavy loops.
    // =========================================================================
    if (useSignalCache) {
      std::cout << "[INFO] CorrelationTask: Super Cache (Signal L2) is ON. Skipping Calculator." << std::endl;

      for (int i = 0; i < globalCfgs.nBinMult; i++) {
        double totalTriggerSignalPerMult = 0.0;
        TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

        for (int j = 0; j < nBinPtPhi; j++) {
          double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
          double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
          totalTriggerSignalPerMult += triggerSignal / phiEff;
        }

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          TDirectory* sourceDir = AnalysisUtils::GetOrCreatePath(filesPhiAssocDataOutput[pIdx], logicalPath, true);
          if (!sourceDir)
            throw std::runtime_error("[FATAL] Cache L2 missing: Directory " + dirName + " not found!");

          for (int k = 0; k < config.nBinPt; k++) {
            std::string cacheName = "h1Phi" + config.name + "Final_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);
            h1PhiAssocNoPtPhi[pIdx][i][k] = static_cast<TH1*>(sourceDir->Get(cacheName.c_str()));
            if (!h1PhiAssocNoPtPhi[pIdx][i][k])
              throw std::runtime_error("[FATAL] Cache L2 missing for: " + cacheName + ". Run with use_signal_cache: false first!");
            h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
          }
        }
        GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
      }
      return;
    }

    // =========================================================================
    // MODE B: STANDARD CALCULATION (If L2 Cache is disabled)
    // The CorrelationCalculator is invoked here (managing L1 Cache internally)
    // =========================================================================
    CorrelationCalculator corrCalculator(applyME, useProjectionCache, use2DMENormalization, doMoreQA);

    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      AnalysisUtils::AxisToCut axisToCutMult{0, i + 1, i + 1};
      double totalTriggerSignalPerMult = 0.0;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i] : nullptr;

      for (int j = 0; j < nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
        double triggerBkgRatio = h2TriggerBkgRatio->GetBinContent(i + 1, j + 1);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i] : nullptr;

          TDirectory* targetDir = AnalysisUtils::GetOrCreatePath(filesPhiAssocDataOutput[pIdx], logicalPath, useProjectionCache);

          TDirectory* currentQADir{nullptr};
          if (doMoreQA && pIdx < filesPhiAssocQAOutput.size()) {
            currentQADir = AnalysisUtils::GetOrCreatePath(filesPhiAssocQAOutput[pIdx], logicalPath, false);
          }

          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string baseNameStd = "hPhi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptBin" + std::to_string(k);

            TH1* h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
              data, axesToCut, totalEff, triggerBkgRatio, baseNameStd + suffix, targetDir, currentQADir, projectionAxis);

            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);
              h1PhiAssocNoPtPhi[pIdx][i][k] = static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str()));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal);
            }
            delete h1FinalSignal;
          }
        }
      }

      // =======================================================================
      // BATCH WRITE: SAVING AGGREGATED SIGNALS FOR L2 CACHE
      // Written only once per multiplicity bin after trigger aggregation
      // =======================================================================
      std::cout << "[INFO] Multiplicity bin " << i << " completed. Committing results to Level 2 Cache..." << std::endl;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        TDirectory* targetDir = AnalysisUtils::GetOrCreatePath(filesPhiAssocDataOutput[pIdx], logicalPath, false);
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

      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    }
  }
};

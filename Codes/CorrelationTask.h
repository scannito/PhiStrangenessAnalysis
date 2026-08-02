#pragma once

#include "CorrelationTaskBase.h"
#include "RootIOHelpers.h"

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

    /*TFile* filePhiDataInput = new TFile(phiDataName.c_str(), "READ");
    if (!filePhiDataInput || filePhiDataInput->IsZombie())
      throw std::runtime_error("[FATAL] CorrelationTask: Missing PhiFitTask output file: " + phiDataName);*/
    std::unique_ptr<TFile> filePhiDataInput = RootIO::OpenOrThrow(phiDataName, "READ", "CorrelationTask");

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};
    std::string folderPath = RootIO::MakeDirPath(summaryPath);

    /*h2TriggerSignal = static_cast<TH2D*>(filePhiDataInput->Get((folderPath + "h2TriggerSignal").c_str()));
    h2TriggerBkgRatio = static_cast<TH2D*>(filePhiDataInput->Get((folderPath + "h2TriggerBkgRatio").c_str()));
    if (!h2TriggerSignal || !h2TriggerBkgRatio)
      throw std::runtime_error("[FATAL] CorrelationTask: Missing trigger stats in " + phiDataName);
    h2TriggerSignal->SetDirectory(0);
    h2TriggerBkgRatio->SetDirectory(0);*/
    h2TriggerSignal = RootIO::GetUniqueOrThrow<TH2D>(filePhiDataInput.get(), folderPath + "h2TriggerSignal", "CorrelationTask");
    h2TriggerBkgRatio = RootIO::GetUniqueOrThrow<TH2D>(filePhiDataInput.get(), folderPath + "h2TriggerBkgRatio", "CorrelationTask");

    // 3. Associated particles + data loading
    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      std::cout << "[INFO] CorrelationTask: Cache DISABLED. Loading heavy THnSparse data..." << std::endl;

      /*if (!taskConfig.HasMember("input_data_file") || !taskConfig.HasMember("base_path_data"))
        throw std::runtime_error("[FATAL ERROR] CorrelationTask: Missing input_data_file or base_path_data in JSON!");
      std::string inputFile = taskConfig["input_data_file"].GetString();
      basePathData = taskConfig["base_path_data"].GetString();

      TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
      if (!fileDataInput || fileDataInput->IsZombie())
        throw std::runtime_error("[FATAL] CorrelationTask: Cannot open Data input file: " + inputFile);*/

      std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
      std::unique_ptr<TFile> fileDataInput = RootIO::OpenOrThrow(inputFile, "READ", "CorrelationTask");

      basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());

      /*TFile* fileDataMEInput{nullptr};
      if (applyME) {
        if (!taskConfig.HasMember("input_me_file") || !taskConfig.HasMember("base_path_me"))
          throw std::runtime_error("[FATAL ERROR] CorrelationTask: ME files or paths missing in JSON despite applyME=true!");
        std::string inputMEFile = taskConfig["input_me_file"].GetString();
        basePathDataME = taskConfig["base_path_me"].GetString();

        fileDataMEInput = new TFile(inputMEFile.c_str(), "READ");
        if (!fileDataMEInput || fileDataMEInput->IsZombie())
          throw std::runtime_error("[FATAL] CorrelationTask: Cannot open ME input file: " + inputMEFile);
      }*/

      std::unique_ptr<TFile> fileDataMEInput{nullptr};
      if (applyME) {
        std::string inputMEFile = JsonConfig::RequireString(taskConfig, "input_me_file", GetName());
        fileDataMEInput = RootIO::OpenOrThrow(inputMEFile, "READ", "CorrelationTask");

        basePathDataME = JsonConfig::RequireString(taskConfig, "base_path_me", GetName());
      }

      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
        // data.h5DataSignal = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSignal").c_str()));
        // data.h5DataSideband = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSideband").c_str()));
        data.h5DataSignal = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataInput.get(), (baseData + "DataSignal"), "CorrelationTask");
        data.h5DataSideband = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataInput.get(), (baseData + "DataSideband"), "CorrelationTask");

        if (applyME) {
          std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;
          // data.h5DataMESignal = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESignal").c_str()));
          // data.h5DataMESideband = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESideband").c_str()));
          data.h5DataMESignal = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataMEInput.get(), (baseDataME + "DataMESignal"), "CorrelationTask");
          data.h5DataMESideband = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataMEInput.get(), (baseDataME + "DataMESideband"), "CorrelationTask");
        }

        loadedDataCollection.push_back(std::move(data));
      }
    } else {
      std::cout << "[INFO] CorrelationTask: Cache ENABLED. Skipping THnSparse loading." << std::endl;
      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(std::move(data));
      }
    }

    // 4. Output files (opened before the corrections: the cache carries the
    //    binning that everything below is loaded against)
    std::string projMode = useProjectionCache ? "READ" : "UPDATE";
    std::string basePathFinal = taskConfig["output_dir_final"].GetString();
    std::string phiSpectraName = basePathFinal + outputPrefix + "PhiAssocSpectra.root";
    fileOutputSpectra = RootIO::OpenOrThrow(phiSpectraName, "UPDATE", "CorrelationTask");

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + outputPrefix + "Phi" + p.name + "DataHistograms.root";
      std::unique_ptr<TFile> fProj = RootIO::OpenOrThrow(fName, projMode.c_str(), "CorrelationTask");
      filesPhiAssocDataOutput.push_back(std::move(fProj));

      /*TFile* fProj = new TFile(fName.c_str(), projMode.c_str());
      if (useProjectionCache && (!fProj || fProj->IsZombie()))
        throw std::runtime_error("[FATAL] Missing cache file: " + fName + ". Run with 'use_projection_cache': false first!");
      filesPhiAssocDataOutput.push_back(fProj);*/

      if (doMoreQA) {
        std::string fQAName = basePathProj + outputPrefix + "Phi" + p.name + "QAHistograms.root";
        std::unique_ptr<TFile> fQA = RootIO::OpenOrThrow(fQAName, "RECREATE", "CorrelationTask");
        filesPhiAssocQAOutput.push_back(std::move(fQA));

        /*TFile* fQA = new TFile(fQAName.c_str(), "RECREATE");
        filesPhiAssocQAOutput.push_back(fQA);*/
      }
    }

    ResolveBinningAndCache();

    // 5. Corrections / Purity / Extrapolation, rebinned onto the binning that
    //    was just resolved from the data
    InitCorrectionsAndExtrapolation(taskConfig);
    if (applyPurity)
      LoadPurities(taskConfig);

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
  TH1* GetPurityHist(const std::string& particleName, int multBin) override
  {
    if (!applyPurity)
      return nullptr;
    auto it = purityCollection.find(particleName);
    if (it == purityCollection.end())
      throw std::runtime_error("[FATAL] CorrelationTask: Missing purity data for particle '" + particleName + "'");
    return it->second.h1Purity[multBin].get();
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

  // TH2D* h2TriggerSignal{nullptr};
  // TH2D* h2TriggerBkgRatio{nullptr};
  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  std::map<std::string, LoadedPurity> purityCollection;

  void LoadPurities(const rapidjson::Value& taskConfig)
  {
    /*if (!taskConfig.HasMember("input_dir_purity"))
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'input_dir_purity' missing in JSON!");
    if (!taskConfig.HasMember("purity_sources") || !taskConfig["purity_sources"].IsArray())
      throw std::runtime_error("[FATAL ERROR] CorrelationTask: 'purity_sources' missing or invalid in JSON!");*/

    // std::string purityDir = taskConfig["input_dir_purity"].GetString();

    std::string purityDir = JsonConfig::RequireString(taskConfig, "input_dir_purity", GetName());
    auto puritySources = JsonConfig::RequireArray(taskConfig, "purity_sources", GetName());

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};
    std::string folderPath = RootIO::MakeDirPath(summaryPath);

    purityCollection.clear();

    for (const auto& src : puritySources) {
      std::string name = src["name"].GetString();
      std::string purityKey = src["purity_key"].GetString();
      std::string fileSuffix = src["file_suffix"].GetString();
      std::string purityFilePath = purityDir + purityPrefix + fileSuffix;

      LoadedPurity purity;
      purity.name = name;
      purity.h1Purity.resize(BinningUtils::NBins(multBinning));

      std::vector<double> targetBinning;
      if (name == "Phi") {
        targetBinning = ptPhiBinning;
      } else {
        auto it = std::ranges::find_if(assocParticles, [&](const AssocParticleConfig& p) { return p.name == name; });
        if (it == assocParticles.end())
          throw std::runtime_error("[FATAL] CorrelationTask: Unknown particle name in 'purity_sources': " + name);
        targetBinning = it->binning;
      }

      /*TFile* filePurity = new TFile(purityFilePath.c_str(), "READ");
      if (!filePurity || filePurity->IsZombie())
        throw std::runtime_error("[FATAL] CorrelationTask: Cannot open purity file: " + purityFilePath);*/
      std::unique_ptr<TFile> filePurity = RootIO::OpenOrThrow(purityFilePath, "READ", "CorrelationTask");

      // Same as for the corrections: the purities are addressed by multiplicity
      // index, and only the stamp says what those indices meant when they were fitted.
      RootIO::RequireMatchingBinningStamp(
        RootIO::GetOrCreatePath(filePurity.get(), {globalCfgs.binningName}, true),
        "binning_mult", multBinning, "purity file '" + purityFilePath + "'");

      for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
        std::string hName = folderPath + "h1" + purityKey + "Purity_multBin" + std::to_string(i);
        /*TH1F* h1Pur = static_cast<TH1F*>(filePurity->Get(hName.c_str()));
        if (!h1Pur)
          throw std::runtime_error("[FATAL] CorrelationTask: Missing purity histogram: " + hName + " in " + purityFilePath);*/
        std::unique_ptr<TH1F> h1Pur = RootIO::GetUniqueOrThrow<TH1F>(filePurity.get(), hName, "CorrelationTask");

        // Verified, NOT rebinned: a purity is a ratio, and merging its bins would
        // sum the ratios. A coarser purity can only be obtained by fitting the
        // merged mass distribution - see 'rebinning_pt' in the purity config.
        const std::string diff = BinningUtils::Compare(targetBinning, BinningUtils::AxisEdges(h1Pur->GetXaxis()),
                                                       "analysis binning (data production)", "purity spectrum");
        if (!diff.empty()) {
          throw std::runtime_error("[FATAL] CorrelationTask::LoadPurities: '" + hName + "' in '" + purityFilePath +
                                   "' is not binned like the data being corrected:\n" + diff +
                                   "Set 'rebinning_pt' in the purity configuration so the fits are done at this "
                                   "binning, or re-run the production whose axis does not match.");
        }

        purity.h1Purity[i] = std::move(h1Pur);
      }

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

    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    std::vector<std::string> logicalPath{globalCfgs.binningName, dirName};

    // =========================================================================
    // MODE A: SUPER CACHE MANAGEMENT (LEVEL 2)
    // If active, completely bypasses the CorrelationCalculator and heavy loops.
    // =========================================================================
    if (useSignalCache) {
      std::cout << "[INFO] CorrelationTask: Super Cache (Signal L2) is ON. Skipping Calculator." << std::endl;

      for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
        double totalTriggerSignalPerMult = 0.0;
        TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i].get() : nullptr;

        for (int j = 0; j < BinningUtils::NBins(ptPhiBinning); j++) {
          double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
          double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
          totalTriggerSignalPerMult += triggerSignal / phiEff;
        }

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          TDirectory* sourceDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, true);
          if (!sourceDir)
            throw std::runtime_error("[FATAL] Cache L2 missing: Directory " + dirName + " not found!");

          for (int k = 0; k < BinningUtils::NBins(config.binning); k++) {
            std::string cacheName = "h1Phi" + config.name + "Final" + CellSuffix(config, i, k);
            h1PhiAssocNoPtPhi[pIdx][i][k].reset(static_cast<TH1*>(sourceDir->Get(cacheName.c_str())));
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

    for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
      AnalysisUtils::AxisToCut axisToCutMult{.axis = 0, .bins = {i + 1, i + 1}};
      double totalTriggerSignalPerMult = 0.0;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i].get() : nullptr;

      for (int j = 0; j < BinningUtils::NBins(ptPhiBinning); j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{.axis = 1, .bins = {j + 1, j + 1}};

        double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
        double triggerBkgRatio = h2TriggerBkgRatio->GetBinContent(i + 1, j + 1);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i].get() : nullptr;

          TDirectory* targetDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, useProjectionCache);

          TDirectory* currentQADir{nullptr};
          if (doMoreQA && pIdx < filesPhiAssocQAOutput.size()) {
            currentQADir = RootIO::GetOrCreatePath(filesPhiAssocQAOutput[pIdx].get(), logicalPath, false);
          }

          for (int k = 0; k < BinningUtils::NBins(config.binning); k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{.axis = 2, .bins = {k + 1, k + 1}};
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string baseNameStd = "h1Phi" + config.name + "Data";
            std::string suffix = CellSuffix(config, i, j, k);

            std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
              data, axesToCut, totalEff, triggerBkgRatio, baseNameStd + suffix, targetDir, currentQADir, projectionAxis);

            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal" + CellSuffix(config, i, k);
              h1PhiAssocNoPtPhi[pIdx][i][k].reset(static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str())));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal.get());
            }
          }
        }
      }

      // =======================================================================
      // BATCH WRITE: SAVING AGGREGATED SIGNALS FOR L2 CACHE
      // Written only once per multiplicity bin after trigger aggregation
      // =======================================================================
      std::cout << "[INFO] Multiplicity bin " << i << " completed. Committing results to Level 2 Cache..." << std::endl;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        TDirectory* targetDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, false);
        if (targetDir)
          targetDir->cd();
        for (int k = 0; k < BinningUtils::NBins(assocParticles[pIdx].binning); k++) {
          if (h1PhiAssocNoPtPhi[pIdx][i][k]) {
            std::string saveName = "h1Phi" + assocParticles[pIdx].name + "Final" + CellSuffix(assocParticles[pIdx], i, k);
            h1PhiAssocNoPtPhi[pIdx][i][k]->SetName(saveName.c_str());
            h1PhiAssocNoPtPhi[pIdx][i][k]->Write(nullptr, TObject::kOverwrite);
          }
        }
      }

      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    }
  }
};

#pragma once

#include "CorrelationTaskBase.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "RootIOHelpers.h"

#include <algorithm>
#include <format>

class CorrelationTask : public CorrelationTaskBase
{
 public:
  enum class RunMode { Legacy = 0,
                       Optimized };

  std::string GetName() const override { return "correlation_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    Logger::Info("{}: INITIALIZING...", GetName());

    InitCommonFlags(taskConfig, globalSettings);

    applyPurity = JsonConfig::RequireBool(taskConfig, "apply_purity", GetName());

    useSignalCache = JsonConfig::OptionalBool(taskConfig, "use_signal_cache", useSignalCache, GetName());
    doMoreQA = JsonConfig::OptionalBool(taskConfig, "do_more_qa", doMoreQA, GetName());
    runMode = JsonConfig::OptionalEnum<RunMode>(taskConfig, "run_mode", "legacy",
                                                {{"legacy", RunMode::Legacy},
                                                 {"optimized", RunMode::Optimized}},
                                                GetName());
    // RunLegacy builds the 1D engine whatever use_2d_me_normalization says.
    RequireEngineSettingsSupported(runMode == RunMode::Optimized && use2DMENormalization);

    // Prefix handling: search specific key -> Search fallback -> Search legacy
    auto getPrefix = [ctx = GetName()](const rapidjson::Value& config, const std::string& specificKey, const std::string& fallbackKey) -> std::string {
      // Most specific key first, then the shared one, then the legacy spelling.
      std::string prefix = JsonConfig::OptionalString(config, specificKey.c_str(), "", ctx);
      if (prefix.empty())
        prefix = JsonConfig::OptionalString(config, fallbackKey.c_str(), "", ctx);
      if (prefix.empty())
        prefix = JsonConfig::OptionalString(config, "input_output_prefix", "", ctx);
      return prefix;
    };

    purityPrefix = getPrefix(taskConfig, "purity_prefix", "input_prefix");
    triggerPrefix = getPrefix(taskConfig, "trigger_prefix", "input_prefix");
    outputPrefix = getPrefix(taskConfig, "output_prefix", "output_prefix");

    // 2. Load Trigger information from PhiFitTask
    std::string basePathProj = JsonConfig::RequireString(taskConfig, "input_dir_proj", GetName());
    std::string phiDataName = basePathProj + triggerPrefix + "PhiDataHistograms.root";

    std::unique_ptr<TFile> filePhiDataInput = RootIO::OpenOrThrow(phiDataName, "READ", GetName());

    RootIO::PrintProvenance(RootIO::GetOrCreatePath(filePhiDataInput.get(), {globalCfgs.binningName, "Provenance"}, true),
                            std::format("trigger file '{}'", phiDataName));

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};
    std::string folderPath = RootIO::MakeDirPath(summaryPath);

    h2TriggerSignal = RootIO::GetUniqueOrThrow<TH2D>(filePhiDataInput.get(), folderPath + "h2TriggerSignal", GetName());
    h2TriggerBkgRatio = RootIO::GetUniqueOrThrow<TH2D>(filePhiDataInput.get(), folderPath + "h2TriggerBkgRatio", GetName());

    // The same exposure the WPDG task has, by a different route: this matrix is written
    // cell by cell by PhiFitTask, so a run where every fit failed leaves it present,
    // correctly binned and entirely zero. From there the normalisation is skipped and
    // the spectra come out as raw counts, which is exactly the failure that went
    // unnoticed in the closure - the object is not missing, it is empty.
    RequireNonEmptyTrigger(h2TriggerSignal.get(), std::format("'{}h2TriggerSignal'", folderPath), phiDataName,
                           "It is written by phi_fit_task: check its log for fits that did not "
                           "converge, and that it ran on the production this task is reading.");

    // 3. Associated particles + data loading
    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      Logger::Info("{}: Cache DISABLED. Loading heavy THnSparse data...", GetName());

      std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
      std::unique_ptr<TFile> fileDataInput = RootIO::OpenOrThrow(inputFile, "READ", GetName());

      basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());

      std::unique_ptr<TFile> fileDataMEInput{nullptr};
      if (applyME) {
        std::string inputMEFile = JsonConfig::RequireString(taskConfig, "input_me_file", GetName());
        fileDataMEInput = RootIO::OpenOrThrow(inputMEFile, "READ", GetName());

        basePathDataME = JsonConfig::RequireString(taskConfig, "base_path_me", GetName());
      }

      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
        data.h5DataSignal = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataInput.get(), (baseData + "DataSignal"), GetName());
        data.h5DataSideband = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataInput.get(), (baseData + "DataSideband"), GetName());

        if (applyME) {
          std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;
          data.h5DataMESignal = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataMEInput.get(), (baseDataME + "DataMESignal"), GetName());
          data.h5DataMESideband = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataMEInput.get(), (baseDataME + "DataMESideband"), GetName());
        }

        loadedDataCollection.push_back(std::move(data));
      }
    } else {
      Logger::Info("{}: Cache ENABLED. Skipping THnSparse loading.", GetName());
      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(std::move(data));
      }
    }

    // 4. Output files (opened before the corrections: the cache carries the
    //    binning that everything below is loaded against)
    std::string projMode = useProjectionCache ? "READ" : "UPDATE";
    std::string basePathFinal = JsonConfig::RequireString(taskConfig, "output_dir_final", GetName());
    std::string phiSpectraName = basePathFinal + outputPrefix + "PhiAssocSpectra.root";
    fileOutputSpectra = RootIO::OpenOrThrow(phiSpectraName, "UPDATE", GetName());

    // This run rewrites its whole scheme directory, so whatever a previous one left
    // there - produced when the layout or the object names were different - would sit
    // beside the new objects with nothing to tell them apart. UPDATE and not RECREATE
    // because only this subtree is ours: another 'binning_name', or the other
    // Extract1D/2D, belongs to somebody else and stays.
    RootIO::ClearPath(fileOutputSpectra.get(), {globalCfgs.binningName, SchemeDirName()});

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + outputPrefix + "Phi" + p.name + "DataHistograms.root";
      std::unique_ptr<TFile> fProj = RootIO::OpenOrThrow(fName, projMode.c_str(), GetName());
      filesPhiAssocDataOutput.push_back(std::move(fProj));

      if (doMoreQA) {
        std::string fQAName = basePathProj + outputPrefix + "Phi" + p.name + "QAHistograms.root";
        std::unique_ptr<TFile> fQA = RootIO::OpenOrThrow(fQAName, "RECREATE", GetName());
        filesPhiAssocQAOutput.push_back(std::move(fQA));

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
        throw std::runtime_error("[FATAL] CorrelationTask: Unknown run mode!");
    }
  }

 protected:
  TH1* GetPurityHist(const std::string& particleName, int multBin) override
  {
    if (!applyPurity)
      return nullptr;
    auto it = purityCollection.find(particleName);
    if (it == purityCollection.end())
      throw std::runtime_error(std::format("[FATAL] {}: Missing purity data for particle '{}'", GetName(), particleName));
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

  std::pair<const TAxis*, const TAxis*> TriggerAxes() const override
  {
    return {h2TriggerSignal->GetXaxis(), h2TriggerSignal->GetYaxis()};
  }

 private:
  RunMode runMode{RunMode::Legacy};

  bool applyPurity{false};
  bool useSignalCache{false}, doMoreQA{false};

  std::string purityPrefix{""}, triggerPrefix{""}, outputPrefix{""};

  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  std::map<std::string, LoadedPurity> purityCollection;

  void LoadPurities(const rapidjson::Value& taskConfig)
  {

    std::string purityDir = JsonConfig::RequireString(taskConfig, "input_dir_purity", GetName());
    auto puritySources = JsonConfig::RequireArray(taskConfig, "purity_sources", GetName());

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};
    std::string folderPath = RootIO::MakeDirPath(summaryPath);

    purityCollection.clear();

    int srcIdx = 0;
    for (const auto& src : puritySources) {
      const std::string ctx = std::format("{} purity_sources[{}]", GetName(), srcIdx++);
      std::string name = JsonConfig::RequireString(src, "name", ctx);
      std::string purityKey = JsonConfig::RequireString(src, "purity_key", ctx);
      std::string fileSuffix = JsonConfig::RequireString(src, "file_suffix", ctx);
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
          throw std::runtime_error(std::format("[FATAL] {}: Unknown particle name in 'purity_sources': {}", GetName(), name));
        targetBinning = it->binning;
      }

      std::unique_ptr<TFile> filePurity = RootIO::OpenOrThrow(purityFilePath, "READ", GetName());

      RootIO::PrintProvenance(RootIO::GetOrCreatePath(filePurity.get(), {globalCfgs.binningName, "Provenance"}, true),
                              std::format("purity file '{}'", purityFilePath));

      // Same as for the corrections: the purities are addressed by multiplicity
      // index, and only the stamp says what those indices meant when they were fitted.
      RootIO::RequireMatchingBinningStamp(
        RootIO::GetOrCreatePath(filePurity.get(), {globalCfgs.binningName}, true),
        "binning_mult", multBinning, std::format("purity file '{}'", purityFilePath));

      for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
        std::string hName = std::format("{}h1{}Purity_multBin{}", folderPath, purityKey, i);
        std::unique_ptr<TH1F> h1Pur = RootIO::GetUniqueOrThrow<TH1F>(filePurity.get(), hName, GetName());

        // Verified, NOT rebinned: a purity is a ratio, and merging its bins would
        // sum the ratios. A coarser purity can only be obtained by fitting the
        // merged mass distribution - see 'rebinning_pt' in the purity config.
        const std::string diff = BinningUtils::Compare(targetBinning, BinningUtils::AxisEdges(h1Pur->GetXaxis()),
                                                       "analysis binning (data production)", "purity spectrum");
        if (!diff.empty()) {
          throw std::runtime_error(std::format("[FATAL] CorrelationTask::LoadPurities: '{}' in '{}' "
                                               "is not binned like the data being corrected:\n{}"
                                               "Set 'rebinning_pt' in the purity configuration so the fits are done at this "
                                               "binning, or re-run the production whose axis does not match.",
                                               hName, purityFilePath, diff));
        }

        purity.h1Purity[i] = std::move(h1Pur);
      }

      purityCollection[name] = std::move(purity);
    }
    Logger::Info("{}: Purities loaded successfully.", GetName());
  }

  void RunOptimized()
  {
    Logger::Info("{}: RUNNING OPTIMIZED CORRELATIONS AND QA...", GetName());

    // =========================================================================
    // 0. EFFICIENCY POINTERS SETUP
    // Hoisted to the top so both the Cache L2 branch and the calculation branch
    // can access them to properly normalize the trigger signals.
    // =========================================================================
    const std::vector<std::unique_ptr<TH1>>* phiCorrs{nullptr};
    std::vector<const std::vector<std::unique_ptr<TH1>>*> assocCorrs(assocParticles.size(), nullptr);

    if (!correctionCollection.empty()) {
      auto itPhi = correctionCollection.find("Phi");
      if (itPhi != correctionCollection.end())
        phiCorrs = &itPhi->second.h1Corrections;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        auto it = correctionCollection.find(assocParticles[pIdx].name);
        if (it != correctionCollection.end())
          assocCorrs[pIdx] = &it->second.h1Corrections;
      }
    }

    const std::string dirName = SchemeDirName();
    std::vector<std::string> logicalPath{globalCfgs.binningName, dirName};

    // =========================================================================
    // MODE A: SUPER CACHE MANAGEMENT (LEVEL 2)
    // If active, completely bypasses the CorrelationCalculator and heavy loops.
    // =========================================================================
    if (useSignalCache) {
      Logger::Info("{}: Super Cache (Signal L2) is ON. Skipping Calculator.", GetName());

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
            throw std::runtime_error(std::format("[FATAL] Cache L2 missing: Directory {} not found!", dirName));

          for (int k = 0; k < BinningUtils::NBins(config.binning); k++) {
            std::string cacheName = std::format("h1Phi{}Final{}", config.name, CellSuffix(config, i, k));
            // dynamic_cast, as in RootIO::GetUniqueOrThrow: an object of another class under this
            // name must fail here, with the message below, and not on first use.
            h1PhiAssocNoPtPhi[pIdx][i][k].reset(dynamic_cast<TH1*>(sourceDir->Get(cacheName.c_str())));
            if (!h1PhiAssocNoPtPhi[pIdx][i][k])
              throw std::runtime_error(std::format("[FATAL] Cache L2 missing for: {}. Run with use_signal_cache: false first!", cacheName));
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
    CorrelationCalculator corrCalculator({.mixedEvents = applyME, .fromCache = useProjectionCache, .engine2D = use2DMENormalization, .moreQA = doMoreQA, .meNorm = meNormalization});

    for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
      HistogramUtils::AxisToCut axisToCutMult{.axis = 0, .bins = {i + 1, i + 1}};
      double totalTriggerSignalPerMult = 0.0;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i].get() : nullptr;

      for (int j = 0; j < BinningUtils::NBins(ptPhiBinning); j++) {
        HistogramUtils::AxisToCut axisToCutPtPhi{.axis = 1, .bins = {j + 1, j + 1}};

        double triggerSignal = h2TriggerSignal->GetBinContent(i + 1, j + 1);
        double triggerBkgRatio = h2TriggerBkgRatio->GetBinContent(i + 1, j + 1);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i].get() : nullptr;

          TDirectory* targetDir = ProjectionCacheDir(pIdx, logicalPath);

          TDirectory* currentQADir{nullptr};
          if (doMoreQA && pIdx < filesPhiAssocQAOutput.size()) {
            currentQADir = RootIO::GetOrCreatePath(filesPhiAssocQAOutput[pIdx].get(), logicalPath, false);
          }

          for (int k = 0; k < BinningUtils::NBins(config.binning); k++) {
            HistogramUtils::AxisToCut axisToCutPtAssoc{.axis = 2, .bins = {k + 1, k + 1}};
            std::vector<HistogramUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string baseNameStd = "h1Phi" + config.name + "Data";
            std::string suffix = CellSuffix(config, i, j, k);

            std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
              data, {.axesToCut = axesToCut, .totalEff = totalEff, .triggerBkgRatio = triggerBkgRatio, .histNameBase = baseNameStd + suffix, .ioDir = targetDir, .qaDir = currentQADir, .targetAxis = projectionAxis});

            if (j == 0) {
              std::string accumName = std::format("h1Phi{}DataSignal{}", config.name, CellSuffix(config, i, k));
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
      Logger::Info("Multiplicity bin {} completed. Committing results to Level 2 Cache...", i);

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        TDirectory* targetDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, false);
        if (targetDir)
          targetDir->cd();
        for (int k = 0; k < BinningUtils::NBins(assocParticles[pIdx].binning); k++) {
          if (h1PhiAssocNoPtPhi[pIdx][i][k]) {
            std::string saveName = std::format("h1Phi{}Final{}", assocParticles[pIdx].name, CellSuffix(assocParticles[pIdx], i, k));
            h1PhiAssocNoPtPhi[pIdx][i][k]->SetName(saveName.c_str());
            h1PhiAssocNoPtPhi[pIdx][i][k]->Write(nullptr, TObject::kOverwrite);
          }
        }
      }

      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    }
  }
};

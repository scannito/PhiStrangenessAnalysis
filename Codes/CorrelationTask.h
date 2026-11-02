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
  // RunOptimized builds the engine use_2d_engine asks for; RunLegacy builds the 1D one.
  bool CanBuild2DEngine() const override { return runMode == RunMode::Optimized; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    Logger::Info("{}: INITIALIZING...", GetName());

    InitCommonFlags(taskConfig, globalSettings);

    applyPurity = JsonConfig::RequireBool(taskConfig, "apply_purity", GetName());

    useSignalCache = JsonConfig::OptionalBool(taskConfig, "use_signal_cache", useSignalCache, GetName());
    doMoreQA = JsonConfig::OptionalBool(taskConfig, "do_more_qa", doMoreQA, GetName());
    // Required, with no default: it chooses the engine (CLAUDE.md, Configuration).
    runMode = JsonConfig::RequireEnum<RunMode>(taskConfig, "run_mode",
                                               {{"legacy", RunMode::Legacy}, {"optimized", RunMode::Optimized}}, GetName());
    RequireEngineSettingsSupported();

    // Required, as every key a rename left behind (CLAUDE.md, Configuration): input_prefix and input_output_prefix
    // were their fallbacks until 30 October 2026, and a configuration still using them stops here by name.
    purityPrefix = JsonConfig::RequireString(taskConfig, "purity_prefix", GetName());
    triggerPrefix = JsonConfig::RequireString(taskConfig, "trigger_prefix", GetName());

    // 2. Load Trigger information from PhiFitTask
    std::string basePathProj = JsonConfig::RequireString(taskConfig, "projections_dir", GetName());
    std::string triggerPath = std::format("{}{}{}", basePathProj, triggerPrefix, OutputNames::kPhiDataFile);

    std::unique_ptr<TFile> triggerFile = RootIO::OpenOrThrow(triggerPath, "READ", GetName());

    RootIO::PrintProvenance(triggerFile.get(), globalCfgs.binningName,
                            std::format("trigger file '{}'", triggerPath));

    const std::string folderPath = RootIO::MakeDirPath(OutputNames::SummaryPath(globalCfgs.binningName));

    h2TriggerSignal = RootIO::GetUniqueOrThrow<TH2D>(triggerFile.get(), folderPath + OutputNames::kTriggerSignal, GetName());
    h2TriggerBkgRatio = RootIO::GetUniqueOrThrow<TH2D>(triggerFile.get(), folderPath + OutputNames::kTriggerBkgRatio, GetName());

    // The same exposure the WPDG task has, by a different route: this matrix is written
    // cell by cell by PhiFitTask, so a run where every fit failed leaves it present,
    // correctly binned and entirely zero. From there the normalisation is skipped and
    // the spectra come out as raw counts, which is exactly the failure that went
    // unnoticed in the closure - the object is not missing, it is empty.
    RequireNonEmptyTrigger(h2TriggerSignal.get(), std::format("'{}{}'", folderPath, OutputNames::kTriggerSignal), triggerPath,
                           "It is written by phi_fit_task: check its log for fits that did not "
                           "converge, and that it ran on the production this task is reading.");

    // 3. Associated particles + data loading
    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      Logger::Info("{}: Cache DISABLED. Loading heavy THnSparse data...", GetName());

      std::string inputPath = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
      std::unique_ptr<TFile> inputFile = RootIO::OpenOrThrow(inputPath, "READ", GetName());

      basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());

      std::unique_ptr<TFile> inputMEFile{nullptr};
      if (applyME) {
        std::string inputMEPath = JsonConfig::RequireString(taskConfig, "input_me_file", GetName());
        inputMEFile = RootIO::OpenOrThrow(inputMEPath, "READ", GetName());

        basePathDataME = JsonConfig::RequireString(taskConfig, "base_path_me", GetName());
      }

      for (const auto& config : assocParticles) {
        AnalysisStructures::LoadedAssocData data;
        data.name = config.name;
        std::string baseData = std::format("{}{}/h5Phi{}", basePathData, config.dirName, config.name);
        data.h5DataSignal = RootIO::GetUniqueOrThrow<THnSparseF>(inputFile.get(), std::format("{}DataSignal", baseData), GetName());
        data.h5DataSideband = RootIO::GetUniqueOrThrow<THnSparseF>(inputFile.get(), std::format("{}DataSideband", baseData), GetName());

        if (applyME) {
          std::string baseDataME = std::format("{}{}/h5Phi{}", basePathDataME, config.dirName, config.name);
          data.h5DataMESignal = RootIO::GetUniqueOrThrow<THnSparseF>(inputMEFile.get(), std::format("{}DataMESignal", baseDataME), GetName());
          data.h5DataMESideband = RootIO::GetUniqueOrThrow<THnSparseF>(inputMEFile.get(), std::format("{}DataMESideband", baseDataME), GetName());
        }

        loadedDataCollection.push_back(std::move(data));
      }
    } else {
      Logger::Info("{}: Cache ENABLED. Skipping THnSparse loading.", GetName());
      for (const auto& config : assocParticles) {
        AnalysisStructures::LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(std::move(data));
      }
    }

    // 4. Output files (opened before the corrections: the cache carries the
    //    binning that everything below is loaded against)
    OpenOutputFiles(taskConfig, doMoreQA);
    ResolveBinningAndCache();

    // 5. Corrections / Purity / Extrapolation, rebinned onto the binning that
    //    was just resolved from the data
    InitCorrectionsAndExtrapolation(taskConfig);
    if (applyPurity)
      LoadPurities(taskConfig);

    SetupTrendHistograms();
    ClearOwnSection();
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
        throw std::runtime_error(std::format("[FATAL] {}: Unknown run mode!", GetName()));
    }
  }

 protected:
  TH1* GetPurityHist(const std::string& particle, int multBin) override
  {
    if (!applyPurity)
      return nullptr;
    auto it = purityCollection.find(particle);
    if (it == purityCollection.end())
      throw std::runtime_error(std::format("[FATAL] {}: Missing purity data for particle '{}'", GetName(), particle));
    return it->second.h1Purity[multBin].get();
  }

  // The error is the phi fit's (PhiFitTask writes it with the yield).
  std::pair<double, double> GetTriggerSignal(int multBin, int ptPhiBin) override
  {
    return HistogramUtils::BinValueAndError(h2TriggerSignal.get(), multBin, ptPhiBin);
  }

  double GetTriggerBkgRatio(int multBin, int ptPhiBin) override
  {
    return h2TriggerBkgRatio->GetBinContent(BinningUtils::RootBin(multBin), BinningUtils::RootBin(ptPhiBin));
  }

  std::pair<const TAxis*, const TAxis*> TriggerAxes() const override
  {
    return {h2TriggerSignal->GetXaxis(), h2TriggerSignal->GetYaxis()};
  }

 private:
  RunMode runMode{RunMode::Legacy}; // always set from run_mode, required: initialised only to be determinate

  bool applyPurity{false};
  bool useSignalCache{false}, doMoreQA{false};

  std::string purityPrefix{""}, triggerPrefix{""};

  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  std::map<std::string, AnalysisStructures::LoadedPurity> purityCollection;

  void LoadPurities(const rapidjson::Value& taskConfig)
  {

    std::string purityDir = JsonConfig::RequireString(taskConfig, "input_dir_purity", GetName());
    auto puritySources = JsonConfig::RequireArray(taskConfig, "purity_sources", GetName());

    const std::string folderPath = RootIO::MakeDirPath(OutputNames::SummaryPath(globalCfgs.binningName));

    purityCollection.clear();

    // One file for every particle of a purity production, named by OutputNames: opened, recorded and checked once.
    const std::string purityPath = std::format("{}{}{}", purityDir, purityPrefix, OutputNames::kPurityFile);
    std::unique_ptr<TFile> purityFile = RootIO::OpenOrThrow(purityPath, "READ", GetName());
    RootIO::PrintProvenance(purityFile.get(), globalCfgs.binningName, std::format("purity file '{}'", purityPath));

    RootIO::RequireMatchingBinningStamp(RootIO::FindPath(purityFile.get(), {globalCfgs.binningName}), RootIO::kStampMult,
                                        multBinning, std::format("purity file '{}'", purityPath));

    int srcIdx = 0;
    for (const auto& src : puritySources) {
      const std::string ctx = std::format("{} purity_sources[{}]", GetName(), srcIdx++);
      std::string particle = JsonConfig::RequireString(src, "name", ctx);
      std::string purityKey = JsonConfig::RequireString(src, "purity_key", ctx);

      AnalysisStructures::LoadedPurity purity;
      purity.name = particle;
      purity.h1Purity.resize(BinningUtils::NBins(multBinning));

      const std::vector<double>& targetBinning = PtBinningOf(particle, ctx);


      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        std::string hName = folderPath + OutputNames::Purity(purityKey, OutputNames::MultTag(multBinning, multBin));
        std::unique_ptr<TH1F> h1Pur = RootIO::GetUniqueOrThrow<TH1F>(purityFile.get(), hName, GetName());

        // Verified, NOT rebinned: a purity is a ratio, and merging its bins would
        // sum the ratios. A coarser purity can only be obtained by fitting the
        // merged mass distribution - see 'rebinning_pt' in the purity config.
        const std::string diff = BinningUtils::Compare(targetBinning, BinningUtils::AxisEdges(h1Pur->GetXaxis()),
                                                       "analysis binning (data production)", "purity spectrum");
        if (!diff.empty()) {
          throw std::runtime_error(std::format("[FATAL] {}: '{}' in '{}' "
                                               "is not binned like the data being corrected:\n{}"
                                               "Set 'rebinning_pt' in the purity configuration so the fits are done at this "
                                               "binning, or re-run the production whose axis does not match.",
                                               GetName(), hName, purityPath, diff));
        }

        purity.h1Purity[multBin] = std::move(h1Pur);
      }

      purityCollection[particle] = std::move(purity);
    }
    Logger::Info("{}: Purities loaded successfully.", GetName());
  }

  void RunOptimized()
  {
    Logger::Info("{}: RUNNING OPTIMIZED CORRELATIONS AND QA...", GetName());

    const std::vector<std::string> logicalPath = SectionPath();

    // =========================================================================
    // MODE A: SUPER CACHE MANAGEMENT (LEVEL 2)
    // If active, completely bypasses the CorrelationCalculator and heavy loops.
    // =========================================================================
    if (useSignalCache) {
      Logger::Info("{}: Super Cache (Signal L2) is ON. Skipping Calculator.", GetName());
      // Here, where the numbers are at risk, and not in the run that cannot update the cache.
      Logger::Warn("{}: use_signal_cache reads the signals an earlier run cached, and nothing records which corrections, "
                   "purities and background ratios that run applied: they are not verified (DESIGN_NOTES, \"The signal "
                   "cache records nothing\").",
                   GetName());

      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          TDirectory* sourceDir = RootIO::FindPath(projectionFiles[pIdx].get(), logicalPath);
          if (!sourceDir)
            throw std::runtime_error(std::format("[FATAL] {}: use_signal_cache is on, but '{}' has no '{}'. Run with "
                                                 "use_signal_cache false first.",
                                                 GetName(), projectionFiles[pIdx]->GetName(), RootIO::MakeDirPath(logicalPath)));

          const std::string ctx = std::format("{} (use_signal_cache; run with it false first if the signal was never cached)", GetName());
          for (int ptAssocBin : BinningUtils::BinIndices(config.binning))
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin] = RootIO::GetUniqueOrThrow<TH1>(
              sourceDir, std::format("h1Phi{}Final{}", config.name, CellSuffix(config, multBin, ptAssocBin)), ctx);
        }
        GenerateSpectraAndTrends(multBin);
      }
      return;
    }

    // =========================================================================
    // MODE B: STANDARD CALCULATION (If L2 Cache is disabled)
    // The CorrelationCalculator is invoked here (managing L1 Cache internally)
    // =========================================================================
    CorrelationCalculator corrCalculator(CalculatorOptions(doMoreQA));

    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      for (int ptPhiBin : BinningUtils::BinIndices(ptPhiBinning)) {
        double triggerBkgRatio = GetTriggerBkgRatio(multBin, ptPhiBin);
        double phiCorrection = Correction("Phi", multBin, ptPhiBin);

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];

          TDirectory* targetDir = ProjectionCacheDir(pIdx, logicalPath);

          TDirectory* currentQADir{nullptr};
          if (doMoreQA && pIdx < qaFiles.size()) {
            currentQADir = RootIO::MakePath(qaFiles[pIdx].get(), logicalPath);
          }

          for (int ptAssocBin : BinningUtils::BinIndices(config.binning)) {
            const std::vector<HistogramUtils::AxisToCut> axesToCut = CellCuts(multBin, ptPhiBin, ptAssocBin);

            double assocCorrection = Correction(config.name, multBin, ptAssocBin);
            double totalCorrection = phiCorrection * assocCorrection;

            std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
              data, {.axesToCut = axesToCut, .totalCorrection = totalCorrection, .triggerBkgRatio = triggerBkgRatio, .histNameBase = std::format("h1Phi{}Data{}", config.name, CellSuffix(config, multBin, ptPhiBin, ptAssocBin)), .ioDir = targetDir, .qaDir = currentQADir});

            if (ptPhiBin == 0) {
              std::string accumName = std::format("h1Phi{}DataSignal{}", config.name, CellSuffix(config, multBin, ptAssocBin));
              h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin].reset(static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str())));
              h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->Add(h1FinalSignal.get());
            }
          }
        }
      }

      // =======================================================================
      // BATCH WRITE: SAVING AGGREGATED SIGNALS FOR L2 CACHE
      // Written only once per multiplicity bin after trigger aggregation; not at all when the projection files are
      // read-only, and the cache then keeps what the run that last wrote it put there.
      // =======================================================================
      if (!ProjectionFilesReadOnly()) {
        Logger::Info("Multiplicity bin {} completed. Committing results to Level 2 Cache...", multBin);
        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          RootIO::MakePath(projectionFiles[pIdx].get(), logicalPath)->cd();
          for (int ptAssocBin : BinningUtils::BinIndices(assocParticles[pIdx].binning)) {
            if (h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]) {
              std::string saveName = std::format("h1Phi{}Final{}", assocParticles[pIdx].name, CellSuffix(assocParticles[pIdx], multBin, ptAssocBin));
              h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->SetName(saveName.c_str());
              h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->Write(nullptr, TObject::kOverwrite);
            }
          }
        }
      }

      GenerateSpectraAndTrends(multBin);
    }
  }
};

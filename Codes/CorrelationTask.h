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
  // RunOptimized builds the engine use_2d_me_normalization asks for; RunLegacy builds the 1D one.
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
    std::string basePathProj = JsonConfig::RequireString(taskConfig, "projections_dir", GetName());
    std::string phiDataName = basePathProj + triggerPrefix + OutputNames::kPhiDataFile;

    std::unique_ptr<TFile> filePhiDataInput = RootIO::OpenOrThrow(phiDataName, "READ", GetName());

    RootIO::PrintProvenance(filePhiDataInput.get(), globalCfgs.binningName,
                            std::format("trigger file '{}'", phiDataName));

    const std::string folderPath = RootIO::MakeDirPath(OutputNames::SummaryPath(globalCfgs.binningName));

    h2TriggerSignal = RootIO::GetUniqueOrThrow<TH2D>(filePhiDataInput.get(), folderPath + OutputNames::kTriggerSignal, GetName());
    h2TriggerBkgRatio = RootIO::GetUniqueOrThrow<TH2D>(filePhiDataInput.get(), folderPath + OutputNames::kTriggerBkgRatio, GetName());

    // The same exposure the WPDG task has, by a different route: this matrix is written
    // cell by cell by PhiFitTask, so a run where every fit failed leaves it present,
    // correctly binned and entirely zero. From there the normalisation is skipped and
    // the spectra come out as raw counts, which is exactly the failure that went
    // unnoticed in the closure - the object is not missing, it is empty.
    RequireNonEmptyTrigger(h2TriggerSignal.get(), std::format("'{}{}'", folderPath, OutputNames::kTriggerSignal), phiDataName,
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

    // This run rewrites its whole section directory, so whatever a previous one left
    // there - produced when the layout or the object names were different - would sit
    // beside the new objects with nothing to tell them apart. UPDATE and not RECREATE
    // because only this subtree is ours: another 'binning_name', or the other
    // Extract1D/2D, belongs to somebody else and stays.
    RootIO::ClearPath(fileOutputSpectra.get(), SectionPath());

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

  std::string purityPrefix{""}, triggerPrefix{""}, outputPrefix{""};

  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  std::map<std::string, LoadedPurity> purityCollection;

  void LoadPurities(const rapidjson::Value& taskConfig)
  {

    std::string purityDir = JsonConfig::RequireString(taskConfig, "input_dir_purity", GetName());
    auto puritySources = JsonConfig::RequireArray(taskConfig, "purity_sources", GetName());

    const std::string folderPath = RootIO::MakeDirPath(OutputNames::SummaryPath(globalCfgs.binningName));

    purityCollection.clear();

    // One file for every particle of a purity production, named by OutputNames: opened, recorded and checked once.
    const std::string purityFilePath = purityDir + purityPrefix + OutputNames::kPurityFile;
    std::unique_ptr<TFile> filePurity = RootIO::OpenOrThrow(purityFilePath, "READ", GetName());
    RootIO::PrintProvenance(filePurity.get(), globalCfgs.binningName, std::format("purity file '{}'", purityFilePath));

    RootIO::RequireMatchingBinningStamp(RootIO::GetOrCreatePath(filePurity.get(), {globalCfgs.binningName}, true), RootIO::kStampMult,
                                        multBinning, std::format("purity file '{}'", purityFilePath));

    int srcIdx = 0;
    for (const auto& src : puritySources) {
      const std::string ctx = std::format("{} purity_sources[{}]", GetName(), srcIdx++);
      std::string particle = JsonConfig::RequireString(src, "name", ctx);
      std::string purityKey = JsonConfig::RequireString(src, "purity_key", ctx);

      LoadedPurity purity;
      purity.name = particle;
      purity.h1Purity.resize(BinningUtils::NBins(multBinning));

      std::vector<double> targetBinning;
      if (particle == "Phi") {
        targetBinning = ptPhiBinning;
      } else {
        auto it = std::ranges::find_if(assocParticles, [&](const AssocParticleConfig& p) { return p.name == particle; });
        if (it == assocParticles.end())
          throw std::runtime_error(std::format("[FATAL] {}: Unknown particle name in 'purity_sources': {}", GetName(), particle));
        targetBinning = it->binning;
      }


      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        std::string hName = folderPath + OutputNames::Purity(purityKey, OutputNames::MultTag(multBinning, multBin));
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

      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          TDirectory* sourceDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, true);
          if (!sourceDir)
            throw std::runtime_error(std::format("[FATAL] Cache L2 missing: Directory {} not found!", RootIO::MakeDirPath(logicalPath)));

          for (int ptAssocBin : BinningUtils::BinIndices(config.binning)) {
            std::string cacheName = std::format("h1Phi{}Final{}", config.name, CellSuffix(config, multBin, ptAssocBin));
            // dynamic_cast, as in RootIO::GetUniqueOrThrow: an object of another class under this
            // name must fail here, with the message below, and not on first use.
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin].reset(dynamic_cast<TH1*>(sourceDir->Get(cacheName.c_str())));
            if (!h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin])
              throw std::runtime_error(std::format("[FATAL] Cache L2 missing for: {}. Run with use_signal_cache: false first!", cacheName));
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->SetDirectory(0);
          }
        }
        GenerateSpectraAndTrends(multBin);
      }
      return;
    }

    // =========================================================================
    // MODE B: STANDARD CALCULATION (If L2 Cache is disabled)
    // The CorrelationCalculator is invoked here (managing L1 Cache internally)
    // =========================================================================
    CorrelationCalculator corrCalculator({.mixedEvents = applyME, .fromCache = useProjectionCache, .engine2D = Engine2D(), .moreQA = doMoreQA, .meNorm = meNormalization, .targetAxis = projectionAxis});

    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      for (int ptPhiBin : BinningUtils::BinIndices(ptPhiBinning)) {
        double triggerBkgRatio = GetTriggerBkgRatio(multBin, ptPhiBin);
        double phiCorrection = Correction("Phi", multBin, ptPhiBin);

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];

          TDirectory* targetDir = ProjectionCacheDir(pIdx, logicalPath);

          TDirectory* currentQADir{nullptr};
          if (doMoreQA && pIdx < filesPhiAssocQAOutput.size()) {
            currentQADir = RootIO::GetOrCreatePath(filesPhiAssocQAOutput[pIdx].get(), logicalPath, false);
          }

          for (int ptAssocBin : BinningUtils::BinIndices(config.binning)) {
            const std::vector<HistogramUtils::AxisToCut> axesToCut = CellCuts(multBin, ptPhiBin, ptAssocBin);

            double assocCorrection = Correction(config.name, multBin, ptAssocBin);
            double totalCorrection = phiCorrection * assocCorrection;

            std::string baseNameStd = "h1Phi" + config.name + "Data";
            std::string suffix = CellSuffix(config, multBin, ptPhiBin, ptAssocBin);

            std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
              data, {.axesToCut = axesToCut, .totalCorrection = totalCorrection, .triggerBkgRatio = triggerBkgRatio, .histNameBase = baseNameStd + suffix, .ioDir = targetDir, .qaDir = currentQADir});

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
      // Written only once per multiplicity bin after trigger aggregation
      // =======================================================================
      Logger::Info("Multiplicity bin {} completed. Committing results to Level 2 Cache...", multBin);

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        TDirectory* targetDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, false);
        if (targetDir)
          targetDir->cd();
        for (int ptAssocBin : BinningUtils::BinIndices(assocParticles[pIdx].binning)) {
          if (h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]) {
            std::string saveName = std::format("h1Phi{}Final{}", assocParticles[pIdx].name, CellSuffix(assocParticles[pIdx], multBin, ptAssocBin));
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->SetName(saveName.c_str());
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->Write(nullptr, TObject::kOverwrite);
          }
        }
      }

      GenerateSpectraAndTrends(multBin);
    }
  }
};

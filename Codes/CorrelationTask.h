#pragma once

#include "CorrelationTaskBase.h"
#include "IONames.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "RootIOHelpers.h"
#include "TrainConfig.h"

#include "TH1D.h"
#include "TParameter.h"

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
    std::string triggerPath = std::format("{}{}{}", basePathProj, triggerPrefix, IONames::kPhiDataFile);

    std::unique_ptr<TFile> triggerFile = RootIO::OpenOrThrow(triggerPath, "READ", GetName());

    RootIO::PrintProvenance(triggerFile.get(), globalCfgs.binningName,
                            std::format("trigger file '{}'", triggerPath));

    const std::vector<std::string> summaryPath = IONames::SummaryPath(globalCfgs.binningName);
    const std::string triggerSignalPath = RootIO::ObjectPath(summaryPath, IONames::kTriggerSignal);

    h2TriggerSignal = RootIO::GetUniqueOrThrow<TH2D>(triggerFile.get(), triggerSignalPath, GetName());
    h2TriggerBkgRatio = RootIO::GetUniqueOrThrow<TH2D>(triggerFile.get(), RootIO::ObjectPath(summaryPath, IONames::kTriggerBkgRatio), GetName());
    // How phi_fit_task's input weighted the phi online, compared with the pairs by CheckOnlineCorrections
    const std::optional<OnlineWeighting> triggerPhi = ReadTriggerPhiWeighting(triggerFile.get(), triggerPath, summaryPath);

    // The same exposure the WPDG task has, by a different route: this matrix is written
    // cell by cell by PhiFitTask, so a run where every fit failed leaves it present,
    // correctly binned and entirely zero. From there the normalisation is skipped and
    // the spectra come out as raw counts, which is exactly the failure that went
    // unnoticed in the closure - the object is not missing, it is empty.
    RequireNonEmptyTrigger(h2TriggerSignal.get(), std::format("'{}'", triggerSignalPath), triggerPath,
                           "It is written by phi_fit_task: check its log for fits that did not "
                           "converge, and that it ran on the production this task is reading.");

    // 3. Associated particles, the corrections checked against how the train ran, data loading
    InitAssocParticles(taskConfig);
    InitCorrectionsAndCheckOnline(taskConfig, triggerPhi);
    // Here, before the containers are loaded: it needs only the configuration and the train record, and a block that
    // cannot be right stops before the expensive part. The comparison itself waits for the binnings, in LoadPurities.
    const PurityWindows purityWindows = applyPurity ? PurityWindowsToCompare(taskConfig) : PurityWindows{};

    LoadAssocData(taskConfig,
                  {.signal = "DataSignal", .sideband = "DataSideband", .meSignal = "DataMESignal", .meSideband = "DataMESideband"});

    // 4. Output files (opened before the corrections: the cache carries the
    //    binning that everything below is loaded against)
    OpenOutputFiles(taskConfig, doMoreQA);
    ResolveBinningAndCache();

    // 5. Corrections / Purity / Extrapolation, rebinned onto the binning that
    //    was just resolved from the data
    LoadCorrectionsAndExtrapolation(taskConfig);
    if (applyPurity)
      LoadPurities(taskConfig, purityWindows);

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

  // The fact PhiFitTask writes in its summary (IONames::kPhiWeightedOnline). Absent from every file written before
  // 14 November 2026, and when phi_fit_task's input had no train configuration: a warning, the comparison skipped.
  std::optional<OnlineWeighting> ReadTriggerPhiWeighting(TFile* triggerFile, const std::string& triggerPath,
                                                         const std::vector<std::string>& summaryPath) const
  {
    std::unique_ptr<TObject> object(triggerFile->Get(RootIO::ObjectPath(summaryPath, IONames::kPhiWeightedOnline).c_str()));
    if (!object) {
      Logger::Warn("{}: the trigger file '{}' does not say whether its phi was weighted online, so the triggers are not "
                   "compared with the pairs. Re-run phi_fit_task on it (after Macros/downloadHYOutput.py if its input "
                   "has no train configuration).",
                   GetName(), triggerPath);
      return std::nullopt;
    }
    const auto* fact = dynamic_cast<const TParameter<bool>*>(object.get());
    if (!fact) {
      throw std::runtime_error(std::format("[FATAL] {}: '{}' in '{}' is a {}, not the TParameter<bool> phi_fit_task writes.",
                                           GetName(), IONames::kPhiWeightedOnline, triggerPath, object->ClassName()));
    }
    return OnlineWeighting{fact->GetVal(), std::format("trigger file '{}'", triggerPath)};
  }

  std::map<std::string, AnalysisStructures::LoadedPurity> purityCollection;

  // The purity sources to compare with the online selection, by species, with the window it was kept in online.
  using PurityWindows = std::map<std::string, TrainConfig::Sourced<AnalysisStructures::Range>>;

  void LoadPurities(const rapidjson::Value& taskConfig, const PurityWindows& windowsToCompare)
  {

    std::string purityDir = JsonConfig::RequireString(taskConfig, "input_dir_purity", GetName());
    auto puritySources = JsonConfig::RequireArray(taskConfig, "purity_sources", GetName());

    const std::vector<std::string> summaryPath = IONames::SummaryPath(globalCfgs.binningName);

    purityCollection.clear();

    // One file for every particle of a purity production, named by IONames: opened, recorded and checked once.
    const std::string purityPath = std::format("{}{}{}", purityDir, purityPrefix, IONames::kPurityFile);
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
        std::string hName = RootIO::ObjectPath(summaryPath, IONames::Purity(purityKey, IONames::MultTag(multBinning, multBin)));
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

      if (const auto window = windowsToCompare.find(particle); window != windowsToCompare.end()) {
        CheckPurityWindow(purityFile.get(), purityPath, summaryPath, purityKey, targetBinning, particle, window->second);
      }

      purityCollection[particle] = std::move(purity);
    }
    Logger::Info("{}: Purities loaded successfully.", GetName());
  }

  // A purity describes the candidates in the mass window it was integrated in: applied to pairs kept in another
  // window, or in none, it corrects the wrong ones (DESIGN_NOTES.md section 12). What each purity source becomes, by
  // what is known of its species online (onlineMassWindows says what each case means):
  //  - not a particle of this block: the run stops;
  //  - no mass window to have (the pion): a warning, not compared;
  //  - not known (no train configuration): a warning, not compared;
  //  - known, no window: the run stops;
  //  - known, a window: returned, for CheckPurityWindow.
  PurityWindows PurityWindowsToCompare(const rapidjson::Value& taskConfig) const
  {
    PurityWindows windows;
    int srcIdx = 0;
    for (const auto& src : JsonConfig::RequireArray(taskConfig, "purity_sources", GetName())) {
      const std::string ctx = std::format("{} purity_sources[{}]", GetName(), srcIdx++);
      const std::string particle = JsonConfig::RequireString(src, "name", ctx);
      RequireKnownParticle(particle, ctx);
      if (!TrainConfig::FindMassWindowOptions(particle)) {
        Logger::Warn("{}: the purity of {} is not compared with its online selection (not a mass window): "
                     "not implemented yet.",
                     GetName(), particle);
        continue;
      }
      if (!onlineMassWindows) {
        Logger::Warn("{}: the purity of {} is not compared with its online mass window: not known (no train "
                     "configuration).",
                     GetName(), particle);
        continue;
      }
      const auto& online = onlineMassWindows->at(particle);
      if (!online.value) {
        throw std::runtime_error(std::format("[FATAL] {}: the purity of {} is applied, but the correlation device counted "
                                             "every mass of it ({}): a purity integrated in a window describes none of "
                                             "the pairs outside it. Switch 'apply_purity' off, or use an input whose "
                                             "device kept {} in its signal region.",
                                             ctx, particle, online.source, particle));
      }
      windows.emplace(particle, TrainConfig::Sourced<AnalysisStructures::Range>{*online.value, online.source});
    }
    return windows;
  }

  // The mass window each bin of a purity was integrated in, stamped by PurityTask next to it (two histograms per class,
  // one per edge), against the window the species was kept in online. Every cell of every class is compared, and the run stops once, naming all those
  // that differ. A purity file without the stamp, produced before it existed, is warned about and not compared.
  void CheckPurityWindow(TFile* purityFile, const std::string& purityPath, const std::vector<std::string>& summaryPath,
                         const std::string& purityKey, const std::vector<double>& ptBinning, const std::string& particle,
                         const TrainConfig::Sourced<AnalysisStructures::Range>& online) const
  {
    auto edgeName = [&](int multBin, IONames::WindowEdge edge) {
      return IONames::PurityWindow(purityKey, IONames::MultTag(multBinning, multBin), edge);
    };
    auto readEdge = [&](int multBin, IONames::WindowEdge edge) {
      return RootIO::GetUniqueOrThrow<TH1D>(purityFile, RootIO::ObjectPath(summaryPath, edgeName(multBin, edge)), GetName());
    };

    // Looked for once: PurityTask writes the stamps with the purities, after clearing its subtree, so a file has them
    // for every class or for none.
    if (!RootIO::FindPath(purityFile, summaryPath)->GetKey(edgeName(0, IONames::WindowEdge::Low).c_str())) {
      Logger::Warn("{}: '{}' has no record of the mass window the purity of {} was integrated in: it is not compared "
                   "with the online one. Re-run purity_task to have it.",
                   GetName(), purityPath, particle);
      return;
    }

    std::string differing;
    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      const std::unique_ptr<TH1D> low = readEdge(multBin, IONames::WindowEdge::Low);
      const std::unique_ptr<TH1D> high = readEdge(multBin, IONames::WindowEdge::High);
      for (int ptBin : BinningUtils::BinIndices(ptBinning)) {
        const AnalysisStructures::Range integrated{HistogramUtils::BinValueAndError(low.get(), ptBin).first,
                                                   HistogramUtils::BinValueAndError(high.get(), ptBin).first};
        if (!AnalysisStructures::SameWindow(integrated, online.value)) {
          differing += std::format("  {}: {}\n", IONames::CellTag(multBinning, multBin, ptBinning, ptBin),
                                   TrainConfig::DescribeMassWindow(integrated));
        }
      }
    }
    if (!differing.empty()) {
      throw std::runtime_error(std::format("[FATAL] {}: the purity of {} ('{}') was integrated in mass windows other than "
                                           "the one {} was kept in online, {} ({}):\n{}"
                                           "Set the signal window of '{}' in the fit configuration of the purity production "
                                           "to the online one and re-run it, or use an input selected in the window of the purity.",
                                           GetName(), particle, purityPath, particle, TrainConfig::DescribeMassWindow(online.value),
                                           online.source, differing, purityKey));
    }
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
      AccumulateSignals(corrCalculator, multBin);

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

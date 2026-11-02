#pragma once

#include "CorrelationTaskBase.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "RootIOHelpers.h"

#include "TFile.h"
#include "TH1D.h"
#include "TH3F.h"

#include <format>
#include <memory>
#include <stdexcept>
#include <string>

class CorrelationWPDGTask : public CorrelationTaskBase
{
 public:
  std::string GetName() const override { return "correlation_wpdg_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    Logger::Info("{}: INITIALIZING...", GetName());

    InitCommonFlags(taskConfig, globalSettings);
    // This task always runs RunLegacy (1D engine): CanBuild2DEngine keeps the base's false.
    RequireEngineSettingsSupported();

    isPureGen = JsonConfig::RequireBool(taskConfig, "is_pure_gen", GetName());

    // 2. Load Data

    std::string inputPath = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
    std::unique_ptr<TFile> inputFile = RootIO::OpenOrThrow(inputPath, "READ", GetName());

    basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());

    if (!isPureGen) {
      h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(inputFile.get(), std::format("{}phi/h3PhiData", basePathData), GetName());
    } else {
      h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(inputFile.get(), std::format("{}phi/h3PhiMCClosureGen", basePathData), GetName());
    }

    // Checked here rather than left to the guard in GenerateSpectraAndTrends, which is
    // correct but only fires after every projection has been built: this fails in
    // seconds, and it can name the O2 process function to enable, which that guard
    // cannot because by then the container is out of scope.
    RequireNonEmptyTrigger(
      h3PhiData.get(),
      std::format("'{}phi/{}'", basePathData, isPureGen ? "h3PhiMCClosureGen" : "h3PhiData"),
      inputPath,
      isPureGen
        ? "It is filled by the O2 process function 'processMCGenClosure' - check it was enabled "
          "in the production this file comes from. Note that 'h3PhiMCGen' is a different "
          "container, filled by 'processParticleEfficiency', and is empty in a closure production."
        : "Check that the production this file comes from actually reconstructed phi candidates.");

    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      Logger::Info("{}: Cache DISABLED. Loading heavy THnSparse data...", GetName());

      std::unique_ptr<TFile> inputMEFile{nullptr};
      if (applyME) {
        std::string inputMEPath = JsonConfig::RequireString(taskConfig, "input_me_file", GetName());
        inputMEFile = RootIO::OpenOrThrow(inputMEPath, "READ", GetName());

        basePathDataME = JsonConfig::RequireString(taskConfig, "base_path_me", GetName());
      }

      for (const auto& config : assocParticles) {
        AnalysisStructures::LoadedAssocData data;
        data.name = config.name;
        data.h5DataSignal = RootIO::GetUniqueOrThrow<THnSparseF>(
          inputFile.get(), std::format("{}{}/h5Phi{}{}", basePathData, config.dirName, config.name, isPureGen ? "ClosureMCGen" : "DataSignal"), GetName());

        if (applyME) {
          data.h5DataMESignal = RootIO::GetUniqueOrThrow<THnSparseF>(
            inputMEFile.get(), std::format("{}{}/h5Phi{}{}", basePathDataME, config.dirName, config.name, isPureGen ? "ClosureMCGenME" : "DataMESignal"), GetName());
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
    OpenOutputFiles(taskConfig, false);
    ResolveBinningAndCache();

    // 5. Corrections / Extrapolation, rebinned onto the binning just resolved
    InitCorrectionsAndExtrapolation(taskConfig);

    SetupTrendHistograms();
    ClearOwnSection();
  }

  void Run() override
  {
    // WPDG uses the shared, simple pass for now — no L2 cache needed here yet.
    RunLegacy();
  }

 protected:
  // One expression for both modes, which needs saying carefully: the two containers do
  // NOT hold the same thing on their third axis.
  //
  //   h3PhiData    (mult, pT, M_inv)  200 bins over [0.95, 1.2]
  //   h3PhiMCGen   (mult, pT, y)       20 bins over [-0.5, 0.5]
  //
  // So this is not one quantity computed twice. It is a count over the whole third
  // axis, which happens to be the right count in both cases for INDEPENDENT reasons:
  // [0.9, 1.2] is the mass window the candidates were filled in, and |y| < 0.5 is the
  // acceptance of the analysis. Both are the trigger count under the selection that
  // container already carries. That coincidence is written down because relying on it
  // silently is how the two paths came to be maintained as if they were the same.
  //
  // What this replaces: the pure-gen branch read its trigger from a cached
  // Project3D("yx") indexed as (mult, ptPhi). Which axis of a TH3 becomes X of the
  // projection is a ROOT convention, not something the code stated - and with 10
  // multiplicity bins against 7 trigger-pT bins, getting it the wrong way round sends
  // multiplicity indices 8, 9 and 10 into the overflow of a 7-bin axis. Those classes
  // then get a trigger count of zero, the normalisation in GenerateSpectraAndTrends is
  // skipped, and the "per-trigger yields" come out as raw counts. Naming both bin
  // ranges explicitly, as the reconstructed branch always did, removes the convention
  // from the problem.
  //
  // VerifyTriggerAxes() could not catch it: it checks the axes of h3PhiData, which were
  // always right. The projected TH2 was the object actually indexed and nothing looked
  // at it - an object addressed by bin index whose axes were never compared with what
  // the consumer assumed, which is the one thing this framework exists to prevent.
  //
  // The cell is integrated in place, all three bin ranges named; until 27 October 2026 it was projected first and
  // the projection integrated. The two counts are identical, cell by cell (the 70 cells of h3PhiData and of
  // h3PhiMCClosureGen in the 2024 closure files, exactly).
  std::pair<double, double> GetTriggerSignal(int multBin, int ptPhiBin) override
  {
    // Bins 1..N of the third axis, so its under/overflow is excluded. Deliberate on
    // both paths: outside [0.9, 1.2] is not a phi candidate, and outside |y| < 0.5 is
    // outside the acceptance the spectra are normalised to.
    const auto mult = BinningUtils::BinRange::Single(multBin), ptPhi = BinningUtils::BinRange::Single(ptPhiBin);
    return HistogramUtils::IntegralAndErrorInBins(h3PhiData.get(), mult, ptPhi, {1, h3PhiData->GetNbinsZ()});
  }
  // GetTriggerBkgRatio not overridden: base default (0.0) is correct for WPDG.

  std::pair<const TAxis*, const TAxis*> TriggerAxes() const override
  {
    return {h3PhiData->GetXaxis(), h3PhiData->GetYaxis()};
  }

 private:
  bool isPureGen{false};

  std::unique_ptr<TH3F> h3PhiData;
};

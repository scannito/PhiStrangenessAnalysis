#pragma once

#include "CorrelationTaskBase.h"
#include "JsonConfigHelpers.h"
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
    std::cout << "[INFO] CorrelationWPDGTask: INITIALIZING..." << std::endl;

    InitCommonFlags(taskConfig, globalSettings);

    isPureGen = JsonConfig::RequireBool(taskConfig, "is_pure_gen", GetName());

    // 2. Load Data

    std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
    std::unique_ptr<TFile> fileDataInput = RootIO::OpenOrThrow(inputFile, "READ", "CorrelationWPDGTask");

    basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());

    if (!isPureGen) {
      h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiData", "CorrelationWPDGTask");
    } else {
      h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiMCClosureGen", "CorrelationWPDGTask");
    }

    // Checked here rather than left to the guard in GenerateSpectraAndTrends, which is
    // correct but only fires after every projection has been built: this fails in
    // seconds, and it can name the O2 process function to enable, which that guard
    // cannot because by then the container is out of scope.
    RequireNonEmptyTrigger(
      h3PhiData.get(),
      "'" + basePathData + "phi/" + (isPureGen ? "h3PhiMCClosureGen" : "h3PhiData") + "'",
      inputFile,
      isPureGen
        ? "It is filled by the O2 process function 'processMCGenClosure' - check it was enabled "
          "in the production this file comes from. Note that 'h3PhiMCGen' is a different "
          "container, filled by 'processParticleEfficiency', and is empty in a closure production."
        : "Check that the production this file comes from actually reconstructed phi candidates.");

    InitAssocParticles(taskConfig);

    if (!useProjectionCache) {
      std::cout << "[INFO] CorrelationWPDGTask: Cache DISABLED. Loading heavy THnSparse data..." << std::endl;

      std::unique_ptr<TFile> fileDataMEInput{nullptr};
      if (applyME) {
        std::string inputMEFile = JsonConfig::RequireString(taskConfig, "input_me_file", GetName());
        fileDataMEInput = RootIO::OpenOrThrow(inputMEFile, "READ", "CorrelationWPDGTask");

        basePathDataME = JsonConfig::RequireString(taskConfig, "base_path_me", GetName());
      }

      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
        data.h5DataSignal = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataInput.get(), (baseData + (isPureGen ? "ClosureMCGen" : "DataSignal")), "CorrelationWPDGTask");

        if (applyME) {
          std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;
          data.h5DataMESignal = RootIO::GetUniqueOrThrow<THnSparseF>(fileDataMEInput.get(), (baseDataME + (isPureGen ? "ClosureMCGenME" : "DataMESignal")), "CorrelationWPDGTask");
        }
        loadedDataCollection.push_back(std::move(data));
      }
    } else {
      std::cout << "[INFO] CorrelationWPDGTask: Cache ENABLED. Skipping THnSparse loading." << std::endl;
      for (const auto& config : assocParticles) {
        LoadedAssocData data;
        data.name = config.name;
        loadedDataCollection.push_back(std::move(data));
      }
    }

    // 4. Output files (opened before the corrections: the cache carries the
    //    binning that everything below is loaded against)
    std::string prefix = JsonConfig::OptionalString(taskConfig, "input_output_prefix", "", GetName());

    std::string basePathProj = JsonConfig::RequireString(taskConfig, "output_dir_proj", GetName());
    std::string basePathFinal = JsonConfig::RequireString(taskConfig, "output_dir_final", GetName());
    std::string phiSpectraName = basePathFinal + prefix + "PhiAssocSpectra.root";
    fileOutputSpectra = RootIO::OpenOrThrow(phiSpectraName, "UPDATE", "CorrelationWPDGTask");

    // Same as CorrelationTask, which this used to differ from for no stated reason:
    // this run rewrites its whole scheme directory, so a previous run's objects would
    // otherwise sit beside the new ones. UPDATE and not RECREATE because only this
    // subtree is ours - another 'binning_name', or the other Extract1D/2D, stays.
    RootIO::ClearPath(fileOutputSpectra.get(), {globalCfgs.binningName, SchemeDirName()});

    // UPDATE and not RECREATE here too, and for a different reason: the cache is
    // expensive to rebuild, so a scheme directory that already holds projections of
    // another binning must be REFUSED rather than silently replaced. That guard lives
    // in ResolveBinningAndCache and only has something to guard if the file survives.
    std::string projMode = useProjectionCache ? "READ" : "UPDATE";

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + prefix + "Phi" + p.name + "DataHistograms.root";
      std::unique_ptr<TFile> fProj = RootIO::OpenOrThrow(fName, projMode.c_str(), "CorrelationWPDGTask");
      filesPhiAssocDataOutput.push_back(std::move(fProj));
    }

    ResolveBinningAndCache();

    // 5. Corrections / Extrapolation, rebinned onto the binning just resolved
    InitCorrectionsAndExtrapolation(taskConfig);

    SetupTrendHistograms();
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
  double GetTriggerSignal(int multBin, int ptPhiBin) override
  {
    std::string phiHistName = "h1PhiTrigger_multBin" + std::to_string(multBin) + "_ptBin" + std::to_string(ptPhiBin);
    std::unique_ptr<TH1D> h1PhiTrigger(static_cast<TH1D*>(h3PhiData->ProjectionZ(
      phiHistName.c_str(), multBin + 1, multBin + 1, ptPhiBin + 1, ptPhiBin + 1)));
    h1PhiTrigger->SetDirectory(0);

    // Bins 1..N, so the under/overflow of the third axis is excluded. Deliberate on
    // both paths: outside [0.9, 1.2] is not a phi candidate, and outside |y| < 0.5 is
    // outside the acceptance the spectra are normalised to.
    return h1PhiTrigger->Integral();
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

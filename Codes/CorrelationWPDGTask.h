#pragma once

#include "CorrelationTaskBase.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"

#include "TFile.h"
#include "TH2D.h"
#include "TH3F.h"

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
      h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiMCGen", "CorrelationWPDGTask");

      // Pre-compute the 2D projection once, up front, so GetTriggerSignal()
      // during Run() is a pure lookup with no per-call state management.
      TH2D* rawh2PhiData = static_cast<TH2D*>(h3PhiData->Project3D("yx"));
      rawh2PhiData->SetDirectory(0);
      h2PhiData = std::unique_ptr<TH2D>(rawh2PhiData);
    }

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
  double GetTriggerSignal(int multBin, int ptPhiBin) override
  {
    if (!isPureGen) {
      std::string phiHistName = "h1PhiData_multBin" + std::to_string(multBin) + "_ptBin" + std::to_string(ptPhiBin);
      std::unique_ptr<TH1D> h1PhiData(static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), multBin + 1, multBin + 1, ptPhiBin + 1, ptPhiBin + 1)));
      h1PhiData->SetDirectory(0);
      return h1PhiData->Integral();
    }
    return h2PhiData->GetBinContent(multBin + 1, ptPhiBin + 1);
  }
  // GetTriggerBkgRatio not overridden: base default (0.0) is correct for WPDG.

  // h2PhiData, when it exists, is a projection of h3PhiData and shares its axes,
  // so one answer covers both branches of GetTriggerSignal above.
  std::pair<const TAxis*, const TAxis*> TriggerAxes() const override
  {
    return {h3PhiData->GetXaxis(), h3PhiData->GetYaxis()};
  }

 private:
  bool isPureGen{false};

  std::unique_ptr<TH3F> h3PhiData;
  std::unique_ptr<TH2D> h2PhiData;
};

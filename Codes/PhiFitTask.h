#pragma once

#include "AnalysisConstants.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "CanvasManager.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TH3F.h"

#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

class PhiFitTask : public IAnalysisTask
{
 public:
  std::string GetName() const override { return "phi_fit_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] PhiFitTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings; // Store the global settings for later use

    // Documentation, not a check: see RootIO::WriteProvenance. The whole merged
    // block, so a key added to the JSON tomorrow is recorded without touching this.
    provenance["produced_at"] = RunEnvironment::TimestampNow();
    provenance["config_block"] = JsonConfig::Serialize(taskConfig);
    // The facts about the run itself. std::map has no range insert before C++23,
    // hence the iterator pair - but the reference is taken once: Facts() returns
    // the same static object every time, so naming it says so.
    const auto& runEnv = RunEnvironment::Facts();
    provenance.insert(runEnv.begin(), runEnv.end());

    // 1. Input Configuration
    std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", "PhiFitTask");
    std::unique_ptr<TFile> fileDataInput = RootIO::OpenOrThrow(inputFile, "READ", "PhiFitTask");

    basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", "PhiFitTask");
    h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiData", "PhiFitTask");

    // The binning comes from the file, never from the configuration: the loops
    // below address these very bins. h3PhiData axes: (mult, pT, invariant mass).
    const std::string origin = "h3PhiData in '" + inputFile + "'";
    multBinning = globalCfgs.ResolveMultBinning(h3PhiData->GetXaxis(), origin);
    ptPhiBinning = globalCfgs.ResolvePtBinning("Phi", h3PhiData->GetYaxis(), origin);

    // 2. Fit Configuration
    std::string fitCfgPath = JsonConfig::RequireString(taskConfig, "fit_config_file", "PhiFitTask");
    fitConfigManager = std::make_unique<FitConfigManager>(fitCfgPath);

    // A snapshot, not a path: this is the file being edited while the fits are
    // tuned, so knowing which one it was says nothing about what was in it.
    provenance["fit_config"] = JsonConfig::ReadFileText(fitCfgPath);

    // 3. Output Configuration
    std::string basePathProj = JsonConfig::RequireString(taskConfig, "output_dir_proj", "PhiFitTask");
    std::string prefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", "PhiFitTask");

    std::string phiDataName = basePathProj + prefix + "PhiDataHistograms.root";
    filePhiDataOutput = RootIO::OpenOrThrow(phiDataName, "RECREATE", "PhiFitTask");

    // Real bin edges on both axes, not 0..N counters: these matrices cross the
    // boundary to CorrelationTask, and with index axes the association to a pT
    // interval would exist only in the shared configuration, unverifiable.
    const std::string axesTitles = ";Multiplicity percentile (%);p_{T} (GeV/#it{c})";

    auto makeTriggerMatrix = [&](const char* name, const std::string& title) {
      auto h = std::make_unique<TH2D>(name, (title + axesTitles).c_str(),
                                      BinningUtils::NBins(multBinning), multBinning.data(),
                                      BinningUtils::NBins(ptPhiBinning), ptPhiBinning.data());
      h->SetDirectory(0);
      return h;
    };

    h2TriggerSignal = makeTriggerMatrix("h2TriggerSignal", "Raw Signal Yield");
    h2TriggerBkgSigRegion = makeTriggerMatrix("h2TriggerBkgSigRegion", "Bkg in Signal Region");
    h2TriggerBkgSideRegion = makeTriggerMatrix("h2TriggerBkgSideRegion", "Bkg in Sideband Region");
    h2TriggerBkgRatio = makeTriggerMatrix("h2TriggerBkgRatio", "Bkg(SigRegion)/Bkg(Sideband)");
  }

  void Run() override
  {
    std::cout << "[INFO] PhiFitTask: RUNNING TRIGGER SIGNAL EXTRACTION..." << std::endl;

    TDirectory* fitDir = RootIO::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Fits"}, false);
    if (!fitDir) {
      throw std::runtime_error("[FATAL] PhiFitTask: Cannot create the '" + globalCfgs.binningName +
                               "/Fits' directory in the output file.");
    }

    for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
      for (int j = 0; j < BinningUtils::NBins(ptPhiBinning); j++) {
        std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
        std::unique_ptr<TH1> h1PhiData(static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1)));
        h1PhiData->SetDirectory(0);

        // Fetch configuration dynamically from JSON
        FitConfig cfg = fitConfigManager->GetConfig("phi", i, j);

        // Initialize RooFit Engine
        DynamicRooFitter fitter(h1PhiData.get(), cfg);
        fitter.DoFit();

        auto res = fitter.ExtractYieldsAndPurity();

        std::string canvasName = "cFit_Phi_mult" + std::to_string(i) + "_pt" + std::to_string(j);
        fitter.SaveFitToCanvas(canvasManager, fitDir, canvasName);

        // Save the results for CorrelationTask
        auto fillTriggerMatrix = [&](TH2D* h, const std::pair<double, double>& valueAndError) {
          h->SetBinContent(i + 1, j + 1, valueAndError.first);
          h->SetBinError(i + 1, j + 1, valueAndError.second);
        };

        fillTriggerMatrix(h2TriggerSignal.get(), res.signalAndError);
        fillTriggerMatrix(h2TriggerBkgSigRegion.get(), res.backgroundAndError);
        fillTriggerMatrix(h2TriggerBkgSideRegion.get(), res.bkgInSidebandAndError);
        fillTriggerMatrix(h2TriggerBkgRatio.get(), res.bkgRatioAndError);
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PhiFitTask: TERMINATING..." << std::endl;

    TDirectory* summaryDir = RootIO::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Summary"}, false);
    if (!summaryDir) {
      throw std::runtime_error("[FATAL] PhiFitTask: Cannot create the '" + globalCfgs.binningName +
                               "/Summary' directory in the output file. Trigger yields would be lost!");
    }
    summaryDir->cd();

    h2TriggerSignal->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSigRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSideRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgRatio->Write(nullptr, TObject::kOverwrite);

    // The trigger matrices cross into CorrelationTask, so this file is a link in
    // the chain and not a by-product: it has to say what produced it. No binning
    // stamp is needed here - both axes of the matrices are the real binnings.
    RootIO::WriteProvenance(RootIO::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Provenance"}, false), provenance);

    if (filePhiDataOutput)
      filePhiDataOutput->Close();

    std::cout << "[INFO] PhiFitTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  CanvasManager canvasManager;

  std::string basePathData;

  // Read from the input file in Init(), never from the configuration
  std::vector<double> multBinning;
  std::vector<double> ptPhiBinning;
  std::unique_ptr<TH3F> h3PhiData;

  std::unique_ptr<TFile> filePhiDataOutput;

  // Filled in Init, written into the output file in Terminate
  std::map<std::string, std::string> provenance;

  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgSigRegion;
  std::unique_ptr<TH2D> h2TriggerBkgSideRegion;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  std::unique_ptr<FitConfigManager> fitConfigManager;
};

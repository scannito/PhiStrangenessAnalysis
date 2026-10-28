#pragma once

#include "AnalysisConstants.h"
#include "AnalysisSettings.h"
#include "CanvasManager.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "FitQualityRecorder.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TH3F.h"

#include <cmath>
#include <format>
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
    Logger::Info("{}: INITIALIZING...", GetName());

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
    std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
    std::unique_ptr<TFile> fileDataInput = RootIO::OpenOrThrow(inputFile, "READ", GetName());

    basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());
    h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiData", GetName());

    // The binning comes from the file, never from the configuration: the loops
    // below address these very bins. h3PhiData axes: (mult, pT, invariant mass).
    const std::string origin = std::format("h3PhiData in '{}'", inputFile);
    multBinning = globalCfgs.ResolveMultBinning(h3PhiData->GetXaxis(), origin);
    ptPhiBinning = globalCfgs.ResolvePtBinning("Phi", h3PhiData->GetYaxis(), origin);

    // 2. Fit Configuration
    std::string fitCfgPath = JsonConfig::RequireString(taskConfig, "fit_config_file", GetName());
    fitConfigManager = std::make_unique<FitConfigManager>(fitCfgPath);
    fitConfigManager->DeclareCells(kFitSection, multBinning, {ptPhiBinning});

    // A snapshot, not a path: this is the file being edited while the fits are
    // tuned, so knowing which one it was says nothing about what was in it.
    provenance["fit_config"] = JsonConfig::ReadFileText(fitCfgPath);

    // 3. Output Configuration
    std::string basePathProj = JsonConfig::RequireString(taskConfig, "output_dir_proj", GetName());
    std::string prefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

    // UPDATE and this binning's subtree cleared, not RECREATE: the file holds one subtree per
    // binning_name, and another scheme's must survive this one's run (CLAUDE.md, How a task
    // opens the file it writes).
    std::string phiDataName = basePathProj + prefix + OutputNames::kPhiDataFile;
    filePhiDataOutput = RootIO::OpenOrThrow(phiDataName, "UPDATE", GetName());
    RootIO::ClearPath(filePhiDataOutput.get(), {globalCfgs.binningName});

    // Real bin edges on both axes, not 0..N counters: these matrices cross the
    // boundary to CorrelationTask, and with index axes the association to a pT
    // interval would exist only in the shared configuration, unverifiable.
    const std::string axesTitles = ";Multiplicity percentile (%);p_{T} (GeV/#it{c})";

    auto makeTriggerMatrix = [&](const std::string& name, const std::string& title) {
      auto h = std::make_unique<TH2D>(name.c_str(), (title + axesTitles).c_str(),
                                      BinningUtils::NBins(multBinning), multBinning.data(),
                                      BinningUtils::NBins(ptPhiBinning), ptPhiBinning.data());
      h->SetDirectory(0);
      return h;
    };

    h2TriggerSignal = makeTriggerMatrix(OutputNames::kTriggerSignal, "Raw Signal Yield");
    h2TriggerBkgSigRegion = makeTriggerMatrix("h2TriggerBkgSigRegion", "Bkg in Signal Region");
    h2TriggerBkgSideRegion = makeTriggerMatrix("h2TriggerBkgSideRegion", "Bkg in Sideband Region");
    h2TriggerBkgRatio = makeTriggerMatrix(OutputNames::kTriggerBkgRatio, "Bkg(SigRegion)/Bkg(Sideband)");
  }

  void Run() override
  {
    Logger::Info("{}: RUNNING TRIGGER SIGNAL EXTRACTION...", GetName());

    TDirectory* fitDir = RootIO::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Fits"}, false);
    if (!fitDir) {
      throw std::runtime_error(std::format("[FATAL] {}: Cannot create the '{}/Fits' directory in the output file.", GetName(),
                                           globalCfgs.binningName));
    }

    for (int multBin = 0; multBin < BinningUtils::NBins(multBinning); multBin++) {
      for (int ptPhiBin = 0; ptPhiBin < BinningUtils::NBins(ptPhiBinning); ptPhiBin++) {
        const std::string cell = OutputNames::CellTag(multBinning, multBin, ptPhiBinning, ptPhiBin);
        std::string phiHistName = "h1PhiData_" + cell;
        std::unique_ptr<TH1> h1PhiData(static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), multBin + 1, multBin + 1, ptPhiBin + 1, ptPhiBin + 1)));
        h1PhiData->SetDirectory(0);

        // Fetch configuration dynamically from JSON
        FitConfig cfg = fitConfigManager->GetConfig(kFitSection, cell);

        // Initialize RooFit Engine
        DynamicRooFitter fitter(h1PhiData.get(), cfg);
        fitter.DoFit();
        fitQuality.Add(kFitSection, FitQualityRecorder::kAnalysisBinning, multBinning, multBin, ptPhiBinning, ptPhiBin, cfg.binOverride,
                       fitter.Diagnostics());

        auto res = fitter.ExtractYieldsAndPurity();

        // The fitter draws; the canvas, the label, the file and the lifetime are the
        // task's. Written and destroyed here, per cell, and not left to a WriteAll in
        // Terminate: nothing reads a fit canvas back, and holding all of them until
        // the end would only cost memory.
        const std::string canvasName = "cFit_Phi_" + cell;
        const std::string title = OutputNames::CellTitle("#phi", multBinning, multBin, ptPhiBinning, ptPhiBin);
        TCanvas* cFit = canvasManager.CreateCanvas(canvasName, title, 800, 800);
        fitter.DrawFit(cFit, title, /*logY=*/false); // a peak on a smooth background, which log scale flattens
        CanvasManager::DrawAliceLabel(cFit, 0.18, 0.84, "Work in progress", "", 0.045);
        canvasManager.WriteAndDestroy(canvasName, fitDir);

        // Save the results for CorrelationTask
        auto fillTriggerMatrix = [&](TH2D* h, const std::pair<double, double>& valueAndError) {
          h->SetBinContent(multBin + 1, ptPhiBin + 1, valueAndError.first);
          h->SetBinError(multBin + 1, ptPhiBin + 1, valueAndError.second);
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
    Logger::Info("{}: TERMINATING...", GetName());

    const std::vector<std::string> summaryPath = OutputNames::SummaryPath(globalCfgs.binningName);
    TDirectory* summaryDir = RootIO::GetOrCreatePath(filePhiDataOutput.get(), summaryPath, false);
    if (!summaryDir) {
      throw std::runtime_error(std::format("[FATAL] {}: Cannot create the '{}' directory in the output "
                                           "file. Trigger yields would be lost!",
                                           GetName(), RootIO::MakeDirPath(summaryPath)));
    }
    summaryDir->cd();

    h2TriggerSignal->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSigRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSideRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgRatio->Write(nullptr, TObject::kOverwrite);

    // The trigger matrices cross into CorrelationTask, so this file is a link in
    // the chain and not a by-product: it has to say what produced it. No binning
    // stamp is needed here - both axes of the matrices are the real binnings.
    RootIO::WriteProvenance(filePhiDataOutput.get(), globalCfgs.binningName, provenance);

    fitQuality.Write(RootIO::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName}, false), GetName());

    filePhiDataOutput->Close();

    Logger::Info("{}: DONE.", GetName());
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
  FitQualityRecorder fitQuality;

  // Filled in Init, written into the output file in Terminate
  std::map<std::string, std::string> provenance;

  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgSigRegion;
  std::unique_ptr<TH2D> h2TriggerBkgSideRegion;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  // The section of the fit configuration this task fits with. A name fixed by what the
  // task is, unlike PurityTask's, which comes from each particle's 'purity_key'.
  static constexpr const char* kFitSection = "phi";

  std::unique_ptr<FitConfigManager> fitConfigManager;
};

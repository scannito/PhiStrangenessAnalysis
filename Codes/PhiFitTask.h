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

    provenance = RootIO::StartProvenance(JsonConfig::Serialize(taskConfig));

    // 1. Input Configuration
    std::string inputPath = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
    std::unique_ptr<TFile> inputFile = RootIO::OpenOrThrow(inputPath, "READ", GetName());

    basePathData = JsonConfig::RequireString(taskConfig, "base_path_data", GetName());
    h3PhiData = RootIO::GetUniqueOrThrow<TH3F>(inputFile.get(), std::format("{}phi/h3PhiData", basePathData), GetName());

    // The binning comes from the file, never from the configuration: the loops
    // below address these very bins. h3PhiData axes: (mult, pT, invariant mass).
    const std::string origin = std::format("h3PhiData in '{}'", inputPath);
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
    std::string basePathProj = JsonConfig::RequireString(taskConfig, "projections_dir", GetName());
    std::string outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

    // UPDATE and this binning's subtree cleared, not RECREATE: the file holds one subtree per
    // binning_name, and another scheme's must survive this one's run (CLAUDE.md, How a task
    // opens the file it writes).
    std::string outputPath = std::format("{}{}{}", basePathProj, outputPrefix, OutputNames::kPhiDataFile);
    outputFile = RootIO::OpenOrThrow(outputPath, "UPDATE", GetName());
    RootIO::ClearPath(outputFile.get(), {globalCfgs.binningName});

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

    TDirectory* fitDir = RootIO::MakePath(outputFile.get(), {globalCfgs.binningName, "Fits"});

    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      for (int ptPhiBin : BinningUtils::BinIndices(ptPhiBinning)) {
        const std::string cell = OutputNames::CellTag(multBinning, multBin, ptPhiBinning, ptPhiBin);
        std::string phiHistName = std::format("h1PhiData_{}", cell);
        const auto mult = BinningUtils::BinRange::Single(multBin), ptPhi = BinningUtils::BinRange::Single(ptPhiBin);
        std::unique_ptr<TH1> h1PhiData(static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), mult.first, mult.last, ptPhi.first, ptPhi.last)));
        h1PhiData->SetDirectory(0);

        // Fetch configuration dynamically from JSON
        AnalysisStructures::FitConfig cfg = fitConfigManager->GetConfig(kFitSection, cell);

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
        const std::string canvasName = std::format("cFit_Phi_{}", cell);
        const std::string title = OutputNames::CellTitle("#phi", multBinning, multBin, ptPhiBinning, ptPhiBin);
        TCanvas* cFit = canvasManager.CreateCanvas(canvasName, title, 800, 800);
        fitter.DrawFit(cFit, title, /*logY=*/false); // a peak on a smooth background, which log scale flattens
        CanvasManager::DrawAliceLabel(cFit, 0.18, 0.84, "Work in progress", "", 0.045);
        canvasManager.WriteAndDestroy(canvasName, fitDir);

        // Save the results for CorrelationTask
        HistogramUtils::SetBinValueAndError(h2TriggerSignal.get(), multBin, ptPhiBin, res.signalAndError);
        HistogramUtils::SetBinValueAndError(h2TriggerBkgSigRegion.get(), multBin, ptPhiBin, res.bkgInSignalRegionAndError);
        HistogramUtils::SetBinValueAndError(h2TriggerBkgSideRegion.get(), multBin, ptPhiBin, res.bkgInSidebandAndError);
        HistogramUtils::SetBinValueAndError(h2TriggerBkgRatio.get(), multBin, ptPhiBin, res.bkgRatioAndError);
      }
    }
  }

  void Terminate() override
  {
    Logger::Info("{}: TERMINATING...", GetName());

    const std::vector<std::string> summaryPath = OutputNames::SummaryPath(globalCfgs.binningName);
    TDirectory* summaryDir = RootIO::MakePath(outputFile.get(), summaryPath);
    summaryDir->cd();

    h2TriggerSignal->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSigRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSideRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgRatio->Write(nullptr, TObject::kOverwrite);

    // The trigger matrices cross into CorrelationTask, so this file is a link in
    // the chain and not a by-product: it has to say what produced it. No binning
    // stamp is needed here - both axes of the matrices are the real binnings.
    RootIO::WriteProvenance(outputFile.get(), globalCfgs.binningName, provenance);

    fitQuality.Write(RootIO::MakePath(outputFile.get(), {globalCfgs.binningName}), GetName());

    outputFile->Close();

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

  std::unique_ptr<TFile> outputFile;
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

#pragma once

#include "AnalysisConstants.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "FitPhiSignalAndBkg.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"

#include "TCanvas.h"
#include "TDirectory.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TH3F.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

class PhiFitTask : public IAnalysisTask
{
 public:
  enum class FitterType { DynamicRooFitter = 0,
                          FitPhiSignalAndBkg };

  std::string GetName() const override { return "phi_fit_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] PhiFitTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings; // Store the global settings for later use

    // 1. Input Configuration
    std::string inputFile = RequireString(taskConfig, "input_data_file", "PhiFitTask");
    std::unique_ptr<TFile> fileDataInput = OpenOrThrow(inputFile, "READ", "PhiFitTask");

    basePathData = RequireString(taskConfig, "base_path_data", "PhiFitTask");
    h3PhiData = GetUniqueOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiData", "PhiFitTask");

    /*if (!taskConfig.HasMember("input_data_file") || !taskConfig.HasMember("base_path_data"))
    {
      throw std::runtime_error("[FATAL ERROR] PhiFitTask: Missing input_data_file or base_path_data in JSON!");
    }
    std::string inputFile = taskConfig["input_data_file"].GetString();
    basePathData = taskConfig["base_path_data"].GetString();

    TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
    if (!fileDataInput || fileDataInput->IsZombie()) {
      throw std::runtime_error("[FATAL] PhiFitTask: Cannot open Data input file: " + inputFile);
    }

    h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiData").c_str()));
    if (!h3PhiData)
      throw std::runtime_error("[FATAL] PhiFitTask: Missing h3PhiData!");
    h3PhiData->SetDirectory(0);
    fileDataInput->Close();
    delete fileDataInput;*/

    // 2. Fit Configuration
    std::string fitCfgPath = RequireString(taskConfig, "fit_config_file", "PhiFitTask");
    fitConfigManager = std::make_unique<FitConfigManager>(fitCfgPath);
    /*if (!taskConfig.HasMember("fit_config_file")) {
      throw std::runtime_error("[FATAL ERROR] PhiFitTask: 'fit_config_file' missing in JSON!");
    }
    fitConfigManager = new FitConfigManager(taskConfig["fit_config_file"].GetString());*/

    // Resolve the fitter: an invalid name must stop the task before any output file is created,
    // and the loop should switch on a value that cannot be invalid.
    if (taskConfig.HasMember("fitter_type")) {
      std::string requestedFitter = taskConfig["fitter_type"].GetString();
      if (requestedFitter == "dynamicroofitter") {
        fitterType = FitterType::DynamicRooFitter;
      } else if (requestedFitter == "fitphisignalandbkg") {
        fitterType = FitterType::FitPhiSignalAndBkg;
      } else {
        throw std::runtime_error("[FATAL] PhiFitTask: Unknown fitter_type '" + requestedFitter +
                                 "'. Available: dynamicroofitter, fitphisignalandbkg");
      }
    }

    // 3. Output Configuration
    std::string basePathProj = RequireString(taskConfig, "output_dir_proj", "PhiFitTask");
    // std::string basePathProj = taskConfig["output_dir_proj"].GetString();
    std::string prefix = taskConfig.HasMember("output_prefix") ? taskConfig["output_prefix"].GetString() : "";

    std::string phiDataName = basePathProj + prefix + "PhiDataHistograms.root";
    filePhiDataOutput = OpenOrThrow(phiDataName, "RECREATE", "PhiFitTask");
    // filePhiDataOutput = new TFile(phiDataName.c_str(), "RECREATE");

    const int nBinPtPhi = globalCfgs.GetNBinPt("Phi");

    // Histograms to pass parameters to CorrelationTask
    h2TriggerSignal = std::make_unique<TH2D>("h2TriggerSignal", "Raw Signal Yield;Multiplicity Bin;p_{T} Bin",
                                             globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerSignal->SetDirectory(0);
    h2TriggerBkgSigRegion = std::make_unique<TH2D>("h2TriggerBkgSigRegion", "Bkg in Signal Region;Multiplicity Bin;p_{T} Bin",
                                                   globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerBkgSigRegion->SetDirectory(0);
    h2TriggerBkgSideRegion = std::make_unique<TH2D>("h2TriggerBkgSideRegion", "Bkg in Sideband Region;Multiplicity Bin;p_{T} Bin",
                                                    globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerBkgSideRegion->SetDirectory(0);
    h2TriggerBkgRatio = std::make_unique<TH2D>("h2TriggerBkgRatio", "Bkg(SigRegion)/Bkg(Sideband);Multiplicity Bin;p_{T} Bin",
                                               globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerBkgRatio->SetDirectory(0);
  }

  void Run() override
  {
    std::cout << "[INFO] PhiFitTask: RUNNING TRIGGER SIGNAL EXTRACTION..." << std::endl;

    TDirectory* fitDir = AnalysisUtils::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Fits"}, false);
    if (!fitDir) {
      throw std::runtime_error("[FATAL] PhiFitTask: Cannot create the '" + globalCfgs.binningName +
                               "/Fits' directory in the output file.");
    }

    const int nBinPtPhi = globalCfgs.GetNBinPt("Phi");

    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      for (int j = 0; j < nBinPtPhi; j++) {
        std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
        std::unique_ptr<TH1> h1PhiData(static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1)));
        h1PhiData->SetDirectory(0);

        std::pair<double, double> triggerSignalAndError{0.0, 0.0};
        std::pair<double, double> triggerBkgSigRegionAndError{0.0, 0.0};
        std::pair<double, double> triggerBkgSideRegionAndError{0.0, 0.0};
        std::pair<double, double> triggerBkgRatioAndError{0.0, 0.0};

        if (fitterType == FitterType::FitPhiSignalAndBkg) {
          // Method 1: FitPhiSignalAndBkg
          std::unique_ptr<TF1> fitVoigtBkgSourav = std::make_unique<TF1>("fitVoigtBkgSourav", VoigtBkgSourav, 0.995, 1.06, 7);
          fitVoigtBkgSourav->SetParameter(1, 1.019);
          fitVoigtBkgSourav->SetParameter(2, 0.001);
          fitVoigtBkgSourav->FixParameter(3, 0.00426);
          fitVoigtBkgSourav->SetNpx(400);
          fitVoigtBkgSourav->SetLineColor(kRed);

          FitPhiSignalAndBkg<false> fitPhiSignalAndBkg{h1PhiData.get(), fitVoigtBkgSourav.get(), 4,
                                                       AnalysisConstants::phiMassSignalRange,
                                                       AnalysisConstants::phiMassSidebandRange};

          triggerSignalAndError = fitPhiSignalAndBkg.GetSignalAndError();
          triggerBkgSigRegionAndError = fitPhiSignalAndBkg.GetBkgInSigRegionAndError();
          triggerBkgSideRegionAndError = fitPhiSignalAndBkg.GetBkgInSideRegionAndError();
          if (triggerBkgSideRegionAndError.first > 0.0) {
            const double ratio = triggerBkgSigRegionAndError.first / triggerBkgSideRegionAndError.first;
            const double relNum = (triggerBkgSigRegionAndError.first != 0.0) ? triggerBkgSigRegionAndError.second / triggerBkgSigRegionAndError.first : 0.0;
            const double relDen = triggerBkgSideRegionAndError.second / triggerBkgSideRegionAndError.first;
            triggerBkgRatioAndError = {ratio, ratio * std::sqrt(relNum * relNum + relDen * relDen)};
          }

          fitDir->cd();
          std::unique_ptr<TCanvas> cFit = std::make_unique<TCanvas>(("cFit_Phi_mult" + std::to_string(i) + "_pt" + std::to_string(j)).c_str());
          h1PhiData->Draw();
          // fitVoigtBkgSourav->DrawCopy("SAME");
          cFit->Write(nullptr, TObject::kOverwrite);
        } else {
          // Method 2: DynamicRooFitter
          // Fetch configuration dynamically from JSON
          FitConfig cfg = fitConfigManager->GetConfig("phi", i, j);

          // Initialize RooFit Engine
          DynamicRooFitter fitter(h1PhiData.get(), cfg);
          fitter.DoFit();

          auto res = fitter.ExtractYieldsAndPurity();
          triggerSignalAndError = res.signalAndError;
          triggerBkgSigRegionAndError = res.backgroundAndError;
          triggerBkgSideRegionAndError = res.bkgInSidebandAndError;
          triggerBkgRatioAndError = res.bkgRatioAndError;

          std::string canvasName = "cFit_Phi_mult" + std::to_string(i) + "_pt" + std::to_string(j);
          fitter.SaveFitCanvas(fitDir, canvasName);
        }

        // Save the results for CorrelationTask
        h2TriggerSignal->SetBinContent(i + 1, j + 1, triggerSignalAndError.first);
        h2TriggerSignal->SetBinError(i + 1, j + 1, triggerSignalAndError.second);
        h2TriggerBkgSigRegion->SetBinContent(i + 1, j + 1, triggerBkgSigRegionAndError.first);
        h2TriggerBkgSigRegion->SetBinError(i + 1, j + 1, triggerBkgSigRegionAndError.second);
        h2TriggerBkgSideRegion->SetBinContent(i + 1, j + 1, triggerBkgSideRegionAndError.first);
        h2TriggerBkgSideRegion->SetBinError(i + 1, j + 1, triggerBkgSideRegionAndError.second);
        h2TriggerBkgRatio->SetBinContent(i + 1, j + 1, triggerBkgRatioAndError.first);
        h2TriggerBkgRatio->SetBinError(i + 1, j + 1, triggerBkgRatioAndError.second);
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PhiFitTask: TERMINATING..." << std::endl;

    TDirectory* summaryDir = AnalysisUtils::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Summary"}, false);
    if (!summaryDir) {
      throw std::runtime_error("[FATAL] PhiFitTask: Cannot create the '" + globalCfgs.binningName +
                               "/Summary' directory in the output file. Trigger yields would be lost!");
    }
    summaryDir->cd();

    h2TriggerSignal->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSigRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSideRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgRatio->Write(nullptr, TObject::kOverwrite);

    if (filePhiDataOutput)
      filePhiDataOutput->Close();

    std::cout << "[INFO] PhiFitTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::string basePathData;
  FitterType fitterType{FitterType::DynamicRooFitter}; // Default fitting method
  std::unique_ptr<TH3F> h3PhiData;

  std::unique_ptr<TFile> filePhiDataOutput;

  std::unique_ptr<TH2D> h2TriggerSignal;
  std::unique_ptr<TH2D> h2TriggerBkgSigRegion;
  std::unique_ptr<TH2D> h2TriggerBkgSideRegion;
  std::unique_ptr<TH2D> h2TriggerBkgRatio;

  std::unique_ptr<FitConfigManager> fitConfigManager;
};

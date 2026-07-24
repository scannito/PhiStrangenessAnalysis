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

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

class PhiFitTask : public IAnalysisTask
{
 public:
  std::string GetName() const override { return "phi_fit_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] PhiFitTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings; // Store the global settings for later use

    // 1. Input Configuration
    std::string inputFile = RequireString(taskConfig, "input_data_file", "PhiFitTask");
    std::unique_ptr<TFile> fileDataInput = OpenOrThrow(inputFile, "READ", "PhiFitTask");

    basePathData = RequireString(taskConfig, "base_path_data", "PhiFitTask");
    h3PhiData = GetOrThrow<TH3F>(fileDataInput.get(), basePathData + "phi/h3PhiData", "PhiFitTask");

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

    if (taskConfig.HasMember("fitter_type")) {
      fitterType = taskConfig["fitter_type"].GetString();
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
    h2TriggerSignal = new TH2D("h2TriggerSignal", "Raw Signal Yield;Multiplicity Bin;p_{T} Bin",
                               globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerSignal->SetDirectory(0);
    h2TriggerBkgSigRegion = new TH2D("h2TriggerBkgSigRegion", "Bkg in Signal Region;Multiplicity Bin;p_{T} Bin",
                                     globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerBkgSigRegion->SetDirectory(0);
    h2TriggerBkgSideRegion = new TH2D("h2TriggerBkgSideRegion", "Bkg in Sideband Region;Multiplicity Bin;p_{T} Bin",
                                      globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerBkgSideRegion->SetDirectory(0);
    h2TriggerBkgRatio = new TH2D("h2TriggerBkgRatio", "Bkg(SigRegion)/Bkg(Sideband);Multiplicity Bin;p_{T} Bin",
                                 globalCfgs.nBinMult, 0, globalCfgs.nBinMult, nBinPtPhi, 0, nBinPtPhi);
    h2TriggerBkgRatio->SetDirectory(0);
  }

  void Run() override
  {
    std::cout << "[INFO] PhiFitTask: RUNNING TRIGGER SIGNAL EXTRACTION..." << std::endl;

    TDirectory* fitDir = AnalysisUtils::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Fits"}, false);

    const int nBinPtPhi = globalCfgs.GetNBinPt("Phi");

    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      for (int j = 0; j < nBinPtPhi; j++) {
        std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
        TH1* h1PhiData = static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1));
        h1PhiData->SetDirectory(0);

        double triggerSignal{0.0};
        double triggerBkgSigRegion{0.0};
        double triggerBkgSideRegion{0.0};
        double triggerBkgRatio{0.0};

        if (fitterType == "fitphisignalandbkg") {
          // Method 1: FitPhiSignalAndBkg
          TF1* fitVoigtBkgSourav = new TF1("fitVoigtBkgSourav", VoigtBkgSourav, 0.995, 1.06, 7);
          fitVoigtBkgSourav->SetParameter(1, 1.019);
          fitVoigtBkgSourav->SetParameter(2, 0.001);
          fitVoigtBkgSourav->FixParameter(3, 0.00426);
          fitVoigtBkgSourav->SetNpx(400);
          fitVoigtBkgSourav->SetLineColor(kRed);

          FitPhiSignalAndBkg<false> fitPhiSignalAndBkg{h1PhiData, fitVoigtBkgSourav, 4,
                                                       AnalysisConstants::phiMassSignalRange,
                                                       AnalysisConstants::phiMassSidebandRange};

          triggerSignal = fitPhiSignalAndBkg.GetSignal();
          triggerBkgSigRegion = fitPhiSignalAndBkg.GetBkgInSigRegion();
          triggerBkgSideRegion = fitPhiSignalAndBkg.GetBkgInSideRegion();
          triggerBkgRatio = (triggerBkgSideRegion > 0) ? (triggerBkgSigRegion / triggerBkgSideRegion) : 0;

          fitDir->cd();
          TCanvas* cFit = new TCanvas(("cFit_Phi_mult" + std::to_string(i) + "_pt" + std::to_string(j)).c_str());
          h1PhiData->Draw();
          // fitVoigtBkgSourav->DrawCopy("SAME");
          cFit->Write(nullptr, TObject::kOverwrite);
          delete cFit;
          delete fitVoigtBkgSourav;
        } else if (fitterType == "dynamicroofitter") {
          // Method 2: DynamicRooFitter
          // Fetch configuration dynamically from JSON
          FitConfig cfg = fitConfigManager->GetConfig("phi", i, j);

          // Initialize RooFit Engine
          DynamicRooFitter fitter(h1PhiData, cfg);
          fitter.DoFit();

          auto res = fitter.ExtractYieldsAndPurity();
          triggerSignal = res.signal.first;
          triggerBkgSigRegion = res.background.first;
          triggerBkgSideRegion = res.bkgInSideband.first;
          triggerBkgRatio = (triggerBkgSideRegion > 0) ? (triggerBkgSigRegion / triggerBkgSideRegion) : 0;

          std::string canvasName = "cFit_Phi_mult" + std::to_string(i) + "_pt" + std::to_string(j);
          fitter.SaveFitCanvas(fitDir, canvasName);
        } else {
          throw std::runtime_error("[FATAL] PhiFitTask: Unknown fitter_type: " + fitterType);
        }

        // Save the results for CorrelationTask
        h2TriggerSignal->SetBinContent(i + 1, j + 1, triggerSignal);
        h2TriggerBkgSigRegion->SetBinContent(i + 1, j + 1, triggerBkgSigRegion);
        h2TriggerBkgSideRegion->SetBinContent(i + 1, j + 1, triggerBkgSideRegion);
        h2TriggerBkgRatio->SetBinContent(i + 1, j + 1, triggerBkgRatio);

        delete h1PhiData;
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PhiFitTask: TERMINATING..." << std::endl;

    TDirectory* summaryDir = AnalysisUtils::GetOrCreatePath(filePhiDataOutput.get(), {globalCfgs.binningName, "Summary"}, false);
    summaryDir->cd();

    h2TriggerSignal->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSigRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgSideRegion->Write(nullptr, TObject::kOverwrite);
    h2TriggerBkgRatio->Write(nullptr, TObject::kOverwrite);

    if (filePhiDataOutput)
      filePhiDataOutput->Close();

    delete h2TriggerSignal;
    delete h2TriggerBkgSigRegion;
    delete h2TriggerBkgSideRegion;
    delete h2TriggerBkgRatio;

    delete h3PhiData;

    std::cout << "[INFO] PhiFitTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::string basePathData;
  std::string fitterType{"dynamicroofitter"}; // Default fitting method
  TH3F* h3PhiData{nullptr};

  // TFile* filePhiDataOutput{nullptr};
  std::unique_ptr<TFile> filePhiDataOutput;

  TH2D* h2TriggerSignal{nullptr};
  TH2D* h2TriggerBkgSigRegion{nullptr};
  TH2D* h2TriggerBkgSideRegion{nullptr};
  TH2D* h2TriggerBkgRatio{nullptr};

  // FitConfigManager* fitConfigManager{nullptr};
  std::unique_ptr<FitConfigManager> fitConfigManager;
};

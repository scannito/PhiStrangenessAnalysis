#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisUtils.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "IAnalysisTask.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH3F.h"

#include <iostream>
#include <string>
#include <vector>

class PurityTask : public IAnalysisTask
{
 public:
  // Tells the WorkflowManager which JSON node to pass to this task
  std::string GetName() const override { return "purity_task"; }

  void Init(const rapidjson::Value& taskConfig) override
  {
    std::cout << "[INFO] PurityTask: INITIALIZING..." << std::endl;

    // 1. Open input file, extract 3D histograms to RAM, and immediately close it
    if (!taskConfig.HasMember("input_data_file")) {
      throw std::runtime_error("[FATAL] PurityTask: 'input_data_file' missing in JSON!");
    }

    std::string inputFile = taskConfig["input_data_file"].GetString();
    TFile* fileInput = new TFile(inputFile.c_str(), "READ");
    if (!fileInput || fileInput->IsZombie()) {
      throw std::runtime_error("[FATAL] PurityTask: Cannot open input file!");
    }

    h3K0SData = static_cast<TH3F*>(fileInput->Get("k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass"));
    h3PiTPCData = static_cast<TH3F*>(fileInput->Get("pion-track-producer/pionTracks/h3PionTPCnSigma"));
    h3PiTOFData = static_cast<TH3F*>(fileInput->Get("pion-track-producer/pionTracks/h3PionTOFnSigma"));

    // Decouple histograms from the file so they survive in RAM when the file closes
    h3K0SData->SetDirectory(0);
    h3PiTPCData->SetDirectory(0);
    h3PiTOFData->SetDirectory(0);

    fileInput->Close();
    delete fileInput;

    // 2. Open Output files
    std::string outputDir = taskConfig["output_dir"].GetString();

    // Prefix to specify the type of analysis (Data vs MCClosure)
    std::string prefix = "";
    if (taskConfig.HasMember("output_prefix"))
      prefix = taskConfig["output_prefix"].GetString();

    std::string outK0S = outputDir + prefix + "K0SPurity.root";
    std::string outPi = outputDir + prefix + "PiPurity.root";

    fileOutputK0S = new TFile(outK0S.c_str(), "RECREATE");
    fileOutputPi = new TFile(outPi.c_str(), "RECREATE");

    // 3. Create Canvases
    canvasPurityK0S = new TCanvas("canvasK0SPurity", "K0S Purity", 800, 600);
    canvasPurityPiTPC = new TCanvas("canvasPiTPCPurity", "Pi TPC Purity", 800, 600);
    canvasPurityPiTOF = new TCanvas("canvasPiTOFPurity", "Pi TOF Purity", 800, 600);

    // 4. Setup the task list (assuming nBinPtK0S, binspTK0S, etc. are accessible globally or defined here)
    particleTasks = {
      {"k0s", h3K0SData, AnalysisConstants::nBinPtK0S, AnalysisConstants::binspTK0S, fileOutputK0S, canvasPurityK0S},
      {"pi_tpc", h3PiTPCData, AnalysisConstants::nBinPtPi, AnalysisConstants::binspTPi, fileOutputPi, canvasPurityPiTPC},
      {"pi_tof", h3PiTOFData, AnalysisConstants::nBinPtPi, AnalysisConstants::binspTPi, fileOutputPi, canvasPurityPiTOF}};

    // 5. Read task-specific settings from the JSON node (DOM)
    if (!taskConfig.HasMember("fit_config_file")) {
      throw std::runtime_error("[FATAL] PurityTask: 'fit_config_file' missing in JSON!");
    }

    // Keep the fit configuration file completely separated for physics tuning
    std::string fitCfgPath = taskConfig["fit_config_file"].GetString();

    // 6. Initialize specific mathematical tools
    fitConfigManager = new FitConfigManager(fitCfgPath);
  }

  void Run() override
  {
    std::cout << "[INFO] PurityTask: RUNNING FITS..." << std::endl;

    for (int i{0}; i < AnalysisConstants::nBinMult; i++) {
      for (auto& task : particleTasks) {
        std::string hName = "h1" + task.name + "Purity_multBin" + std::to_string(i);
        TH1* h1PuritySpectrum = new TH1F(hName.c_str(), "; p_{T} (GeV/#it{c}); S/(S+B)", task.nBinPt, task.binning.data());
        h1PuritySpectrum->SetDirectory(0);

        for (int k{0}; k < task.nBinPt; k++) {
          std::string histName = "h1" + task.name + "_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);

          // Project 1D slice
          TH1* h1Data = static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), i + 1, i + 1, k + 1, k + 1));
          h1Data->SetDirectory(0);

          // Run Fitter using JSON-based configuration
          FitConfig cfg = fitConfigManager->GetConfig(task.name, i, k);
          DynamicRooFitter fitter(h1Data, cfg);

          fitter.DoFit();

          auto res = fitter.ExtractYieldsAndPurity();
          h1PuritySpectrum->SetBinContent(k + 1, res.purity.first);
          h1PuritySpectrum->SetBinError(k + 1, res.purity.second);

          // Save diagnostic plot directly to the task's output file
          std::string cName = "cFit_" + task.name + "_m" + std::to_string(i) + "_p" + std::to_string(k);
          fitter.SaveFitCanvas(task.outputFile, cName);

          // Cleanup temporary 1D histogram
          delete h1Data;
        }

        // Style and draw on the summary canvas
        AnalysisUtils::SetHistogramStyle(h1PuritySpectrum, AnalysisConstants::spectraColors[i]);
        task.canvas->cd();
        h1PuritySpectrum->DrawCopy(i == 0 ? "" : "SAME");
        task.outputFile->cd();
        h1PuritySpectrum->Write();

        // Cleanup temporary spectrum
        delete h1PuritySpectrum;
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PurityTask: TERMINATING AND SAVING..." << std::endl;

    // 1. Save the summary canvases
    fileOutputK0S->cd();
    canvasPurityK0S->Write();

    fileOutputPi->cd();
    canvasPurityPiTPC->Write();
    canvasPurityPiTOF->Write();

    // 2. Close the output files safely
    fileOutputK0S->Close();
    fileOutputPi->Close();

    // 3. Free up RAM (clean pointers)
    delete fileOutputK0S;
    delete fileOutputPi;
    delete canvasPurityK0S;
    delete canvasPurityPiTPC;
    delete canvasPurityPiTOF;
    delete h3K0SData;
    delete h3PiTPCData;
    delete h3PiTOFData;
    delete fitConfigManager;

    std::cout << "[INFO] PurityTask: DONE." << std::endl;
  }

 private:
  TH3F* h3K0SData{nullptr};
  TH3F* h3PiTPCData{nullptr};
  TH3F* h3PiTOFData{nullptr};

  TFile* fileOutputK0S{nullptr};
  TFile* fileOutputPi{nullptr};

  TCanvas* canvasPurityK0S{nullptr};
  TCanvas* canvasPurityPiTPC{nullptr};
  TCanvas* canvasPurityPiTOF{nullptr};

  std::vector<ParticleTask> particleTasks;
  FitConfigManager* fitConfigManager{nullptr};
};

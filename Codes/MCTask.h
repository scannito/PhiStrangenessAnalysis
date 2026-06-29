#pragma once

#include "AnalysisConstants.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "EfficiencyCalculator.h"
#include "IAnalysisTask.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class MCTask : public IAnalysisTask
{
 public:
  // Tells the WorkflowManager which JSON node to pass to this task
  std::string GetName() const override { return "mc_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] MCTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings; // Store the global settings for later use

    // 1. Check if MC input file is provided
    if (!taskConfig.HasMember("input_mc_file")) {
      throw std::runtime_error("[FATAL ERROR] MCTask: 'input_mc_file' missing in JSON!");
    }

    std::string inputFile = taskConfig["input_mc_file"].GetString();
    // Open the input file temporarily
    TFile* fileMCInput = new TFile(inputFile.c_str(), "READ");
    if (!fileMCInput || fileMCInput->IsZombie()) {
      throw std::runtime_error("[FATAL] MCTask: Cannot open MC input file: " + inputFile);
    }

    // 2. Allow JSON to override the internal ROOT directory base path
    if (taskConfig.HasMember("mc_base_path")) {
      mcBasePath = taskConfig["mc_base_path"].GetString();
    }

    outputDirectory = taskConfig["output_dir"].GetString();
    outputPrefix = taskConfig.HasMember("output_prefix") ? taskConfig["output_prefix"].GetString() : "";

    // 3. Define the particles and their respective histogram paths
    std::vector<ParticleConfig<3>> particles = {
      {"Phi", {"phi/h3PhiMCGen", "phi/h4PhiMCGenAssocReco", "phi/h4PhiMCReco"}},
      {"K0S", {"k0s/h3K0SMCGen", "k0s/h4K0SMCGenAssocReco", "k0s/h4K0SMCReco"}},
      {"Pi", {"pi/h3PiMCGen", "pi/h4PiMCGenAssocReco", "pi/h4PiMCReco"}}};

    dataCollection.reserve(particles.size());

    // 4. Load the histograms into RAM
    for (const auto& config : particles) {
      LoadedMC data;
      data.name = config.name;
      data.h3MCGen = static_cast<TH3F*>(fileMCInput->Get((mcBasePath + config.titles[0]).c_str()));
      if (data.h3MCGen) {
        data.h3MCGen->SetDirectory(0);
      }
      data.h4MCGenAssocReco = static_cast<THnSparseF*>(fileMCInput->Get((mcBasePath + config.titles[1]).c_str()));
      data.h4MCReco = static_cast<THnSparseF*>(fileMCInput->Get((mcBasePath + config.titles[2]).c_str()));

      if (!data.h3MCGen || !data.h4MCGenAssocReco || !data.h4MCReco) {
        std::cerr << "[WARNING] MCTask: Missing one or more histograms for " << data.name << ". Skipping." << std::endl;
        continue;
      }

      // Initialize the diagnostic canvases for this particle
      std::string cEffName = "c_" + data.name + "_Efficiency";
      std::string cSigName = "c_" + data.name + "_SignalLoss";
      data.canvasEfficiency = new TCanvas(cEffName.c_str(), cEffName.c_str(), 800, 600);
      data.canvasSignalLoss = new TCanvas(cSigName.c_str(), cSigName.c_str(), 800, 600);

      dataCollection.push_back(data);
    }

    // 5. Load Event-Level Histograms for Global Event Loss
    std::string genAssocRecoEventPath = mcBasePath + "event/hGenMCAssocRecoMultiplicityPercent";
    std::string genEventPath = mcBasePath + "event/hGenMCMultiplicityPercent";

    hEventMultGenAssocReco = static_cast<TH1F*>(fileMCInput->Get(genAssocRecoEventPath.c_str()));
    hEventMultGen = static_cast<TH1F*>(fileMCInput->Get(genEventPath.c_str()));

    if (!hEventMultGenAssocReco || !hEventMultGen) {
      std::cerr << "[WARNING] MCTask: Missing Event Multiplicity histograms. Event Loss will not be computed!" << std::endl;
    } else {
      hEventMultGenAssocReco->SetDirectory(0);
      hEventMultGen->SetDirectory(0);
    }

    // 6. Close the input file IMMEDIATELY. All required objects are now safely residing in RAM.
    fileMCInput->Close();
    delete fileMCInput;
    std::cout << "[INFO] MCTask: Input file closed safely. Data loaded in RAM." << std::endl;

    // 7. Open the master output file for corrections
    std::string outPath = outputDirectory + outputPrefix + "Corrections.root";
    fileMCOutput = new TFile(outPath.c_str(), "RECREATE");
    if (!fileMCOutput || fileMCOutput->IsZombie()) {
      throw std::runtime_error("[FATAL] MCTask: Cannot create output file: " + outPath);
    }

    std::cout << "[INFO] MCTask: Initialization complete." << std::endl;
  }

  void Run() override
  {
    std::cout << "[INFO] MCTask: RUNNING EFFICIENCY COMPUTATIONS..." << std::endl;

    fileMCOutput->cd();

    if (hEventMultGenAssocReco && hEventMultGen) {
      std::cout << " ---> Computing Global Event Efficiency (Event Loss)..." << std::endl;

      // Delegate the math to the static calculator
      TH1* hEventLoss = EfficiencyCalculator::ComputeEventEfficiency(hEventMultGenAssocReco, hEventMultGen);

      if (hEventLoss) {
        hEventLoss->Write();
        delete hEventLoss;
      }
    }

    for (auto& data : dataCollection) {
      std::cout << " ---> Processing particle: " << data.name << std::endl;

      // Prepare the 3D generator-level histogram
      data.h3MCGen->SetName(("h3" + data.name + "MCGen").c_str());
      data.h3MCGen->GetZaxis()->SetRange(1, AnalysisConstants::nBinY);
      data.h3MCGen->Sumw2();

      // 1. Compute and save the 3D and 2D correction map
      TH3* h3TotalMap = EfficiencyCalculator::Compute3DTotalMap(data);
      TH2* h2TotalMapMultInt = EfficiencyCalculator::Compute2DTotalMapMultIntegrated(data, globalCfgs);

      std::string fileMCOutputPerPartPath3D = outputDirectory + outputPrefix + "h3EffMap" + data.name + ".root";
      TFile* fileMCOutputPerPart3D = new TFile(fileMCOutputPerPartPath3D.c_str(), "RECREATE");
      fileMCOutputPerPart3D->cd();
      h3TotalMap->Write();
      fileMCOutputPerPart3D->Close();

      std::string fileMCOutputPerPartPath2D = outputDirectory + outputPrefix + "h2EffMap" + data.name + ".root";
      TFile* fileMCOutputPerPart2D = new TFile(fileMCOutputPerPartPath2D.c_str(), "RECREATE");
      fileMCOutputPerPart2D->cd();
      h2TotalMapMultInt->Write();
      fileMCOutputPerPart2D->Close();

      delete fileMCOutputPerPart3D;
      delete fileMCOutputPerPart2D;
      delete h3TotalMap;
      delete h2TotalMapMultInt;

      // 2. Compute 1D Spectra across multiplicity bins
      fileMCOutput->cd();

      // Process 1D Spectra across multiplicity bins
      for (int i{0}; i < globalCfgs.nBinMult; i++) {
        auto [h1Efficiency1D, h1SignalLoss1D] = EfficiencyCalculator::Compute1DMaps(data, globalCfgs, i);

        // Draw on canvases
        data.canvasEfficiency->cd();
        h1Efficiency1D->DrawCopy(i == 0 ? "" : "SAME");

        data.canvasSignalLoss->cd();
        h1SignalLoss1D->DrawCopy(i == 0 ? "" : "SAME");

        // Write to file
        h1Efficiency1D->Write();
        h1SignalLoss1D->Write();

        // Memory cleanup
        delete h1Efficiency1D;
        delete h1SignalLoss1D;
      }

      auto [h1Efficiency1D, h1SignalLoss1D] = EfficiencyCalculator::Compute1DMapsMultIntegrated(data, globalCfgs);

      data.canvasEfficiency->cd();
      h1Efficiency1D->DrawCopy("SAME");

      data.canvasSignalLoss->cd();
      h1SignalLoss1D->DrawCopy("SAME");

      h1Efficiency1D->Write();
      h1SignalLoss1D->Write();

      delete h1Efficiency1D;
      delete h1SignalLoss1D;
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] MCTask: TERMINATING AND CLEANING UP..." << std::endl;

    fileMCOutput->cd();

    // Save all diagnostic canvases and free the memory for the loaded objects
    for (auto& data : dataCollection) {
      if (data.canvasEfficiency) {
        data.canvasEfficiency->Write();
        delete data.canvasEfficiency;
      }
      if (data.canvasSignalLoss) {
        data.canvasSignalLoss->Write();
        delete data.canvasSignalLoss;
      }

      // WE MUST delete the source objects from RAM since the TFile is already closed!
      if (data.h3MCGen)
        delete data.h3MCGen;
      if (data.h4MCGenAssocReco)
        delete data.h4MCGenAssocReco;
      if (data.h4MCReco)
        delete data.h4MCReco;
    }

    if (hEventMultGenAssocReco)
      delete hEventMultGenAssocReco;
    if (hEventMultGen)
      delete hEventMultGen;

    // Close and delete the output file
    fileMCOutput->Close();
    delete fileMCOutput;

    std::cout << "[INFO] MCTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::vector<LoadedMC> dataCollection;

  TFile* fileMCOutput{nullptr};

  std::string outputDirectory;
  std::string outputPrefix{""};
  std::string mcBasePath{"phi-strangeness-correlation/phiStrangenessCorrelation/"};

  // --- Variables for Global Event Loss ---
  TH1F* hEventMultGenAssocReco{nullptr};
  TH1F* hEventMultGen{nullptr};
};

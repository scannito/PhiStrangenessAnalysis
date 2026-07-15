#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "EfficiencyCalculator.h"
#include "IAnalysisTask.h"

#include "TCanvas.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
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

    // 1. Check if a list of particle to compute MC corrections is provided
    if (!taskConfig.HasMember("mc_particles") || !taskConfig["mc_particles"].IsArray()) {
      throw std::runtime_error("[FATAL ERROR] MCTask: 'mc_particles' array missing in JSON!");
    }

    // 'input_mc_file' is now an optional TASK-LEVEL DEFAULT: used by particles that
    // don't specify their own "input_mc_file".
    std::string defaultInputFile = taskConfig.HasMember("input_mc_file") ? taskConfig["input_mc_file"].GetString() : "";

    // 2. Allow JSON to override the internal ROOT directory base path
    if (taskConfig.HasMember("mc_base_path")) {
      mcBasePath = taskConfig["mc_base_path"].GetString();
    }

    outputDirectory = taskConfig["output_dir"].GetString();
    outputPrefix = taskConfig.HasMember("output_prefix") ? taskConfig["output_prefix"].GetString() : "";

    // CCDB-ready per-particle maps live under outputDirectory/{binningName}/ on disk,
    // since the ROOT object inside must be named literally "ccdb_object" with no
    // internal subfolder. Ensure the directory exists — TFile won't create it.
    ccdbOutputDir = outputDirectory + globalCfgs.binningName + "/";
    std::error_code ec;
    std::filesystem::create_directories(ccdbOutputDir, ec);
    if (ec) {
      throw std::runtime_error("[FATAL] MCTask: Cannot create output directory '" + ccdbOutputDir + "': " + ec.message());
    }

    if (taskConfig.HasMember("particle_correction_mode"))
      particleCorrectionMode = static_cast<EfficiencyCalculator::ParticleCorrectionMode>(taskConfig["particle_correction_mode"].GetInt());

    // Input files may be shared across particles (default) or overridden per
    // particle via "input_mc_file". Open each distinct file only once.
    std::map<std::string, TFile*> openInputFiles;

    auto getOrOpenFile = [&](const std::string& path) -> TFile* {
      auto it = openInputFiles.find(path);
      if (it != openInputFiles.end())
        return it->second;
      TFile* f = new TFile(path.c_str(), "READ");
      if (!f || f->IsZombie())
        throw std::runtime_error("[FATAL] MCTask: Cannot open MC input file: " + path);
      openInputFiles[path] = f;
      return f;
    };

    dataCollection.reserve(taskConfig["mc_particles"].Size());

    // 3+4. Build the particle list from JSON and load its histograms directly.
    // By convention, histogram paths follow "{dirName}/h3{Name}MCGen",
    // "{dirName}/h4{Name}MCGenAssocReco", "{dirName}/h4{Name}MCReco", where
    // dirName defaults to the lowercased particle name ("dir_name" overrides it).
    // Each entry may specify its own "input_mc_file" — useful when different
    // species live in different MC production files (e.g. K0S vs Xi vs Pi).
    for (const auto& p : taskConfig["mc_particles"].GetArray()) {
      std::string name = p["name"].GetString();

      std::string dirName;
      if (p.HasMember("dir_name") && p["dir_name"].IsString()) {
        dirName = p["dir_name"].GetString();
      } else {
        dirName = name;
        std::transform(dirName.begin(), dirName.end(), dirName.begin(), ::tolower);
      }

      std::string particleInputFile = (p.HasMember("input_mc_file") && p["input_mc_file"].IsString())
                                        ? p["input_mc_file"].GetString()
                                        : defaultInputFile;
      if (particleInputFile.empty()) {
        throw std::runtime_error("[FATAL ERROR] MCTask: No input file for particle '" + name +
                                 "' (neither task-level 'input_mc_file' nor a per-particle override was provided)!");
      }

      TFile* fileMCInput = getOrOpenFile(particleInputFile);

      LoadedMC data;
      data.name = name;
      data.h3MCGen = static_cast<TH3F*>(fileMCInput->Get((mcBasePath + dirName + "/h3" + name + "MCGen").c_str()));
      if (data.h3MCGen) {
        data.h3MCGen->SetDirectory(0);
      }
      data.h4MCGenAssocReco = static_cast<THnSparseF*>(fileMCInput->Get((mcBasePath + dirName + "/h4" + name + "MCGenAssocReco").c_str()));
      data.h4MCReco = static_cast<THnSparseF*>(fileMCInput->Get((mcBasePath + dirName + "/h4" + name + "MCReco").c_str()));

      if (!data.h3MCGen || !data.h4MCGenAssocReco || !data.h4MCReco) {
        std::cerr << "[WARNING] MCTask: Missing one or more histograms for " << data.name
                  << " in '" << particleInputFile << "'. Skipping." << std::endl;
        if (data.h3MCGen)
          delete data.h3MCGen;
        if (data.h4MCGenAssocReco)
          delete data.h4MCGenAssocReco;
        if (data.h4MCReco)
          delete data.h4MCReco;
        continue;
      }

      std::string cEffName = "c_" + data.name + "_Efficiency";
      std::string cSigName = "c_" + data.name + "_SignalLoss";
      data.canvasEfficiency = new TCanvas(cEffName.c_str(), cEffName.c_str(), 800, 600);
      data.canvasSignalLoss = new TCanvas(cSigName.c_str(), cSigName.c_str(), 800, 600);

      dataCollection.push_back(data);
    }

    // 5. Load Event-Level Histograms for Global Event Loss.
    // O2 always produces these for every MC file regardless of which particle(s)
    // it contains, so they're identical duplicates across all opened files —
    // simply read them from whichever file happens to be open already.
    if (!openInputFiles.empty()) {
      TFile* anyFile = openInputFiles.begin()->second;

      std::string genAssocRecoEventPath = mcBasePath + "event/hGenMCAssocRecoMultiplicityPercent";
      std::string genEventPath = mcBasePath + "event/hGenMCMultiplicityPercent";

      hEventMultGenAssocReco = static_cast<TH1F*>(anyFile->Get(genAssocRecoEventPath.c_str()));
      hEventMultGen = static_cast<TH1F*>(anyFile->Get(genEventPath.c_str()));

      if (!hEventMultGenAssocReco || !hEventMultGen) {
        std::cerr << "[WARNING] MCTask: Missing Event Multiplicity histograms. Event Loss will not be computed!" << std::endl;
      } else {
        hEventMultGenAssocReco->SetDirectory(0);
        hEventMultGen->SetDirectory(0);
      }
    } else {
      std::cerr << "[WARNING] MCTask: No input files were opened. Event Loss will not be computed!" << std::endl;
    }

    // 6. Close the input files IMMEDIATELY. All required objects are now safely residing in RAM.
    for (auto& [path, file] : openInputFiles) {
      file->Close();
      delete file;
    }
    std::cout << "[INFO] MCTask: Input file(s) closed safely. Data loaded in RAM." << std::endl;

    // 7. Open the master output file for corrections
    std::string outPath = outputDirectory + outputPrefix + "Corrections.root";
    fileMCOutput = new TFile(outPath.c_str(), "UPDATE");
    if (!fileMCOutput || fileMCOutput->IsZombie()) {
      throw std::runtime_error("[FATAL] MCTask: Cannot create output file: " + outPath);
    }

    std::cout << "[INFO] MCTask: Initialization complete." << std::endl;
  }

  void Run() override
  {
    std::cout << "[INFO] MCTask: RUNNING EFFICIENCY COMPUTATIONS..." << std::endl;

    if (hEventMultGenAssocReco && hEventMultGen) {
      std::cout << " ---> Computing Global Event Efficiency (Event Loss)..." << std::endl;

      // Delegate the math to the static calculator
      TH1* hEventLoss = EfficiencyCalculator::ComputeEventEfficiency(hEventMultGenAssocReco, hEventMultGen);

      if (hEventLoss) {
        TDirectory* evLossDir = AnalysisUtils::GetOrCreatePath(fileMCOutput, {globalCfgs.binningName, "EvLoss"}, false);
        if (evLossDir)
          evLossDir->cd();
        hEventLoss->Write(nullptr, TObject::kOverwrite);
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
      TH3* h3TotalMap = EfficiencyCalculator::Compute3DTotalMap(data, particleCorrectionMode);

      std::string fileMCOutputPerPartPath3D = ccdbOutputDir + outputPrefix + "h3EffMap" + data.name + ".root";
      TFile* fileMCOutputPerPart3D = new TFile(fileMCOutputPerPartPath3D.c_str(), "RECREATE");
      if (!fileMCOutputPerPart3D || fileMCOutputPerPart3D->IsZombie()) {
        throw std::runtime_error("[FATAL] MCTask: Cannot create CCDB output file: " + fileMCOutputPerPartPath3D);
      }
      fileMCOutputPerPart3D->cd();
      h3TotalMap->SetName("ccdb_object");
      h3TotalMap->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart3D->Close();

      TH2* h2TotalMapMultInt = EfficiencyCalculator::Compute2DTotalMapMultIntegrated(data, globalCfgs, particleCorrectionMode);

      std::string fileMCOutputPerPartPath2D = ccdbOutputDir + outputPrefix + "h2EffMap" + data.name + ".root";
      TFile* fileMCOutputPerPart2D = new TFile(fileMCOutputPerPartPath2D.c_str(), "RECREATE");
      if (!fileMCOutputPerPart2D || fileMCOutputPerPart2D->IsZombie()) {
        throw std::runtime_error("[FATAL] MCTask: Cannot create CCDB output file: " + fileMCOutputPerPartPath2D);
      }
      fileMCOutputPerPart2D->cd();
      h2TotalMapMultInt->SetName("ccdb_object");
      h2TotalMapMultInt->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart2D->Close();

      // QA: error/relative-error maps alongside the CCDB map
      TH2* h2TotalMapMultIntError = EfficiencyCalculator::BuildErrorMap(h2TotalMapMultInt, "h2EffMapError" + data.name);
      TH2* h2TotalMapMultIntRelError = EfficiencyCalculator::BuildRelativeErrorMap(h2TotalMapMultInt, "h2EffMapRelError" + data.name);

      std::string fileMCOutputPerPartErrorPath2D = ccdbOutputDir + outputPrefix + "h2EffMapError" + data.name + ".root";
      TFile* fileMCOutputPerPartError2D = new TFile(fileMCOutputPerPartErrorPath2D.c_str(), "RECREATE");
      if (!fileMCOutputPerPartError2D || fileMCOutputPerPartError2D->IsZombie()) {
        throw std::runtime_error("[FATAL] MCTask: Cannot create CCDB output file: " + fileMCOutputPerPartErrorPath2D);
      }
      fileMCOutputPerPartError2D->cd();
      // h2TotalMapMultIntError->SetName("ccdb_object");
      if (h2TotalMapMultIntError)
        h2TotalMapMultIntError->Write(nullptr, TObject::kOverwrite);
      // h2TotalMapMultIntRelError->SetName("ccdb_object");
      if (h2TotalMapMultIntRelError)
        h2TotalMapMultIntRelError->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPartError2D->Close();

      delete fileMCOutputPerPart3D;
      delete fileMCOutputPerPart2D;
      delete h3TotalMap;
      delete h2TotalMapMultInt;
      delete h2TotalMapMultIntError;
      delete h2TotalMapMultIntRelError;

      // 2. Compute 1D Spectra across multiplicity bins
      TDirectory* accEffMultDir = AnalysisUtils::GetOrCreatePath(fileMCOutput, {globalCfgs.binningName, "AccEff", "MultBin"}, false);
      TDirectory* sigLossMultDir = AnalysisUtils::GetOrCreatePath(fileMCOutput, {globalCfgs.binningName, "SigLoss", "MultBin"}, false);

      // Process 1D Spectra across multiplicity bins
      for (int i{0}; i < globalCfgs.nBinMult; i++) {
        auto [h1Efficiency1D, h1SignalLoss1D] = EfficiencyCalculator::Compute1DMaps(data, globalCfgs, i);

        // Draw on canvases
        data.canvasEfficiency->cd();
        h1Efficiency1D->DrawCopy(i == 0 ? "" : "SAME");

        data.canvasSignalLoss->cd();
        h1SignalLoss1D->DrawCopy(i == 0 ? "" : "SAME");

        // Write to file
        if (accEffMultDir) {
          accEffMultDir->cd();
          h1Efficiency1D->Write(nullptr, TObject::kOverwrite);
        }
        if (sigLossMultDir) {
          sigLossMultDir->cd();
          h1SignalLoss1D->Write(nullptr, TObject::kOverwrite);
        }

        // Memory cleanup
        delete h1Efficiency1D;
        delete h1SignalLoss1D;
      }

      auto [h1Efficiency1D, h1SignalLoss1D] = EfficiencyCalculator::Compute1DMapsMultIntegrated(data, globalCfgs);

      data.canvasEfficiency->cd();
      h1Efficiency1D->DrawCopy("SAME");

      data.canvasSignalLoss->cd();
      h1SignalLoss1D->DrawCopy("SAME");

      if (accEffMultDir) {
        accEffMultDir->cd();
        h1Efficiency1D->Write(nullptr, TObject::kOverwrite);
      }
      if (sigLossMultDir) {
        sigLossMultDir->cd();
        h1SignalLoss1D->Write(nullptr, TObject::kOverwrite);
      }

      // Memory cleanup
      delete h1Efficiency1D;
      delete h1SignalLoss1D;
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] MCTask: TERMINATING AND CLEANING UP..." << std::endl;

    TDirectory* accEffSummaryDir = AnalysisUtils::GetOrCreatePath(fileMCOutput, {globalCfgs.binningName, "AccEff", "Summary"}, false);
    TDirectory* sigLossSummaryDir = AnalysisUtils::GetOrCreatePath(fileMCOutput, {globalCfgs.binningName, "SigLoss", "Summary"}, false);

    // Save all diagnostic canvases and free the memory for the loaded objects
    for (auto& data : dataCollection) {
      if (data.canvasEfficiency) {
        if (accEffSummaryDir) {
          accEffSummaryDir->cd();
          data.canvasEfficiency->Write(nullptr, TObject::kOverwrite);
        }
        delete data.canvasEfficiency;
      }
      if (data.canvasSignalLoss) {
        if (sigLossSummaryDir) {
          sigLossSummaryDir->cd();
          data.canvasSignalLoss->Write(nullptr, TObject::kOverwrite);
        }
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
  std::string ccdbOutputDir;
  std::string outputPrefix{""};
  std::string mcBasePath{"phi-strangeness-correlation/phiStrangenessCorrelation/"};

  // --- Variables for Global Event Loss ---
  TH1F* hEventMultGenAssocReco{nullptr};
  TH1F* hEventMultGen{nullptr};

  EfficiencyCalculator::ParticleCorrectionMode particleCorrectionMode{EfficiencyCalculator::ParticleCorrectionMode::EfficiencyOnly};
};

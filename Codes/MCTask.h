#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "EfficiencyCalculator.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"

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
#include <memory>
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
    auto particles = RequireArray(taskConfig, "mc_particles", "MCTask");
    /*if (!taskConfig.HasMember("mc_particles") || !taskConfig["mc_particles"].IsArray()) {
      throw std::runtime_error("[FATAL ERROR] MCTask: 'mc_particles' array missing in JSON!");
    }*/

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
    std::map<std::string, std::shared_ptr<TFile>> openInputFiles;

    auto getOrOpenFile = [&](const std::string& path) -> TFile* {
      auto it = openInputFiles.find(path);
      if (it != openInputFiles.end())
        return it->second.get();
      std::unique_ptr<TFile> f = OpenOrThrow(path, "READ", "MCTask");
      openInputFiles[path] = std::move(f);
      return openInputFiles[path].get();
    };

    dataCollection.reserve(particles.Size());

    // 3+4. Build the particle list from JSON and load its histograms directly.
    // By convention, histogram paths follow "{dirName}/h3{Name}MCGen",
    // "{dirName}/h4{Name}MCGenAssocReco", "{dirName}/h4{Name}MCReco", where
    // dirName defaults to the lowercased particle name ("dir_name" overrides it).
    // Each entry may specify its own "input_mc_file" — useful when different
    // species live in different MC production files (e.g. K0S vs Xi vs Pi).
    for (const auto& p : particles) {
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

      LoadedMC data;
      data.name = name;

      std::string h3MCGenName = mcBasePath + dirName + "/h3" + name + "MCGen";
      std::string h4MCGenAssocRecoName = mcBasePath + dirName + "/h4" + name + "MCGenAssocReco";
      std::string h4MCRecoName = mcBasePath + dirName + "/h4" + name + "MCReco";

      data.h3MCGen = GetUniqueOrThrow<TH3F>(getOrOpenFile(particleInputFile), h3MCGenName, "MCTask");
      data.h4MCGenAssocReco = GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCGenAssocRecoName, "MCTask");
      data.h4MCReco = GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCRecoName, "MCTask");

      // These three are divided by each other to build efficiency and signal
      // loss, so they must share their axes whatever the configuration says.
      // Checked directly between them: it holds even with nothing declared.
      // h3MCGen axes: (mult, pT, y) -- h4* axes: (zvtx, mult, pT, y)
      BinningUtils::RequireSameAxis(data.h3MCGen->GetYaxis(), data.h4MCReco->GetAxis(2),
                                    h3MCGenName + " pT", h4MCRecoName + " pT");
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(2), data.h4MCReco->GetAxis(2),
                                    h4MCGenAssocRecoName + " pT", h4MCRecoName + " pT");
      BinningUtils::RequireSameAxis(data.h3MCGen->GetXaxis(), data.h4MCReco->GetAxis(1),
                                    h3MCGenName + " multiplicity", h4MCRecoName + " multiplicity");
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(1), data.h4MCReco->GetAxis(1),
                                    h4MCGenAssocRecoName + " multiplicity", h4MCRecoName + " multiplicity");
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(0), data.h4MCReco->GetAxis(0),
                                    h4MCGenAssocRecoName + " zvtx", h4MCRecoName + " zvtx");
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(3), data.h4MCReco->GetAxis(3),
                                    h4MCGenAssocRecoName + " rapidity", h4MCRecoName + " rapidity");
      BinningUtils::RequireSameAxis(data.h3MCGen->GetZaxis(), data.h4MCReco->GetAxis(3),
                                    h3MCGenName + " rapidity", h4MCRecoName + " rapidity");

      // The multiplicity binning is kept because the loops below address its
      // bins. The pT binning is not: this task consumes the pT dimension by
      // projecting the whole axis, so nothing here indexes it - it is resolved
      // only to have it verified against the declared production.
      const std::string origin = "'" + particleInputFile + "'";
      multBinning = globalCfgs.ResolveMultBinning(data.h3MCGen->GetXaxis(), h3MCGenName + " in " + origin);
      globalCfgs.ResolvePtBinning(name, data.h3MCGen->GetYaxis(), h3MCGenName + " in " + origin);

      if (p.HasMember("rebinning_pt") && p["rebinning_pt"].IsArray()) {
        std::vector<double> bins;
        bins.reserve(p["rebinning_pt"].Size());
        for (const auto& v : p["rebinning_pt"].GetArray()) {
          bins.push_back(v.GetDouble());
        }
        if (bins.size() < 2) {
          throw std::runtime_error("[FATAL ERROR] MCTask: 'rebinning_pt' for particle '" + name + "' must have at least 2 edges!");
        }
        data.rebinningPt = std::move(bins);
      }

      std::string cEffName = "c_" + data.name + "_Efficiency";
      std::string cSigName = "c_" + data.name + "_SignalLoss";

      data.canvasEfficiency = std::make_unique<TCanvas>(cEffName.c_str(), cEffName.c_str(), 800, 600);
      data.canvasSignalLoss = std::make_unique<TCanvas>(cSigName.c_str(), cSigName.c_str(), 800, 600);

      dataCollection.push_back(std::move(data));
    }

    // 5. Load Event-Level Histograms for Global Event Loss.
    // O2 always produces these for every MC file regardless of which particle(s)
    // it contains, so they're identical duplicates across all opened files —
    // simply read them from whichever file happens to be open already.
    if (!openInputFiles.empty()) {
      std::string genAssocRecoEventPath = mcBasePath + "event/hGenMCAssocRecoMultiplicityPercent";
      std::string genEventPath = mcBasePath + "event/hGenMCMultiplicityPercent";

      hEventMultGenAssocReco = GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genAssocRecoEventPath, "MCTask");
      hEventMultGen = GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genEventPath, "MCTask");
    } else {
      std::cerr << "[WARNING] MCTask: No input files were opened. Event Loss will not be computed!" << std::endl;
    }

    for (auto& [name, file] : openInputFiles) {
      if (file) {
        file->Close();
      }
    }

    // 6. Open the master output file for corrections
    std::string outPath = outputDirectory + outputPrefix + "Corrections.root";
    fileMCOutput = OpenOrThrow(outPath, "UPDATE", "MCTask");

    std::cout << "[INFO] MCTask: Initialization complete." << std::endl;
  }

  void Run() override
  {
    std::cout << "[INFO] MCTask: RUNNING EFFICIENCY COMPUTATIONS..." << std::endl;

    if (hEventMultGenAssocReco && hEventMultGen) {
      std::cout << " ---> Computing Global Event Efficiency (Event Loss)..." << std::endl;

      // Delegate the math to the static calculator
      std::unique_ptr<TH1> hEventLoss = EfficiencyCalculator::ComputeEventEfficiency(hEventMultGenAssocReco.get(), hEventMultGen.get());

      if (hEventLoss) {
        TDirectory* evLossDir = AnalysisUtils::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "EvLoss"}, false);
        if (evLossDir)
          evLossDir->cd();
        hEventLoss->Write(nullptr, TObject::kOverwrite);
      }
    }

    for (auto& data : dataCollection) {
      std::cout << " ---> Processing particle: " << data.name << std::endl;

      // Prepare the 3D generator-level histogram
      data.h3MCGen->SetName(("h3" + data.name + "MCGen").c_str());
      data.h3MCGen->GetZaxis()->SetRange(1, data.h3MCGen->GetZaxis()->GetNbins());
      // Only create the error structure if the histogram read from file lacks it
      if (data.h3MCGen->GetSumw2N() == 0)
        data.h3MCGen->Sumw2();

      // 1. Compute and save the 3D and 2D correction map
      std::unique_ptr<TH3> h3TotalMap = EfficiencyCalculator::Compute3DTotalMap(data, particleCorrectionMode);

      std::string fileMCOutputPerPartPath3D = ccdbOutputDir + outputPrefix + "h3EffMap" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPart3D = OpenOrThrow(fileMCOutputPerPartPath3D, "RECREATE", "MCTask");
      fileMCOutputPerPart3D->cd();
      h3TotalMap->SetName("ccdb_object");
      h3TotalMap->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart3D->Close();

      std::unique_ptr<TH2> h2TotalMapMultInt = EfficiencyCalculator::Compute2DTotalMapMultIntegrated(data, particleCorrectionMode);

      std::string fileMCOutputPerPartPath2D = ccdbOutputDir + outputPrefix + "h2EffMap" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPart2D = OpenOrThrow(fileMCOutputPerPartPath2D, "RECREATE", "MCTask");
      fileMCOutputPerPart2D->cd();
      h2TotalMapMultInt->SetName("ccdb_object");
      h2TotalMapMultInt->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart2D->Close();

      // QA: error/relative-error maps alongside the CCDB map
      std::unique_ptr<TH2> h2TotalMapMultIntError = EfficiencyCalculator::BuildErrorMap(h2TotalMapMultInt.get(), "h2EffMapError" + data.name);
      std::unique_ptr<TH2> h2TotalMapMultIntRelError = EfficiencyCalculator::BuildRelativeErrorMap(h2TotalMapMultInt.get(), "h2EffMapRelError" + data.name);

      std::string fileMCOutputPerPartErrorPath2D = ccdbOutputDir + outputPrefix + "h2EffMapError" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPartError2D = OpenOrThrow(fileMCOutputPerPartErrorPath2D, "RECREATE", "MCTask");
      fileMCOutputPerPartError2D->cd();
      // h2TotalMapMultIntError->SetName("ccdb_object");
      if (h2TotalMapMultIntError)
        h2TotalMapMultIntError->Write(nullptr, TObject::kOverwrite);
      // h2TotalMapMultIntRelError->SetName("ccdb_object");
      if (h2TotalMapMultIntRelError)
        h2TotalMapMultIntRelError->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPartError2D->Close();

      // 2. Compute 1D Spectra across multiplicity bins
      TDirectory* accEffMultDir = AnalysisUtils::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "AccEff", "MultBin"}, false);
      TDirectory* sigLossMultDir = AnalysisUtils::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "SigLoss", "MultBin"}, false);

      // Process 1D Spectra across multiplicity bins
      for (int i{0}; i < BinningUtils::NBins(multBinning); i++) {
        // 'rebinning_pt' is applied inside the calculator, on the counts, before
        // they are divided: rebinning the ratio afterwards would sum efficiencies.
        auto [h1Efficiency1D, h1SignalLoss1D] = EfficiencyCalculator::Compute1DMaps(data, i, globalCfgs.GetSpectraColor(i));

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
      }

      auto [h1Efficiency1D, h1SignalLoss1D] = EfficiencyCalculator::Compute1DMapsMultIntegrated(data);

      // Draw on canvases
      data.canvasEfficiency->cd();
      h1Efficiency1D->DrawCopy("SAME");

      data.canvasSignalLoss->cd();
      h1SignalLoss1D->DrawCopy("SAME");

      // Write to file
      if (accEffMultDir) {
        accEffMultDir->cd();
        h1Efficiency1D->Write(nullptr, TObject::kOverwrite);
      }
      if (sigLossMultDir) {
        sigLossMultDir->cd();
        h1SignalLoss1D->Write(nullptr, TObject::kOverwrite);
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] MCTask: TERMINATING AND CLEANING UP..." << std::endl;

    TDirectory* accEffSummaryDir = AnalysisUtils::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "AccEff", "Summary"}, false);
    TDirectory* sigLossSummaryDir = AnalysisUtils::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "SigLoss", "Summary"}, false);

    // Save all diagnostic canvases and free the memory for the loaded objects
    for (auto& data : dataCollection) {
      if (data.canvasEfficiency) {
        if (accEffSummaryDir) {
          accEffSummaryDir->cd();
          data.canvasEfficiency->Write(nullptr, TObject::kOverwrite);
        }
      }
      if (data.canvasSignalLoss) {
        if (sigLossSummaryDir) {
          sigLossSummaryDir->cd();
          data.canvasSignalLoss->Write(nullptr, TObject::kOverwrite);
        }
      }
    }

    if (fileMCOutput)
      fileMCOutput->Close();

    std::cout << "[INFO] MCTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::vector<LoadedMC> dataCollection;

  // Read from the input files in Init(), never from the configuration
  std::vector<double> multBinning;

  std::unique_ptr<TFile> fileMCOutput;

  std::string outputDirectory;
  std::string ccdbOutputDir;
  std::string outputPrefix{""};
  std::string mcBasePath{"phi-strange-correlation/phiStrangenessCorrelation/"};

  // --- Variables for Global Event Loss ---
  std::unique_ptr<TH1F> hEventMultGenAssocReco;
  std::unique_ptr<TH1F> hEventMultGen;

  EfficiencyCalculator::ParticleCorrectionMode particleCorrectionMode{EfficiencyCalculator::ParticleCorrectionMode::EfficiencyOnly};
};

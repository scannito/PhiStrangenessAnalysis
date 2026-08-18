#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "EfficiencyCalculator.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TCanvas.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <algorithm>
#include <cctype>
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
  // The mode is defined by EfficiencyCalculator, which is what switches on it;
  // this alias only spares the qualification at every mention below.
  using CorrectionMode = EfficiencyCalculator::ParticleCorrectionMode;

  // Tells the WorkflowManager which JSON node to pass to this task
  std::string GetName() const override { return "mc_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] MCTask: INITIALIZING..." << std::endl;

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

    // 1. Check if a list of particle to compute MC corrections is provided
    auto particles = JsonConfig::RequireArray(taskConfig, "mc_particles", "MCTask");

    // Task-level input file. Required: a particle may point somewhere else with
    // its own "input_mc_file", but there must be a file to fall back on, and an
    // empty string is not one.
    std::string defaultInputFile = JsonConfig::RequireString(taskConfig, "input_mc_file", GetName());

    // 2. Allow JSON to override the internal ROOT directory base path
    mcBasePath = JsonConfig::RequireString(taskConfig, "mc_base_path", GetName());

    outputDirectory = JsonConfig::RequireString(taskConfig, "output_dir", GetName());
    outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

    // CCDB-ready per-particle maps live under outputDirectory/{binningName}/ on disk,
    // since the ROOT object inside must be named literally "ccdb_object" with no
    // internal subfolder. Ensure the directory exists — TFile won't create it.
    ccdbOutputDir = outputDirectory + globalCfgs.binningName + "/";
    std::error_code ec;
    std::filesystem::create_directories(ccdbOutputDir, ec);
    if (ec) {
      throw std::runtime_error("[FATAL] MCTask: Cannot create output directory '" + ccdbOutputDir + "': " + ec.message());
    }

    particleCorrectionMode = JsonConfig::OptionalEnum<CorrectionMode>(taskConfig, "particle_correction_mode", "efficiency_only",
                                                                      {{"efficiency_only", CorrectionMode::EfficiencyOnly},
                                                                       {"signal_loss_only", CorrectionMode::SignalLossOnly},
                                                                       {"combined", CorrectionMode::Combined}},
                                                                      GetName());

    // Input files may be shared across particles (default) or overridden per
    // particle via "input_mc_file". Open each distinct file only once.
    std::map<std::string, std::shared_ptr<TFile>> openInputFiles;

    auto getOrOpenFile = [&](const std::string& path) -> TFile* {
      auto it = openInputFiles.find(path);
      if (it != openInputFiles.end())
        return it->second.get();
      std::unique_ptr<TFile> f = RootIO::OpenOrThrow(path, "READ", "MCTask");
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

      // Default: the particle name lowercased, which is how the O2 task names
      // its output directories. Override it when several species share one
      // directory - Lambda and AntiLambda both live under "lambda" - since the
      // histogram name still carries the species, only the folder is shared.
      std::string dirName = name;
      std::ranges::transform(dirName, dirName.begin(), [](unsigned char c) { return std::tolower(c); });
      dirName = JsonConfig::OptionalString(p, "dir_name", dirName, GetName());

      std::string particleInputFile = JsonConfig::OptionalString(p, "input_mc_file", defaultInputFile, GetName());
      // The task-level key is required, so this only fires on an explicitly
      // empty string, in the JSON or in the per-particle override.
      if (particleInputFile.empty()) {
        throw std::runtime_error("[FATAL ERROR] MCTask: empty 'input_mc_file' for particle '" + name + "'!");
      }

      LoadedMC data;
      data.name = name;

      std::string h3MCGenName = mcBasePath + dirName + "/h3" + name + "MCGen";
      std::string h4MCGenAssocRecoName = mcBasePath + dirName + "/h4" + name + "MCGenAssocReco";
      std::string h4MCRecoName = mcBasePath + dirName + "/h4" + name + "MCReco";

      data.h3MCGen = RootIO::GetUniqueOrThrow<TH3F>(getOrOpenFile(particleInputFile), h3MCGenName, "MCTask");
      data.h4MCGenAssocReco = RootIO::GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCGenAssocRecoName, "MCTask");
      data.h4MCReco = RootIO::GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCRecoName, "MCTask");

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

      if (auto rebin = JsonConfig::TryArray(p, "rebinning_pt", GetName())) {
        std::vector<double> bins = JsonConfig::ReadNumberArray(*rebin, "rebinning_pt for '" + name + "'", GetName());
        if (bins.size() < 2) {
          throw std::runtime_error("[FATAL ERROR] MCTask: 'rebinning_pt' for particle '" + name + "' must have at least 2 edges!");
        }
        data.rebinningPt = std::move(bins);
      }

      // Names and which ones exist both follow from what was just filled in.
      data.CreateCanvases();

      dataCollection.push_back(std::move(data));
    }

    // 5. Load Event-Level Histograms for Global Event Loss.
    // O2 always produces these for every MC file regardless of which particle(s)
    // it contains, so they're identical duplicates across all opened files —
    // simply read them from whichever file happens to be open already.
    if (!openInputFiles.empty()) {
      std::string genAssocRecoEventPath = mcBasePath + "event/hGenMCAssocRecoMultiplicityPercent";
      std::string genEventPath = mcBasePath + "event/hGenMCMultiplicityPercent";

      hEventMultGenAssocReco = RootIO::GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genAssocRecoEventPath, "MCTask");
      hEventMultGen = RootIO::GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genEventPath, "MCTask");
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
    fileMCOutput = RootIO::OpenOrThrow(outPath, "UPDATE", "MCTask");

    // This run rewrites its whole scheme directory - AccEff, SigLoss, EvLoss and the
    // stamps - so anything a previous one left there would sit beside the new maps
    // with nothing to tell them apart. Not hypothetical: the moment the object names
    // change, the file would carry both generations and the reader would have no way
    // to know which is current.
    //
    // UPDATE and not RECREATE because only this subtree is ours: a run under a
    // different 'binning_name' writing to the same file keeps its own.
    RootIO::ClearPath(fileMCOutput.get(), {globalCfgs.binningName});

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
        TDirectory* evLossDir = RootIO::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "EvLoss"}, false);
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
      std::unique_ptr<TFile> fileMCOutputPerPart3D = RootIO::OpenOrThrow(fileMCOutputPerPartPath3D, "RECREATE", "MCTask");
      fileMCOutputPerPart3D->cd();
      h3TotalMap->SetName("ccdb_object");
      h3TotalMap->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart3D->Close();

      std::unique_ptr<TH2> h2TotalMapMultInt = EfficiencyCalculator::Compute2DTotalMapMultIntegrated(data, particleCorrectionMode);

      std::string fileMCOutputPerPartPath2D = ccdbOutputDir + outputPrefix + "h2EffMap" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPart2D = RootIO::OpenOrThrow(fileMCOutputPerPartPath2D, "RECREATE", "MCTask");
      fileMCOutputPerPart2D->cd();
      h2TotalMapMultInt->SetName("ccdb_object");
      h2TotalMapMultInt->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart2D->Close();

      // QA: error/relative-error maps alongside the CCDB map
      std::unique_ptr<TH2> h2TotalMapMultIntError = EfficiencyCalculator::BuildErrorMap(h2TotalMapMultInt.get(), "h2EffMapError" + data.name);
      std::unique_ptr<TH2> h2TotalMapMultIntRelError = EfficiencyCalculator::BuildRelativeErrorMap(h2TotalMapMultInt.get(), "h2EffMapRelError" + data.name);

      std::string fileMCOutputPerPartErrorPath2D = ccdbOutputDir + outputPrefix + "h2EffMapError" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPartError2D = RootIO::OpenOrThrow(fileMCOutputPerPartErrorPath2D, "RECREATE", "MCTask");
      fileMCOutputPerPartError2D->cd();
      // h2TotalMapMultIntError->SetName("ccdb_object");
      if (h2TotalMapMultIntError)
        h2TotalMapMultIntError->Write(nullptr, TObject::kOverwrite);
      // h2TotalMapMultIntRelError->SetName("ccdb_object");
      if (h2TotalMapMultIntRelError)
        h2TotalMapMultIntRelError->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPartError2D->Close();

      // 2. Compute 1D Spectra across multiplicity bins
      //
      // The maps at the source binning exist only when 'rebinning_pt' merged the
      // counts. They go on their own pair of canvases: each canvas overlays the
      // multiplicity bins against each other, so mixing two binnings on one pad
      // would make exactly the comparison it is drawn for unreadable.
      auto drawMaps = [&](const EfficiencyCalculator::Maps1D& maps, const char* drawOption) {
        data.canvasEfficiency->cd();
        maps.efficiency->DrawCopy(drawOption);

        data.canvasSignalLoss->cd();
        maps.signalLoss->DrawCopy(drawOption);

        if (maps.efficiencySourceBinning && data.canvasEfficiencySourceBinning) {
          data.canvasEfficiencySourceBinning->cd();
          maps.efficiencySourceBinning->DrawCopy(drawOption);
        }
        if (maps.signalLossSourceBinning && data.canvasSignalLossSourceBinning) {
          data.canvasSignalLossSourceBinning->cd();
          maps.signalLossSourceBinning->DrawCopy(drawOption);
        }
      };

      // The source-binning maps get a subdirectory of their own rather than
      // sitting next to the analysis ones, and it is created by the write itself:
      // the directory then exists if and only if something was put in it, with no
      // second condition to keep in agreement. The name suffix stays anyway, since
      // once an object leaves the file its directory is no longer part of its
      // identity.
      auto writeInto = [&](const std::vector<std::string>& path, TH1* h) {
        if (!h)
          return;
        if (TDirectory* dir = RootIO::GetOrCreatePath(fileMCOutput.get(), path, false)) {
          dir->cd();
          h->Write(nullptr, TObject::kOverwrite);
        }
      };

      auto writeMaps = [&](const EfficiencyCalculator::Maps1D& maps) {
        writeInto({globalCfgs.binningName, "AccEff", "MultBin"}, maps.efficiency.get());
        writeInto({globalCfgs.binningName, "SigLoss", "MultBin"}, maps.signalLoss.get());
        writeInto({globalCfgs.binningName, "AccEff", "MultBin", "SourceBinning"}, maps.efficiencySourceBinning.get());
        writeInto({globalCfgs.binningName, "SigLoss", "MultBin", "SourceBinning"}, maps.signalLossSourceBinning.get());
      };

      for (int i{0}; i < BinningUtils::NBins(multBinning); i++) {
        // 'rebinning_pt' is applied inside the calculator, on the counts, before
        // they are divided: rebinning the ratio afterwards would sum efficiencies.
        EfficiencyCalculator::Maps1D maps = EfficiencyCalculator::Compute1DMaps(data, i, globalCfgs.GetSpectraColor(i));

        drawMaps(maps, i == 0 ? "" : "SAME");
        writeMaps(maps);
      }

      EfficiencyCalculator::Maps1D mapsInt = EfficiencyCalculator::Compute1DMapsMultIntegrated(data);

      drawMaps(mapsInt, "SAME");
      writeMaps(mapsInt);
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] MCTask: TERMINATING AND CLEANING UP..." << std::endl;

    // The pT binning of every correction is carried by its own axis, so the
    // consumer can verify it by reading it. The multiplicity binning is not: it
    // survives only as an index in the names ("..._multBin3"), and the correlation
    // task addresses those names with the bin indices of the DATA. Record it, so
    // that a mismatch between the two productions is detectable instead of silent.
    if (TDirectory* schemeDir = RootIO::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName}, false))
      RootIO::WriteBinningStamp(schemeDir, "binning_mult", multBinning);

    RootIO::WriteProvenance(RootIO::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName, "Provenance"}, false), provenance);

    // Same rule as the histograms: the directory is created by the write, so a run
    // with no merging leaves no empty folders behind.
    auto writeCanvas = [&](const std::vector<std::string>& path, TCanvas* canvas) {
      if (!canvas)
        return;
      if (TDirectory* dir = RootIO::GetOrCreatePath(fileMCOutput.get(), path, false)) {
        dir->cd();
        canvas->Write(nullptr, TObject::kOverwrite);
      }
    };

    // Save all diagnostic canvases and free the memory for the loaded objects
    for (auto& data : dataCollection) {
      writeCanvas({globalCfgs.binningName, "AccEff", "Summary"}, data.canvasEfficiency.get());
      writeCanvas({globalCfgs.binningName, "SigLoss", "Summary"}, data.canvasSignalLoss.get());
      writeCanvas({globalCfgs.binningName, "AccEff", "Summary", "SourceBinning"}, data.canvasEfficiencySourceBinning.get());
      writeCanvas({globalCfgs.binningName, "SigLoss", "Summary", "SourceBinning"}, data.canvasSignalLossSourceBinning.get());
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

  // Filled in Init, written into the output file in Terminate
  std::map<std::string, std::string> provenance;
  std::string outputPrefix{""};
  std::string mcBasePath;

  // --- Variables for Global Event Loss ---
  std::unique_ptr<TH1F> hEventMultGenAssocReco;
  std::unique_ptr<TH1F> hEventMultGen;

  CorrectionMode particleCorrectionMode{CorrectionMode::EfficiencyOnly};
};

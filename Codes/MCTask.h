#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "CanvasManager.h"
#include "EfficiencyCalculator.h"
#include "HistogramUtils.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
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
#include <format>
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

    // 1. Check if a list of particle to compute MC corrections is provided
    auto particles = JsonConfig::RequireArray(taskConfig, "mc_particles", GetName());

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
      throw std::runtime_error(std::format("[FATAL] {}: Cannot create output directory '{}': {}", GetName(), ccdbOutputDir, ec.message()));
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
      std::unique_ptr<TFile> f = RootIO::OpenOrThrow(path, "READ", GetName());
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
    int pIdx = 0;
    for (const auto& p : particles) {
      const std::string ctx = std::format("{} mc_particles[{}]", GetName(), pIdx++);
      JsonConfig::RequireKnownKeys(p, {"name", "dir_name", "input_mc_file", "rebinning_pt"}, ctx);
      std::string name = JsonConfig::RequireString(p, "name", ctx);

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
        throw std::runtime_error(std::format("[FATAL] MCTask: empty 'input_mc_file' for particle '{}'!", name));
      }

      LoadedMC data;
      data.name = name;

      std::string h3MCGenName = mcBasePath + dirName + "/h3" + name + "MCGen";
      std::string h4MCGenAssocRecoName = mcBasePath + dirName + "/h4" + name + "MCGenAssocReco";
      std::string h4MCRecoName = mcBasePath + dirName + "/h4" + name + "MCReco";

      data.h3MCGen = RootIO::GetUniqueOrThrow<TH3F>(getOrOpenFile(particleInputFile), h3MCGenName, GetName());
      data.h4MCGenAssocReco = RootIO::GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCGenAssocRecoName, GetName());
      data.h4MCReco = RootIO::GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCRecoName, GetName());

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
        std::vector<double> bins = JsonConfig::ReadNumberArray(*rebin, std::format("rebinning_pt for '{}'", name), GetName());
        if (bins.size() < 2) {
          throw std::runtime_error(std::format("[FATAL] MCTask: 'rebinning_pt' for particle '{}' must have at least 2 edges!", name));
        }
        data.rebinningPt = std::move(bins);
      }

      // Register particle summary canvases with their target ROOT subdirectories.
      // Each one declares its Y range here; the manager applies it when it writes.
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas("c_" + name + "_Efficiency", name + " Acceptance x Efficiency",
                                                             {globalCfgs.binningName, "AccEff", "Summary"}));
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas("c_" + name + "_SignalLoss", name + " Signal Loss",
                                                             {globalCfgs.binningName, "SigLoss", "Summary"}));
      if (data.rebinningPt) {
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas("c_" + name + "_Efficiency_sourceBinning", name + " Efficiency (Source Binning)",
                                                               {globalCfgs.binningName, "AccEff", "Summary", "SourceBinning"}));
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas("c_" + name + "_SignalLoss_sourceBinning", name + " Signal Loss (Source Binning)",
                                                               {globalCfgs.binningName, "SigLoss", "Summary", "SourceBinning"}));
      }

      dataCollection.push_back(std::move(data));
    }

    // 5. Load Event-Level Histograms for Global Event Loss.
    // O2 always produces these for every MC file regardless of which particle(s)
    // it contains, so they're identical duplicates across all opened files —
    // simply read them from whichever file happens to be open already.
    if (!openInputFiles.empty()) {
      std::string genAssocRecoEventPath = mcBasePath + "event/hGenMCAssocRecoMultiplicityPercent";
      std::string genEventPath = mcBasePath + "event/hGenMCMultiplicityPercent";

      hEventMultGenAssocReco = RootIO::GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genAssocRecoEventPath, GetName());
      hEventMultGen = RootIO::GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genEventPath, GetName());

      // The event loss is addressed below with the multiplicity bin index of the
      // particle maps, so the two must share their axis.
      if (!dataCollection.empty()) {
        BinningUtils::RequireSameAxis(hEventMultGen->GetXaxis(), dataCollection.front().h3MCGen->GetXaxis(),
                                      genEventPath + " multiplicity", "h3MCGen multiplicity");
        BinningUtils::RequireSameAxis(hEventMultGenAssocReco->GetXaxis(), hEventMultGen->GetXaxis(),
                                      genAssocRecoEventPath + " multiplicity", genEventPath + " multiplicity");
      }

      // OverEventLoss canvases are registered only when event histograms exist
      for (const auto& data : dataCollection) {
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas("c_" + data.name + "_SignalLossOverEventLoss",
                                                               data.name + " Signal Loss / Event Loss",
                                                               {globalCfgs.binningName, "SigLoss", "Summary", "OverEvLoss"}));
        if (data.rebinningPt) {
          canvasManager.SetAutoYRange(canvasManager.CreateCanvas("c_" + data.name + "_SignalLossOverEventLoss_sourceBinning",
                                                                 data.name + " Signal Loss / Event Loss (Source Binning)",
                                                                 {globalCfgs.binningName, "SigLoss", "Summary", "OverEvLoss", "SourceBinning"}));
        }
      }
    } else {
      Logger::Warn("{}: No input files were opened. Event Loss will not be computed!", GetName());
    }

    for (auto& [name, file] : openInputFiles) {
      if (file) {
        file->Close();
      }
    }

    // 6. Open the master output file for corrections
    std::string outPath = outputDirectory + outputPrefix + "Corrections.root";
    fileMCOutput = RootIO::OpenOrThrow(outPath, "UPDATE", GetName());

    // This run rewrites its whole scheme directory - AccEff, SigLoss, EvLoss and the
    // stamps - so anything a previous one left there would sit beside the new maps
    // with nothing to tell them apart. Not hypothetical: the moment the object names
    // change, the file would carry both generations and the reader would have no way
    // to know which is current.
    //
    // UPDATE and not RECREATE because only this subtree is ours: a run under a
    // different 'binning_name' writing to the same file keeps its own.
    RootIO::ClearPath(fileMCOutput.get(), {globalCfgs.binningName});

    Logger::Info("{}: Initialization complete.", GetName());
  }

  void Run() override
  {
    Logger::Info("{}: RUNNING EFFICIENCY COMPUTATIONS...", GetName());

    // Writes one object at its path, creating the directories; nothing, when there is no object or the path
    // cannot be made (GetOrCreatePath says why), rather than into whatever directory is current.
    auto writeInto = [&](const std::vector<std::string>& path, TH1* h) {
      if (!h)
        return;
      if (TDirectory* dir = RootIO::GetOrCreatePath(fileMCOutput.get(), path, false)) {
        dir->cd();
        h->Write(nullptr, TObject::kOverwrite);
      }
    };

    std::unique_ptr<TH1> hEventLoss;

    if (hEventMultGenAssocReco && hEventMultGen) {
      Logger::Info(" ---> Computing Global Event Efficiency (Event Loss)...");

      // Delegate the math to the static calculator
      hEventLoss = EfficiencyCalculator::ComputeEventEfficiency(hEventMultGenAssocReco.get(), hEventMultGen.get());

      writeInto(OutputNames::EventLossPath(globalCfgs.binningName), hEventLoss.get());
    }

    // The source-binning maps in a subdirectory of their own (see writeMaps), as copies: each name is one directory.
    const std::vector<std::string> effPath = OutputNames::EfficiencyPath(globalCfgs.binningName);
    const std::vector<std::string> lossPath = OutputNames::SignalLossPath(globalCfgs.binningName);
    std::vector<std::string> effSourcePath = effPath;
    effSourcePath.push_back("SourceBinning");
    std::vector<std::string> lossSourcePath = lossPath;
    lossSourcePath.push_back("SourceBinning");

    for (auto& data : dataCollection) {
      Logger::Info(" ---> Processing particle: {}", data.name);

      // Prepare the 3D generator-level histogram
      data.h3MCGen->SetName(("h3" + data.name + "MCGen").c_str());
      data.h3MCGen->GetZaxis()->SetRange(1, data.h3MCGen->GetZaxis()->GetNbins());
      // Only create the error structure if the histogram read from file lacks it
      if (data.h3MCGen->GetSumw2N() == 0)
        data.h3MCGen->Sumw2();

      // 1. Compute and save the 3D and 2D correction map
      std::unique_ptr<TH3> h3TotalMap = EfficiencyCalculator::Compute3DTotalMap(data, particleCorrectionMode);

      std::string fileMCOutputPerPartPath3D = ccdbOutputDir + outputPrefix + "h3EffMap" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPart3D = RootIO::OpenOrThrow(fileMCOutputPerPartPath3D, "RECREATE", GetName());
      fileMCOutputPerPart3D->cd();
      h3TotalMap->SetName("ccdb_object");
      h3TotalMap->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart3D->Close();

      std::unique_ptr<TH2> h2TotalMapMultInt = EfficiencyCalculator::Compute2DTotalMapMultIntegrated(data, particleCorrectionMode);

      std::string fileMCOutputPerPartPath2D = ccdbOutputDir + outputPrefix + "h2EffMap" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPart2D = RootIO::OpenOrThrow(fileMCOutputPerPartPath2D, "RECREATE", GetName());
      fileMCOutputPerPart2D->cd();
      h2TotalMapMultInt->SetName("ccdb_object");
      h2TotalMapMultInt->Write(nullptr, TObject::kOverwrite);
      fileMCOutputPerPart2D->Close();

      // QA: error/relative-error maps alongside the CCDB map
      std::unique_ptr<TH2> h2TotalMapMultIntError = EfficiencyCalculator::BuildErrorMap(h2TotalMapMultInt.get(), "h2EffMapError" + data.name);
      std::unique_ptr<TH2> h2TotalMapMultIntRelError = EfficiencyCalculator::BuildRelativeErrorMap(h2TotalMapMultInt.get(), "h2EffMapRelError" + data.name);

      std::string fileMCOutputPerPartErrorPath2D = ccdbOutputDir + outputPrefix + "h2EffMapError" + data.name + ".root";
      std::unique_ptr<TFile> fileMCOutputPerPartError2D = RootIO::OpenOrThrow(fileMCOutputPerPartErrorPath2D, "RECREATE", GetName());
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
        if (auto* c = canvasManager.GetCanvas("c_" + data.name + "_Efficiency")) {
          c->cd();
          maps.efficiency->DrawCopy(drawOption);
        }

        if (auto* c = canvasManager.GetCanvas("c_" + data.name + "_SignalLoss")) {
          c->cd();
          maps.signalLoss->DrawCopy(drawOption);
        }

        if (maps.efficiencySourceBinning) {
          if (auto* c = canvasManager.GetCanvas("c_" + data.name + "_Efficiency_sourceBinning")) {
            c->cd();
            maps.efficiencySourceBinning->DrawCopy(drawOption);
          }
        }
        if (maps.signalLossSourceBinning) {
          if (auto* c = canvasManager.GetCanvas("c_" + data.name + "_SignalLoss_sourceBinning")) {
            c->cd();
            maps.signalLossSourceBinning->DrawCopy(drawOption);
          }
        }
      };

      // Signal loss over event loss, in the multiplicity bin the map belongs to
      // (-1: the multiplicity-integrated one). The event loss is a constant in pT,
      // so it is put in a histogram on the binning of the signal loss and divided
      // with the default option: errors are propagated as if the two were
      // independent. Not "B": that one assumes the numerator is a subset of the
      // denominator, which is not the case for a ratio of two efficiencies. The
      // division is done on a clone, so the maps written to file are untouched.
      // The independence is still an approximation: both come from the same sample.
      auto drawOverEvLoss = [&](const EfficiencyCalculator::Maps1D& maps, const char* drawOption, int multBin) {
        if (!hEventLoss || !maps.signalLoss)
          return;

        double evLoss, evLossErr;
        if (multBin >= 0) {
          evLoss = hEventLoss->GetBinContent(multBin + 1);
          evLossErr = hEventLoss->GetBinError(multBin + 1);
        } else {
          // Same convention as the per-bin event loss: collapse the two event
          // histograms into one bin spanning the multiplicity range of the analysis
          // and hand them to the same calculator.
          auto collapse = [&](TH1* h) {
            const auto [integral, error] = HistogramUtils::IntegralAndErrorPair(h, multBinning.front(), multBinning.back());
            auto c = std::make_unique<TH1D>("", "", 1, 0., 1.);
            c->SetDirectory(0);
            c->Sumw2();
            c->SetBinContent(1, integral);
            c->SetBinError(1, error);
            return c;
          };

          auto gen = collapse(hEventMultGen.get());
          auto reco = collapse(hEventMultGenAssocReco.get());
          auto integrated = EfficiencyCalculator::ComputeEventEfficiency(reco.get(), gen.get());
          if (!integrated)
            return;
          evLoss = integrated->GetBinContent(1);
          evLossErr = integrated->GetBinError(1);
        }
        if (evLoss <= 0.) {
          Logger::Warn("{}: event loss is zero for {}, signal loss over event loss skipped.", GetName(), data.name);
          return;
        }

        // The event loss does not depend on pT, so the same value divides the
        // signal loss at both binnings.
        auto divideAndDraw = [&](const TH1* signalLoss, TCanvas* canvas) {
          if (!signalLoss || !canvas)
            return;

          const std::string baseName = signalLoss->GetName();

          std::unique_ptr<TH1> hEvLossConst{static_cast<TH1*>(signalLoss->Clone((baseName + "_evLossConst").c_str()))};
          hEvLossConst->SetDirectory(0);
          hEvLossConst->Reset();
          for (int bin = 1; bin <= hEvLossConst->GetNbinsX(); bin++) {
            hEvLossConst->SetBinContent(bin, evLoss);
            hEvLossConst->SetBinError(bin, evLossErr);
          }

          std::unique_ptr<TH1> ratio{static_cast<TH1*>(signalLoss->Clone((baseName + "_overEvLoss").c_str()))};
          ratio->GetYaxis()->SetTitle("Signal Loss / Event Loss");
          ratio->SetDirectory(0);
          ratio->Divide(hEvLossConst.get());

          canvas->cd();
          ratio->DrawCopy(drawOption);
        };

        divideAndDraw(maps.signalLoss.get(), canvasManager.GetCanvas("c_" + data.name + "_SignalLossOverEventLoss"));
        divideAndDraw(maps.signalLossSourceBinning.get(), canvasManager.GetCanvas("c_" + data.name + "_SignalLossOverEventLoss_sourceBinning"));
      };

      // The source-binning maps get a subdirectory of their own rather than
      // sitting next to the analysis ones, and it is created by the write itself:
      // the directory then exists if and only if something was put in it, with no
      // second condition to keep in agreement. The name suffix stays anyway, since
      // once an object leaves the file its directory is no longer part of its
      // identity.
      auto writeMaps = [&](const EfficiencyCalculator::Maps1D& maps) {
        writeInto(effPath, maps.efficiency.get());
        writeInto(lossPath, maps.signalLoss.get());
        writeInto(effSourcePath, maps.efficiencySourceBinning.get());
        writeInto(lossSourcePath, maps.signalLossSourceBinning.get());
      };

      for (int multBin{0}; multBin < BinningUtils::NBins(multBinning); multBin++) {
        // 'rebinning_pt' is applied inside the calculator, on the counts, before
        // they are divided: rebinning the ratio afterwards would sum efficiencies.
        EfficiencyCalculator::Maps1D maps = EfficiencyCalculator::Compute1DMaps(data, multBinning, multBin, globalCfgs.GetSpectraColor(multBin));

        drawMaps(maps, multBin == 0 ? "" : "SAME");
        drawOverEvLoss(maps, multBin == 0 ? "" : "SAME", multBin);
        writeMaps(maps);
      }

      EfficiencyCalculator::Maps1D mapsInt = EfficiencyCalculator::Compute1DMapsMultIntegrated(data);

      drawMaps(mapsInt, "SAME");
      drawOverEvLoss(mapsInt, "SAME", -1);
      writeMaps(mapsInt);
    }
  }

  void Terminate() override
  {
    Logger::Info("{}: TERMINATING AND CLEANING UP...", GetName());

    // The pT binning of every correction is its own axis; the multiplicity binning is
    // in the names, and stamped (RootIO::RequireMatchingBinningStamp).
    if (TDirectory* schemeDir = RootIO::GetOrCreatePath(fileMCOutput.get(), {globalCfgs.binningName}, false))
      RootIO::WriteBinningStamp(schemeDir, RootIO::kStampMult, multBinning);

    RootIO::WriteProvenance(fileMCOutput.get(), globalCfgs.binningName, provenance);

    // Save all diagnostic canvases into their registered subdirectories; the Y ranges
    // declared at creation are applied on the way out.
    canvasManager.WriteAll(fileMCOutput.get());

    if (fileMCOutput)
      fileMCOutput->Close();

    Logger::Info("{}: DONE.", GetName());
  }

 private:
  AnalysisSettings globalCfgs;

  CanvasManager canvasManager;

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

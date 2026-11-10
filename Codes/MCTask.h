#pragma once

#include "AnalysisConstants.h"
#include "AnalysisStructures.h"
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
#include <format>
#include <map>
#include <memory>
#include <optional>
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

    provenance = RootIO::StartProvenance(JsonConfig::Serialize(taskConfig));

    // 1. Check if a list of particle to compute MC corrections is provided
    auto particles = JsonConfig::RequireArray(taskConfig, "mc_particles", GetName());

    // Task-level input file. Required: a particle may point somewhere else with
    // its own "input_mc_file", but there must be a file to fall back on, and an
    // empty string is not one.
    std::string defaultInputFile = JsonConfig::RequireString(taskConfig, "input_mc_file", GetName());

    // 2. Allow JSON to override the internal ROOT directory base path
    mcBasePath = JsonConfig::RequireString(taskConfig, "mc_base_path", GetName());

    outputDir = JsonConfig::RequireString(taskConfig, "output_dir", GetName());
    outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

    // CCDB-ready per-particle maps live under outputDir/{binningName}/ on disk,
    // since the ROOT object inside must be named literally "ccdb_object" with no
    // internal subfolder. Ensure the directory exists — TFile won't create it.
    ccdbOutputDir = std::format("{}{}/", outputDir, globalCfgs.binningName);
    RootIO::CreateDirectoriesOrThrow(ccdbOutputDir, GetName());

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
    // One summary canvas of a particle: a correction map, one curve per class, at one binning; name and path from
    // the same (map, binning).
    auto addSummary = [&](const std::string& particle, CorrectionMap map, SpectrumBinning binning, const std::string& title) {
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas(OutputNames::CorrectionSummaryCanvas(particle, map, binning), title,
                                                             OutputNames::CorrectionSummaryPath(globalCfgs.binningName, map, binning)));
    };

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
      dirName = JsonConfig::OptionalString(p, "dir_name", dirName, ctx);

      std::string particleInputFile = JsonConfig::OptionalString(p, "input_mc_file", defaultInputFile, ctx);
      // The task-level key is required, so this only fires on an explicitly
      // empty string, in the JSON or in the per-particle override.
      if (particleInputFile.empty()) {
        throw std::runtime_error(std::format("[FATAL] {}: empty 'input_mc_file' for particle '{}'!", ctx, name));
      }

      AnalysisStructures::LoadedMC data;
      data.name = name;

      std::string h3MCGenName = std::format("{}{}/h3{}MCGen", mcBasePath, dirName, name);
      std::string h4MCGenAssocRecoName = std::format("{}{}/h4{}MCGenAssocReco", mcBasePath, dirName, name);
      std::string h4MCRecoName = std::format("{}{}/h4{}MCReco", mcBasePath, dirName, name);

      data.h3MCGen = RootIO::GetUniqueOrThrow<TH3F>(getOrOpenFile(particleInputFile), h3MCGenName, GetName());
      data.h4MCGenAssocReco = RootIO::GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCGenAssocRecoName, GetName());
      data.h4MCReco = RootIO::GetUniqueOrThrow<THnSparseF>(getOrOpenFile(particleInputFile), h4MCRecoName, GetName());

      // These three are divided by each other to build efficiency and signal
      // loss, so they must share their axes whatever the configuration says.
      // Checked directly between them: it holds even with nothing declared.
      // h3MCGen axes: (mult, pT, y) -- h4* axes: (zvtx, mult, pT, y)
      BinningUtils::RequireSameAxis(data.h3MCGen->GetYaxis(), data.h4MCReco->GetAxis(2),
                                    std::format("{} pT", h3MCGenName), std::format("{} pT", h4MCRecoName));
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(2), data.h4MCReco->GetAxis(2),
                                    std::format("{} pT", h4MCGenAssocRecoName), std::format("{} pT", h4MCRecoName));
      BinningUtils::RequireSameAxis(data.h3MCGen->GetXaxis(), data.h4MCReco->GetAxis(1),
                                    std::format("{} multiplicity", h3MCGenName), std::format("{} multiplicity", h4MCRecoName));
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(1), data.h4MCReco->GetAxis(1),
                                    std::format("{} multiplicity", h4MCGenAssocRecoName), std::format("{} multiplicity", h4MCRecoName));
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(0), data.h4MCReco->GetAxis(0),
                                    std::format("{} zvtx", h4MCGenAssocRecoName), std::format("{} zvtx", h4MCRecoName));
      BinningUtils::RequireSameAxis(data.h4MCGenAssocReco->GetAxis(3), data.h4MCReco->GetAxis(3),
                                    std::format("{} rapidity", h4MCGenAssocRecoName), std::format("{} rapidity", h4MCRecoName));
      BinningUtils::RequireSameAxis(data.h3MCGen->GetZaxis(), data.h4MCReco->GetAxis(3),
                                    std::format("{} rapidity", h3MCGenName), std::format("{} rapidity", h4MCRecoName));

      // The multiplicity binning is kept because the loops below address its
      // bins. The pT binning is not: this task consumes the pT dimension by
      // projecting the whole axis, so nothing here indexes it - it is resolved
      // only to have it verified against the declared production.
      multBinning = globalCfgs.ResolveMultBinning(data.h3MCGen->GetXaxis(), std::format("{} in '{}'", h3MCGenName, particleInputFile));
      globalCfgs.ResolvePtBinning(name, data.h3MCGen->GetYaxis(), std::format("{} in '{}'", h3MCGenName, particleInputFile));

      if (auto rebin = JsonConfig::TryArray(p, "rebinning_pt", ctx)) {
        std::vector<double> bins = JsonConfig::ReadNumberArray(*rebin, std::format("rebinning_pt for '{}'", name), ctx);
        if (bins.size() < 2) {
          throw std::runtime_error(std::format("[FATAL] {}: 'rebinning_pt' for particle '{}' must have at least 2 edges!", ctx, name));
        }
        data.rebinningPt = std::move(bins);
      }

      // Register particle summary canvases with their target ROOT subdirectories; the source-binning ones exist only
      // with 'rebinning_pt'. Each one declares its Y range here; the manager applies it when it writes.
      addSummary(name, CorrectionMap::Efficiency, SpectrumBinning::Analysis, std::format("{} Acceptance x Efficiency", name));
      addSummary(name, CorrectionMap::SignalLoss, SpectrumBinning::Analysis, std::format("{} Signal Loss", name));
      if (data.rebinningPt) {
        addSummary(name, CorrectionMap::Efficiency, SpectrumBinning::Source, std::format("{} Efficiency (Source Binning)", name));
        addSummary(name, CorrectionMap::SignalLoss, SpectrumBinning::Source, std::format("{} Signal Loss (Source Binning)", name));
      }

      dataCollection.push_back(std::move(data));
    }

    // 5. Load Event-Level Histograms for Global Event Loss.
    // O2 always produces these for every MC file regardless of which particle(s)
    // it contains, so they're identical duplicates across all opened files —
    // simply read them from whichever file happens to be open already.
    if (!openInputFiles.empty()) {
      std::string genAssocRecoEventPath = std::format("{}event/hGenMCAssocRecoMultiplicityPercent", mcBasePath);
      std::string genEventPath = std::format("{}event/hGenMCMultiplicityPercent", mcBasePath);

      hEventMultGenAssocReco = RootIO::GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genAssocRecoEventPath, GetName());
      hEventMultGen = RootIO::GetUniqueOrThrow<TH1F>(openInputFiles.begin()->second.get(), genEventPath, GetName());

      // The event loss is addressed below with the multiplicity bin index of the
      // particle maps, so the two must share their axis.
      if (!dataCollection.empty()) {
        BinningUtils::RequireSameAxis(hEventMultGen->GetXaxis(), dataCollection.front().h3MCGen->GetXaxis(),
                                      std::format("{} multiplicity", genEventPath), "h3MCGen multiplicity");
        BinningUtils::RequireSameAxis(hEventMultGenAssocReco->GetXaxis(), hEventMultGen->GetXaxis(),
                                      std::format("{} multiplicity", genAssocRecoEventPath), std::format("{} multiplicity", genEventPath));
      }

      // OverEventLoss canvases are registered only when event histograms exist
      for (const auto& data : dataCollection) {
        addSummary(data.name, CorrectionMap::SignalLossOverEventLoss, SpectrumBinning::Analysis,
                   std::format("{} Signal Loss / Event Loss", data.name));
        if (data.rebinningPt)
          addSummary(data.name, CorrectionMap::SignalLossOverEventLoss, SpectrumBinning::Source,
                     std::format("{} Signal Loss / Event Loss (Source Binning)", data.name));
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
    std::string outputPath = std::format("{}{}Corrections.root", outputDir, outputPrefix);
    outputFile = RootIO::OpenOrThrow(outputPath, "UPDATE", GetName());

    // This run rewrites its whole scheme directory - AccEff, SigLoss, EvLoss and the
    // stamps - so anything a previous one left there would sit beside the new maps
    // with nothing to tell them apart. Not hypothetical: the moment the object names
    // change, the file would carry both generations and the reader would have no way
    // to know which is current.
    //
    // UPDATE and not RECREATE because only this subtree is ours: a run under a
    // different 'binning_name' writing to the same file keeps its own.
    RootIO::ClearPath(outputFile.get(), {globalCfgs.binningName});

    Logger::Info("{}: Initialization complete.", GetName());
  }

  void Run() override
  {
    Logger::Info("{}: RUNNING EFFICIENCY COMPUTATIONS...", GetName());

    // Writes one object at its path, creating the directories; nothing when there is no object.
    auto writeInto = [&](const std::vector<std::string>& path, TH1* h) {
      if (!h)
        return;
      RootIO::MakePath(outputFile.get(), path)->cd();
      h->Write(nullptr, TObject::kOverwrite);
    };

    std::unique_ptr<TH1> hEventLoss;

    if (hEventMultGenAssocReco && hEventMultGen) {
      Logger::Info(" ---> Computing Global Event Efficiency (Event Loss)...");

      // Delegate the math to the static calculator
      hEventLoss = EfficiencyCalculator::ComputeEventLoss(hEventMultGenAssocReco.get(), hEventMultGen.get());

      writeInto(OutputNames::EventLossPath(globalCfgs.binningName), hEventLoss.get());
    }


    for (auto& data : dataCollection) {
      Logger::Info(" ---> Processing particle: {}", data.name);

      // Prepare the 3D generator-level histogram
      data.h3MCGen->SetName(std::format("h3{}MCGen", data.name).c_str());
      data.h3MCGen->GetZaxis()->SetRange(1, data.h3MCGen->GetZaxis()->GetNbins());
      // Only create the error structure if the histogram read from file lacks it
      if (data.h3MCGen->GetSumw2N() == 0)
        data.h3MCGen->Sumw2();

      // 1. Compute and save the 3D and 2D correction map
      std::unique_ptr<TH3> h3CorrectionMap = EfficiencyCalculator::Compute3DCorrectionMap(data, particleCorrectionMode);

      std::string ccdbPath3D = std::format("{}{}h3EffMap{}.root", ccdbOutputDir, outputPrefix, data.name);
      RootIO::WriteCcdbFile(ccdbPath3D, h3CorrectionMap.get(), GetName());

      std::unique_ptr<TH2> h2CorrectionMap = EfficiencyCalculator::Compute2DCorrectionMapMultIntegrated(data, particleCorrectionMode);

      std::string ccdbPath2D = std::format("{}{}h2EffMap{}.root", ccdbOutputDir, outputPrefix, data.name);
      RootIO::WriteCcdbFile(ccdbPath2D, h2CorrectionMap.get(), GetName());

      // QA: error/relative-error maps alongside the CCDB map
      std::unique_ptr<TH2> h2CorrectionMapError = EfficiencyCalculator::BuildErrorMap(h2CorrectionMap.get(), std::format("h2EffMapError{}", data.name));
      std::unique_ptr<TH2> h2CorrectionMapRelError = EfficiencyCalculator::BuildRelativeErrorMap(h2CorrectionMap.get(), std::format("h2EffMapRelError{}", data.name));

      std::string errorMapPath = std::format("{}{}h2EffMapError{}.root", ccdbOutputDir, outputPrefix, data.name);
      std::unique_ptr<TFile> errorMapFile = RootIO::OpenOrThrow(errorMapPath, "RECREATE", GetName());
      errorMapFile->cd();
      // h2CorrectionMapError->SetName("ccdb_object");
      if (h2CorrectionMapError)
        h2CorrectionMapError->Write(nullptr, TObject::kOverwrite);
      // h2CorrectionMapRelError->SetName("ccdb_object");
      if (h2CorrectionMapRelError)
        h2CorrectionMapRelError->Write(nullptr, TObject::kOverwrite);
      errorMapFile->Close();

      // 2. Compute 1D Spectra across multiplicity bins
      //
      // The maps at the source binning exist only when 'rebinning_pt' merged the
      // counts. They go on their own pair of canvases: each canvas overlays the
      // multiplicity bins against each other, so mixing two binnings on one pad
      // would make exactly the comparison it is drawn for unreadable.
      // A source-binning map exists only with 'rebinning_pt', and so does its canvas.
      auto drawOn = [&](CorrectionMap map, SpectrumBinning binning, const TH1* h, const char* option, int color) {
        if (h)
          if (auto* c = canvasManager.GetCanvas(OutputNames::CorrectionSummaryCanvas(data.name, map, binning))) {
            c->cd();
            CanvasManager::DrawStyledCopy(h, option, color);
          }
      };

      // One multiplicity class (none: the multiplicity-integrated maps): its maps, and the signal loss over the event
      // loss of the class, drawn on the summary canvases and written. The tag, the colour and the draw option all
      // follow from the class.
      auto summarise = [&](EfficiencyCalculator::Maps1D maps, std::optional<int> multBin) {
        const std::string multTag = multBin ? OutputNames::MultTag(multBinning, *multBin) : OutputNames::MultUnionTag(multBinning);
        const int color = globalCfgs.GetSpectraColor(multBin);
        const char* option = multBin == 0 ? "" : "SAME";

        if (hEventLoss) {
          const auto eventLoss = EfficiencyCalculator::EventLossOfClass(hEventLoss.get(), hEventMultGenAssocReco.get(), hEventMultGen.get(),
                                                                        multBinning, multBin);
          if (eventLoss.first > 0.)
            EfficiencyCalculator::DivideByEventLoss(maps, eventLoss, data.name, multTag);
          else
            Logger::Warn("{}: event loss is zero for {}, {}: its signal loss over event loss is not written.", GetName(), data.name, multTag);
        }

        drawOn(CorrectionMap::Efficiency, SpectrumBinning::Analysis, maps.efficiency.get(), option, color);
        drawOn(CorrectionMap::SignalLoss, SpectrumBinning::Analysis, maps.signalLoss.get(), option, color);
        drawOn(CorrectionMap::SignalLossOverEventLoss, SpectrumBinning::Analysis, maps.signalLossOverEventLoss.get(), option, color);
        drawOn(CorrectionMap::Efficiency, SpectrumBinning::Source, maps.efficiencySourceBinning.get(), option, color);
        drawOn(CorrectionMap::SignalLoss, SpectrumBinning::Source, maps.signalLossSourceBinning.get(), option, color);
        drawOn(CorrectionMap::SignalLossOverEventLoss, SpectrumBinning::Source, maps.signalLossOverEventLossSourceBinning.get(), option, color);

        // The source-binning maps get a subdirectory of their own rather than
        // sitting next to the analysis ones, and it is created by the write itself:
        // the directory then exists if and only if something was put in it, with no
        // second condition to keep in agreement. The name suffix stays anyway, since
        // once an object leaves the file its directory is no longer part of its
        // identity.
        auto write = [&](CorrectionMap map, SpectrumBinning binning, TH1* h) {
          writeInto(OutputNames::CorrectionMapPath(globalCfgs.binningName, map, binning), h);
        };
        write(CorrectionMap::Efficiency, SpectrumBinning::Analysis, maps.efficiency.get());
        write(CorrectionMap::SignalLoss, SpectrumBinning::Analysis, maps.signalLoss.get());
        write(CorrectionMap::SignalLossOverEventLoss, SpectrumBinning::Analysis, maps.signalLossOverEventLoss.get());
        write(CorrectionMap::Efficiency, SpectrumBinning::Source, maps.efficiencySourceBinning.get());
        write(CorrectionMap::SignalLoss, SpectrumBinning::Source, maps.signalLossSourceBinning.get());
        write(CorrectionMap::SignalLossOverEventLoss, SpectrumBinning::Source, maps.signalLossOverEventLossSourceBinning.get());
      };

      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        // 'rebinning_pt' is applied inside the calculator, on the counts, before
        // they are divided: rebinning the ratio afterwards would sum efficiencies.
        summarise(EfficiencyCalculator::Compute1DMaps(data, multBinning, multBin), multBin);
      }
      // The union, unless it is the only class, whose maps are already written under its name.
      if (BinningUtils::HasMultUnion(multBinning))
        summarise(EfficiencyCalculator::Compute1DMapsMultIntegrated(data, multBinning), std::nullopt);
    }
  }

  void Terminate() override
  {
    Logger::Info("{}: TERMINATING AND CLEANING UP...", GetName());

    // The pT binning of every correction is its own axis; the multiplicity binning is
    // in the names, and stamped (RootIO::RequireMatchingBinningStamp).
    RootIO::WriteBinningStamp(RootIO::MakePath(outputFile.get(), {globalCfgs.binningName}), RootIO::kStampMult,
                              multBinning);

    RootIO::WriteProvenance(outputFile.get(), globalCfgs.binningName, provenance);

    // Save all diagnostic canvases into their registered subdirectories; the Y ranges
    // declared at creation are applied on the way out.
    canvasManager.WriteAll(outputFile.get());

    if (outputFile)
      outputFile->Close();

    Logger::Info("{}: DONE.", GetName());
  }

 private:
  using CorrectionMap = OutputNames::CorrectionMap;
  using SpectrumBinning = OutputNames::SpectrumBinning;

  AnalysisSettings globalCfgs;

  CanvasManager canvasManager;

  std::vector<AnalysisStructures::LoadedMC> dataCollection;

  // Read from the input files in Init(), never from the configuration
  std::vector<double> multBinning;

  std::unique_ptr<TFile> outputFile;

  std::string outputDir;
  std::string ccdbOutputDir;

  // Filled in Init, written into the output file in Terminate
  std::map<std::string, std::string> provenance;
  std::string outputPrefix;
  std::string mcBasePath;

  // --- Variables for Global Event Loss ---
  std::unique_ptr<TH1F> hEventMultGenAssocReco;
  std::unique_ptr<TH1F> hEventMultGen;

  CorrectionMode particleCorrectionMode{CorrectionMode::EfficiencyOnly};
};

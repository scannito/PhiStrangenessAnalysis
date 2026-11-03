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

      // Register particle summary canvases with their target ROOT subdirectories.
      // Each one declares its Y range here; the manager applies it when it writes.
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas(SummaryCanvas(name, SummaryMap::Efficiency), std::format("{} Acceptance x Efficiency", name),
                                                             {globalCfgs.binningName, "AccEff", "Summary"}));
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas(SummaryCanvas(name, SummaryMap::SignalLoss), std::format("{} Signal Loss", name),
                                                             {globalCfgs.binningName, "SigLoss", "Summary"}));
      if (data.rebinningPt) {
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas(SummaryCanvas(name, SummaryMap::Efficiency, OutputNames::SpectrumBinning::Source), std::format("{} Efficiency (Source Binning)", name),
                                                               OutputNames::SourceBinningPath({globalCfgs.binningName, "AccEff", "Summary"})));
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas(SummaryCanvas(name, SummaryMap::SignalLoss, OutputNames::SpectrumBinning::Source), std::format("{} Signal Loss (Source Binning)", name),
                                                               OutputNames::SourceBinningPath({globalCfgs.binningName, "SigLoss", "Summary"})));
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
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas(SummaryCanvas(data.name, SummaryMap::SignalLossOverEventLoss),
                                                               std::format("{} Signal Loss / Event Loss", data.name),
                                                               {globalCfgs.binningName, "SigLoss", "Summary", "OverEvLoss"}));
        if (data.rebinningPt) {
          canvasManager.SetAutoYRange(canvasManager.CreateCanvas(SummaryCanvas(data.name, SummaryMap::SignalLossOverEventLoss, OutputNames::SpectrumBinning::Source),
                                                                 std::format("{} Signal Loss / Event Loss (Source Binning)", data.name),
                                                                 OutputNames::SourceBinningPath({globalCfgs.binningName, "SigLoss", "Summary", "OverEvLoss"})));
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

    // The source-binning maps in a subdirectory of their own (see summarise), as copies: each name is one directory.
    const std::vector<std::string> effPath = OutputNames::EfficiencyPath(globalCfgs.binningName);
    const std::vector<std::string> lossPath = OutputNames::SignalLossPath(globalCfgs.binningName);
    const std::vector<std::string> effSourcePath = OutputNames::SourceBinningPath(effPath);
    const std::vector<std::string> lossSourcePath = OutputNames::SourceBinningPath(lossPath);
    const std::vector<std::string> overEvLossPath = OutputNames::SignalLossOverEventLossPath(globalCfgs.binningName);
    const std::vector<std::string> overEvLossSourcePath = OutputNames::SourceBinningPath(overEvLossPath);

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
      using OutputNames::SpectrumBinning;
      // A source-binning map exists only with 'rebinning_pt', and so does its canvas.
      auto drawOn = [&](SummaryMap map, SpectrumBinning binning, const TH1* h, const char* option, int color) {
        if (h)
          if (auto* c = canvasManager.GetCanvas(SummaryCanvas(data.name, map, binning))) {
            c->cd();
            CanvasManager::DrawStyledCopy(h, option, color);
          }
      };

      // The event loss of a multiplicity class (none: the multiplicity-integrated one), with its error; nothing when
      // there is none, or it is zero.
      auto eventLossOf = [&](std::optional<int> multBin) -> std::optional<std::pair<double, double>> {
        if (!hEventLoss)
          return std::nullopt;
        std::pair<double, double> eventLossAndError;
        if (multBin) {
          eventLossAndError = HistogramUtils::BinValueAndError(hEventLoss.get(), *multBin);
        } else {
          // Same convention as the per-bin event loss: collapse the two event
          // histograms into one bin spanning the multiplicity range of the analysis
          // and hand them to the same calculator.
          auto collapse = [&](TH1* h) {
            const auto [integral, error] = HistogramUtils::IntegralAndErrorWholeBins(h, multBinning.front(), multBinning.back());
            auto c = std::make_unique<TH1D>("", "", 1, 0., 1.);
            c->SetDirectory(0);
            c->Sumw2();
            HistogramUtils::SetBinValueAndError(c.get(), 0, {integral, error});
            return c;
          };

          auto gen = collapse(hEventMultGen.get());
          auto reco = collapse(hEventMultGenAssocReco.get());
          auto integrated = EfficiencyCalculator::ComputeEventLoss(reco.get(), gen.get());
          if (!integrated)
            return std::nullopt;
          eventLossAndError = HistogramUtils::BinValueAndError(integrated.get(), 0);
        }
        if (eventLossAndError.first <= 0.) {
          Logger::Warn("{}: event loss is zero for {}, signal loss over event loss skipped.", GetName(), data.name);
          return std::nullopt;
        }
        return eventLossAndError;
      };

      // The signal loss over the event loss of the same class, written next to the maps, so that it is data and not
      // only a canvas. The event loss does not depend on pT, so it is put in a histogram on the binning of the signal
      // loss and divided with the default option: errors are propagated as if the two were independent. Not "B":
      // that one assumes the numerator is a subset of the denominator, which is not the case for a ratio of two
      // efficiencies. The division is done on a clone, so the maps written to file are untouched. The independence
      // is still an approximation: both come from the same sample.
      auto divide = [&](const TH1* signalLoss, std::pair<double, double> eventLoss, const std::string& name) -> std::unique_ptr<TH1> {
        if (!signalLoss)
          return nullptr;

        std::unique_ptr<TH1> hEventLossConst{static_cast<TH1*>(signalLoss->Clone(std::format("{}_evLossConst", name).c_str()))};
        hEventLossConst->SetDirectory(0);
        hEventLossConst->Reset();
        for (int bin = 1; bin <= hEventLossConst->GetNbinsX(); bin++) {
          hEventLossConst->SetBinContent(bin, eventLoss.first);
          hEventLossConst->SetBinError(bin, eventLoss.second);
        }

        std::unique_ptr<TH1> ratio{static_cast<TH1*>(signalLoss->Clone(name.c_str()))};
        ratio->GetYaxis()->SetTitle("Signal Loss / Event Loss");
        ratio->SetDirectory(0);
        ratio->Divide(hEventLossConst.get());
        return ratio;
      };

      // One multiplicity class (none: the multiplicity-integrated maps): its maps and its signal loss over event
      // loss drawn on the summary canvases and written. The tag, the colour and the draw option all follow from it.
      auto summarise = [&](const EfficiencyCalculator::Maps1D& maps, std::optional<int> multBin) {
        const std::string multTag = multBin ? OutputNames::MultTag(multBinning, *multBin) : OutputNames::kMultIntegrated;
        const int color = multBin ? globalCfgs.GetSpectraColor(*multBin) : kBlack;
        const char* option = multBin == 0 ? "" : "SAME";

        drawOn(SummaryMap::Efficiency, SpectrumBinning::Analysis, maps.efficiency.get(), option, color);
        drawOn(SummaryMap::SignalLoss, SpectrumBinning::Analysis, maps.signalLoss.get(), option, color);
        drawOn(SummaryMap::Efficiency, SpectrumBinning::Source, maps.efficiencySourceBinning.get(), option, color);
        drawOn(SummaryMap::SignalLoss, SpectrumBinning::Source, maps.signalLossSourceBinning.get(), option, color);

        if (const auto eventLoss = eventLossOf(multBin)) {
          auto ratio = divide(maps.signalLoss.get(), *eventLoss, OutputNames::SignalLossOverEventLoss(data.name, multTag));
          auto ratioSource = divide(maps.signalLossSourceBinning.get(), *eventLoss,
                                    OutputNames::SignalLossOverEventLoss(data.name, multTag, SpectrumBinning::Source));
          drawOn(SummaryMap::SignalLossOverEventLoss, SpectrumBinning::Analysis, ratio.get(), option, color);
          drawOn(SummaryMap::SignalLossOverEventLoss, SpectrumBinning::Source, ratioSource.get(), option, color);
          writeInto(overEvLossPath, ratio.get());
          writeInto(overEvLossSourcePath, ratioSource.get());
        }

        // The source-binning maps get a subdirectory of their own rather than
        // sitting next to the analysis ones, and it is created by the write itself:
        // the directory then exists if and only if something was put in it, with no
        // second condition to keep in agreement. The name suffix stays anyway, since
        // once an object leaves the file its directory is no longer part of its
        // identity.
        writeInto(effPath, maps.efficiency.get());
        writeInto(lossPath, maps.signalLoss.get());
        writeInto(effSourcePath, maps.efficiencySourceBinning.get());
        writeInto(lossSourcePath, maps.signalLossSourceBinning.get());
      };

      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        // 'rebinning_pt' is applied inside the calculator, on the counts, before
        // they are divided: rebinning the ratio afterwards would sum efficiencies.
        summarise(EfficiencyCalculator::Compute1DMaps(data, multBinning, multBin), multBin);
      }
      summarise(EfficiencyCalculator::Compute1DMapsMultIntegrated(data), std::nullopt);
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
  // The summary canvases of a particle, one per map, at the analysis binning or at the source one: created in
  // Init, drawn on per class in Run. An enum and not a string, so that the two sites cannot spell it differently.
  enum class SummaryMap { Efficiency,
                          SignalLoss,
                          SignalLossOverEventLoss };
  static std::string SummaryCanvas(const std::string& particle, SummaryMap map,
                                   OutputNames::SpectrumBinning binning = OutputNames::SpectrumBinning::Analysis)
  {
    constexpr std::string_view kMapNames[] = {"Efficiency", "SignalLoss", "SignalLossOverEventLoss"};
    return std::format("c_{}_{}{}", particle, kMapNames[static_cast<int>(map)], OutputNames::BinningSuffix(binning));
  }

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

#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "CanvasManager.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "FitQualityRecorder.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH2F.h"
#include "TH3F.h"

#include <filesystem>
#include <algorithm>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class PurityTask : public IAnalysisTask
{
 public:
  // Tells the WorkflowManager which JSON node to pass to this task
  std::string GetName() const override { return "purity_task"; }

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

    // 1. Open input file, extract 3D histograms to RAM, and immediately close it
    std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
    std::unique_ptr<TFile> fileInput = RootIO::OpenOrThrow(inputFile, "READ", GetName());

    // 2. Open the output file: one for every particle of the block. UPDATE and this binning's
    // subtree cleared, not RECREATE: the file holds one subtree per binning_name, and another
    // scheme's must survive this one's run (CLAUDE.md, How a task opens the file it writes).
    std::string outputDir = JsonConfig::RequireString(taskConfig, "output_dir", GetName());

    // Prefix to specify the type of analysis (Data vs MCClosure)
    outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());
    outputFile = RootIO::OpenOrThrow(std::format("{}{}{}", outputDir, outputPrefix, OutputNames::kPurityFile), "UPDATE", GetName());
    RootIO::ClearPath(outputFile.get(), {globalCfgs.binningName});

    // CCDB-ready maps live under outputDir/{binningName}/ on disk, since the ROOT
    // object inside must be named literally "ccdb_object" with no subfolder.
    // Same layout as MCTask, so the two kinds of correction sit side by side.
    ccdbOutputDir = outputDir + globalCfgs.binningName + "/";
    std::error_code ec;
    std::filesystem::create_directories(ccdbOutputDir, ec);
    if (ec)
      throw std::runtime_error(std::format("[FATAL] {}: Cannot create output directory '{}': {}", GetName(), ccdbOutputDir, ec.message()));

    auto particles = JsonConfig::RequireArray(taskConfig, "purity_particles", GetName());

    int pIdx = 0;
    const std::vector<std::string> summaryPath = OutputNames::SummaryPath(globalCfgs.binningName);
    std::vector<std::string> summarySourcePath = summaryPath;
    summarySourcePath.push_back("SourceBinning");

    for (const auto& entry : particles) {
      const std::string ctx = std::format("{} purity_particles[{}]", GetName(), pIdx++);
      JsonConfig::RequireKnownKeys(entry, {"name", "purity_key", "hist_name", "rebinning_pt"}, ctx);
      std::string particle = JsonConfig::RequireString(entry, "name", ctx);
      std::string purityKey = JsonConfig::RequireString(entry, "purity_key", ctx);
      std::string histName = JsonConfig::RequireString(entry, "hist_name", ctx);

      std::unique_ptr<TH3F> h3Source = RootIO::GetUniqueOrThrow<TH3F>(fileInput.get(), histName, GetName());

      // The binning comes from the file, never from the configuration: the loops
      // below address these very bins, and the purity spectrum is built on these
      // very edges. h3 axes: (mult, pT, invariant mass).
      const std::string origin = std::format("{} in '{}'", histName, inputFile);
      multBinning = globalCfgs.ResolveMultBinning(h3Source->GetXaxis(), origin);
      std::vector<double> sourceBinning = globalCfgs.ResolvePtBinning(particle, h3Source->GetYaxis(), origin);

      // A coarser analysis binning is obtained by fitting the MERGED mass
      // distribution of the source bins it covers, not by merging the purities
      // afterwards: a purity is a ratio, and ratios do not add up.
      std::optional<std::vector<double>> rebinningPt;
      if (auto rebin = JsonConfig::TryArray(entry, "rebinning_pt", GetName())) {
        std::vector<double> bins = JsonConfig::ReadNumberArray(*rebin, std::format("rebinning_pt for '{}'", particle), GetName());
        if (bins.size() < 2) {
          throw std::runtime_error(std::format("[FATAL] {}: 'rebinning_pt' for '{}' needs at least 2 edges!", GetName(), particle));
        }
        rebinningPt = std::move(bins);
      }

      // Real bin edges on both axes: the map leaves this framework, so the pT
      // interval a bin stands for must be readable from the object itself.
      std::string ccdbName = "h2" + purityKey + "Purity";
      auto h2CCDB = std::make_unique<TH2F>(ccdbName.c_str(), ";Multiplicity percentile (%);p_{T} (GeV/#it{c})",
                                           BinningUtils::NBins(multBinning), multBinning.data(),
                                           BinningUtils::NBins(sourceBinning), sourceBinning.data());
      h2CCDB->SetDirectory(0);

      // The mapping between the two binnings is derived by the constructor, so it
      // is not passed here: an analysis edge that is not an edge of the source
      // makes this line throw, before any fit has run.
      particleTasks.emplace_back(purityKey, std::move(h3Source), std::move(sourceBinning),
                                 std::move(rebinningPt), std::move(h2CCDB));

      // Each summary canvas declares its Y range here; the manager applies it when it writes.
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas("canvas" + purityKey + "Purity", purityKey + " Purity",
                                                             summaryPath, 800, 600));
      if (particleTasks.back().rebinningPt) {
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas("canvas" + purityKey + "Purity_sourceBinning",
                                                               purityKey + " Purity (source binning)",
                                                               summarySourcePath, 800, 600));
      }
    }

    // 3. Read task-specific settings from the JSON node (DOM)
    // Keep the fit configuration file completely separated for physics tuning
    std::string fitCfgPath = JsonConfig::RequireString(taskConfig, "fit_config_file", GetName());

    // A snapshot, not a path: this is the file being edited while the fits are
    // tuned, so knowing which one it was says nothing about what was in it.
    provenance["fit_config"] = JsonConfig::ReadFileText(fitCfgPath);

    // 4. Initialize specific mathematical tools
    fitConfigManager = std::make_unique<FitConfigManager>(fitCfgPath);
    for (const ParticleTask& task : particleTasks)
      fitConfigManager->DeclareCells(task.purityKey, multBinning, {task.sourceBinning, task.AnalysisBinning()});
  }

  void Run() override
  {
    Logger::Info("{}: RUNNING FITS...", GetName());

    const std::vector<std::string> summaryPath = OutputNames::SummaryPath(globalCfgs.binningName);

    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      for (auto& task : particleTasks) {
        // Every particle's fits in a directory of its own: they share the file.
        std::vector<std::string> fitPath = {globalCfgs.binningName, "Fits", task.purityKey};

        TDirectory* summaryDir = RootIO::GetOrCreatePath(outputFile.get(), summaryPath, false);
        TDirectory* fitDir = RootIO::GetOrCreatePath(outputFile.get(), fitPath, false);

        std::unique_ptr<TH1> h1PurityAnalysis = FitPuritySpectrum(task, multBin, SpectrumBinning::Analysis, fitDir);

        // Style and draw on the summary canvas
        CanvasManager::SetHistogramStyle(h1PurityAnalysis.get(), globalCfgs.GetSpectraColor(multBin));
        canvasManager.RequireCanvas("canvas" + task.purityKey + "Purity")->cd();
        h1PurityAnalysis->DrawCopy(multBin == 0 ? "" : "SAME");

        if (summaryDir) {
          summaryDir->cd();
          h1PurityAnalysis->Write(nullptr, TObject::kOverwrite);
        }

        // When 'rebinning_pt' asked for a coarser analysis binning, the purity is
        // produced at the source binning as well. It is a second full set of fits,
        // not a reshaping of the first: a purity is a ratio, so the fine values
        // cannot be recovered from the merged ones. It is not what the analysis
        // uses - a per-candidate correction needs the fine one.
        std::unique_ptr<TH1> h1PuritySeparateSource;
        if (task.rebinningPt) {
          // A copy rather than a push_back on fitPath: that vector is still the
          // analysis path, and anything added below this line would silently start
          // resolving to the subdirectory instead.
          std::vector<std::string> sourceFitPath = fitPath;
          sourceFitPath.push_back("SourceBinning");

          TDirectory* fitDirSource = RootIO::GetOrCreatePath(outputFile.get(), sourceFitPath, false);
          h1PuritySeparateSource = FitPuritySpectrum(task, multBin, SpectrumBinning::Source, fitDirSource);

          CanvasManager::SetHistogramStyle(h1PuritySeparateSource.get(), globalCfgs.GetSpectraColor(multBin));
          canvasManager.RequireCanvas("canvas" + task.purityKey + "Purity_sourceBinning")->cd();
          h1PuritySeparateSource->DrawCopy(multBin == 0 ? "" : "SAME");

          // Own subdirectory rather than sitting next to the analysis spectra.
          // Created here, inside the branch that produces the spectrum, so it
          // exists if and only if something was written into it.
          std::vector<std::string> sourceSummaryPath = summaryPath;
          sourceSummaryPath.push_back("SourceBinning");
          if (TDirectory* sourceDir = RootIO::GetOrCreatePath(outputFile.get(), sourceSummaryPath, false)) {
            sourceDir->cd();
            h1PuritySeparateSource->Write(nullptr, TObject::kOverwrite);
          }
        }

        // A separate source-binning spectrum is only produced when the analysis one
        // is coarser; without a merge the analysis spectrum IS at the source binning
        // and fitting it twice would be wasted work. Named here so that the call
        // below reads as what it is, and checked inside FillCCDBColumn so that a
        // coarse spectrum can never reach the map.
        const TH1* h1PuritySource = h1PuritySeparateSource ? h1PuritySeparateSource.get() : h1PurityAnalysis.get();
        FillCCDBColumn(task, multBin, h1PuritySource);
      }
    }
  }

  void Terminate() override
  {
    Logger::Info("{}: TERMINATING AND SAVING...", GetName());

    // 1. The summary canvases, the provenance, the stamp and the fit quality
    canvasManager.WriteAll(outputFile.get());
    RootIO::WriteProvenance(outputFile.get(), globalCfgs.binningName, provenance);

    // The pT binning is the axis of each purity spectrum; the multiplicity binning is
    // in the names, and stamped (RootIO::RequireMatchingBinningStamp).
    TDirectory* schemeDir = RootIO::GetOrCreatePath(outputFile.get(), {globalCfgs.binningName}, false);
    if (!schemeDir)
      throw std::runtime_error(std::format("[FATAL] {}: cannot create '{}' in {}: the binning stamp and the fit quality would be lost.",
                                           GetName(), globalCfgs.binningName, outputFile->GetName()));
    RootIO::WriteBinningStamp(schemeDir, RootIO::kStampMult, multBinning);
    fitQuality.Write(schemeDir, GetName());

    // 2. Write the CCDB-ready maps, one file per particle, object named "ccdb_object"
    for (auto& task : particleTasks) {
      if (!task.h2PurityCCDB)
        continue;

      std::string ccdbPath = std::format("{}{}h2PurityMap{}.root", ccdbOutputDir, outputPrefix, task.purityKey);
      RootIO::WriteCcdbFile(ccdbPath, task.h2PurityCCDB.get(), GetName());
    }

    // 3. Close the output file
    outputFile->Close();

    Logger::Info("{}: DONE.", GetName());
  }

 private:
  AnalysisSettings globalCfgs;

  CanvasManager canvasManager;

  std::string ccdbOutputDir;
  std::string outputPrefix;

  // Filled in Init, written into the output file in Terminate
  std::map<std::string, std::string> provenance;

  std::unique_ptr<TFile> outputFile;
  std::vector<ParticleTask> particleTasks;
  FitQualityRecorder fitQuality; // every fit of every particle, both binnings

  // Read from the input file in Init(), never from the configuration
  std::vector<double> multBinning;

  std::unique_ptr<FitConfigManager> fitConfigManager{nullptr};

  // Which of the two binnings a spectrum is produced at. One parameter and not
  // three because the edges, the bin mapping and the name suffix are three faces
  // of a single choice: passed separately, nothing would stop them disagreeing.
  enum class SpectrumBinning { Analysis,
                               Source };

  // Builds one purity spectrum: for every bin, fits the mass distribution obtained
  // by merging the source bins it covers and reads S/(S+B) off the fit. At the
  // source binning each bin covers exactly itself.
  std::unique_ptr<TH1> FitPuritySpectrum(ParticleTask& task, int multBin, SpectrumBinning which, TDirectory* fitDir)
  {
    const bool atSource = (which == SpectrumBinning::Source);
    const std::vector<double>& binning = atSource ? task.sourceBinning : task.AnalysisBinning();
    const std::string nameSuffix = atSource ? "_sourceBinning" : "";
    const std::string titleSuffix = atSource ? ", source binning" : "";
    // The purity_key as a TLatex label: an underscore would make the rest a subscript ("pi_tpc").
    std::string label = task.purityKey;
    std::ranges::replace(label, '_', ' ');

    std::string hName = OutputNames::Purity(task.purityKey, OutputNames::MultTag(multBinning, multBin)) + nameSuffix;
    std::unique_ptr<TH1> h1PuritySpectrum = std::make_unique<TH1F>(hName.c_str(), "; p_{T} (GeV/#it{c}); S/(S+B)",
                                                                   BinningUtils::NBins(binning), binning.data());
    h1PuritySpectrum->SetDirectory(0);

    for (int ptBin : BinningUtils::BinIndices(binning)) {
      const std::string cell = OutputNames::CellTag(multBinning, multBin, binning, ptBin);
      std::string histName = std::format("h1{}_{}{}", task.purityKey, cell, nameSuffix);

      const auto mult = BinningUtils::BinRange::Single(multBin);
      const BinningUtils::BinRange pt = atSource ? BinningUtils::BinRange::Single(ptBin) : task.mappedSourceBins[ptBin];
      std::unique_ptr<TH1> h1Data(static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), mult.first, mult.last, pt.first, pt.last)));
      h1Data->SetDirectory(0);

      // Run Fitter using JSON-based configuration
      FitConfig cfg = fitConfigManager->GetConfig(task.purityKey, cell);
      DynamicRooFitter fitter(h1Data.get(), cfg);

      fitter.DoFit();
      fitQuality.Add(task.purityKey, atSource ? FitQualityRecorder::kSourceBinning : FitQualityRecorder::kAnalysisBinning, multBinning,
                     multBin, binning, ptBin, cfg.binOverride, fitter.Diagnostics());

      auto res = fitter.ExtractYieldsAndPurity();
      HistogramUtils::SetBinValueAndError(h1PuritySpectrum.get(), ptBin, res.purityAndError);

      // The fit's diagnostic canvas, written into the particle's directory and destroyed
      // per fit, as in PhiFitTask: holding them all for the WriteAll in Terminate would
      // only cost memory, since nothing reads a fit canvas back.
      const std::string cName = std::format("cFit_{}_{}{}", task.purityKey, cell, nameSuffix);
      const std::string title = OutputNames::CellTitle(label, multBinning, multBin, binning, ptBin) + titleSuffix;
      TCanvas* cFit = canvasManager.CreateCanvas(cName, title, 800, 800);
      // log: the background under a peak of 1e5-1e8 counts is at 1e-2-1e-4 of it, and it decides the purity
      fitter.DrawFit(cFit, title, /*logY=*/true);
      CanvasManager::DrawAliceLabel(cFit, 0.18, 0.84, "Work in progress", "", 0.045);
      canvasManager.WriteAndDestroy(cName, fitDir);
    }

    return h1PuritySpectrum;
  }

  // Copies one multiplicity column into the CCDB map. The map must stay at the
  // source binning whatever the offline analysis chose: O2 applies it to single
  // candidates, so a coarser binning there would be this framework's choice
  // leaking into a correction that is not its own. Edges are compared, not bin
  // counts: two different binnings can have the same number of bins.
  static void FillCCDBColumn(ParticleTask& task, int multBin, const TH1* h1Purity)
  {
    BinningUtils::RequireSameAxis(h1Purity->GetXaxis(), task.h2PurityCCDB->GetYaxis(),
                                  std::format("spectrum given to the '{}' CCDB map", task.purityKey),
                                  "that map's pT axis (the source binning)");

    for (int ptBin = 0; ptBin < h1Purity->GetNbinsX(); ++ptBin)
      HistogramUtils::SetBinValueAndError(task.h2PurityCCDB.get(), multBin, ptBin, HistogramUtils::BinValueAndError(h1Purity, ptBin));
  }
};

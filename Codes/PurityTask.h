#pragma once

#include "AnalysisConstants.h"
#include "AnalysisStructures.h"
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

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <span>
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

    provenance = RootIO::StartProvenance(JsonConfig::Serialize(taskConfig));

    // 1. Open input file, extract 3D histograms to RAM, and immediately close it
    std::string inputPath = JsonConfig::RequireString(taskConfig, "input_data_file", GetName());
    std::unique_ptr<TFile> inputFile = RootIO::OpenOrThrow(inputPath, "READ", GetName());

    // 2. Open the output file: one for every particle of the block. UPDATE and this binning's
    // subtree cleared, not RECREATE: the file holds one subtree per binning_name, and another
    // scheme's must survive this one's run (CLAUDE.md, How a task opens the file it writes).
    std::string outputDir = JsonConfig::RequireString(taskConfig, "output_dir", GetName());

    // Prefix to specify the type of analysis (Data vs MCClosure)
    outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());
    outputFile = RootIO::OpenOrThrow(std::format("{}{}{}", outputDir, outputPrefix, OutputNames::kPurityFile), "UPDATE", GetName());

    // CCDB-ready maps live under outputDir/{binningName}/ on disk, since the ROOT
    // object inside must be named literally "ccdb_object" with no subfolder.
    // Same layout as MCTask, so the two kinds of correction sit side by side.
    ccdbOutputDir = std::format("{}{}/", outputDir, globalCfgs.binningName);
    RootIO::CreateDirectoriesOrThrow(ccdbOutputDir, GetName());

    auto particles = JsonConfig::RequireArray(taskConfig, "purity_particles", GetName());

    int pIdx = 0;
    const std::vector<std::string> summaryPath = OutputNames::SummaryPath(globalCfgs.binningName);

    for (const auto& entry : particles) {
      const std::string ctx = std::format("{} purity_particles[{}]", GetName(), pIdx++);
      JsonConfig::RequireKnownKeys(entry, {"name", "purity_key", "hist_name", "rebinning_pt"}, ctx);
      std::string particle = JsonConfig::RequireString(entry, "name", ctx);
      std::string purityKey = JsonConfig::RequireString(entry, "purity_key", ctx);
      std::string histName = JsonConfig::RequireString(entry, "hist_name", ctx);

      std::unique_ptr<TH3F> h3Source = RootIO::GetUniqueOrThrow<TH3F>(inputFile.get(), histName, GetName());

      // The binning comes from the file, never from the configuration: the loops
      // below address these very bins, and the purity spectrum is built on these
      // very edges. h3 axes: (mult, pT, invariant mass).
      const std::string origin = std::format("{} in '{}'", histName, inputPath);
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
      std::string ccdbName = std::format("h2{}Purity", purityKey);
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
      canvasManager.SetAutoYRange(canvasManager.CreateCanvas(OutputNames::PuritySummaryCanvas(purityKey, SpectrumBinning::Analysis), std::format("{} Purity", purityKey),
                                                             summaryPath, 800, 600));
      if (particleTasks.back().rebinningPt) {
        canvasManager.SetAutoYRange(canvasManager.CreateCanvas(OutputNames::PuritySummaryCanvas(purityKey, SpectrumBinning::Source),
                                                               std::format("{} Purity (source binning)", purityKey),
                                                               OutputNames::SummaryPath(globalCfgs.binningName, SpectrumBinning::Source), 800, 600));
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
    // The classes and their union (with one class, the union is that class: the same cells, declared once).
    for (const AnalysisStructures::ParticleTask& task : particleTasks)
      fitConfigManager->DeclareCells(task.purityKey, {multBinning, BinningUtils::Whole(multBinning)},
                                     {task.sourceBinning, task.AnalysisBinning()});

    // Last, once the whole configuration has been read: a mistake in it must not cost the previous production.
    RootIO::ClearPath(outputFile.get(), {globalCfgs.binningName});
  }

  void Run() override
  {
    Logger::Info("{}: RUNNING FITS...", GetName());

    for (int multBin : BinningUtils::BinIndices(multBinning))
      for (auto& task : particleTasks)
        FillCCDBColumn(task, multBin,
                       FitClass(task, multBinning, multBin, globalCfgs.GetSpectraColor(multBin), multBin == 0 ? "" : "SAME").get());

    // The union of the classes, fitted on its merged mass distribution like any class: a purity is a ratio, and an
    // average of the classes' would not be it. Written next to them, for what compares a class with the whole; not
    // in the CCDB map, which O2 reads class by class.
    if (const auto classesUnion = OutputNames::MultUnion(multBinning))
      for (auto& task : particleTasks)
        FitClass(task, *classesUnion, 0, kBlack, "SAME");
  }

  void Terminate() override
  {
    Logger::Info("{}: TERMINATING AND SAVING...", GetName());

    // 1. The summary canvases, the provenance, the stamp and the fit quality
    canvasManager.WriteAll(outputFile.get());
    RootIO::WriteProvenance(outputFile.get(), globalCfgs.binningName, provenance);

    // The pT binning is the axis of each purity spectrum; the multiplicity binning is
    // in the names, and stamped (RootIO::RequireMatchingBinningStamp).
    TDirectory* schemeDir = RootIO::MakePath(outputFile.get(), {globalCfgs.binningName});
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
  std::vector<AnalysisStructures::ParticleTask> particleTasks;
  FitQualityRecorder fitQuality; // every fit of every particle, both binnings

  // Read from the input file in Init(), never from the configuration
  std::vector<double> multBinning;

  std::unique_ptr<FitConfigManager> fitConfigManager{nullptr};

  // Which of the two binnings a spectrum is produced at. One parameter and not
  // three because the edges, the bin mapping and the name suffix are three faces
  // of a single choice: passed separately, nothing would stop them disagreeing.
  using SpectrumBinning = OutputNames::SpectrumBinning;

  // The purity of class 'classBin' of 'classes' (multBinning, or OutputNames::MultUnion), written into the summary directory
  // and drawn on the summary canvases in 'color'; returned at the source binning, which the CCDB map takes.
  std::unique_ptr<TH1> FitClass(AnalysisStructures::ParticleTask& task, std::span<const double> classes, int classBin, int color,
                                const char* drawOption)
  {
    auto drawOnSummary = [&](const TH1* h, SpectrumBinning which) {
      canvasManager.RequireCanvas(OutputNames::PuritySummaryCanvas(task.purityKey, which))->cd();
      CanvasManager::DrawStyledCopy(h, drawOption, color);
    };

    // Every particle's fits in a directory of its own: they share the file.
    TDirectory* fitDir = RootIO::MakePath(outputFile.get(), OutputNames::PurityFitsPath(globalCfgs.binningName, task.purityKey));
    std::unique_ptr<TH1> h1PurityAnalysis = FitPuritySpectrum(task, classes, classBin, SpectrumBinning::Analysis, fitDir);
    drawOnSummary(h1PurityAnalysis.get(), SpectrumBinning::Analysis);
    RootIO::MakePath(outputFile.get(), OutputNames::SummaryPath(globalCfgs.binningName))->cd();
    h1PurityAnalysis->Write(nullptr, TObject::kOverwrite);

    // A separate source-binning spectrum is only produced when the analysis one
    // is coarser; without a merge the analysis spectrum IS at the source binning
    // and fitting it twice would be wasted work. FillCCDBColumn checks the axis, so
    // that a coarse spectrum can never reach the map.
    if (!task.rebinningPt)
      return h1PurityAnalysis;

    // When 'rebinning_pt' asked for a coarser analysis binning, the purity is
    // produced at the source binning as well. It is a second full set of fits,
    // not a reshaping of the first: a purity is a ratio, so the fine values
    // cannot be recovered from the merged ones. It is not what the analysis
    // uses - a per-candidate correction needs the fine one.
    TDirectory* fitDirSource = RootIO::MakePath(outputFile.get(), OutputNames::PurityFitsPath(globalCfgs.binningName, task.purityKey, SpectrumBinning::Source));
    std::unique_ptr<TH1> h1PuritySource = FitPuritySpectrum(task, classes, classBin, SpectrumBinning::Source, fitDirSource);
    drawOnSummary(h1PuritySource.get(), SpectrumBinning::Source);

    // Own subdirectory rather than sitting next to the analysis spectra.
    // Created here, inside the branch that produces the spectrum, so it
    // exists if and only if something was written into it.
    RootIO::MakePath(outputFile.get(), OutputNames::SummaryPath(globalCfgs.binningName, SpectrumBinning::Source))->cd();
    h1PuritySource->Write(nullptr, TObject::kOverwrite);
    return h1PuritySource;
  }

  // Builds one purity spectrum: for every bin, fits the mass distribution obtained
  // by merging the source bins it covers and reads S/(S+B) off the fit. At the
  // source binning each bin covers exactly itself. The same holds on the
  // multiplicity axis: class 'classBin' of 'classes' covers the bins of multBinning
  // between its edges.
  std::unique_ptr<TH1> FitPuritySpectrum(AnalysisStructures::ParticleTask& task, std::span<const double> classes, int classBin,
                                         SpectrumBinning which, TDirectory* fitDir)
  {
    const bool atSource = (which == SpectrumBinning::Source);
    const std::vector<double>& binning = atSource ? task.sourceBinning : task.AnalysisBinning();
    const std::string nameSuffix{OutputNames::BinningSuffix(which)};
    const std::string titleSuffix = atSource ? ", source binning" : "";
    // The purity_key as a TLatex label: an underscore would make the rest a subscript ("pi_tpc").
    std::string label = task.purityKey;
    std::ranges::replace(label, '_', ' ');

    std::string hName = OutputNames::Purity(task.purityKey, OutputNames::MultTag(classes, classBin), which);
    std::unique_ptr<TH1> h1PuritySpectrum = std::make_unique<TH1F>(hName.c_str(), "; p_{T} (GeV/#it{c}); S/(S+B)",
                                                                   BinningUtils::NBins(binning), binning.data());
    h1PuritySpectrum->SetDirectory(0);

    const BinningUtils::BinRange mult = BinningUtils::MapToSourceBins(multBinning, classes)[classBin];
    for (int ptBin : BinningUtils::BinIndices(binning)) {
      const std::string cell = OutputNames::CellTag(classes, classBin, binning, ptBin);
      std::string histName = std::format("h1{}_{}{}", task.purityKey, cell, nameSuffix);

      const BinningUtils::BinRange pt = atSource ? BinningUtils::BinRange::Single(ptBin) : task.mappedSourceBins[ptBin];
      std::unique_ptr<TH1> h1Data(static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), mult.first, mult.last, pt.first, pt.last)));
      h1Data->SetDirectory(0);

      // Run Fitter using JSON-based configuration
      AnalysisStructures::FitConfig cfg = fitConfigManager->GetConfig(task.purityKey, cell);
      DynamicRooFitter fitter(h1Data.get(), cfg);

      fitter.DoFit();
      fitQuality.Add(task.purityKey, atSource ? FitQualityRecorder::kSourceBinning : FitQualityRecorder::kAnalysisBinning, classes,
                     classBin, binning, ptBin, cfg, fitter.Diagnostics());

      auto res = fitter.ExtractYieldsAndPurity();
      HistogramUtils::SetBinValueAndError(h1PuritySpectrum.get(), ptBin, res.purityAndError);

      // The fit's diagnostic canvas, written into the particle's directory and destroyed
      // per fit, as in PhiFitTask: holding them all for the WriteAll in Terminate would
      // only cost memory, since nothing in the run reads a fit canvas back.
      const std::string cName = OutputNames::PurityFitCanvas(task.purityKey, cell, which);
      const std::string title = OutputNames::CellTitle(label, classes, classBin, binning, ptBin) + titleSuffix;
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
  static void FillCCDBColumn(AnalysisStructures::ParticleTask& task, int multBin, const TH1* h1Purity)
  {
    BinningUtils::RequireSameAxis(h1Purity->GetXaxis(), task.h2PurityCCDB->GetYaxis(),
                                  std::format("spectrum given to the '{}' CCDB map", task.purityKey),
                                  "that map's pT axis (the source binning)");

    for (int ptBin = 0; ptBin < h1Purity->GetNbinsX(); ++ptBin)
      HistogramUtils::SetBinValueAndError(task.h2PurityCCDB.get(), multBin, ptBin, HistogramUtils::BinValueAndError(h1Purity, ptBin));
  }
};

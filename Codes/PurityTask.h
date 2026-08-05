#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH2F.h"
#include "TH3F.h"

#include <filesystem>
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
    std::cout << "[INFO] PurityTask: INITIALIZING..." << std::endl;

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
    std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", "PurityTask");
    std::unique_ptr<TFile> fileInput = RootIO::OpenOrThrow(inputFile, "READ", "PurityTask");

    // 2. Open Output files
    std::string outputDir = JsonConfig::RequireString(taskConfig, "output_dir", GetName());

    // Prefix to specify the type of analysis (Data vs MCClosure)
    outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

    // CCDB-ready maps live under outputDir/{binningName}/ on disk, since the ROOT
    // object inside must be named literally "ccdb_object" with no subfolder.
    // Same layout as MCTask, so the two kinds of correction sit side by side.
    ccdbOutputDir = outputDir + globalCfgs.binningName + "/";
    std::error_code ec;
    std::filesystem::create_directories(ccdbOutputDir, ec);
    if (ec)
      throw std::runtime_error("[FATAL] PurityTask: Cannot create output directory '" + ccdbOutputDir + "': " + ec.message());

    auto particles = JsonConfig::RequireArray(taskConfig, "purity_particles", "PurityTask");

    for (const auto& particle : particles) {
      std::string name = particle["name"].GetString();
      std::string purityKey = particle["purity_key"].GetString();
      std::string histName = particle["hist_name"].GetString();
      std::string outputFileSuffix = particle["output_file_suffix"].GetString();

      std::unique_ptr<TH3F> h3Source = RootIO::GetUniqueOrThrow<TH3F>(fileInput.get(), histName, "PurityTask");

      // The binning comes from the file, never from the configuration: the loops
      // below address these very bins, and the purity spectrum is built on these
      // very edges. h3 axes: (mult, pT, invariant mass).
      const std::string origin = histName + " in '" + inputFile + "'";
      multBinning = globalCfgs.ResolveMultBinning(h3Source->GetXaxis(), origin);
      std::vector<double> sourceBinning = globalCfgs.ResolvePtBinning(name, h3Source->GetYaxis(), origin);

      // A coarser analysis binning is obtained by fitting the MERGED mass
      // distribution of the source bins it covers, not by merging the purities
      // afterwards: a purity is a ratio, and ratios do not add up.
      std::optional<std::vector<double>> rebinningPt;
      if (auto rebin = JsonConfig::TryArray(particle, "rebinning_pt", GetName())) {
        std::vector<double> bins = JsonConfig::ReadNumberArray(*rebin, "rebinning_pt for '" + name + "'", GetName());
        if (bins.size() < 2) {
          throw std::runtime_error("[FATAL] PurityTask: 'rebinning_pt' for '" + name + "' needs at least 2 edges!");
        }
        rebinningPt = std::move(bins);
      }

      std::string outputFileName = outputDir + outputPrefix + outputFileSuffix;

      // Reuse an already-open file if another particle points to the same suffix
      TFile* outputFilePtr{nullptr};

      auto it = outputFiles.find(outputFileName);
      if (it != outputFiles.end()) {
        outputFilePtr = it->second.get();
      } else {
        std::unique_ptr<TFile> outputFile = RootIO::OpenOrThrow(outputFileName, "RECREATE", "PurityTask");
        outputFiles[outputFileName] = std::move(outputFile);
        outputFilePtr = outputFiles[outputFileName].get();
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
                                 std::move(rebinningPt), outputFilePtr, std::move(h2CCDB));
    }

    // 3. Read task-specific settings from the JSON node (DOM)
    // Keep the fit configuration file completely separated for physics tuning
    std::string fitCfgPath = JsonConfig::RequireString(taskConfig, "fit_config_file", "PurityTask");

    // A snapshot, not a path: this is the file being edited while the fits are
    // tuned, so knowing which one it was says nothing about what was in it.
    provenance["fit_config"] = JsonConfig::ReadFileText(fitCfgPath);

    // 4. Initialize specific mathematical tools
    fitConfigManager = std::make_unique<FitConfigManager>(fitCfgPath);
  }

  void Run() override
  {
    std::cout << "[INFO] PurityTask: RUNNING FITS..." << std::endl;

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};

    for (int i{0}; i < BinningUtils::NBins(multBinning); i++) {
      for (auto& task : particleTasks) {
        std::vector<std::string> fitPath = {globalCfgs.binningName, "Fits"};
        if (task.name == "pi_tpc" || task.name == "pi_tof")
          fitPath.push_back(task.name);

        TDirectory* summaryDir = RootIO::GetOrCreatePath(task.outputFile, summaryPath, false);
        TDirectory* fitDir = RootIO::GetOrCreatePath(task.outputFile, fitPath, false);

        std::unique_ptr<TH1> h1PurityAnalysis = FitPuritySpectrum(task, i, SpectrumBinning::Analysis, fitDir);

        // Style and draw on the summary canvas
        AnalysisUtils::SetHistogramStyle(h1PurityAnalysis.get(), globalCfgs.GetSpectraColor(i));
        task.canvas->cd();
        h1PurityAnalysis->DrawCopy(i == 0 ? "" : "SAME");

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
          h1PuritySeparateSource = FitPuritySpectrum(task, i, SpectrumBinning::Source, fitDir);

          AnalysisUtils::SetHistogramStyle(h1PuritySeparateSource.get(), globalCfgs.GetSpectraColor(i));
          task.canvasSourceBinning->cd();
          h1PuritySeparateSource->DrawCopy(i == 0 ? "" : "SAME");

          // Own subdirectory rather than sitting next to the analysis spectra.
          // Created here, inside the branch that produces the spectrum, so it
          // exists if and only if something was written into it.
          std::vector<std::string> fineSummaryPath = summaryPath;
          fineSummaryPath.push_back("SourceBinning");
          if (TDirectory* fineDir = RootIO::GetOrCreatePath(task.outputFile, fineSummaryPath, false)) {
            fineDir->cd();
            h1PuritySeparateSource->Write(nullptr, TObject::kOverwrite);
          }
        }

        // A separate source-binning spectrum is only produced when the analysis one
        // is coarser; without a merge the analysis spectrum IS at the source binning
        // and fitting it twice would be wasted work. Named here so that the call
        // below reads as what it is, and checked inside FillCCDBColumn so that a
        // coarse spectrum can never reach the map.
        const TH1* h1PuritySource = h1PuritySeparateSource ? h1PuritySeparateSource.get() : h1PurityAnalysis.get();
        FillCCDBColumn(task, i, h1PuritySource);
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PurityTask: TERMINATING AND SAVING..." << std::endl;

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};

    // 1. Save each particle's summary canvas into its own output file
    auto writeCanvas = [](TFile* file, const std::vector<std::string>& path, TCanvas* canvas) {
      if (!canvas)
        return;
      if (TDirectory* dir = RootIO::GetOrCreatePath(file, path, false)) {
        dir->cd();
        canvas->Write(nullptr, TObject::kOverwrite);
      }
    };

    for (auto& task : particleTasks) {
      writeCanvas(task.outputFile, summaryPath, task.canvas.get());

      std::vector<std::string> sourceSummaryPath = summaryPath;
      sourceSummaryPath.push_back("SourceBinning");
      writeCanvas(task.outputFile, sourceSummaryPath, task.canvasSourceBinning.get());

      // The pT binning is the axis of each purity spectrum, so the consumer can
      // read it. The multiplicity binning is only an index in the names
      // ("..._multBin3") and would otherwise be unverifiable.
      if (TDirectory* schemeDir = RootIO::GetOrCreatePath(task.outputFile, {globalCfgs.binningName}, false))
        RootIO::WriteBinningStamp(schemeDir, "binning_mult", multBinning);

      // Written per file because several particles can share one, e.g. pi_tpc and pi_tof.
      RootIO::WriteProvenance(RootIO::GetOrCreatePath(task.outputFile, {globalCfgs.binningName, "Provenance"}, false), provenance);
    }

    // 2. Write the CCDB-ready maps, one file per particle, object named "ccdb_object"
    for (auto& task : particleTasks) {
      if (!task.h2PurityCCDB)
        continue;

      std::string ccdbPath = ccdbOutputDir + outputPrefix + "h2PurityMap" + task.name + ".root";
      std::unique_ptr<TFile> ccdbFile = RootIO::OpenOrThrow(ccdbPath, "RECREATE", "PurityTask");
      ccdbFile->cd();
      task.h2PurityCCDB->SetName("ccdb_object");
      task.h2PurityCCDB->Write(nullptr, TObject::kOverwrite);
      ccdbFile->Close();
    }

    // 3. Close output files
    for (auto& [name, file] : outputFiles) {
      if (file) {
        file->Close();
      }
    }

    std::cout << "[INFO] PurityTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::string ccdbOutputDir;
  std::string outputPrefix;

  // Filled in Init, written into the output file in Terminate
  std::map<std::string, std::string> provenance;

  // outputFiles must be declared before particleTasks to ensure proper destruction order:
  // particleTasks holds raw pointers to TFile objects managed by outputFiles, so outputFiles must outlive particleTasks.
  std::map<std::string, std::unique_ptr<TFile>> outputFiles;
  std::vector<ParticleTask> particleTasks;

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

    std::string hName = "h1" + task.name + "Purity_multBin" + std::to_string(multBin) + nameSuffix;
    std::unique_ptr<TH1> h1PuritySpectrum = std::make_unique<TH1F>(hName.c_str(), "; p_{T} (GeV/#it{c}); S/(S+B)",
                                                                   BinningUtils::NBins(binning), binning.data());
    h1PuritySpectrum->SetDirectory(0);

    for (int k{0}; k < BinningUtils::NBins(binning); k++) {
      std::string histName = "h1" + task.name + "_multBin" + std::to_string(multBin) + "_ptBin" + std::to_string(k) + nameSuffix;

      const BinningUtils::BinRange range = atSource ? BinningUtils::BinRange{k + 1, k + 1} : task.mappedSourceBins[k];
      std::unique_ptr<TH1> h1Data(static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), multBin + 1, multBin + 1,
                                                                                range.first, range.last)));
      h1Data->SetDirectory(0);

      // Run Fitter using JSON-based configuration
      FitConfig cfg = fitConfigManager->GetConfig(task.name, multBin, k);
      DynamicRooFitter fitter(h1Data.get(), cfg);

      fitter.DoFit();

      auto res = fitter.ExtractYieldsAndPurity();
      h1PuritySpectrum->SetBinContent(k + 1, res.purityAndError.first);
      h1PuritySpectrum->SetBinError(k + 1, res.purityAndError.second);

      // Save diagnostic plot directly to the task's output file
      std::string cName = "cFit_" + task.name + "_m" + std::to_string(multBin) + "_p" + std::to_string(k) + nameSuffix;
      fitter.SaveFitCanvas(fitDir, cName);
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
                                  "spectrum given to the '" + task.name + "' CCDB map",
                                  "that map's pT axis (the source binning)");

    for (int k{0}; k < h1Purity->GetNbinsX(); k++) {
      task.h2PurityCCDB->SetBinContent(multBin + 1, k + 1, h1Purity->GetBinContent(k + 1));
      task.h2PurityCCDB->SetBinError(multBin + 1, k + 1, h1Purity->GetBinError(k + 1));
    }
  }
};

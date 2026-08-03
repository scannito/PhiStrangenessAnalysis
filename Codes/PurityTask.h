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

#include "TCanvas.h"
#include "TFile.h"
#include "TH3F.h"

#include <iostream>
#include <map>
#include <memory>
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

    // 1. Open input file, extract 3D histograms to RAM, and immediately close it
    std::string inputFile = JsonConfig::RequireString(taskConfig, "input_data_file", "PurityTask");
    std::unique_ptr<TFile> fileInput = RootIO::OpenOrThrow(inputFile, "READ", "PurityTask");

    // 2. Open Output files
    std::string outputDir = JsonConfig::RequireString(taskConfig, "output_dir", GetName());

    // Prefix to specify the type of analysis (Data vs MCClosure)
    std::string prefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

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
      std::vector<double> analysisBinning = sourceBinning;
      if (auto rebin = JsonConfig::OptionalArray(particle, "rebinning_pt", GetName())) {
        analysisBinning.clear();
        for (const auto& v : *rebin)
          analysisBinning.push_back(v.GetDouble());

        if (analysisBinning.size() < 2) {
          throw std::runtime_error("[FATAL] PurityTask: 'rebinning_pt' for '" + name + "' needs at least 2 edges!");
        }
      }

      std::vector<BinningUtils::BinRange> sourceBins = BinningUtils::MapToSourceBins(sourceBinning, analysisBinning);

      std::string outputFileName = outputDir + prefix + outputFileSuffix;

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

      std::string canvasName = "canvas" + purityKey + "Purity";
      std::string canvasTitle = purityKey + " Purity";
      std::unique_ptr<TCanvas> canvas = std::make_unique<TCanvas>(canvasName.c_str(), canvasTitle.c_str(), 800, 600);

      particleTasks.emplace_back(purityKey, std::move(h3Source), std::move(analysisBinning),
                                 std::move(sourceBins), outputFilePtr, std::move(canvas));
    }

    // 3. Read task-specific settings from the JSON node (DOM)
    // Keep the fit configuration file completely separated for physics tuning
    std::string fitCfgPath = JsonConfig::RequireString(taskConfig, "fit_config_file", "PurityTask");

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

        std::string hName = "h1" + task.name + "Purity_multBin" + std::to_string(i);
        std::unique_ptr<TH1> h1PuritySpectrum = std::make_unique<TH1F>(hName.c_str(), "; p_{T} (GeV/#it{c}); S/(S+B)",
                                                                       BinningUtils::NBins(task.analysisBinning),
                                                                       task.analysisBinning.data());
        h1PuritySpectrum->SetDirectory(0);

        for (int k{0}; k < BinningUtils::NBins(task.analysisBinning); k++) {
          std::string histName = "h1" + task.name + "_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);

          // Project the whole range of source bins that this analysis bin covers.
          // With no rebinning the range is a single bin and this is the old behaviour.
          const BinningUtils::BinRange& range = task.sourceBins[k];
          std::unique_ptr<TH1> h1Data(static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), i + 1, i + 1,
                                                                                    range.first, range.last)));
          h1Data->SetDirectory(0);

          // Run Fitter using JSON-based configuration
          FitConfig cfg = fitConfigManager->GetConfig(task.name, i, k);
          DynamicRooFitter fitter(h1Data.get(), cfg);

          fitter.DoFit();

          auto res = fitter.ExtractYieldsAndPurity();
          h1PuritySpectrum->SetBinContent(k + 1, res.purityAndError.first);
          h1PuritySpectrum->SetBinError(k + 1, res.purityAndError.second);

          // Save diagnostic plot directly to the task's output file
          std::string cName = "cFit_" + task.name + "_m" + std::to_string(i) + "_p" + std::to_string(k);
          fitter.SaveFitCanvas(fitDir, cName);
        }

        // Style and draw on the summary canvas
        AnalysisUtils::SetHistogramStyle(h1PuritySpectrum.get(), globalCfgs.GetSpectraColor(i));
        task.canvas->cd();
        h1PuritySpectrum->DrawCopy(i == 0 ? "" : "SAME");

        if (summaryDir) {
          summaryDir->cd();
          h1PuritySpectrum->Write(nullptr, TObject::kOverwrite);
        }
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PurityTask: TERMINATING AND SAVING..." << std::endl;

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};

    // 1. Save each particle's summary canvas into its own output file
    for (auto& task : particleTasks) {
      TDirectory* summaryDir = RootIO::GetOrCreatePath(task.outputFile, summaryPath, false);
      if (summaryDir) {
        summaryDir->cd();
        task.canvas->Write(nullptr, TObject::kOverwrite);
      }

      // The pT binning is the axis of each purity spectrum, so the consumer can
      // read it. The multiplicity binning is only an index in the names
      // ("..._multBin3") and would otherwise be unverifiable.
      if (TDirectory* schemeDir = RootIO::GetOrCreatePath(task.outputFile, {globalCfgs.binningName}, false))
        RootIO::WriteBinningStamp(schemeDir, "binning_mult", multBinning);
    }

    // 2. Close output files
    for (auto& [name, file] : outputFiles) {
      if (file) {
        file->Close();
      }
    }

    std::cout << "[INFO] PurityTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  // outputFiles must be declared before particleTasks to ensure proper destruction order:
  // particleTasks holds raw pointers to TFile objects managed by outputFiles, so outputFiles must outlive particleTasks.
  std::map<std::string, std::unique_ptr<TFile>> outputFiles;
  std::vector<ParticleTask> particleTasks;

  // Read from the input file in Init(), never from the configuration
  std::vector<double> multBinning;

  std::unique_ptr<FitConfigManager> fitConfigManager{nullptr};
};

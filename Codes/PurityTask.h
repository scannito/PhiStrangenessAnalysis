#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "IAnalysisTask.h"

#include "TCanvas.h"
#include "TFile.h"
#include "TH3F.h"

#include <iostream>
#include <map>
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
    if (!taskConfig.HasMember("input_data_file")) {
      throw std::runtime_error("[FATAL] PurityTask: 'input_data_file' missing in JSON!");
    }
    if (!taskConfig.HasMember("purity_particles") || !taskConfig["purity_particles"].IsArray()) {
      throw std::runtime_error("[FATAL] PurityTask: 'purity_particles' array missing in JSON!");
    }

    std::string inputFile = taskConfig["input_data_file"].GetString();
    TFile* fileInput = new TFile(inputFile.c_str(), "READ");
    if (!fileInput || fileInput->IsZombie()) {
      throw std::runtime_error("[FATAL] PurityTask: Cannot open input file!");
    }

    // 2. Open Output files
    std::string outputDir = taskConfig["output_dir"].GetString();

    // Prefix to specify the type of analysis (Data vs MCClosure)
    std::string prefix = "";
    if (taskConfig.HasMember("output_prefix"))
      prefix = taskConfig["output_prefix"].GetString();

    for (const auto& particle : taskConfig["purity_particles"].GetArray()) {
      std::string name = particle["name"].GetString();
      std::string purityKey = particle["purity_key"].GetString();
      std::string histName = particle["hist_name"].GetString();
      std::string outputFileSuffix = particle["output_file_suffix"].GetString();

      TH3F* h3Source = static_cast<TH3F*>(fileInput->Get(histName.c_str()));
      if (!h3Source) {
        throw std::runtime_error("[FATAL] PurityTask: Missing histogram '" + histName + "' for particle '" + name + "'!");
      }
      h3Source->SetDirectory(0);

      std::string outputFileName = outputDir + prefix + outputFileSuffix;

      // Reuse an already-open file if another particle points to the same suffix
      TFile* outputFile{nullptr};
      auto it = outputFiles.find(outputFileName);
      if (it != outputFiles.end()) {
        outputFile = it->second;
      } else {
        outputFile = new TFile(outputFileName.c_str(), "RECREATE");
        if (!outputFile || outputFile->IsZombie()) {
          throw std::runtime_error("[FATAL] PurityTask: Cannot create output file '" + outputFileName + "'!");
        }
        outputFiles[outputFileName] = outputFile;
      }

      std::string canvasName = "canvas" + purityKey + "Purity";
      std::string canvasTitle = purityKey + " Purity";
      TCanvas* canvas = new TCanvas(canvasName.c_str(), canvasTitle.c_str(), 800, 600);

      const auto& binning = globalCfgs.GetPtBinning(name);
      particleTasks.emplace_back(purityKey, h3Source, static_cast<int>(binning.size()) - 1, binning, outputFile, canvas);
    }

    fileInput->Close();
    delete fileInput;

    // 3. Read task-specific settings from the JSON node (DOM)
    if (!taskConfig.HasMember("fit_config_file")) {
      throw std::runtime_error("[FATAL] PurityTask: 'fit_config_file' missing in JSON!");
    }

    // Keep the fit configuration file completely separated for physics tuning
    std::string fitCfgPath = taskConfig["fit_config_file"].GetString();

    // 4. Initialize specific mathematical tools
    fitConfigManager = new FitConfigManager(fitCfgPath);
  }

  void Run() override
  {
    std::cout << "[INFO] PurityTask: RUNNING FITS..." << std::endl;

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};

    for (int i{0}; i < globalCfgs.nBinMult; i++) {
      for (auto& task : particleTasks) {
        std::vector<std::string> fitPath = {globalCfgs.binningName, "Fits"};
        if (task.name == "pi_tpc" || task.name == "pi_tof")
          fitPath.push_back(task.name);

        TDirectory* summaryDir = AnalysisUtils::GetOrCreatePath(task.outputFile, summaryPath, false);
        TDirectory* fitDir = AnalysisUtils::GetOrCreatePath(task.outputFile, fitPath, false);

        std::string hName = "h1" + task.name + "Purity_multBin" + std::to_string(i);
        TH1* h1PuritySpectrum = new TH1F(hName.c_str(), "; p_{T} (GeV/#it{c}); S/(S+B)", task.nBinPt, task.binning.data());
        h1PuritySpectrum->SetDirectory(0);

        for (int k{0}; k < task.nBinPt; k++) {
          std::string histName = "h1" + task.name + "_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);

          // Project 1D slice
          TH1* h1Data = static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), i + 1, i + 1, k + 1, k + 1));
          h1Data->SetDirectory(0);

          // Run Fitter using JSON-based configuration
          FitConfig cfg = fitConfigManager->GetConfig(task.name, i, k);
          DynamicRooFitter fitter(h1Data, cfg);

          fitter.DoFit();

          auto res = fitter.ExtractYieldsAndPurity();
          h1PuritySpectrum->SetBinContent(k + 1, res.purity.first);
          h1PuritySpectrum->SetBinError(k + 1, res.purity.second);

          // Save diagnostic plot directly to the task's output file
          std::string cName = "cFit_" + task.name + "_m" + std::to_string(i) + "_p" + std::to_string(k);
          fitter.SaveFitCanvas(fitDir, cName);

          // Cleanup temporary 1D histogram
          delete h1Data;
        }

        // Style and draw on the summary canvas
        AnalysisUtils::SetHistogramStyle(h1PuritySpectrum, globalCfgs.GetSpectraColor(i));
        task.canvas->cd();
        h1PuritySpectrum->DrawCopy(i == 0 ? "" : "SAME");

        if (summaryDir) {
          summaryDir->cd();
          h1PuritySpectrum->Write(nullptr, TObject::kOverwrite);
        }

        // Cleanup temporary spectrum
        delete h1PuritySpectrum;
      }
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] PurityTask: TERMINATING AND SAVING..." << std::endl;

    std::vector<std::string> summaryPath = {globalCfgs.binningName, "Summary"};

    // 1. Save each particle's summary canvas into its own output file
    for (auto& task : particleTasks) {
      TDirectory* summaryDir = AnalysisUtils::GetOrCreatePath(task.outputFile, summaryPath, false);
      if (summaryDir) {
        summaryDir->cd();
        task.canvas->Write(nullptr, TObject::kOverwrite);
      }
    }

    // 2. Close output files (one Close/delete per distinct file, see note below)
    for (auto& [name, file] : outputFiles) {
      if (file) {
        file->Close();
        delete file;
      }
    }

    // 3. Free up RAM
    for (auto& task : particleTasks) {
      if (task.canvas)
        delete task.canvas;
      if (task.h3Source)
        delete task.h3Source;
    }

    if (fitConfigManager)
      delete fitConfigManager;

    std::cout << "[INFO] PurityTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  std::map<std::string, TFile*> outputFiles;

  std::vector<ParticleTask> particleTasks;
  FitConfigManager* fitConfigManager{nullptr};
};

#pragma once

// #include "CorrelationCalculator.h"
#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "CorrelationCalculator2.h"
#include "ExtrapolationModelFactory.h"
#include "SpectrumExtrapolator.h"
// #include "FitPhiSignalAndBkg.h"
#include "AnalysisConstants.h"
#include "AnalysisUtils.h"
#include "DynamicRooFitter.h"
#include "FitConfigManager.h"
#include "IAnalysisTask.h"

#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH1F.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <algorithm>
#include <array>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class DataTask : public IAnalysisTask
{
 public:
  std::string GetName() const override { return "data_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] DataTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings;

    // 1. Inherit global flags
    applyME = taskConfig["apply_mixed_events"].GetBool();
    applyEfficiency = taskConfig["apply_efficiency"].GetBool();
    applyPurity = taskConfig["apply_purity"].GetBool();
    applyExtrapolation = taskConfig["apply_extrapolation"].GetBool();

    // 2. Check if Data input file is provided
    if (!taskConfig.HasMember("input_data_file")) {
      throw std::runtime_error("[FATAL ERROR] DataTask: 'input_data_file' missing in JSON!");
    }

    std::string inputFile = taskConfig["input_data_file"].GetString();
    // 3. Open Input Files
    TFile* fileDataInput = new TFile(inputFile.c_str(), "READ");
    if (!fileDataInput || fileDataInput->IsZombie()) {
      throw std::runtime_error("[FATAL] DataTask: Cannot open Data input file: " + inputFile);
    }

    TFile* fileDataMEInput{nullptr};
    if (applyME) {
      if (!taskConfig.HasMember("input_me_file")) {
        throw std::runtime_error("[FATAL ERROR] DataTask: 'input_me_file' missing in JSON!");
      }

      std::string inputMEFile = taskConfig["input_me_file"].GetString();
      fileDataMEInput = new TFile(inputMEFile.c_str(), "READ");
      if (!fileDataMEInput || fileDataMEInput->IsZombie()) {
        throw std::runtime_error("[FATAL] DataTask: Cannot open ME Data input file: " + inputMEFile);
      }
    }

    if (!taskConfig.HasMember("base_path_data"))
      throw std::runtime_error("[FATAL ERROR] DataTask: 'base_path_data' missing in JSON!");
    basePathData = taskConfig["base_path_data"].GetString();

    if (applyME) {
      if (!taskConfig.HasMember("base_path_me"))
        throw std::runtime_error("[FATAL ERROR] DataTask: 'base_path_me' missing in JSON!");
      basePathDataME = taskConfig["base_path_me"].GetString();
    }

    // 4. Load Main Phi 3D Histogram
    h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiData").c_str()));
    if (h3PhiData) {
      h3PhiData->SetDirectory(0);
    } else {
      throw std::runtime_error("[FATAL] DataTask: Missing h3PhiData!");
    }

    // 5. Define Associated Particles and load their THnSparse
    assocParticles = {
      {"K0S", "phiK0S", globalCfgs.nBinPtK0S, 1, globalCfgs.binspTK0S, AnalysisConstants::k0sMass},
      {"Pi", "phiPi", globalCfgs.nBinPtPi, 2, globalCfgs.binspTPi, AnalysisConstants::piMass}};

    loadedDataCollection.reserve(assocParticles.size());

    for (const auto& config : assocParticles) {
      LoadedAssocData data;
      data.name = config.name;
      std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
      data.h5DataSignal = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSignal").c_str()));
      data.h5DataSideband = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSideband").c_str()));

      if (applyME) {
        std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;
        data.h5DataMESignal = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESignal").c_str()));
        data.h5DataMESideband = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESideband").c_str()));
      }

      loadedDataCollection.push_back(data);
    }

    // 6. Safely close input files (All objects are in RAM)
    fileDataInput->Close();
    delete fileDataInput;
    if (applyME) {
      fileDataMEInput->Close();
      delete fileDataMEInput;
    }

    // 7. Load Corrections (if enabled)
    if (applyEfficiency) {
      if (!taskConfig.HasMember("input_efficiency_file")) {
        throw std::runtime_error("[FATAL ERROR] DataTask: 'input_efficiency_file' missing in JSON!");
      }

      std::string inputEffFile = taskConfig["input_efficiency_file"].GetString();
      TFile* fileEffInput = new TFile(inputEffFile.c_str(), "READ");

      if (!fileEffInput || fileEffInput->IsZombie()) {
        throw std::runtime_error("[FATAL] DataTask: Efficiency requested but Corrections.root not found!");
      }

      hEventLoss = static_cast<TH1*>(fileEffInput->Get("hEventLoss"));
      if (hEventLoss) {
        hEventLoss->SetDirectory(0); // Sgancialo dal file
        std::cout << "[INFO] DataTask: Loaded Global Event Efficiency (Event Loss)." << std::endl;
      } else {
        std::cerr << "[WARNING] DataTask: 'hEventEfficiency' not found in "
                     "corrections file! Event Loss will be assumed 100%."
                  << std::endl;
      }

      std::vector<ParticleConfig<2>> particles = {
        {"Phi", {"h1PhiEfficiency", "h1PhiSigLoss"}},
        {"K0S", {"h1K0SEfficiency", "h1K0SSigLoss"}},
        {"Pi", {"h1PiEfficiency", "h1PiSigLoss"}}};

      for (const auto& p : particles) {
        LoadedCorrections corrections;
        corrections.name = p.name;

        for (int i = 0; i < globalCfgs.nBinMult; i++) {
          TH1F* h1Eff = static_cast<TH1F*>(fileEffInput->Get((p.titles[0] + "_multBin" + std::to_string(i)).c_str()));
          TH1F* h1Loss = static_cast<TH1F*>(fileEffInput->Get((p.titles[1] + "_multBin" + std::to_string(i)).c_str()));

          if (!h1Eff || !h1Loss)
            throw std::runtime_error("[FATAL] DataTask: Missing correction histograms!");

          corrections.h1Corrections[i] = static_cast<TH1F*>(h1Eff->Clone());
          // corrections.h1Corrections[i]->Multiply(h1Eff, h1Loss);
          corrections.h1Corrections[i]->SetDirectory(0);
        }
        correctionCollection.push_back(corrections);
      }
      fileEffInput->Close();
      delete fileEffInput;
    }

    // 8. Load Purities (if enabled)
    if (applyPurity) {
      if (!taskConfig.HasMember("input_dir_purity")) {
        throw std::runtime_error("[FATAL ERROR] DataTask: 'input_dir_purity' missing in JSON!");
      }
      std::string purityDir = taskConfig["input_dir_purity"].GetString();

      std::string prefix = "";
      if (taskConfig.HasMember("input_output_prefix"))
        prefix = taskConfig["input_output_prefix"].GetString();

      std::vector<std::pair<std::string, std::string>> purityConfig = {
        {"k0s", purityDir + prefix + "K0SPurity.root"},
        {"pi_tpc", purityDir + prefix + "PiPurity.root"}};

      for (const auto& p : purityConfig) {
        LoadedPurity purity;
        purity.name = p.first;

        TFile* filePurity = new TFile(p.second.c_str(), "READ");
        if (!filePurity || filePurity->IsZombie()) {
          throw std::runtime_error("[FATAL] DataTask: Cannot open purity file: " + p.second);
        }

        for (int i = 0; i < globalCfgs.nBinMult; i++) {
          std::string hName = "h1" + p.first + "Purity_multBin" + std::to_string(i);
          TH1* h1Pur = static_cast<TH1*>(filePurity->Get(hName.c_str()));

          if (!h1Pur) {
            throw std::runtime_error("[FATAL] DataTask: Missing purity histogram: " + hName);
          }

          purity.h1Purity[i] = static_cast<TH1*>(h1Pur->Clone());
          purity.h1Purity[i]->SetDirectory(0);
        }
        purityCollection.push_back(purity);

        filePurity->Close();
        delete filePurity;
      }
    }

    // 9. Load spectra-extrapolation utilities
    if (applyExtrapolation) {
      if (taskConfig.HasMember("extrap_function"))
        extrapFunction = taskConfig["extrap_function"].GetString();
      if (taskConfig.HasMember("extrap_fit_range") &&
          taskConfig["extrap_fit_range"].IsArray()) {
        const auto& rangeArray = taskConfig["extrap_fit_range"].GetArray();
        if (rangeArray.Size() == 2) {
          extrapFitRange.first = rangeArray[0].GetDouble();
          extrapFitRange.second = rangeArray[1].GetDouble();
        } else {
          std::cerr << "[WARNING] DataTask: 'extrap_fit_range' must contain "
                       "exactly 2 values. Using default limits."
                    << std::endl;
        }
      }
    }

    // 10. Initialize Data Structures for Accumulation
    h1PhiAssocNoPtPhi.resize(assocParticles.size());
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      h1PhiAssocNoPtPhi[pIdx].resize(globalCfgs.nBinMult);
      for (int i{0}; i < globalCfgs.nBinMult; ++i) {
        h1PhiAssocNoPtPhi[pIdx][i].resize(assocParticles[pIdx].nBinPt, nullptr);
      }
    }

    // 11. Prepare Output Files & Canvases
    std::string basePathProj = taskConfig["output_dir_proj"].GetString();
    std::string basePathFinal = taskConfig["output_dir_final"].GetString();

    // Prefix to specify the type of analysis (Data vs MCClosure)
    std::string prefix = "";
    if (taskConfig.HasMember("input_output_prefix"))
      prefix = taskConfig["input_output_prefix"].GetString();

    std::string phiDataName = basePathProj + prefix + "PhiDataHistograms.root";
    filePhiDataOutput = new TFile(phiDataName.c_str(), "RECREATE");

    std::string phiSpectraName = basePathFinal + prefix + "PhiAssocSpectra.root";
    fileOutputSpectra = new TFile(phiSpectraName.c_str(), "RECREATE");

    for (const auto& p : assocParticles) {
      std::string fName = basePathProj + prefix + "Phi" + p.name + "DataHistograms.root";
      filesPhiAssocDataOutput.push_back(new TFile(fName.c_str(), "RECREATE"));

      spectraCanvases.push_back(new TCanvas(("canvasSpectra" + p.name).c_str(), ("Spectra " + p.name).c_str(), 800, 600));
    }

    if (taskConfig.HasMember("delta_y_limits") && taskConfig["delta_y_limits"].IsArray()) {
      deltaYLimits.clear();
      for (const auto& v : taskConfig["delta_y_limits"].GetArray()) {
        deltaYLimits.push_back(v.GetDouble());
      }
    }

    // Matrix size: [ParticleIndex][DeltaYIndex]
    h1MultTrends.resize(assocParticles.size());
    if (applyExtrapolation) {
      h1MultTrendsExtrap.resize(assocParticles.size());
    }

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& p = assocParticles[pIdx];

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];

        // std::string dyTitleStr = std::format("{:.2f}", dyLimit);
        std::string dyTitleStr = Form("%.2f", dyLimit);
        std::string dyNameStr = dyTitleStr;
        std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

        std::string hName = std::format("h1MultTrend_{}_dy{}", assocParticles[pIdx].name, dyNameStr);
        std::string hTitle = std::format("Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);Raw Yield",
                                         assocParticles[pIdx].name, dyTitleStr);

        TH1* hTrend = new TH1F(hName.c_str(), hTitle.c_str(), globalCfgs.nBinMult, globalCfgs.binsMult.data());
        hTrend->SetDirectory(0);
        h1MultTrends[pIdx].push_back(hTrend);

        if (applyExtrapolation) {
          std::string hExtrapName = std::format("h1MultTrendExtrap_{}_dy{}", assocParticles[pIdx].name, dyNameStr);
          // std::string hExtrapTitle = std::format("Extrapolated Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);Extrapolated Yield", assocParticles[pIdx].name, dyTitleStr);
          std::string hExtrapTitle = Form("Extrapolated Yield Trend %s |#Delta y| < %s;Multiplicity Percentile (%%);Extrapolated Yield", assocParticles[pIdx].name.c_str(), dyTitleStr.c_str());

          TH1* hTrendExtrap = new TH1F(hExtrapName.c_str(), hExtrapTitle.c_str(), globalCfgs.nBinMult, globalCfgs.binsMult.data());
          hTrendExtrap->SetDirectory(0);
          h1MultTrendsExtrap[pIdx].push_back(hTrendExtrap);
        }
      }
    }

    // 12. Load the Fit Configuration Manager
    if (!taskConfig.HasMember("fit_config_file")) {
      throw std::runtime_error("[FATAL ERROR] DataTask: 'fit_config_file' missing in JSON!");
    }
    std::string fitCfgPath = taskConfig["fit_config_file"].GetString();
    fitConfigManager = new FitConfigManager(fitCfgPath);

    std::cout << "[INFO] DataTask: Initialization complete." << std::endl;
  }

  void Run() override
  {
    std::cout << "[INFO] DataTask: RUNNING SIGNAL EXTRACTION..." << std::endl;

    // Initialize the calculator once at the beginning of the Run
    CorrelationCalculator corrCalculator(applyME, false, false, false);

    for (int i = 0; i < globalCfgs.nBinMult; i++) {
      AnalysisUtils::AxisToCut axisToCutMult{0, i + 1, i + 1};
      double totalTriggerSignalPerMult = 0.0;

      for (int j = 0; j < globalCfgs.nBinPtPhi; j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

        // --- 1. Fit the Phi Trigger ---
        std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
        TH1* h1PhiData = static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1));
        h1PhiData->SetDirectory(0);

        /*TF1 *fitVoigtBkgSourav = new TF1("fitVoigtBkgSourav", VoigtBkgSourav, 0.995, 1.06, 7);
        fitVoigtBkgSourav->SetParameter(1, 1.019);
        fitVoigtBkgSourav->SetParameter(2, 0.001);
        fitVoigtBkgSourav->FixParameter(3, 0.00426);
        fitVoigtBkgSourav->SetNpx(400);
        fitVoigtBkgSourav->SetLineColor(kRed);

        FitPhiSignalAndBkg<false> fitPhiSignalAndBkg{h1PhiData, fitVoigtBkgSourav, 4,
                                                     AnalysisConstants::phiMassSignalRange,
                                                     AnalysisConstants::phiMassSidebandRange};
        double triggerSignal = fitPhiSignalAndBkg.GetSignal();
        double triggerBkgRatio = fitPhiSignalAndBkg.GetBkgInSigRegion() / fitPhiSignalAndBkg.GetBkgInSideRegion();*/

        // Fetch configuration dynamically from JSON
        FitConfig cfg = fitConfigManager->GetConfig("phi", i, j);

        // Initialize RooFit Engine
        DynamicRooFitter fitter(h1PhiData, cfg);

        fitter.DoFit();

        // Extract Physics Observables
        auto res = fitter.ExtractYieldsAndPurity();
        double triggerSignal = res.signal.first;
        double bkgInSigRegion = res.background.first;
        double bkgInSidebandRegion = res.bkgInSideband.first;
        double triggerBkgRatio = bkgInSigRegion / bkgInSidebandRegion;

        // Log the results (optional, but good for debugging)
        std::cout << "Signal Integral: " << triggerSignal << " +/- " << res.signal.second << std::endl;
        std::cout << "Bkg Integral in Signal Region: " << bkgInSigRegion << " +/- " << res.background.second << std::endl;
        std::cout << "Bkg Integral in Sideband Region: " << bkgInSidebandRegion << " +/- " << res.bkgInSideband.second << std::endl;

        // Calculate total trigger signal adjusted for efficiency
        double phiEff = applyEfficiency ? correctionCollection[0].h1Corrections[i]->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        // Save the Canvas with the RooFit Drawing
        std::string canvasName = "cFit_Phi_mult" + std::to_string(i) + "_pt" + std::to_string(j);
        fitter.SaveFitCanvas(filePhiDataOutput, canvasName);

        filePhiDataOutput->cd();

        // h1PhiData->Write();
        // delete fitVoigtBkgSourav;

        delete h1PhiData;

        // --- 2. Process Associated Particles (Sideband Subtraction) ---
        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];

          for (int k = 0; k < config.nBinPt; k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};

            // Prepare inputs for the calculator
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};
            std::string histNameBase = "h1Phi" + config.name + "Data";
            std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

            double assocEff = applyEfficiency ? correctionCollection[config.effIndex].h1Corrections[i]->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            // Call the calculator: project, scale, apply ME, subtract, and save intermediate files
            TH1* h1FinalSignal = corrCalculator.ExtractCorrectedSignal(data, axesToCut, totalEff, triggerBkgRatio,
                                                                       histNameBase + suffix, filesPhiAssocDataOutput[pIdx], nullptr);

            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_pt" + config.name + "Bin" + std::to_string(k);
              h1PhiAssocNoPtPhi[pIdx][i][k] = static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str()));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal);
            }

            delete h1FinalSignal; // Destroy the temporary histogram returned
                                  // by the calculator
          }
        }
      } // End of Phi Pt loop

      // --- 3. Construct Final Spectra & Trends ---
      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        const auto& config = assocParticles[pIdx];
        const auto& purity = purityCollection[pIdx];

        // Lambda function to extrapolate spectra
        auto extrapolateSpectrum = [&](TH1* hSpec, double& extY, double& extE) {
          double maxVal = hSpec->GetMaximum();
          double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");

          std::unique_ptr<TF1> extrapModel;

          // Dynamically select the model based on JSON

          if (extrapFunction == "LevyTsallis") {
            // extrapModel = ExtrapolationModelFactory::CreateLevyTsallis(config.mass, 5.0, 0.7, maxVal);
            extrapModel = ExtrapolationModelFactory::CreateLevyTsallis(config.mass, 5.0, 0.7, rawIntegral);
          } else if (extrapFunction == "BlastWave") {
            // extrapModel = ExtrapolationModelFactory::CreateBlastWave(config.mass, 0.7, 0.4, 25.0, maxVal);
            extrapModel = ExtrapolationModelFactory::CreateBlastWave(config.mass, 0.7, 0.4, 25.0, rawIntegral);
          } else if (extrapFunction == "BoseEinstein") {
            // extrapModel = ExtrapolationModelFactory::CreateBoseEinstein(config.mass, 0.7, maxVal);
            extrapModel = ExtrapolationModelFactory::CreateBoseEinstein(config.mass, 0.7, rawIntegral);
          } else if (extrapFunction == "MtExponential") {
            // extrapModel = ExtrapolationModelFactory::CreateMtExponential(config.mass, 0.7, maxVal);
            extrapModel = ExtrapolationModelFactory::CreateMtExponential(config.mass, 0.7, rawIntegral);
          } else if (extrapFunction == "PtExponential") {
            // extrapModel = ExtrapolationModelFactory::CreatePtExponential(0.7, maxVal);
            extrapModel = ExtrapolationModelFactory::CreatePtExponential(0.7, rawIntegral);
          } else {
            throw std::runtime_error("[FATAL] DataTask: Unknown extrap_function: " + extrapFunction);
          }

          SpectrumExtrapolator extrapolator(hSpec, extrapModel.get());
          // Temporary solution
          if (pIdx == 0) {
            extrapFitRange = {0.1, 6.0};
          } else if (pIdx == 1) {
            extrapFitRange = {0.2, 1.0};
          }
          // // // // // // //
          extrapolator.SetFitRange(extrapFitRange.first, extrapFitRange.second);

          auto res = extrapolator.CalculateYieldAndMean();
          extY = res.yield;
          extE = res.yieldStatErr;

          {
            // --- DEBUG ---
            double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");

            double firstDataPt = config.binning[0];
            double fitLowPtIntegral = 0.0;
            if (firstDataPt > 0.0) {
              fitLowPtIntegral = extrapModel->Integral(0.0, firstDataPt);
            }

            std::cout << "\n[DEBUG EXTRAP] " << std::endl;
            std::cout << "  -> Raw Data Integral (ROOT): " << rawIntegral << std::endl;
            std::cout << "  -> Total Extrapolated (extY): " << extY << std::endl;
            std::cout << "  -> Fit Integral [0.0 - " << firstDataPt << "]: " << fitLowPtIntegral << std::endl;
            // ---------------------
          }

          // ==========================================
          // --- EXTENDED HISTOGRAM CREATION ---
          // ==========================================

          // 1. Create extended binning dynamically (add 0.0 at the beginning)
          std::vector<double> extBinning;
          extBinning.push_back(0.0); // Lower limit for extrapolation
          for (double edge : config.binning) {
            extBinning.push_back(edge);
          }

          // 2. Create the new extended histogram
          std::string extName = std::string(hSpec->GetName()) + "_extended";
          // std::string extTitle = specName + " Extrapolated;#it{p}_{T}
          // (GeV/#it{c});1/#it{N}_{ev} d^{2}#it{N}/d#it{y}d#it{p}_{T}";

          TH1D* hSpecExt = new TH1D(extName.c_str(), extName.c_str(), extBinning.size() - 1, extBinning.data());
          hSpecExt->SetDirectory(0);

          // 3. Copy measured contents (shifted by 1 bin to the right)
          for (int b = 1; b <= hSpec->GetNbinsX(); ++b) {
            hSpecExt->SetBinContent(b + 1, hSpec->GetBinContent(b));
            hSpecExt->SetBinError(b + 1, hSpec->GetBinError(b));
          }

          // 4. Leave the first bin empty (visual placeholder for the fit curve)
          hSpecExt->SetBinContent(1, 0.0);
          hSpecExt->SetBinError(1, 0.0);

          AnalysisUtils::SetHistogramStyle(hSpecExt, globalCfgs.GetSpectraColor(pIdx));

          hSpecExt->GetListOfFunctions()->Add(extrapModel->Clone());

          fileOutputSpectra->cd();
          hSpecExt->Write();
          delete hSpecExt;
        };

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          double dyLimit = deltaYLimits[yIdx];

          // std::string dyTitleStr = std::format("{:.2f}", dyLimit);
          std::string dyTitleStr = Form("%.2f", dyLimit);
          std::string dyNameStr = dyTitleStr;
          std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

          std::string spectraName = std::format("h1SpectrumPhi{}_dy{}_multBin{}", config.name, dyNameStr, i);

          // Construct the 1D spectrum from the accumulated Pt bins
          TH1* h1Spectrum = AnalysisUtils::constructSpectrum(h1PhiAssocNoPtPhi[pIdx][i], config.binning, spectraName, dyLimit);

          // Normalize by the total number of triggers AND the Delta Y phase space
          if (totalTriggerSignalPerMult > 0)
            h1Spectrum->Scale(1.0 / (totalTriggerSignalPerMult * 2.0 * dyLimit));

          // Apply Purity if enabled
          if (applyPurity)
            h1Spectrum->Multiply(purityCollection[pIdx].h1Purity[i]);

          AnalysisUtils::SetHistogramStyle(h1Spectrum, globalCfgs.GetSpectraColor(pIdx));

          // Draw and Write (Only draw the nominal Delta Y limit on the summary canvas)
          if (yIdx == 0) {
            spectraCanvases[pIdx]->cd();
            h1Spectrum->DrawCopy(i == 0 ? "" : "SAME");
          }

          fileOutputSpectra->cd();
          h1Spectrum->Write();

          AnalysisUtils::constructMultTrend(h1MultTrends[pIdx][yIdx], h1Spectrum, i, false);

          // Extrapolate and Fill Trend
          if (applyExtrapolation) {
            double extY = 0.0, extE = 0.0;
            extrapolateSpectrum(h1Spectrum, extY, extE);
            AnalysisUtils::constructMultTrend(h1MultTrendsExtrap[pIdx][yIdx], h1Spectrum, i, true, extY, extE);
          }

          delete h1Spectrum;
        }
      }
    } // End of Multiplicity loop
  }

  void Terminate() override
  {
    std::cout << "[INFO] DataTask: TERMINATING AND CLEANING UP..." << std::endl;

    // 1. Write the Ratios
    fileOutputSpectra->cd();

    TCanvas* canvasRatioMultTrend = new TCanvas("canvasRatioMultTrend", "Ratio Mult Trend", 800, 600);
    canvasRatioMultTrend->cd();

    for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
      double dyLimit = deltaYLimits[yIdx];

      // std::string dyTitleStr = std::format("{:.2f}", dyLimit);
      std::string dyTitleStr = Form("%.2f", dyLimit);
      std::string dyNameStr = dyTitleStr;
      std::replace(dyNameStr.begin(), dyNameStr.end(), '.', '_');

      std::string ratioName = std::format("Ratio_K0S_Pi_dy{}", dyNameStr);
      std::string ratioTitle = std::format("Ratio K0S / Pi |#Delta y| < {}", dyTitleStr);

      // Assuming 0 is K0S and 1 is Pi based on assocParticles order
      TH1* hRatio = static_cast<TH1*>(h1MultTrends[0][yIdx]->Clone(ratioName.c_str()));
      hRatio->SetTitle(ratioTitle.c_str());
      hRatio->SetDirectory(0);

      // Note: 2.0 factor scales K0S
      hRatio->Divide(h1MultTrends[0][yIdx], h1MultTrends[1][yIdx], 2.0, 1.0);

      AnalysisUtils::SetHistogramStyle(hRatio, globalCfgs.GetMultTrendColor(yIdx));
      hRatio->DrawCopy(yIdx == 0 ? "" : "SAME");

      // hRatio->Write();
      delete hRatio;
    }

    canvasRatioMultTrend->Write();

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      std::string cNameMeas = std::format("cTrend_Meas_{}", assocParticles[pIdx].name);
      TCanvas* cTrendMeas = new TCanvas(cNameMeas.c_str(), "Measured Yield Trend", 800, 600);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        h1MultTrends[pIdx][yIdx]->Write();

        // Lo disegna nel Canvas
        cTrendMeas->cd();
        AnalysisUtils::SetHistogramStyle(h1MultTrends[pIdx][yIdx], globalCfgs.GetMultTrendColor(yIdx));
        h1MultTrends[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
      }
      cTrendMeas->Write();
      delete cTrendMeas;

      if (applyExtrapolation) {
        std::string cNameExtrap = std::format("cTrend_Extrap_{}", assocParticles[pIdx].name);
        TCanvas* cTrendExtrap = new TCanvas(cNameExtrap.c_str(), "Extrapolated Yield Trend", 800, 600);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          h1MultTrendsExtrap[pIdx][yIdx]->Write();

          cTrendExtrap->cd();
          AnalysisUtils::SetHistogramStyle(h1MultTrendsExtrap[pIdx][yIdx], globalCfgs.GetMultTrendColor(yIdx));
          h1MultTrendsExtrap[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
        }
        cTrendExtrap->Write();
        delete cTrendExtrap;
      }
    }

    if (applyExtrapolation) {
      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        std::string canvasName = std::format("cRatio_ExtrapVsMeas_{}", assocParticles[pIdx].name);
        TCanvas* cRatioExtrapMeas = new TCanvas(canvasName.c_str(), "Extrap / Measured Ratio", 800, 600);
        cRatioExtrapMeas->cd();

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          double dyLimit = deltaYLimits[yIdx];
          // std::string dyStr = std::format("{:.2f}", dyLimit);
          std::string dyStr = Form("%.2f", dyLimit);
          std::replace(dyStr.begin(), dyStr.end(), '.', '_');

          std::string ratioName = std::format("Ratio_Extrap_Meas_{}_dy{}", assocParticles[pIdx].name, dyStr);
          // std::string title = std::format("Extrap/Measured Contribution {} |#Delta y| < {:.2f}", assocParticles[pIdx].name, dyLimit);
          std::string title = Form("Extrap/Measured Contribution %s |#Delta y| < %.2f", assocParticles[pIdx].name.c_str(), dyLimit);

          // Clone the extrapolated trend to safely divide it
          TH1* hRatioExtrapMeas = static_cast<TH1*>(h1MultTrendsExtrap[pIdx][yIdx]->Clone(ratioName.c_str()));
          hRatioExtrapMeas->SetTitle(title.c_str());
          hRatioExtrapMeas->SetDirectory(0);

          // Perform division: Yield_{Extrap} / Yield_{Measured}
          hRatioExtrapMeas->Divide(h1MultTrendsExtrap[pIdx][yIdx], h1MultTrends[pIdx][yIdx], 1.0, 1.0);

          // Set Y-axis title for clarity
          hRatioExtrapMeas->GetYaxis()->SetTitle("Yield_{Extrap} / Yield_{Meas}");

          AnalysisUtils::SetHistogramStyle(hRatioExtrapMeas, globalCfgs.GetMultTrendColor(yIdx));
          hRatioExtrapMeas->DrawCopy(yIdx == 0 ? "" : "SAME");

          delete hRatioExtrapMeas;
        }

        cRatioExtrapMeas->Write();
        delete cRatioExtrapMeas;
      }
    }

    // 2. Write Canvases
    for (const auto& canvas : spectraCanvases) {
      canvas->Write();
      delete canvas;
    }

    // 3. Close Files
    filePhiDataOutput->Close();
    delete filePhiDataOutput;
    fileOutputSpectra->Close();
    delete fileOutputSpectra;

    for (auto& file : filesPhiAssocDataOutput) {
      file->Close();
      delete file;
    }

    // 4. Memory Cleanup for RAM-resident objects
    delete h3PhiData;
    delete canvasRatioMultTrend;

    for (auto& particleTrends : h1MultTrends) {
      for (auto* hTrend : particleTrends) {
        if (hTrend)
          delete hTrend;
      }
    }

    if (applyExtrapolation) {
      for (auto& particleTrends : h1MultTrendsExtrap) {
        for (auto* hTrend : particleTrends) {
          if (hTrend)
            delete hTrend;
        }
      }
    }

    for (auto& pIdxArr : h1PhiAssocNoPtPhi) {
      for (auto& mIdxArr : pIdxArr) {
        for (auto& h1 : mIdxArr) {
          if (h1)
            delete h1;
        }
      }
    }

    for (auto& corr : correctionCollection) {
      for (auto& h1 : corr.h1Corrections) {
        if (h1)
          delete h1;
      }
    }

    for (auto& pur : purityCollection) {
      for (auto& h1 : pur.h1Purity) {
        if (h1)
          delete h1;
      }
    }

    for (auto& data : loadedDataCollection) {
      if (data.h5DataSignal)
        delete data.h5DataSignal;
      if (data.h5DataSideband)
        delete data.h5DataSideband;
      if (data.h5DataMESignal)
        delete data.h5DataMESignal;
      if (data.h5DataMESideband)
        delete data.h5DataMESideband;
    }

    delete fitConfigManager;

    std::cout << "[INFO] DataTask: DONE." << std::endl;
  }

 private:
  AnalysisSettings globalCfgs;

  // --- Configuration & Flags ---
  std::string basePathData{
    "phi-strangeness-correlation/phiStrangenessCorrelation/"};
  std::string basePathDataME{
    "phi-strangeness-correlation/phiStrangenessCorrelation/"};

  bool applyME{false};
  bool applyEfficiency{false};
  bool applyPurity{false};
  bool applyExtrapolation{false};

  // --- Data Storage in RAM ---
  TH3F* h3PhiData{nullptr};
  std::vector<AssocParticleConfig> assocParticles;
  std::vector<LoadedAssocData> loadedDataCollection;
  std::vector<LoadedCorrections> correctionCollection;
  std::vector<LoadedPurity> purityCollection;
  TH1* hEventLoss{nullptr};

  // Accumulators for the final spectra: [ParticleIndex][MultBin][PtAssocBin]
  std::vector<std::vector<std::vector<TH1*>>> h1PhiAssocNoPtPhi;

  // DeltaY limit and corresponding multiplicity trend histograms
  std::vector<double> deltaYLimits{1.0, 0.5, 0.1};
  std::vector<std::vector<TH1*>> h1MultTrends;
  std::vector<std::vector<TH1*>> h1MultTrendsExtrap;

  // Extrapolation members
  std::string extrapFunction{"LevyTsallis"};
  std::pair<double, double> extrapFitRange{0.4, 6.0};

  // --- Output Files & Canvases ---
  TFile* filePhiDataOutput{nullptr};
  TFile* fileOutputSpectra{nullptr};
  std::vector<TFile*> filesPhiAssocDataOutput;
  std::vector<TCanvas*> spectraCanvases;

  // FitConfigManager to load fit parameters from JSON
  FitConfigManager* fitConfigManager{nullptr};
};

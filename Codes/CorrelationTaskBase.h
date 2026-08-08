#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "CorrelationCalculator.h"
#include "ExtrapConfigManager.h"
#include "ExtrapolationModelFactory.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"
#include "SpectrumExtrapolator.h"

#include "TCanvas.h"
#include "TDirectory.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH1F.h"
#include "TH2D.h"
#include "TH3F.h"
#include "THnSparse.h"
#include "TLegend.h"
#include "TString.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Shared logic between CorrelationTask and CorrelationWPDGTask.
// Init() and Run() stay pure virtual: the data source and correlation
// extraction strategy differ enough between derived tasks that forcing
// a shared implementation would just mean flags to switch behavior off.
// Everything else (corrections loading, trend bookkeeping, spectra/extrap
// generation, and Terminate) is identical or near-identical and lives here.
class CorrelationTaskBase : public IAnalysisTask
{
 public:
  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override = 0;
  void Run() override = 0;

  void Terminate() override
  {
    std::cout << "[INFO] " << GetName() << ": TERMINATING AND CLEANING UP..." << std::endl;

    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    std::vector<std::string> baseLogicalPath = {globalCfgs.binningName, dirName};

    // =========================================================================
    // 1. Write Yield Trends, Ratios Extrap/Meas, and Canvases PER SPECIES
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const std::string& pName = assocParticles[pIdx].name;

      // Create a dedicated subdirectory for each particle (e.g., Extract1D/K0S)
      std::vector<std::string> particlePath = baseLogicalPath;
      particlePath.push_back(pName);
      TDirectory* particleDir = RootIO::GetOrCreatePath(fileOutputSpectra.get(), particlePath);

      if (!particleDir)
        continue;
      particleDir->cd();

      // --- Save measured trends ---
      std::string cNameMeas = std::format("cTrend_Meas_{}", pName);
      std::unique_ptr<TCanvas> cTrendMeas = std::make_unique<TCanvas>(cNameMeas.c_str(), "Measured Yield Trend", 800, 600);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        particleDir->cd();
        h1MultTrends[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

        cTrendMeas->cd();
        AnalysisUtils::SetHistogramStyle(h1MultTrends[pIdx][yIdx].get(), globalCfgs.GetMultTrendColor(yIdx));
        h1MultTrends[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
      }
      particleDir->cd();
      cTrendMeas->Write(nullptr, TObject::kOverwrite);

      // --- Save extrapolated trends ---
      bool hasExtrapTrend = !h1MultTrendsExtrap.empty() && !h1MultTrendsExtrap[pIdx].empty();
      if (hasExtrapTrend) {
        std::string cNameExtrap = std::format("cTrend_Extrap_{}", pName);
        std::unique_ptr<TCanvas> cTrendExtrap = std::make_unique<TCanvas>(cNameExtrap.c_str(), "Extrapolated Yield Trend", 800, 600);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          particleDir->cd();
          h1MultTrendsExtrap[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

          cTrendExtrap->cd();
          AnalysisUtils::SetHistogramStyle(h1MultTrendsExtrap[pIdx][yIdx].get(), globalCfgs.GetMultTrendColor(yIdx));
          h1MultTrendsExtrap[pIdx][yIdx]->DrawCopy(yIdx == 0 ? "" : "SAME");
        }
        particleDir->cd();
        cTrendExtrap->Write(nullptr, TObject::kOverwrite);
      }

      // --- Save Extrapolated/Measured ratios for this species ---
      if (hasExtrapTrend) {
        std::string canvasName = std::format("cRatio_ExtrapVsMeas_{}", pName);
        std::unique_ptr<TCanvas> cRatioExtrapMeas = std::make_unique<TCanvas>(canvasName.c_str(), "Extrap / Measured Ratio", 800, 600);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          double dyLimit = deltaYLimits[yIdx];
          auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

          std::string ratioName = std::format("Ratio_Extrap_Meas_{}_dy{}", pName, dyNameStr);
          std::string title = Form("Extrap/Measured Contribution %s |#Delta y| < %.2f; Multiplicity percentile (%%); Yield_{Extrap} / Yield_{Meas}", pName.c_str(), dyLimit);

          std::unique_ptr<TH1> hRatioExtrapMeas = AnalysisUtils::MakeRatioHist(h1MultTrendsExtrap[pIdx][yIdx].get(), h1MultTrends[pIdx][yIdx].get(),
                                                                               ratioName, title, 1.0, 1.0);

          cRatioExtrapMeas->cd();
          AnalysisUtils::SetHistogramStyle(hRatioExtrapMeas.get(), globalCfgs.GetMultTrendColor(yIdx));
          hRatioExtrapMeas->DrawCopy(yIdx == 0 ? "" : "SAME");
        }

        particleDir->cd();
        cRatioExtrapMeas->Write(nullptr, TObject::kOverwrite);
      }

      // --- Save accumulated spectra Canvases for this species ---
      for (auto& canvas : spectraCanvases[pIdx]) {
        if (canvas) {
          particleDir->cd();
          canvas->Write(nullptr, TObject::kOverwrite);
        }
      }
    }

    // The spectra leave this task and, once the trends and ratios move to a task
    // of their own, this is what will say which method produced them.
    RootIO::WriteProvenance(RootIO::GetOrCreatePath(fileOutputSpectra.get(), {globalCfgs.binningName, "Provenance"}, false), provenance);

    // =========================================================================
    // 2. Write Yield Ratios across species (into a dedicated "Ratios" directory)
    // =========================================================================
    if (!requestedRatios.empty()) {
      // Create or access the "Ratios" directory inside Extract1D/2D
      std::vector<std::string> ratioPath = baseLogicalPath;
      ratioPath.push_back("Ratios");
      TDirectory* ratioDir = RootIO::GetOrCreatePath(fileOutputSpectra.get(), ratioPath);

      if (ratioDir) {
        ratioDir->cd();

        // Helper lambda function to find particle index by name
        auto getParticleIndex = [&](const std::string& name) -> int {
          for (size_t i = 0; i < assocParticles.size(); ++i) {
            if (assocParticles[i].name == name)
              return i;
          }
          return -1;
        };

        for (const auto& ratioCfg : requestedRatios) {
          int idxNum = getParticleIndex(ratioCfg.num);
          int idxDen = getParticleIndex(ratioCfg.den);

          if (idxNum == -1 || idxDen == -1) {
            std::cerr << "[WARNING] Cannot compute ratio " << ratioCfg.num << "/" << ratioCfg.den
                      << " because one or both particles are not configured." << std::endl;
            continue; // Skip this specific ratio and move to the next
          }

          const auto& num = assocParticles[idxNum];
          const auto& den = assocParticles[idxDen];

          double numScale = GetYieldScaleFactor(num.name);
          double denScale = GetYieldScaleFactor(den.name);

          bool numHasExtrap = doExtrapolationPerParticle.contains(num.name) && doExtrapolationPerParticle.at(num.name);
          bool denHasExtrap = doExtrapolationPerParticle.contains(den.name) && doExtrapolationPerParticle.at(den.name);
          bool doExtrapRatio = applyExtrapolation && (numHasExtrap || denHasExtrap);

          std::string canvasName = std::format("canvasRatio_{}_{}_MultTrend", num.name, den.name);
          std::unique_ptr<TCanvas> canvasRatio = std::make_unique<TCanvas>(canvasName.c_str(), ("Ratio Mult Trend " + ratioCfg.label).c_str(), 800, 600);
          canvasRatio->cd();

          TLegend* legend = new TLegend(0.7, 0.7, 0.9, 0.9);
          legend->SetBit(kCanDelete);
          legend->SetNColumns(2);
          legend->SetLineWidth(0);

          for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
            double dyLimit = deltaYLimits[yIdx];
            auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

            std::string ratioMeasName = std::format("Ratio_{}_{}_Meas_dy{}", num.name, den.name, dyNameStr);
            std::string ratioMeasTitle = "Measured Ratio; Multiplicity percentile (%);" + ratioCfg.label;
            std::unique_ptr<TH1> hRatioMeas = AnalysisUtils::MakeRatioHist(h1MultTrends[idxNum][yIdx].get(), h1MultTrends[idxDen][yIdx].get(),
                                                                           ratioMeasName, ratioMeasTitle, numScale, denScale);

            AnalysisUtils::SetHistogramStyle(hRatioMeas.get(), globalCfgs.GetMultTrendColor(yIdx));
            hRatioMeas->SetMarkerStyle(24);

            std::unique_ptr<TH1> hRatioExtrap{nullptr};
            if (doExtrapRatio) {
              TH1* hNumTrend = numHasExtrap ? h1MultTrendsExtrap[idxNum][yIdx].get() : h1MultTrends[idxNum][yIdx].get();
              TH1* hDenTrend = denHasExtrap ? h1MultTrendsExtrap[idxDen][yIdx].get() : h1MultTrends[idxDen][yIdx].get();

              std::string ratioExtrapName = std::format("Ratio_{}_{}_Extrap_dy{}", num.name, den.name, dyNameStr);
              std::string ratioExtrapTitle = "Extrapolated Ratio; Multiplicity percentile (%);" + ratioCfg.label;
              hRatioExtrap = AnalysisUtils::MakeRatioHist(hNumTrend, hDenTrend, ratioExtrapName, ratioExtrapTitle, numScale, denScale);

              AnalysisUtils::SetHistogramStyle(hRatioExtrap.get(), globalCfgs.GetMultTrendColor(yIdx));
              hRatioExtrap->SetMarkerStyle(20);
            }

            double globalMax = hRatioMeas->GetMaximum();
            double globalMin = hRatioMeas->GetMinimum();
            if (hRatioExtrap) {
              globalMax = std::max(globalMax, hRatioExtrap->GetMaximum());
              globalMin = std::min(globalMin, hRatioExtrap->GetMinimum());
            }
            hRatioMeas->GetYaxis()->SetRangeUser(globalMin * 0.9, globalMax * 1.2);

            // if (hRatioExtrap)

            TH1* cloneMeas = hRatioMeas->DrawCopy(yIdx == 0 ? "" : "SAME");
            legend->AddEntry(cloneMeas, std::format("Meas. |#Delta y| < {}", dyTitleStr).c_str(), "p");
            if (hRatioExtrap) {
              TH1* cloneExtrap = hRatioExtrap->DrawCopy("SAME");
              legend->AddEntry(cloneExtrap, std::format("Extrap. |#Delta y| < {}", dyTitleStr).c_str(), "p");
            }
          }

          legend->Draw("SAME");
          ratioDir->cd();
          canvasRatio->Write(nullptr, TObject::kOverwrite);
        }
      }
    } else {
      std::cout << "[INFO] " << GetName() << ": No yield ratios requested. Skipping." << std::endl;
    }

    // =========================================================================
    // 3. Close Files
    // =========================================================================
    if (fileOutputSpectra) {
      fileOutputSpectra->Close();
    }

    for (auto& file : filesPhiAssocDataOutput) {
      file->Close();
    }

    for (auto& file : filesPhiAssocQAOutput) {
      file->Close();
    }

    std::cout << "[INFO] " << GetName() << ": DONE." << std::endl;
  }

 protected:
  AnalysisSettings globalCfgs;

  std::string basePathData, basePathDataME;

  bool applyME{false}, applyEfficiency{false}, applyExtrapolation{false};
  bool useIntegratedEfficiency{false}, useProjectionCache{false}, use2DMENormalization{false};

  std::unique_ptr<TH1> hEventLoss;

  std::vector<AssocParticleConfig> assocParticles;

  // Filled in Init, written into the spectra file in Terminate
  std::map<std::string, std::string> provenance;
  std::vector<LoadedAssocData> loadedDataCollection;

  // Read from the input files in Init(), never from the configuration
  std::vector<double> multBinning;
  std::vector<double> ptPhiBinning;

  std::map<std::string, LoadedCorrections> correctionCollection;

  CorrelationCalculator::AxisTarget projectionAxis{CorrelationCalculator::AxisTarget::DeltaY_Y};

  std::map<std::string, bool> doExtrapolationPerParticle;

  std::vector<std::vector<std::vector<std::unique_ptr<TH1>>>> h1PhiAssocNoPtPhi;

  std::vector<double> deltaYLimits{1.0, 0.5, 0.1};

  std::vector<std::vector<std::unique_ptr<TH1>>> h1MultTrends;
  std::vector<std::vector<std::unique_ptr<TH1>>> h1MultTrendsExtrap;

  std::unique_ptr<ExtrapConfigManager> extrapConfigManager;

  std::unique_ptr<TFile> fileOutputSpectra;
  std::vector<std::unique_ptr<TFile>> filesPhiAssocDataOutput;
  std::vector<std::unique_ptr<TFile>> filesPhiAssocQAOutput;

  std::vector<std::vector<std::unique_ptr<TCanvas>>> spectraCanvases;

  std::vector<YieldRatioConfig> requestedRatios;

  // -------------------------------------------------------------------------
  // Hooks for derived-class-specific behavior in Terminate()
  // -------------------------------------------------------------------------
  // Hook for GenerateSpectraAndTrends: returns a purity correction histogram
  // for this particle/multBin, or nullptr if none should be applied.
  // CorrelationTask overrides this; CorrelationWPDGTask leaves the default.
  virtual TH1* GetPurityHist(const std::string& /*particleName*/, int /*multBin*/) { return nullptr; }

  // Trigger yield / background ratio for a given (multBin, ptPhiBin), used by
  // the shared RunLegacy(). Every derived task MUST provide GetTriggerSignal;
  // GetTriggerBkgRatio defaults to "no background subtraction" (WPDG's case).
  virtual double GetTriggerSignal(int multBin, int ptPhiBin) = 0;
  virtual double GetTriggerBkgRatio(int /*multBin*/, int /*ptPhiBin*/) { return 0.0; }

  // The (multiplicity, trigger pT) axes of whatever container GetTriggerSignal
  // reads from. Asked for as axes and not as a histogram because the two tasks
  // hold different things: a TH2 read from PhiFitTask in one case, a TH3
  // projected in place in the other.
  virtual std::pair<const TAxis*, const TAxis*> TriggerAxes() const = 0;

  // -------------------------------------------------------------------------
  // Shared JSON parsing for flags common to both derived Init() implementations.
  // Call this first thing from each derived Init().
  // -------------------------------------------------------------------------
  void InitCommonFlags(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings)
  {
    globalCfgs = globalSettings;

    applyME = JsonConfig::RequireBool(taskConfig, "apply_mixed_events", GetName());
    applyEfficiency = JsonConfig::RequireBool(taskConfig, "apply_efficiency", GetName());
    applyExtrapolation = JsonConfig::RequireBool(taskConfig, "apply_extrapolation", GetName());
    useIntegratedEfficiency = JsonConfig::RequireBool(taskConfig, "use_integrated_efficiency", GetName());
    // Documentation, not a check: see RootIO::WriteProvenance. The whole merged
    // block, so a key added to the JSON tomorrow is recorded without touching this.
    provenance["produced_at"] = RunEnvironment::TimestampNow();
    provenance["config_block"] = JsonConfig::Serialize(taskConfig);
    // The facts about the run itself. std::map has no range insert before C++23,
    // hence the iterator pair - but the reference is taken once: Facts() returns
    // the same static object every time, so naming it says so.
    const auto& runEnv = RunEnvironment::Facts();
    provenance.insert(runEnv.begin(), runEnv.end());

    useProjectionCache = JsonConfig::RequireBool(taskConfig, "use_projection_cache", GetName());
    use2DMENormalization = JsonConfig::RequireBool(taskConfig, "use_2d_me_normalization", GetName());

    // Both spellings of each axis are entries in the same table, so the accepted
    // values and the error message can never drift apart.
    using AxisTarget = CorrelationCalculator::AxisTarget;
    projectionAxis = JsonConfig::OptionalEnum<AxisTarget>(taskConfig, "projection_axis", "delta_y",
                                                          {{"delta_phi", AxisTarget::DeltaPhi_X},
                                                           {"delta_y", AxisTarget::DeltaY_Y}},
                                                          GetName());
    std::cout << "[INFO] " << GetName() << ": Physics projection axis set to "
              << (projectionAxis == AxisTarget::DeltaPhi_X ? "Delta Phi (X)" : "Delta Y (Y)") << "." << std::endl;

    if (auto yieldRatios = JsonConfig::TryArray(taskConfig, "yield_ratios", GetName())) {
      int idx = 0;
      for (const auto& r : *yieldRatios) {
        // The index is in the context because the entries have no name of their
        // own: without it a missing key only says "somewhere in yield_ratios".
        const std::string ctx = std::format("{} yield_ratios[{}]", GetName(), idx++);

        YieldRatioConfig ratio;
        ratio.num = JsonConfig::RequireString(r, "numerator", ctx);
        ratio.den = JsonConfig::RequireString(r, "denominator", ctx);
        ratio.label = JsonConfig::OptionalString(r, "label", ratio.num + "/" + ratio.den, ctx);
        requestedRatios.push_back(ratio);
      }
    }

    if (auto dyLimits = JsonConfig::TryArray(taskConfig, "delta_y_limits", GetName())) {
      deltaYLimits = JsonConfig::ReadNumberArray(*dyLimits, "delta_y_limits", GetName());
    }
  }

  // -------------------------------------------------------------------------
  // Shared parsing of "associated_particles". Requires globalCfgs already set.
  // -------------------------------------------------------------------------
  void InitAssocParticles(const rapidjson::Value& taskConfig)
  {
    assocParticles.clear();

    int idx = 0;
    for (const auto& sp : JsonConfig::RequireArray(taskConfig, "associated_particles", GetName())) {
      // The index is in the context because a missing 'name' leaves nothing else to
      // identify the entry by.
      const std::string ctx = std::format("{} associated_particles[{}]", GetName(), idx++);

      std::string name = JsonConfig::RequireString(sp, "name", ctx);

      // The directory name is derived from the species unless the configuration
      // overrides it - see AssocParticleConfig for the rule. The binning stays
      // empty: it is resolved from the input files in ResolveBinningAndCache(),
      // once the containers are open.
      assocParticles.emplace_back(name, JsonConfig::TryString(sp, "dir_name", ctx));
    }
  }

  // -------------------------------------------------------------------------
  // Shared efficiency loading + extrapolation config setup.
  // Requires assocParticles already populated.
  // -------------------------------------------------------------------------
  void InitCorrectionsAndExtrapolation(const rapidjson::Value& taskConfig)
  {
    if (applyEfficiency)
      LoadCorrections(taskConfig);

    if (applyExtrapolation) {
      std::string extrapFile = JsonConfig::RequireString(taskConfig, "extrapolation_config_file", GetName());
      extrapConfigManager = std::make_unique<ExtrapConfigManager>(extrapFile);

      // Recorded only when extrapolation is on: a file that was not used has no
      // business in the provenance of a result it did not shape.
      provenance["extrap_config"] = JsonConfig::ReadFileText(extrapFile);
      std::cout << "[INFO] " << GetName() << ": Extrapolation configuration loaded successfully." << std::endl;

      for (const auto& p : assocParticles) {
        bool hasCfg = extrapConfigManager->HasConfig(p.name);
        doExtrapolationPerParticle[p.name] = hasCfg;
        if (!hasCfg)
          std::cout << "[INFO] " << GetName() << ": No extrapolation config found for '" << p.name << "'. Skipping." << std::endl;
      }
    }
  }

  // -------------------------------------------------------------------------
  // Shared efficiency / signal-loss / event-loss loading.
  // -------------------------------------------------------------------------
  void LoadCorrections(const rapidjson::Value& taskConfig)
  {

    std::string inputEffFile = JsonConfig::RequireString(taskConfig, "input_efficiency_file", GetName());
    std::unique_ptr<TFile> fileEffInput = RootIO::OpenOrThrow(inputEffFile, "READ", "CorrelationTaskBase::LoadCorrections");

    RootIO::PrintProvenance(RootIO::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "Provenance"}, true),
                            "corrections file '" + inputEffFile + "'");

    // The corrections are addressed by multiplicity INDEX ("..._multBin3"), with
    // indices that come from the data. The stamp MCTask leaves behind is the only
    // way to know the intervals those indices meant in the MC production.
    RootIO::RequireMatchingBinningStamp(
      RootIO::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName}, true),
      "binning_mult", multBinning, "efficiency map '" + inputEffFile + "'");

    // Mirror the exact layout MCTask writes: {binningName}/AccEff/MultBin,
    // {binningName}/SigLoss/MultBin, {binningName}/EvLoss
    TDirectory* accEffDir = RootIO::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "AccEff", "MultBin"}, true);
    TDirectory* sigLossDir = RootIO::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "SigLoss", "MultBin"}, true);
    TDirectory* evLossDir = RootIO::GetOrCreatePath(fileEffInput.get(), {globalCfgs.binningName, "EvLoss"}, true);

    std::vector<std::string> activeCorrections;
    if (auto corrections = JsonConfig::TryArray(taskConfig, "active_corrections", GetName())) {
      for (const auto& val : *corrections) {
        activeCorrections.push_back(val.GetString());
      }
    } else {
      std::cout << "[INFO] 'active_corrections' missing or not an array. Applying NO corrections!" << std::endl;
    }

    // Lambda to quickly check if a specific correction was requested in the JSON
    auto isActive = [&](const std::string& name) {
      return std::find(activeCorrections.begin(), activeCorrections.end(), name) != activeCorrections.end();
    };

    bool doAccEfficiency = isActive("acceptance_efficiency");
    bool doSigLoss = isActive("signal_loss");

    // Read TO WHICH PARTICLES they should be applied
    std::vector<std::string> effParts;
    if (auto effTargets = JsonConfig::TryArray(taskConfig, "apply_efficiency_to", GetName())) {
      for (const auto& val : *effTargets)
        effParts.push_back(val.GetString());
    } else {
      // Secure fallback: if the user doesn't specify it in the JSON, apply to Phi + associated particles in the analysis
      effParts.push_back("Phi");
      for (const auto& p : assocParticles)
        effParts.push_back(p.name);
      std::cout << "[INFO] 'apply_efficiency_to' not found. Defaulting to all active particles." << std::endl;
    }

    // Event Loss is loaded if requested
    if (isActive("event_loss")) {
      if (!evLossDir)
        throw std::runtime_error("[FATAL] 'event_loss' requested but directory '" + globalCfgs.binningName + "/EvLoss' not found!");

      if (auto hTemp = static_cast<TH1*>(evLossDir->Get("hEventLoss"))) {
        hEventLoss.reset(static_cast<TH1*>(hTemp->Clone("hEventLoss")));
        hEventLoss->SetDirectory(0);
      } else
        throw std::runtime_error("[FATAL] 'event_loss' requested but 'hEventLoss' not found!");
    }

    // LAMBDA HELPER 1: Load the histogram and throw an error if missing
    auto fetchHist = [&](TDirectory* dir, const std::string& name, bool apply, const std::vector<double>& targetBinning) -> std::unique_ptr<TH1F> {
      if (!apply)
        return nullptr;
      if (!dir)
        throw std::runtime_error("[FATAL] Missing directory containing: " + name);

      std::unique_ptr<TH1F> h = RootIO::GetUniqueOrThrow<TH1F>(dir, name, "CorrelationTaskBase::LoadCorrections");

      // Verified, NOT rebinned. An efficiency is a ratio, and merging bins of a
      // ratio sums them: two bins of 0.5 would give 1.0. A coarser efficiency can
      // only be built where numerator and denominator still exist, which is the
      // MC task - see 'rebinning_pt' in the MC configuration.
      const std::string diff = BinningUtils::Compare(targetBinning, BinningUtils::AxisEdges(h->GetXaxis()),
                                                     "analysis binning (data production)",
                                                     "efficiency map (MC production)");
      if (!diff.empty()) {
        throw std::runtime_error(
          "[FATAL] CorrelationTaskBase::LoadCorrections: '" + name + "' in '" + inputEffFile +
          "' is not binned like the data being corrected:\n" + diff +
          "Set 'rebinning_pt' in the MC configuration so the efficiency is produced at this binning, "
          "or re-run the production whose axis does not match.");
      }

      return h;
    };

    // LAMBDA HELPER 2: Clone and multiply (if both are needed) safely
    auto combineHists = [](const TH1F* hEff, const TH1F* hLoss, const std::string& name) -> std::unique_ptr<TH1F> {
      if (!hEff && !hLoss)
        return nullptr;

      std::unique_ptr<TH1F> hRes(static_cast<TH1F*>(hEff ? hEff->Clone(name.c_str()) : hLoss->Clone(name.c_str())));
      if (hEff && hLoss)
        hRes->Multiply(hLoss);
      hRes->SetDirectory(0);

      return hRes;
    };

    correctionCollection.clear();

    for (const std::string& name : effParts) {
      LoadedCorrections corr;
      corr.name = name;
      corr.h1Corrections.resize(BinningUtils::NBins(multBinning));
      corr.h1CorrectionsEffMultInt.resize(BinningUtils::NBins(multBinning));

      std::vector<double> targetBinning;
      if (name == "Phi") {
        targetBinning = ptPhiBinning;
      } else {
        auto it = std::ranges::find_if(assocParticles, [&](const AssocParticleConfig& p) { return p.name == name; });
        if (it == assocParticles.end())
          throw std::runtime_error("[FATAL] Unknown particle name in 'apply_efficiency_to': " + name);
        targetBinning = it->binning;
      }

      // Naming convention for histograms based on particle name
      std::string effHistBase = "h1" + name + "Efficiency";
      std::string sigLossHistBase = "h1" + name + "SigLoss";

      // Fetch integrated histograms ONLY if useIntegratedEfficiency is true
      std::unique_ptr<TH1F> hEffInt = fetchHist(accEffDir, effHistBase + "_multIntegrated", doAccEfficiency && useIntegratedEfficiency, targetBinning);
      // Note: Signal loss is typically not integrated, but we fetch it if requested for consistency

      for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
        std::string iStr = std::to_string(i);

        // Fetch binned histograms ONLY if useIntegratedEfficiency is false
        std::unique_ptr<TH1F> hEffBin = fetchHist(accEffDir, effHistBase + "_multBin" + iStr, doAccEfficiency && !useIntegratedEfficiency, targetBinning);
        // Note: Signal loss is typically not integrated, so we fetch the binned version if signal loss correction is requested,
        // regardless of the useIntegratedEfficiency flag
        std::unique_ptr<TH1F> hLossBin = fetchHist(sigLossDir, sigLossHistBase + "_multBin" + iStr, doSigLoss, targetBinning);

        corr.h1Corrections[i] = combineHists(hEffBin.get(), hLossBin.get(), "h1Corrected_" + name + "_multBin" + iStr);
        corr.h1CorrectionsEffMultInt[i] = combineHists(hEffInt.get(), hLossBin.get(), "h1Corrected_" + name + "_multInt" + iStr);
      }

      correctionCollection[name] = std::move(corr);
    }

    std::cout << "[INFO] CorrelationTask: Efficiencies loaded successfully." << std::endl;
  }

  // -------------------------------------------------------------------------
  // Object naming for the cached projections.
  //
  // Bin RANGES are spelled out instead of bin indices. With an index, a cache
  // built from a different production still contains "_ptBin7" and is read back
  // happily while meaning another pT interval; with the range in the name, the
  // lookup simply misses and the run stops. The name is the primary defence,
  // the binning stamp below only explains what went wrong.
  //
  // Both run modes go through here on purpose: they used to build the suffix
  // separately, with different conventions, so a cache produced by one was not
  // readable by the other.
  // -------------------------------------------------------------------------
  static std::string BinLabel(std::span<const double> edges, int bin)
  {
    return BinningUtils::FormatEdge(edges[bin]) + "-" + BinningUtils::FormatEdge(edges[bin + 1]);
  }

  // Per (multiplicity, trigger pT, associated pT) cell.
  std::string CellSuffix(const AssocParticleConfig& particle, int multBin, int ptPhiBin, int ptAssocBin) const
  {
    return std::format("_mult{}_ptPhi{}_pt{}",
                       BinLabel(multBinning, multBin),
                       BinLabel(ptPhiBinning, ptPhiBin),
                       BinLabel(particle.binning, ptAssocBin));
  }

  // For the objects accumulated over the trigger pT bins.
  std::string CellSuffix(const AssocParticleConfig& particle, int multBin, int ptAssocBin) const
  {
    return std::format("_mult{}_pt{}",
                       BinLabel(multBinning, multBin),
                       BinLabel(particle.binning, ptAssocBin));
  }

  // -------------------------------------------------------------------------
  // Binning resolution and projection cache provenance, in one step.
  //
  // The binning always comes from a file, never from the configuration:
  //   producing the cache -> from the axes of the THnSparse being projected
  //   reusing the cache   -> from the stamps, which describe what is in it
  // and in both cases it is verified against the declared production.
  //
  // The two are done together on purpose. Doing them apart is what would allow
  // indexing one binning while labelling another, or reusing a cache built with
  // a different one - and the stamps only mean something if they are written by
  // whoever resolved the binning.
  //
  // The stamps live INSIDE the binning_name directory, next to the projections
  // they describe. The file is opened UPDATE precisely so several schemes can
  // coexist, each in its own directory: a stamp at the file root would be shared
  // between them and would reject that legitimate case.
  // -------------------------------------------------------------------------
  // A binning that must be common to every associated particle: taken from the
  // first one, then required to match on the others. Without this the last
  // particle would silently win, and the multiplicity loops and trend histograms
  // - which are built once for all species - would be based on it alone.
  static void AdoptOrRequireSame(std::vector<double>& common, std::vector<double> found, bool isFirst,
                                 std::string_view what, std::string_view firstName, std::string_view thisName)
  {
    if (isFirst) {
      common = std::move(found);
      return;
    }

    const std::string diff = BinningUtils::Compare(common, found, firstName, thisName);
    if (!diff.empty()) {
      throw std::runtime_error(std::format(
        "[FATAL] {} differs between associated particles:\n{}"
        "All species share the same multiplicity and trigger loops, "
        "so these must agree.",
        what, diff));
    }
  }

  void ResolveBinningAndCache()
  {
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      auto& particle = assocParticles[pIdx];
      TFile* cacheFile = filesPhiAssocDataOutput[pIdx].get();
      const std::string ctx = "Phi" + particle.name + "DataHistograms.root, scheme '" + globalCfgs.binningName + "'";

      // Resolved for THIS particle, then merged into the common ones below.
      std::vector<double> mult;
      std::vector<double> ptPhi;

      if (!useProjectionCache) {
        // h5Phi{X}Data* axes: (mult, pT(phi), pT(assoc), dy, dphi)
        const THnSparse* source = loadedDataCollection[pIdx].h5DataSignal.get();
        const std::string origin = "h5Phi" + particle.name + "DataSignal";

        mult = globalCfgs.ResolveMultBinning(source->GetAxis(0), origin);
        ptPhi = globalCfgs.ResolvePtBinning("Phi", source->GetAxis(1), origin);
        particle.binning = globalCfgs.ResolvePtBinning(particle.name, source->GetAxis(2), origin);

        TDirectory* schemeDir = RootIO::GetOrCreatePath(cacheFile, {globalCfgs.binningName}, false);

        // Projections from an earlier run under the SAME scheme name are still
        // there: if that run used a different binning, the ones whose bin range
        // no longer exists are not overwritten and would survive as garbage.
        const std::vector<double> previous = RootIO::ReadBinningStamp(schemeDir, "binning_ptAssoc");
        if (!previous.empty()) {
          const std::string diff = BinningUtils::Compare(previous, particle.binning, "existing file", "current run");
          if (!diff.empty()) {
            throw std::runtime_error(
              "[FATAL] " + GetName() + ": " + ctx + " already holds projections built with a different pT binning:\n" +
              diff +
              "Overwriting would leave projections of both binnings under the same scheme. Either give this "
              "binning a distinct 'binning_name', so the two live in separate directories, or delete the file.");
          }
        }

        RootIO::WriteBinningStamp(schemeDir, "binning_ptAssoc", particle.binning);
        RootIO::WriteBinningStamp(schemeDir, "binning_ptPhi", ptPhi);
        RootIO::WriteBinningStamp(schemeDir, "binning_mult", mult);
      } else {
        TDirectory* schemeDir = RootIO::GetOrCreatePath(cacheFile, {globalCfgs.binningName}, true);
        if (!schemeDir) {
          throw std::runtime_error("[FATAL] " + GetName() + ": cache requested but scheme '" + globalCfgs.binningName +
                                   "' does not exist in Phi" + particle.name +
                                   "DataHistograms.root. Run once with 'use_projection_cache': false.");
        }

        particle.binning = RootIO::ReadBinningStamp(schemeDir, "binning_ptAssoc");
        ptPhi = RootIO::ReadBinningStamp(schemeDir, "binning_ptPhi");
        mult = RootIO::ReadBinningStamp(schemeDir, "binning_mult");

        if (particle.binning.empty() || ptPhi.empty() || mult.empty()) {
          throw std::runtime_error("[FATAL] " + GetName() + ": " + ctx +
                                   " carries no binning stamps, so the binning its projections were built "
                                   "with is unknown. Rebuild it with 'use_projection_cache': false.");
        }

        // The cache is the source of truth here, but it still has to be the
        // production we think we are working on.
        globalCfgs.VerifyMultBinning(mult, ctx);
        globalCfgs.VerifyPtBinning("Phi", ptPhi, ctx);
        globalCfgs.VerifyPtBinning(particle.name, particle.binning, ctx);
      }

      const bool isFirst = (pIdx == 0);
      AdoptOrRequireSame(multBinning, std::move(mult), isFirst, "Multiplicity binning",
                         assocParticles.front().name, particle.name);
      AdoptOrRequireSame(ptPhiBinning, std::move(ptPhi), isFirst, "Trigger pT binning",
                         assocParticles.front().name, particle.name);
    }

    VerifyTriggerAxes();
  }

  // -------------------------------------------------------------------------
  // The last positional dependency left between two tasks.
  //
  // GetTriggerSignal(multBin, ptPhiBin) indexes the trigger container with the
  // same pair the analysis loops over, but that container is filled by another
  // task, against the binning IT resolved from ITS own input file. Nothing forced
  // the two to agree: the yields would simply be read from the wrong cells, and
  // the result would be wrong without being visibly broken.
  //
  // Checkable only because those matrices carry real bin edges instead of 0..N
  // counters - which is why they were built that way.
  // -------------------------------------------------------------------------
  void VerifyTriggerAxes() const
  {
    const auto [multAxis, ptPhiAxis] = TriggerAxes();
    if (!multAxis || !ptPhiAxis)
      return;

    auto check = [&](const std::vector<double>& analysis, const TAxis* axis, std::string_view what) {
      const std::string diff = BinningUtils::Compare(analysis, BinningUtils::AxisEdges(axis),
                                                     "analysis binning (associated data)", "trigger container");
      if (!diff.empty()) {
        throw std::runtime_error(std::format(
          "[FATAL] {}: the {} of the trigger container does not match the one resolved from the "
          "associated-particle data:\n{}"
          "Trigger yields are read by bin index, so this would take them from the wrong cells. "
          "The two productions must share this axis, or the trigger file must be regenerated.",
          GetName(), what, diff));
      }
    };

    check(multBinning, multAxis, "multiplicity axis");
    check(ptPhiBinning, ptPhiAxis, "trigger pT axis");
  }

  // -------------------------------------------------------------------------
  // Shared trend/spectra-canvas bookkeeping setup.
  // -------------------------------------------------------------------------
  void SetupTrendHistograms()
  {
    // 1. Initialize structure for pT bin accumulation
    h1PhiAssocNoPtPhi.resize(assocParticles.size());
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      h1PhiAssocNoPtPhi[pIdx].resize(BinningUtils::NBins(multBinning));
      for (int i{0}; i < BinningUtils::NBins(multBinning); ++i) {
        h1PhiAssocNoPtPhi[pIdx][i].resize(BinningUtils::NBins(assocParticles[pIdx].binning));
      }
    }

    // 2. Initialize matrix for Mult Trends: [ParticleIndex][DeltaYIndex]
    h1MultTrends.resize(assocParticles.size());
    if (applyExtrapolation) {
      h1MultTrendsExtrap.resize(assocParticles.size());
    }

    spectraCanvases.resize(assocParticles.size());

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& p = assocParticles[pIdx];
      bool doExtrapForThis = applyExtrapolation && doExtrapolationPerParticle.contains(p.name) && doExtrapolationPerParticle.at(p.name);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];
        auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

        std::string hName = std::format("h1MultTrend_{}_dy{}", p.name, dyNameStr);
        std::string hTitle = std::format("Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

        std::unique_ptr<TH1F> hTrend = std::make_unique<TH1F>(hName.c_str(), hTitle.c_str(), BinningUtils::NBins(multBinning), multBinning.data());
        hTrend->SetDirectory(0);
        h1MultTrends[pIdx].push_back(std::move(hTrend));

        if (doExtrapForThis) {
          std::string hExtrapName = std::format("h1MultTrendExtrap_{}_dy{}", p.name, dyNameStr);
          std::string hExtrapTitle = std::format("Extrapolated Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

          std::unique_ptr<TH1F> hTrendExtrap = std::make_unique<TH1F>(hExtrapName.c_str(), hExtrapTitle.c_str(), BinningUtils::NBins(multBinning), multBinning.data());
          hTrendExtrap->SetDirectory(0);
          h1MultTrendsExtrap[pIdx].push_back(std::move(hTrendExtrap));
        }

        std::string cSpecName = std::format("cSpectra_{}_dy{}", p.name, dyNameStr);
        std::string cSpecTitle = std::format("Spectra {} |#Delta y| < {}", p.name, dyTitleStr);

        std::unique_ptr<TCanvas> cSpec = std::make_unique<TCanvas>(cSpecName.c_str(), cSpecTitle.c_str(), 800, 600);
        cSpec->SetLogy();
        spectraCanvases[pIdx].push_back(std::move(cSpec));
      }
    }
  }

  // -------------------------------------------------------------------------
  // Extrapolation function
  // -------------------------------------------------------------------------
  ExtrapolationResult ExtrapolateSpectrum(TH1* hSpec, const AssocParticleConfig& config, int multBin, TDirectory* targetDir)
  {
    // 1. Fetch configuration for THIS particle and THIS multBin from JSON
    ExtrapConfig eCfg = extrapConfigManager->GetConfig(config.name, multBin);
    eCfg.mass = config.mass; // Inject physical mass

    // 2. Generate configured TF1 model
    std::unique_ptr<TF1> extrapModel = ExtrapolationModelFactory::CreateModel(eCfg, hSpec->GetMaximum());

    /*{
      // std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}", hSpec->GetName(), extrapFunction);
      std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}_multBin{}", hSpec->GetName(), eCfg.model, multBin);
      std::unique_ptr<TCanvas> cDebug = std::make_unique<TCanvas>(debugCanvasName.c_str(), "Debug Guesses", 800, 600);
      // cDebug->SetLogy();

      hSpec->SetMarkerStyle(20);
      hSpec->SetMarkerColor(kBlack);
      hSpec->SetLineColor(kBlack);
      hSpec->DrawCopy();

      extrapModel->SetRange(0.0, 8.0);
      extrapModel->SetLineColor(kRed);
      extrapModel->SetLineWidth(2);
      extrapModel->DrawCopy("SAME");

      targetDir->cd();
      cDebug->Write(nullptr, TObject::kOverwrite);
    }*/

    // 3. Extrapolate.
    //
    // These three are the values the reference implementation was called with -
    // OldCodes/YieldMean.h, which this replaced after being shown to reproduce it
    // number for number (DESIGN_NOTES.md has the comparison). They are spelled out
    // here rather than left to the defaults of SpectrumExtrapolator so that the
    // agreement rests on something visible at the call site: the same three values
    // once lived as literals here and as member defaults there, and drifted apart
    // without either side looking wrong on its own.
    //
    // "I" integrates the function over each bin instead of taking its value at the
    // centre: on a steeply falling spectrum with wide bins the two differ, and the
    // extrapolated yield is an integral of this fit below the first measured point.
    constexpr const char* kFitOption = "0QI";
    constexpr double kLoPrecision = 0.01;
    constexpr double kHiPrecision = 0.1;

    const double firstMeasuredPt = config.binning[0];

    SpectrumExtrapolator extrapolator(hSpec, extrapModel.get());
    extrapolator.SetFitRange(eCfg.fitRange.first, eCfg.fitRange.second);
    // The limits especially: without this call the class kept its own 0..10 and
    // 'domain_range' looked like it controlled the integration while controlling
    // only the range of the TF1.
    extrapolator.SetExtrapolationLimits(eCfg.domainRange.first, eCfg.domainRange.second);
    extrapolator.SetFitOption(kFitOption);
    extrapolator.SetIntegrationPrecisions(kLoPrecision, kHiPrecision);

    ExtrapolationResult res = extrapolator.CalculateYieldAndMean();

    // 'extrapModel' is fitted in place, so from here on it carries the fitted
    // parameters. That is what makes this line - and the curve attached to the
    // extended spectrum further down - show the fit rather than the initial guess,
    // which is what they showed while SpectrumExtrapolator worked on a clone.
    double fitLowPtIntegral = 0.0;
    if (firstMeasuredPt > 0.0)
      fitLowPtIntegral = extrapModel->Integral(0.0, firstMeasuredPt);

    // 4. Debug output
    {
      const double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");
      std::cout << "\n[DEBUG EXTRAP] " << std::endl;
      std::cout << "  -> Raw data integral:            " << rawIntegral << std::endl;
      std::cout << "  -> Yield after extrapolation:    " << res.yield << std::endl;
      std::cout << "  -> Extrapolated part:            " << res.extrapolatedYield
                << " (" << 100. * res.ExtrapolatedFraction() << "% of the yield)" << std::endl;
      std::cout << "  -> Fitted function [0, " << firstMeasuredPt << "]: " << fitLowPtIntegral << std::endl;

      // The four numbers to check against the reference if this ever has to be
      // verified again. Yield and mean are deterministic and must agree exactly;
      // the two errors are the RMS of 1000 toys and agree only to a few percent,
      // so comparing them for equality is chasing noise.
      std::cout << "  -> Yield:  " << res.yield << " +- " << res.yieldStatErr
                << "   <pT>: " << res.meanPt << " +- " << res.meanPtStatErr << std::endl;

      // A converged fit is not a good fit, and roughly a third of the yield above is
      // an integral of this function outside the measured range - so this line is
      // not decoration. It used to be printed only by YieldMean, which meant
      // retiring that path would have silently removed the only sign that the fit
      // does not describe the data. See DESIGN_NOTES.md, which still lists that as
      // open.
      std::cout << "  -> Fit " << eCfg.model << " over [" << eCfg.fitRange.first << ", "
                << eCfg.fitRange.second << "]: chi2/ndf = " << res.chi2 << "/" << res.ndf
                << " = " << res.ReducedChi2() << std::endl;

      // The two being equal is not a coincidence worth leaving to the eye: it means
      // nothing was added, i.e. the extrapolation silently dropped out.
      if (res.yield == rawIntegral)
        std::cerr << "[WARNING] " << hSpec->GetName()
                  << ": the extrapolated yield equals the raw integral exactly - nothing was "
                     "extrapolated. Check the fit above."
                  << std::endl;
    }

    // 5. Create extended binning dynamically
    std::vector<double> extBinning;
    extBinning.push_back(0.0); // Lower limit for extrapolation
    for (double edge : config.binning) {
      extBinning.push_back(edge);
    }

    std::string extName = std::string(hSpec->GetName()) + "_extended";
    std::unique_ptr<TH1> hSpecExt = std::make_unique<TH1D>(extName.c_str(), extName.c_str(), extBinning.size() - 1, extBinning.data());
    hSpecExt->SetDirectory(0);

    // 6. Copy measured contents (shifted by 1 bin to the right)
    for (int b = 1; b <= hSpec->GetNbinsX(); ++b) {
      hSpecExt->SetBinContent(b + 1, hSpec->GetBinContent(b));
      hSpecExt->SetBinError(b + 1, hSpec->GetBinError(b));
    }

    // Leave the first bin empty (visual placeholder for the fit curve)
    hSpecExt->SetBinContent(1, 0.0);
    hSpecExt->SetBinError(1, 0.0);

    AnalysisUtils::SetHistogramStyle(hSpecExt.get(), globalCfgs.GetSpectraColor(multBin));
    // The fitted curve, travelling with the spectrum it describes.
    hSpecExt->GetListOfFunctions()->Add(extrapModel->Clone());

    targetDir->cd();
    hSpecExt->Write(nullptr, TObject::kOverwrite);

    return res;
  }

  // -------------------------------------------------------------------------
  // Shared spectra construction, normalization, and extrapolation.
  // -------------------------------------------------------------------------
  void GenerateSpectraAndTrends(int multBin, double totalTriggerSignalPerMult)
  {
    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    TDirectory* targetSpectraDir = RootIO::GetOrCreatePath(fileOutputSpectra.get(), {globalCfgs.binningName, dirName});

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& config = assocParticles[pIdx];
      bool doExtrapForThis = applyExtrapolation && doExtrapolationPerParticle.contains(config.name) && doExtrapolationPerParticle.at(config.name);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];

        auto [dyTitleStr, dyNameStr] = AnalysisUtils::FormatDeltaY(dyLimit);

        std::string spectraName = std::format("h1SpectrumPhi{}_dy{}_multBin{}", config.name, dyNameStr, multBin);

        std::vector<TH1*> views;
        views.reserve(h1PhiAssocNoPtPhi[pIdx][multBin].size());
        for (const auto& h : h1PhiAssocNoPtPhi[pIdx][multBin])
          views.push_back(h.get());

        // Construct the 1D spectrum from the accumulated Pt bins
        std::unique_ptr<TH1> h1Spectrum = AnalysisUtils::ConstructSpectrum(views, config.binning, spectraName, dyLimit);

        double effectiveTriggers = totalTriggerSignalPerMult;

        // Normalize by the event loss correction if it is required
        if (applyEfficiency && hEventLoss) {
          double eventLossCorr = hEventLoss->GetBinContent(multBin + 1);
          if (eventLossCorr > 0.0) {
            effectiveTriggers /= eventLossCorr;
          }
        }

        // Normalize by the total number of triggers AND the Delta Y phase space
        if (totalTriggerSignalPerMult > 0.0) {
          h1Spectrum->Scale(1.0 / (effectiveTriggers * 2.0 * dyLimit));
        }

        // Purity correction hook: no-op unless overridden (CorrelationTask does).
        if (TH1* hPurity = GetPurityHist(config.name, multBin)) {
          h1Spectrum->Multiply(hPurity);
        }

        AnalysisUtils::SetHistogramStyle(h1Spectrum.get(), globalCfgs.GetSpectraColor(multBin));

        // Draw and Write (Only draw the nominal Delta Y limit on the summary canvas)
        spectraCanvases[pIdx][yIdx]->cd();
        h1Spectrum->DrawCopy(multBin == 0 ? "" : "SAME");

        targetSpectraDir->cd();
        h1Spectrum->Write(nullptr, TObject::kOverwrite);

        AnalysisUtils::ConstructMultTrend(h1MultTrends[pIdx][yIdx].get(), h1Spectrum.get(), multBin);

        // Extrapolate and Fill Trend
        if (doExtrapForThis) {
          ExtrapolationResult res = ExtrapolateSpectrum(h1Spectrum.get(), config, multBin, targetSpectraDir);
          AnalysisUtils::ConstructMultTrend(h1MultTrendsExtrap[pIdx][yIdx].get(), res, multBin);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Shared "simple" run: one pass over mult/ptPhi/pt-assoc bins, no L2 cache.
  // Relies on GetTriggerSignal/GetTriggerBkgRatio hooks for the data-source-
  // specific part. This is what CorrelationWPDGTask uses directly as its Run(),
  // and what CorrelationTask::RunLegacy() is when apply_purity/QA aren't needed
  // beyond what the hooks already provide.
  // -------------------------------------------------------------------------
  void RunLegacy()
  {
    std::cout << "[INFO] " << GetName() << ": RUNNING CORRELATIONS (Legacy)..." << std::endl;
    CorrelationCalculator corrCalculator(applyME, useProjectionCache, false, false);

    const std::vector<std::unique_ptr<TH1F>>* phiCorrs{nullptr};
    std::vector<const std::vector<std::unique_ptr<TH1F>>*> assocCorrs(assocParticles.size(), nullptr);

    if (applyEfficiency && !correctionCollection.empty()) {
      auto itPhi = correctionCollection.find("Phi");
      if (itPhi != correctionCollection.end())
        phiCorrs = useIntegratedEfficiency ? &itPhi->second.h1CorrectionsEffMultInt : &itPhi->second.h1Corrections;

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        auto it = correctionCollection.find(assocParticles[pIdx].name);
        if (it != correctionCollection.end())
          assocCorrs[pIdx] = useIntegratedEfficiency ? &it->second.h1CorrectionsEffMultInt : &it->second.h1Corrections;
      }
    }

    std::string dirName = use2DMENormalization ? "Extract2D" : "Extract1D";
    std::vector<std::string> logicalPath{globalCfgs.binningName, dirName};

    for (int i = 0; i < BinningUtils::NBins(multBinning); i++) {
      AnalysisUtils::AxisToCut axisToCutMult{.axis = 0, .bins = {i + 1, i + 1}};
      double totalTriggerSignalPerMult = 0.0;
      TH1* h1EffPhi = phiCorrs ? (*phiCorrs)[i].get() : nullptr;

      for (int j = 0; j < BinningUtils::NBins(ptPhiBinning); j++) {
        AnalysisUtils::AxisToCut axisToCutPtPhi{.axis = 1, .bins = {j + 1, j + 1}};

        double triggerSignal = GetTriggerSignal(i, j);
        double triggerBkgRatio = GetTriggerBkgRatio(i, j);
        double phiEff = h1EffPhi ? h1EffPhi->GetBinContent(j + 1) : 1.0;
        totalTriggerSignalPerMult += triggerSignal / phiEff;

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
          const auto& config = assocParticles[pIdx];
          const auto& data = loadedDataCollection[pIdx];
          TH1* h1EffAssoc = assocCorrs[pIdx] ? (*assocCorrs[pIdx])[i].get() : nullptr;

          TDirectory* targetDir = RootIO::GetOrCreatePath(filesPhiAssocDataOutput[pIdx].get(), logicalPath, useProjectionCache);

          for (int k = 0; k < BinningUtils::NBins(config.binning); k++) {
            AnalysisUtils::AxisToCut axisToCutPtAssoc{.axis = 2, .bins = {k + 1, k + 1}};
            std::vector<AnalysisUtils::AxisToCut> axesToCut = {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc};

            double assocEff = h1EffAssoc ? h1EffAssoc->GetBinContent(k + 1) : 1.0;
            double totalEff = phiEff * assocEff;

            std::string histNameBase = "h1Phi" + config.name + "Data";
            std::string suffix = CellSuffix(config, i, j, k);

            std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(data, axesToCut, totalEff, triggerBkgRatio, histNameBase + suffix, targetDir, nullptr, projectionAxis);

            if (j == 0) {
              std::string accumName = "h1Phi" + config.name + "DataSignal" + CellSuffix(config, i, k);
              h1PhiAssocNoPtPhi[pIdx][i][k].reset(static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str())));
              h1PhiAssocNoPtPhi[pIdx][i][k]->SetDirectory(0);
            } else {
              h1PhiAssocNoPtPhi[pIdx][i][k]->Add(h1FinalSignal.get());
            }
          }
        }
      }

      GenerateSpectraAndTrends(i, totalTriggerSignalPerMult);
    }
  }

  static double GetYieldScaleFactor(const std::string& name)
  {
    return (name == "K0S") ? 2.0 : 1.0;
  }
};

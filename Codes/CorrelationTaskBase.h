#pragma once

#include "AnalysisConstants.h"
#include "AnalysisSettings.h"
#include "AnalysisStructures.h"
#include "BinningUtils.h"
#include "CanvasManager.h"
#include "CorrelationCalculator.h"
#include "ExtrapConfigManager.h"
#include "ExtrapolationModelFactory.h"
#include "HistogramUtils.h"
#include "IAnalysisTask.h"
#include "IONames.h"
#include "JsonConfigHelpers.h"
#include "Logger.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"
#include "SpectrumExtrapolator.h"
#include "TrainConfig.h"

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
#include <cmath>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
    Logger::Info("{}: TERMINATING AND CLEANING UP...", GetName());

    const std::vector<std::string> baseLogicalPath = SectionPath();

    // =========================================================================
    // 1. Write Yield Trends, Ratios Extrap/Meas, and Canvases PER SPECIES
    // =========================================================================
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const std::string& pName = assocParticles[pIdx].name;

      // One directory per particle, split by what the objects are: the spectra on
      // one side, everything that lives on the multiplicity axis on the other. The
      // spectra used to sit flat in the scheme directory while only the trends had a
      // per-particle folder, so a reader had to know the naming convention to tell
      // which spectrum belonged to which species. This makes the layout say it.
      const std::vector<std::string> particlePath = SectionPath({pName});
      const std::vector<std::string> trendsPath = SectionPath({pName, IONames::kTrendsDir});

      TDirectory* trendsDir = RootIO::MakePath(outputFile.get(), trendsPath);

      // The associated pT binning, with the particle it belongs to rather than
      // encoded in the object name. What it guards is the Trends directory next to
      // it: those are integrals over pT, so a disagreement there does not show up in
      // any axis. See the note by the scheme-level stamps below.
      RootIO::WriteBinningStamp(RootIO::MakePath(outputFile.get(), particlePath),
                                RootIO::kStampPtAssoc, assocParticles[pIdx].binning);
      trendsDir->cd();

      // --- Save measured trends ---
      std::string cNameMeas = IONames::TrendCanvas(pName);
      TCanvas* cTrendMeas = canvasManager.CreateCanvas(cNameMeas, "Measured Yield Trend", trendsPath, 800, 600);
      canvasManager.SetAutoYRange(cTrendMeas);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        trendsDir->cd();
        h1MultTrends[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

        cTrendMeas->cd();
        CanvasManager::DrawStyledCopy(h1MultTrends[pIdx][yIdx].get(), yIdx == 0 ? "" : "SAME", globalCfgs.GetMultTrendColor(yIdx));
      }

      // --- Save extrapolated trends ---
      bool hasExtrapTrend = !h1MultTrendsExtrap.empty() && !h1MultTrendsExtrap[pIdx].empty();
      if (hasExtrapTrend) {
        std::string cNameExtrap = IONames::TrendExtrapCanvas(pName);
        TCanvas* cTrendExtrap = canvasManager.CreateCanvas(cNameExtrap, "Extrapolated Yield Trend", trendsPath, 800, 600);
        canvasManager.SetAutoYRange(cTrendExtrap);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          trendsDir->cd();
          h1MultTrendsExtrap[pIdx][yIdx]->Write(nullptr, TObject::kOverwrite);

          cTrendExtrap->cd();
          CanvasManager::DrawStyledCopy(h1MultTrendsExtrap[pIdx][yIdx].get(), yIdx == 0 ? "" : "SAME", globalCfgs.GetMultTrendColor(yIdx));
        }
      }

      // --- Save Extrapolated/Measured ratios for this species ---
      if (hasExtrapTrend) {
        std::string canvasName = IONames::TrendExtrapOverMeasCanvas(pName);
        TCanvas* cRatioExtrapMeas = canvasManager.CreateCanvas(canvasName, "Extrap / Measured Ratio", trendsPath, 800, 600);
        canvasManager.SetAutoYRange(cRatioExtrapMeas);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          double dyLimit = deltaYLimits[yIdx];

          std::string ratioName = IONames::TrendExtrapOverMeas(pName, dyLimit);
          std::string title = Form("Extrap/Measured Contribution %s |#Delta y| < %.2f; Multiplicity percentile (%%); Yield_{Extrap} / Yield_{Meas}", pName.c_str(), dyLimit);

          std::unique_ptr<TH1> hRatioExtrapMeas = HistogramUtils::MakeRatioHist(h1MultTrendsExtrap[pIdx][yIdx].get(), h1MultTrends[pIdx][yIdx].get(),
                                                                                ratioName, title, 1.0, 1.0);

          trendsDir->cd();
          hRatioExtrapMeas->Write(nullptr, TObject::kOverwrite);

          cRatioExtrapMeas->cd();
          CanvasManager::DrawStyledCopy(hRatioExtrapMeas.get(), yIdx == 0 ? "" : "SAME", globalCfgs.GetMultTrendColor(yIdx));
        }
      }
    }

    // The binnings this file was produced with. Unlike the provenance below, these
    // ARE a check, and each is here for a reason that comparing axes would not cover.
    //
    // Multiplicity is in the NAMES of the spectra, not an axis of anything being divided
    // (RootIO::RequireMatchingBinningStamp). Trigger pT is integrated away everywhere, so it
    // is not an axis either: two productions summing over different trigger ranges would
    // compare cleanly and mean different things.
    //
    // The associated pT is stamped per particle, next to that particle's objects -
    // see the loop in the section above. It is NOT here, and it is not redundant with
    // RequireSameAxis either: for the spectra it would be, since pT is their X axis,
    // but the TRENDS are integrated over pT and carry multiplicity on theirs, so two
    // productions with different pT binnings would produce trends whose axes agree
    // perfectly and whose contents are integrals over different ranges.
    TDirectory* schemeDir = RootIO::MakePath(outputFile.get(), {globalCfgs.binningName});
    RootIO::WriteBinningStamp(schemeDir, RootIO::kStampMult, multBinning);
    RootIO::WriteBinningStamp(schemeDir, RootIO::kStampPtPhi, ptPhiBinning);

    // Next to the trends and not in their errors: two trends that share the triggers - two species of this
    // task, or the same species in two files that comparison_task divides - share this uncertainty, and a
    // ratio would count it twice where it cancels. Whoever builds a total error adds it, once per class.
    // It could be derived from what is already stored (h2TriggerSignal in the PhiFitTask file, the phi
    // efficiency and hEventLoss in the corrections file), and it is written all the same: it holds choices -
    // which errors enter, the phi efficiency's not, how they combine - that a reader would otherwise have to
    // re-implement outside the framework. The trigger count itself is not written, for the opposite reason:
    // it is those inputs and a sum, with nothing to choose.
    RootIO::MakePath(outputFile.get(), baseLogicalPath)->cd();
    hNormalisationRelError->Write(nullptr, TObject::kOverwrite);

    // The spectra leave this task and, once the trends and ratios move to a task
    // of their own, this is what will say which method produced them.
    RootIO::WriteProvenance(outputFile.get(), globalCfgs.binningName, provenance);

    // =========================================================================
    // 2. Write Yield Ratios across species (into a dedicated "Ratios" directory)
    // =========================================================================
    if (!requestedRatios.empty()) {
      // Create or access the "Ratios" directory inside Extract1D/2D
      const std::vector<std::string> ratioPath = SectionPath({IONames::kRatiosDir});
      TDirectory* ratioDir = RootIO::MakePath(outputFile.get(), ratioPath);

      ratioDir->cd();

      for (const auto& ratioCfg : requestedRatios) {
        const std::optional<size_t> idxNum = AssocIndex(ratioCfg.num);
        const std::optional<size_t> idxDen = AssocIndex(ratioCfg.den);
        if (!idxNum || !idxDen) {
          Logger::Warn("{}: cannot compute the ratio {}/{}: one or both particles are not configured.", GetName(),
                       ratioCfg.num, ratioCfg.den);
          continue; // Skip this specific ratio and move to the next
        }

        const auto& num = assocParticles[*idxNum];
        const auto& den = assocParticles[*idxDen];

        double numScale = GetYieldScaleFactor(num.name);
        double denScale = GetYieldScaleFactor(den.name);

        const bool numHasExtrap = extrapolatedParticles.contains(num.name);
        const bool denHasExtrap = extrapolatedParticles.contains(den.name);
        const bool doExtrapRatio = numHasExtrap || denHasExtrap;

        std::string canvasName = IONames::ParticleRatioCanvas(num.name, den.name);
        TCanvas* canvasRatio = canvasManager.CreateCanvas(canvasName, std::format("Ratio Mult Trend {}", ratioCfg.label), ratioPath, 800, 600);
        canvasRatio->cd();

        TLegend* legend = CanvasManager::CreateLegend(0.7, 0.7, 0.9, 0.9, 2);

        for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
          double dyLimit = deltaYLimits[yIdx];
          const std::string dyTitleStr = BinningUtils::FormatDeltaY(dyLimit);

          std::string ratioMeasName = IONames::ParticleRatioMeasured(num.name, den.name, dyLimit);
          std::string ratioMeasTitle = std::format("Measured Ratio; Multiplicity percentile (%);{}", ratioCfg.label);
          std::unique_ptr<TH1> hRatioMeas = HistogramUtils::MakeRatioHist(h1MultTrends[*idxNum][yIdx].get(), h1MultTrends[*idxDen][yIdx].get(),
                                                                          ratioMeasName, ratioMeasTitle, numScale, denScale);

          ratioDir->cd();
          hRatioMeas->Write(nullptr, TObject::kOverwrite);

          std::unique_ptr<TH1> hRatioExtrap{nullptr};
          if (doExtrapRatio) {
            TH1* hNumTrend = numHasExtrap ? h1MultTrendsExtrap[*idxNum][yIdx].get() : h1MultTrends[*idxNum][yIdx].get();
            TH1* hDenTrend = denHasExtrap ? h1MultTrendsExtrap[*idxDen][yIdx].get() : h1MultTrends[*idxDen][yIdx].get();

            std::string ratioExtrapName = IONames::ParticleRatioExtrapolated(num.name, den.name, dyLimit);
            std::string ratioExtrapTitle = std::format("Extrapolated Ratio; Multiplicity percentile (%);{}", ratioCfg.label);
            hRatioExtrap = HistogramUtils::MakeRatioHist(hNumTrend, hDenTrend, ratioExtrapName, ratioExtrapTitle, numScale, denScale);

            ratioDir->cd();
            hRatioExtrap->Write(nullptr, TObject::kOverwrite);
          }

          CanvasManager::AdjustYRange({hRatioMeas.get(), hRatioExtrap.get()});

          canvasRatio->cd();

          TH1* cloneMeas = CanvasManager::DrawStyledCopy(hRatioMeas.get(), yIdx == 0 ? "" : "SAME", globalCfgs.GetMultTrendColor(yIdx), 24);
          legend->AddEntry(cloneMeas, std::format("Meas. |#Delta y| < {}", dyTitleStr).c_str(), "p");
          if (hRatioExtrap) {
            TH1* cloneExtrap = CanvasManager::DrawStyledCopy(hRatioExtrap.get(), "SAME", globalCfgs.GetMultTrendColor(yIdx), 20);
            legend->AddEntry(cloneExtrap, std::format("Extrap. |#Delta y| < {}", dyTitleStr).c_str(), "p");
          }
        }

        legend->Draw("SAME");
      }
    } else {
      Logger::Info("{}: No yield ratios requested. Skipping.", GetName());
    }

    // =========================================================================
    // 3. Save all registered canvases into their respective subdirectories
    // =========================================================================
    canvasManager.WriteAll(outputFile.get());

    // =========================================================================
    // 4. Close Files
    // =========================================================================
    if (outputFile) {
      outputFile->Close();
    }

    for (auto& file : projectionFiles) {
      file->Close();
    }

    for (auto& file : qaFiles) {
      file->Close();
    }

    Logger::Info("{}: DONE.", GetName());
  }

 protected:
  AnalysisSettings globalCfgs;

  bool applyME{false}, applyExtrapolation{false};
  bool useProjectionCache{false}, use2DEngine{false};
  std::optional<AnalysisStructures::CorrectionsConfig> corrections; // Empty: no correction is applied here

  std::unique_ptr<TH1> hEventLoss;

  std::vector<AnalysisStructures::AssocParticleConfig> assocParticles;

  // Filled in Init, written into the spectra file in Terminate
  std::map<std::string, std::string> provenance;
  // Per associated species, the mass window it was kept in online (CheckOnlineMassWindows):
  //  - no map (nullopt): not known, no train configuration (no device read); nothing can be compared.
  //  - a map without the species: the species has no mass window to have (the pion, cut online in n sigma).
  //  - at(name).value empty: known, no window. The device had select...InSigRegion off and counted the pairs at every
  //    mass of the species; a purity, integrated in a window, describes none of those outside it.
  //  - at(name).value = {min, max}: known, the window of select...InSigRegion and rangeM...Signal; a purity applied
  //    to these pairs must have been integrated in it (CorrelationTask::CheckPurityWindow).
  // at(name).source says where it was read (record, wagon, device), for the messages; the same on every device read,
  // or CheckOnlineMassWindows has stopped the run (TrainConfig::SameOnAllDevices).
  std::optional<std::map<std::string, TrainConfig::Sourced<std::optional<AnalysisStructures::Range>>>> onlineMassWindows;
  // The phi mass windows the pairs were split into signal and sideband with (CheckOnlineMassWindows), the same on every
  // device read; empty when not known (no train configuration) or for generator-level containers, which have none.
  // Compared by CorrelationTask with the windows of its triggers.
  std::optional<TrainConfig::Sourced<TrainConfig::PhiMassWindows>> onlinePhiMassWindows;
  // The selection the pion pairs were counted with (CheckOnlineMassWindows), the same on every device read; read only when
  // a pion map is applied here, and compared with it (CheckPionMapSelection). Empty otherwise, when not known (no train
  // configuration), or for generator-level containers.
  std::optional<TrainConfig::Sourced<TrainConfig::PionSelection>> onlinePionSelection;
  std::vector<AnalysisStructures::LoadedAssocData> loadedDataCollection;

  // Read from the input files in Init(), never from the configuration
  std::vector<double> multBinning;
  std::vector<double> ptPhiBinning;

  std::map<std::string, AnalysisStructures::LoadedCorrections> correctionCollection;

  // Always set from their required keys at Init: initialised only so that they are never indeterminate, not defaults.
  CorrelationCalculator::AxisTarget projectionAxis{CorrelationCalculator::AxisTarget::DeltaY_Y};
  CorrelationCalculator::MENormalization meNormalization{CorrelationCalculator::MENormalization::BinsAtZero};

  // The particles extrapolated: those with a section in extrapConfig.json, and none when applyExtrapolation is off.
  std::set<std::string> extrapolatedParticles;

  std::vector<std::vector<std::vector<std::unique_ptr<TH1>>>> h1PhiAssocNoPtPhi;

  std::vector<double> deltaYLimits;

  std::vector<std::vector<std::unique_ptr<TH1>>> h1MultTrends;
  std::vector<std::vector<std::unique_ptr<TH1>>> h1MultTrendsExtrap;
  std::unique_ptr<TH1> hNormalisationRelError; // filled in GenerateSpectraAndTrends, written in Terminate

  std::unique_ptr<ExtrapConfigManager> extrapConfigManager;

  std::unique_ptr<TFile> outputFile;
  std::vector<std::unique_ptr<TFile>> projectionFiles;
  std::vector<std::unique_ptr<TFile>> qaFiles;
  CanvasManager canvasManager;

  std::vector<AnalysisStructures::YieldRatioConfig> requestedRatios;

  // -------------------------------------------------------------------------
  // Hooks for derived-class-specific behavior in Terminate()
  // -------------------------------------------------------------------------
  // Hook for GenerateSpectraAndTrends: returns a purity correction histogram
  // for this particle/multBin, or nullptr if none should be applied.
  // CorrelationTask overrides this; CorrelationWPDGTask leaves the default.
  virtual TH1* GetPurityHist(const std::string& /*particle*/, int /*multBin*/) { return nullptr; }

  // Trigger yield, with its error, for a given (multBin, ptPhiBin), read by CountTriggers; background
  // ratio, read by RunLegacy(). Every derived task MUST provide GetTriggerSignal;
  // GetTriggerBkgRatio defaults to "no background subtraction" (WPDG's case).
  virtual std::pair<double, double> GetTriggerSignal(int multBin, int ptPhiBin) = 0;
  virtual double GetTriggerBkgRatio(int /*multBin*/, int /*ptPhiBin*/) { return 0.0; }

  // The (multiplicity, trigger pT) axes of whatever container GetTriggerSignal
  // reads from. Asked for as axes and not as a histogram because the two tasks
  // hold different things: a TH2 read from PhiFitTask in one case, a TH3
  // projected in place in the other.
  virtual std::pair<const TAxis*, const TAxis*> TriggerAxes() const = 0;

  // Content, which is the one thing the binning machinery structurally cannot see:
  // an empty histogram has impeccable axes, so every check in this framework passes
  // over it. Both tasks need this and for the same reason - every spectrum is divided
  // by a sum over this container, and if the sum is zero the normalisation is skipped
  // and raw counts are left in an object labelled as a per-trigger yield.
  //
  // It is a call the DERIVED task makes, not something GetUniqueOrThrow does, because
  // whether empty is impossible is a statement about the physics and not about the
  // file. A spectrum of a rare species in a small sample may legitimately be empty; a
  // trigger container may not.
  //
  // Tested on the contents and not on GetEntries(): PhiFitTask fills its trigger
  // matrices with SetBinContent, which leaves fEntries saying nothing about whether
  // any of them is non-zero. GetMaximum/GetMinimum work whichever way it was filled.
  // Not null-checked, like every other helper here that takes a histogram: the callers
  // get theirs from GetUniqueOrThrow, which has already refused a missing object.
  // A guard here would only be able to pass silently on a case that cannot arise.
  static void RequireNonEmptyTrigger(const TH1* trigger, std::string_view what,
                                     std::string_view origin, std::string_view remedy)
  {
    if (trigger->GetMaximum() != 0.0 || trigger->GetMinimum() != 0.0)
      return;

    throw std::runtime_error(std::format(
      "[FATAL] the trigger container {} in '{}' holds nothing but zeros. Its axes and binning are "
      "fine, which is why nothing else objects - but every spectrum is normalised by a sum over "
      "it, so with no triggers the normalisation is skipped and the spectra come out as raw "
      "counts rather than per-trigger yields. {}",
      what, origin, remedy));
  }

  // -------------------------------------------------------------------------
  // Shared JSON parsing for flags common to both derived Init() implementations.
  // Call this first thing from each derived Init().
  // -------------------------------------------------------------------------
  void InitCommonFlags(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings)
  {
    globalCfgs = globalSettings;

    applyME = JsonConfig::RequireBool(taskConfig, "apply_mixed_events", GetName());
    applyExtrapolation = JsonConfig::RequireBool(taskConfig, "apply_extrapolation", GetName());

    provenance = RootIO::StartProvenance(JsonConfig::Serialize(taskConfig));

    useProjectionCache = JsonConfig::RequireBool(taskConfig, "use_projection_cache", GetName());
    use2DEngine = JsonConfig::RequireBool(taskConfig, "use_2d_engine", GetName());
    // Required, with no default: it changes the numbers (CLAUDE.md, Configuration).
    meNormalization = JsonConfig::RequireEnum<CorrelationCalculator::MENormalization>(
      taskConfig, "me_normalization",
      {{"bins_at_zero", CorrelationCalculator::MENormalization::BinsAtZero},
       {"fit", CorrelationCalculator::MENormalization::Fit}},
      GetName());

    // Required, with no default: it changes the numbers (CLAUDE.md, Configuration).
    using AxisTarget = CorrelationCalculator::AxisTarget;
    projectionAxis = JsonConfig::RequireEnum<AxisTarget>(taskConfig, "projection_axis",
                                                         {{"delta_phi", AxisTarget::DeltaPhi_X}, {"delta_y", AxisTarget::DeltaY_Y}},
                                                         GetName());
    // Delta phi is supported in principle, the 2D engine projects on it, but a spectrum in Delta phi has never been
    // defined: each cell is integrated over |x| < delta_y_limits and GenerateSpectraAndTrends divides by 2 * limit, the
    // Delta y acceptance. Refused in every engine until that is decided (DESIGN_NOTES.md section 6, "Spectra in Delta
    // phi: to be looked at"; the owner, 30 October 2026); Supports, below, is the engine's rule once it is lifted.
    if (projectionAxis == AxisTarget::DeltaPhi_X)
      throw std::runtime_error(std::format(
        "[FATAL] {}: projection_axis 'delta_phi' is not supported yet: the spectra are integrated over |x| < "
        "delta_y_limits and normalised as for Delta y, which has no meaning in Delta phi. See DESIGN_NOTES.md "
        "section 6, \"Spectra in Delta phi: to be looked at\".",
        GetName()));
    Logger::Info("{}: Physics projection axis set to {}.",
                 GetName(), (projectionAxis == AxisTarget::DeltaPhi_X ? "Delta Phi (X)" : "Delta Y (Y)"));

    if (auto yieldRatios = JsonConfig::TryArray(taskConfig, "yield_ratios", GetName())) {
      int idx = 0;
      for (const auto& r : *yieldRatios) {
        // The index is in the context because the entries have no name of their
        // own: without it a missing key only says "somewhere in yield_ratios".
        const std::string ctx = std::format("{} yield_ratios[{}]", GetName(), idx++);
        JsonConfig::RequireKnownKeys(r, {"numerator", "denominator", "label"}, ctx);

        AnalysisStructures::YieldRatioConfig ratio;
        ratio.num = JsonConfig::RequireString(r, "numerator", ctx);
        ratio.den = JsonConfig::RequireString(r, "denominator", ctx);
        ratio.label = JsonConfig::OptionalString(r, "label", std::format("{}/{}", ratio.num, ratio.den), ctx);
        requestedRatios.push_back(ratio);
      }
    }

    // Required, with no default: the windows every spectrum and trend is integrated over (CLAUDE.md, Configuration).
    // Empty would meet "required" and compute nothing, with no error.
    deltaYLimits = JsonConfig::ReadNumberArray(JsonConfig::RequireArray(taskConfig, "delta_y_limits", GetName()),
                                               "delta_y_limits", GetName());
    if (deltaYLimits.empty())
      throw std::runtime_error(std::format("[FATAL] {}: 'delta_y_limits' is empty: no spectrum would be built.", GetName()));
  }

  // -------------------------------------------------------------------------
  // Which engine of CorrelationCalculator runs, decided once. A task says whether it can build
  // the 2D engine at all (CorrelationTask in run_mode "optimized"; RunLegacy, which the WPDG task
  // always runs, builds the 1D one); use_2d_engine asks for it. Everything that depends
  // on the engine - the calculators, SectionDirName, the checks below - asks Engine2D().
  // -------------------------------------------------------------------------
  virtual bool CanBuild2DEngine() const { return false; }
  bool Engine2D() const { return use2DEngine && CanBuild2DEngine(); }

  // Whether the containers read are ones the train can weight with the efficiency: not those at generator level (the
  // WPDG task with is_pure_gen, filled by processMCGenClosure without any weight), which nothing online corrects
  // whatever the train's options, and which no efficiency must correct here either.
  virtual bool ReadsOnlineWeightedContainers() const { return true; }

  // Whether a particle was weighted online, and by which train or file that is known.
  struct OnlineWeighting {
    bool weighted;
    std::string source;
  };

  // The particles of the analysis: the trigger and the associated species.
  std::vector<std::string> AnalysedParticles() const
  {
    std::vector<std::string> particles{"Phi"};
    for (const auto& p : assocParticles)
      particles.push_back(p.name);
    return particles;
  }

  // The inputs of the associated-particle containers, by role: the same event, and the mixed event when it is applied.
  // One table for the loading and for the online check, so that a role is read from the same keys in both.
  struct AssocInput {
    const char* fileKey;
    const char* basePathKey;
    const char* role;
  };

  std::vector<AssocInput> AssocInputs() const
  {
    std::vector<AssocInput> inputs{{"input_data_file", "base_path_data", "data"}};
    if (applyME)
      inputs.push_back({"input_me_file", "base_path_me", "mixed_event"});
    return inputs;
  }

  // The options of CorrelationCalculator, from the task's switches, built once for both run modes: a switch added to
  // Options and set in one place only would stay off in the other. doMoreQA is CorrelationTask's alone.
  CorrelationCalculator::Options CalculatorOptions(bool doMoreQA = false) const
  {
    return {.applyME = applyME, .useProjectionCache = useProjectionCache, .engine2D = Engine2D(), .doMoreQA = doMoreQA, .meNormalization = meNormalization, .projectionAxis = projectionAxis};
  }

  // The settings that need an engine this run will not build. Both used to pass in silence:
  // use_2d_engine true where only the 1D engine runs (the results went under
  // "Extract2D"), and projection_axis "delta_phi" with the 1D engine, which projects on Delta y
  // only and gave Delta y spectra (Delta phi in the 1D engine is open: DESIGN_NOTES.md section 6). Delta phi is, for
  // now, refused in every engine before this, in InitCommonFlags.
  // Refused at Init, before any data is loaded.
  void RequireEngineSettingsSupported() const
  {
    if (use2DEngine && !CanBuild2DEngine())
      throw std::runtime_error(std::format(
        "[FATAL] {}: use_2d_engine is true, but this run builds the 1D engine (run_mode 'legacy', "
        "or the WPDG task, which always does). Set it to false, or use run_mode 'optimized' for the 2D engine.",
        GetName()));
    if (!CorrelationCalculator::Supports(Engine2D(), projectionAxis))
      throw std::runtime_error(std::format(
        "[FATAL] {}: projection_axis 'delta_phi' needs the 2D engine (run_mode 'optimized' with "
        "use_2d_engine true); with this configuration the 1D engine runs, and it projects on "
        "Delta y only. See DESIGN_NOTES.md section 6.",
        GetName()));
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
      JsonConfig::RequireKnownKeys(sp, {"name", "dir_name", "me_mult"}, ctx);

      std::string particle = JsonConfig::RequireString(sp, "name", ctx);

      // The directory name is derived from the species unless the configuration
      // overrides it - see AssocParticleConfig for the rule. The binning stays
      // empty: it is resolved from the input files in ResolveBinningAndCache(),
      // once the containers are open.
      // me_mult: required, with no default, since it changes the numbers. The shape was found independent of
      // multiplicity in Delta y, with the 1D engine only (DESIGN_NOTES.md section 6).
      const auto& config = assocParticles.emplace_back(particle, RequireMultDependence(sp, "me_mult", ctx), JsonConfig::TryString(sp, "dir_name", ctx));
      if (config.meMult == AnalysisStructures::MultDependence::Integrated && Engine2D())
        throw std::runtime_error(std::format("[FATAL] {}: 'me_mult: integrated' is not supported with the 2D engine: the shape of the mixed "
                                             "event in (Delta phi, Delta y) has not been checked against multiplicity. Use 'me_mult: binned'.",
                                             ctx));
    }
  }

  // -------------------------------------------------------------------------
  // Shared efficiency loading + extrapolation config setup.
  // Requires assocParticles already populated.
  // -------------------------------------------------------------------------
  // The corrections block parsed, and compared with how the trains ran: right after the associated particles, which is
  // all they need, so that a block that cannot be right stops before the containers are loaded, and so that the train
  // configurations are in the provenance the projection cache writes. triggerPhi: how the triggers' phi was weighted
  // online, when the triggers come from a file of their own (CorrelationTask: phi_fit_task's); the WPDG task's come
  // from its own input, so from the device of its pairs, and it passes nothing.
  void InitCorrectionsAndCheckOnline(const rapidjson::Value& taskConfig, const std::optional<OnlineWeighting>& triggerPhi = std::nullopt)
  {
    InitCorrections(taskConfig);
    const TrainInputs inputs = LoadTrainInputs(taskConfig);
    CheckOnlineCorrections(inputs.devices, triggerPhi);
    CheckOnlineMassWindows(inputs.devices);
  }

  void LoadCorrectionsAndExtrapolation(const rapidjson::Value& taskConfig)
  {
    if (corrections)
      LoadCorrections();

    if (applyExtrapolation) {
      std::string extrapFile = JsonConfig::RequireString(taskConfig, "extrapolation_config_file", GetName());
      extrapConfigManager = std::make_unique<ExtrapConfigManager>(extrapFile);

      // Recorded only when extrapolation is on: a file that was not used has no
      // business in the provenance of a result it did not shape.
      provenance["extrap_config"] = JsonConfig::ReadFileText(extrapFile);
      Logger::Info("{}: Extrapolation configuration loaded successfully.", GetName());

      for (const auto& p : assocParticles) {
        if (extrapConfigManager->HasConfig(p.name)) {
          extrapolatedParticles.insert(p.name);
          extrapConfigManager->DeclareCells(p.name, multBinning);
        } else {
          Logger::Info("{}: No extrapolation config found for '{}'. Skipping.", GetName(), p.name);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // The "corrections" block: parsed once into CorrectionsConfig, after which the task
  // reads the structure and never the JSON. Requires assocParticles already populated.
  // See DESIGN_NOTES.md section 13 for the design and why every rule below exists.
  // -------------------------------------------------------------------------
  void InitCorrections(const rapidjson::Value& taskConfig)
  {
    corrections.reset();

    // Required, with null for none: see RequireObjectOrNull.
    const rapidjson::Value* node = JsonConfig::RequireObjectOrNull(taskConfig, "corrections", GetName());
    if (!node) {
      Logger::Info("{}: No corrections applied here (\"corrections\": null).", GetName());
      return;
    }

    const std::string ctx = std::format("{} corrections", GetName());

    AnalysisStructures::CorrectionsConfig cfg;
    // Outside the block on purpose: the file changes per year and the corrections per
    // mode, and with a top-level merge a year block can then set the file alone.
    cfg.file = JsonConfig::RequireString(taskConfig, "corrections_file", GetName());
    // Next to corrections_file for the same reason, set once in the base settings instead of in every block that writes
    // its corrections. Required: it decides whether results that cannot be right are produced at all, and 'warn' is
    // the owner's choice (28 October 2026), so that one map with an empty bin does not stop a run of many blocks.
    cfg.onInvalidBin = JsonConfig::RequireEnum<AnalysisStructures::InvalidBinPolicy>(
      taskConfig, "on_invalid_correction_bin", {{"warn", AnalysisStructures::InvalidBinPolicy::Warn}, {"stop", AnalysisStructures::InvalidBinPolicy::Stop}}, GetName());
    cfg.eventLoss = JsonConfig::RequireBool(*node, "event_loss", ctx);

    // Required, and {} when only the event loss is applied here: absent could equally be a
    // child block that replaced "corrections" and forgot its particles.
    const rapidjson::Value& particles = JsonConfig::RequireObject(*node, "particles", ctx);

    for (const auto& member : particles.GetObject()) {
      const std::string particle = member.name.GetString();
      const std::string pctx = std::format("{}.particles.{}", ctx, particle);

      RequireKnownParticle(particle, pctx);
      if (!member.value.IsObject())
        throw std::runtime_error(std::format("[FATAL] {}: must be an object.", pctx));

      const rapidjson::Value& entry = member.value;
      JsonConfig::RequireKnownKeys(entry, {"acceptance_efficiency", "signal_loss", "feed_down", "primary_fraction"}, pctx);

      // Species-specific corrections that do not exist yet. Refused, not ignored: a
      // configuration that asks for them must not produce numbers without them.
      auto refuseNotImplemented = [&](const char* key, std::initializer_list<std::string_view> appliesTo) {
        if (!JsonConfig::OptionalMember(entry, key))
          return;
        const bool applies = std::ranges::any_of(appliesTo, [&](std::string_view s) { return s == particle; });
        throw std::runtime_error(applies
                                   ? std::format("[FATAL] {}: '{}' is not implemented yet (DESIGN_NOTES.md section 13).", pctx, key)
                                   : std::format("[FATAL] {}: '{}' does not apply to {}.", pctx, key, particle));
      };
      refuseNotImplemented("feed_down", {"Lambda", "AntiLambda"});
      refuseNotImplemented("primary_fraction", {"Pi"});

      AnalysisStructures::ParticleCorrections pc;
      if (const rapidjson::Value* efficiencyNode = JsonConfig::TryObject(entry, "acceptance_efficiency", pctx)) {
        const std::string actx = std::format("{}.acceptance_efficiency", pctx);
        pc.efficiency = AnalysisStructures::EfficiencyCorrection{RequireMultDependence(*efficiencyNode, "mult", actx)};
      }
      if (JsonConfig::OptionalBool(entry, "signal_loss", false, pctx))
        pc.signalLoss = AnalysisStructures::SignalLossCorrection{};

      if (!pc.efficiency && !pc.signalLoss)
        throw std::runtime_error(std::format("[FATAL] {}: no correction is applied. To leave {} uncorrected, do not list it.", pctx, particle));
      if (!cfg.perParticle.emplace(particle, pc).second)
        throw std::runtime_error(std::format("[FATAL] {}: listed twice.", pctx));
    }

    if (cfg.perParticle.empty() && !cfg.eventLoss)
      throw std::runtime_error(std::format("[FATAL] {}: applies no correction. For none, write \"corrections\": null.", ctx));

    for (const auto& [particle, pc] : cfg.perParticle) {
      std::string what;
      if (pc.efficiency)
        what = std::format("acceptance x efficiency ({})", pc.efficiency->mult == AnalysisStructures::MultDependence::Binned ? "per multiplicity class" : "multiplicity-integrated");
      if (pc.signalLoss)
        what += what.empty() ? "signal loss" : " + signal loss";
      Logger::Info("{}: {} corrected for {}.", GetName(), particle, what);
    }
    Logger::Info("{}: Event loss {}.", GetName(), cfg.eventLoss ? "applied" : "not applied");

    corrections = std::move(cfg);
  }

  // -------------------------------------------------------------------------
  // The train configurations of the inputs (TrainConfig.h) and the devices this block reads in them, one per input and
  // per part of a sum. The devices point into the records, so the two travel together. One record per file, so that the
  // mixed event read from the data file (the usual case) loads, checks and warns once; the key is the path made
  // lexically normal, so that the same file written two ways is still one. Each input's record goes into the
  // provenance, or "none".
  // -------------------------------------------------------------------------
  struct TrainInputs {
    std::map<std::string, std::vector<TrainConfig::Record>> recordsByFile;
    std::vector<TrainConfig::Device> devices;
  };

  TrainInputs LoadTrainInputs(const rapidjson::Value& taskConfig)
  {
    TrainInputs inputs;
    for (const auto& input : AssocInputs()) {
      const std::string file = JsonConfig::RequireString(taskConfig, input.fileKey, GetName());
      const std::string basePath = JsonConfig::RequireString(taskConfig, input.basePathKey, GetName());
      const auto [loaded, isNew] = inputs.recordsByFile.try_emplace(std::filesystem::path(file).lexically_normal().string());
      if (isNew) {
        loaded->second = TrainConfig::Load(file, GetName());
      }
      const std::vector<TrainConfig::Device> inputDevices = TrainConfig::Devices(loaded->second, basePath, GetName());
      provenance[TrainConfig::ProvenanceKey(input.role)] = TrainConfig::Describe(inputDevices);
      inputs.devices.insert(inputs.devices.end(), inputDevices.begin(), inputDevices.end());
    }
    return inputs;
  }

  // -------------------------------------------------------------------------
  // How the trains of the inputs weighted the particles online, against what this block corrects offline (DESIGN_NOTES.md
  // sections 12 and 13). Every device read (the same event's and the mixed event's, in every part of a sum) must have run
  // in the DeltaY vs DeltaPhi mode, whose containers these are, and weighted the analysed particles alike. The triggers,
  // when they come from a file of their own (triggerPhi), are one more source of phi entries: weighted otherwise than
  // the pairs, the per-trigger yield is off by the phi efficiency, and the run stops. Then, per particle, one line says
  // where its efficiency is applied: online and offline both stops the run; neither is a warning for an associated
  // species, whose yields are then not corrected for it, and normal for the phi, which the no_phi blocks leave
  // uncorrected on purpose. Generator-level containers are not weighted online whatever the options, so nothing is
  // compared for them. Not checked: a signal loss applied online. The record says which map was read
  // (ccdbEfficiencyPath), not whether it was efficiency alone or efficiency x signal loss.
  // -------------------------------------------------------------------------
  void CheckOnlineCorrections(const std::vector<TrainConfig::Device>& devices, const std::optional<OnlineWeighting>& triggerPhi)
  {
    if (!ReadsOnlineWeightedContainers()) {
      Logger::Info("{}: Generator-level containers: not weighted online, whatever the train's options.", GetName());
      return;
    }
    for (const auto& device : devices) {
      TrainConfig::RequireDeltaYvsDeltaPhi(device);
    }
    const std::vector<std::string> analysed = AnalysedParticles();
    const std::set<std::string> analysedSet(analysed.begin(), analysed.end());

    // TEMPORARY (TrainConfig::MapErrorAddedPerEntry): said, since a reader of these errors would take them for the
    // statistical ones with the map's uncertainty in, which they are not. Once per device: the same event and the mixed
    // event usually read the same one.
    std::set<std::string> said;
    for (const auto& device : devices) {
      if (TrainConfig::MapErrorAddedPerEntry(device) && said.insert(TrainConfig::Source(device)).second) {
        Logger::Warn(
          "{}: the bin errors of {} include the uncertainty of the efficiency maps summed entry by entry "
          "(propagateEffError): that adds only about sqrt(1 + r^2) to the statistical error, and is not the maps' "
          "uncertainty, which adds linearly within a map bin (DESIGN_NOTES.md section 13). Until the "
          "productions are made again without it.",
          GetName(), TrainConfig::Source(device));
      }
    }

    // Per particle, how the pairs were weighted online; a particle is absent when that is not known (no train record)
    std::map<std::string, OnlineWeighting> weightedOnline;
    if (const auto pairs = TrainConfig::WeightedOnline(devices, analysedSet, GetName())) {
      for (const auto& name : analysed) {
        weightedOnline.emplace(name, OnlineWeighting{pairs->value.contains(name), pairs->source});
      }
    }
    const std::set<std::string> correctedOffline = corrections ? corrections->ParticlesWithEfficiency() : std::set<std::string>{};

    if (triggerPhi) {
      if (const auto pairsPhi = weightedOnline.find("Phi"); pairsPhi != weightedOnline.end()) {
        if (pairsPhi->second.weighted != triggerPhi->weighted) {
          throw std::runtime_error(std::format(
            "[FATAL] {}: the phi is {}weighted online in the pairs ({}) and {}in the triggers ({}): the per-trigger "
            "yield would be off by the phi efficiency. Point trigger_prefix at a phi_fit_task run on a subwagon weighted alike.",
            GetName(), pairsPhi->second.weighted ? "" : "not ", pairsPhi->second.source, triggerPhi->weighted ? "" : "not ",
            triggerPhi->source));
        }
      } else {
        // The pairs' weighting is not known, so it cannot be compared; the triggers' alone still says whether the phi
        // efficiency would be applied to them twice, since CountTriggers divides them by it.
        if (triggerPhi->weighted && correctedOffline.contains("Phi")) {
          throw std::runtime_error(std::format(
            "[FATAL] {}: the efficiency of Phi is applied online to the triggers ({}) and again offline here "
            "(corrections.particles.Phi.acceptance_efficiency): keep one of the two (DESIGN_NOTES.md section 13).",
            GetName(), triggerPhi->source));
        }
        Logger::Warn(
          "{}: the phi of the triggers is {}weighted online ({}), that of the pairs is not known (no train "
          "configuration): the pairs and the triggers are not compared.",
          GetName(), triggerPhi->weighted ? "" : "not ", triggerPhi->source);
      }
    }

    for (const auto& name : analysed) {
      const bool offline = correctedOffline.contains(name);
      const auto known = weightedOnline.find(name);
      if (known == weightedOnline.end()) {
        Logger::Info("{}: Efficiency of {}: {} offline; online: not known (no train configuration).", GetName(), name,
                     offline ? "applied" : "not applied");
        continue;
      }
      const auto& [online, source] = known->second;
      if (offline && online) {
        throw std::runtime_error(std::format(
          "[FATAL] {}: the efficiency of {} is applied online ({}) and again offline here "
          "(corrections.particles.{}.acceptance_efficiency): keep one of the two (DESIGN_NOTES.md section 13).",
          GetName(), name, source, name));
      }
      if (offline) {
        Logger::Info("{}: Efficiency of {}: applied offline (here).", GetName(), name);
      } else if (online) {
        Logger::Info("{}: Efficiency of {}: applied online ({}).", GetName(), name, source);
      } else if (name == "Phi") {
        Logger::Info("{}: Efficiency of {}: applied neither online nor offline.", GetName(), name);
      } else {
        Logger::Warn("{}: the efficiency of {} is applied neither online ({}) nor offline: its yields are not corrected for it.",
                     GetName(), name, source);
      }
    }
  }

  // The mass window each associated species was kept in online (TrainConfig::OnlineMassWindow), the same for every device
  // read, said in the log; compared with the purity windows by CorrelationTask (DESIGN_NOTES.md section 12). Generator-
  // level containers are filled without isAssocValid, so no window applies to them. The pion has none: its online cut
  // is in n sigma, with the producer's TOF rule (TrainConfig::OnlinePionSelection), read here as well and compared with
  // the pion map by CheckPionMapSelection. The phi windows too, signal and sideband (TrainConfig::OnlinePhiMassWindows):
  // the same on every device, or the pairs mix two selections, whichever task reads them; compared with the triggers
  // by CorrelationTask.
  void CheckOnlineMassWindows(const std::vector<TrainConfig::Device>& devices)
  {
    onlineMassWindows.reset();
    onlinePhiMassWindows.reset();
    onlinePionSelection.reset();
    if (!ReadsOnlineWeightedContainers() || devices.empty()) {
      return;
    }
    onlineMassWindows.emplace();
    onlinePhiMassWindows = TrainConfig::SameOnAllDevices(devices, TrainConfig::OnlinePhiMassWindows,
                                                         TrainConfig::DescribePhiMassWindows, "the phi mass windows", GetName());
    if (onlinePhiMassWindows) {
      Logger::Info("{}: Online phi mass windows: {} ({}).", GetName(), TrainConfig::DescribePhiMassWindows(onlinePhiMassWindows->value),
                   onlinePhiMassWindows->source);
    }
    for (const auto& p : assocParticles) {
      const TrainConfig::MassWindowOptions* options = TrainConfig::FindMassWindowOptions(p.name);
      if (!options) {
        continue;
      }
      const auto window = TrainConfig::SameOnAllDevices(
        devices, [&](const TrainConfig::Device& device) { return TrainConfig::OnlineMassWindow(device, *options); },
        TrainConfig::DescribeMassWindow, std::format("the mass window of {}", p.name), GetName());
      if (window) {
        Logger::Info("{}: Online mass window of {}: {} ({}).", GetName(), p.name, TrainConfig::DescribeMassWindow(window->value), window->source);
        (*onlineMassWindows)[p.name] = *window;
      }
    }
    if (corrections && corrections->ParticlesWithEfficiency().contains("Pi")) {
      onlinePionSelection = TrainConfig::OnlinePionSelection(devices, GetName());
      if (onlinePionSelection) {
        Logger::Info("{}: Online selection of the pion: {} ({}).", GetName(),
                     TrainConfig::DescribePionSelection(onlinePionSelection->value), onlinePionSelection->source);
      }
    }
  }

  // The selection the pion map was counted with, stamped by mc_task (TrainConfig::WritePionSelection), against the one
  // the pairs were counted with (onlinePionSelection). While no efficiency of the n sigma window is applied here, the
  // map must hold the window the pairs were counted in, and the producer's TOF rule must be the same: a map with
  // another selection corrects pairs it did not count, by a few percent and silently (DESIGN_NOTES.md section 13,
  // "The π"). Only for a map applied here; not known on either side is a warning, and nothing is compared.
  void CheckPionMapSelection(TFile* correctionsFile, const std::string& correctionsPath) const
  {
    if (!ReadsOnlineWeightedContainers()) {
      return; // generator-level containers: no selection online
    }
    const std::string where = std::format("corrections file '{}'", correctionsPath);
    const std::optional<TrainConfig::PionSelection> map =
      TrainConfig::ReadPionSelection(correctionsFile, globalCfgs.binningName, where, GetName());
    if (!map) {
      Logger::Warn("{}: the {} does not say which selection its pion map was counted with (written before 25 November 2026, "
                   "or from an MC without a train configuration): it is not compared with that of the pairs. Re-run mc_task.",
                   GetName(), where);
      return;
    }
    if (!onlinePionSelection) {
      Logger::Warn("{}: the selection the pion pairs were counted with is not known (no train configuration): the pion map "
                   "({}) is not compared with it.",
                   GetName(), TrainConfig::DescribePionSelection(*map));
      return;
    }
    if (*map != onlinePionSelection->value) {
      throw std::runtime_error(std::format(
        "[FATAL] {}: the pion map of the {} was counted with\n  {}\nand the pairs with\n  {} ({}).\nWhile no efficiency of the "
        "n sigma window is applied here, the map must hold the selection of the pairs. Re-run mc_task on an MC production "
        "whose phiStrangeCorrelation and pion-track-producer ran like those of the data (DESIGN_NOTES.md section 13, \"The π\").",
        GetName(), where, TrainConfig::DescribePionSelection(*map), TrainConfig::DescribePionSelection(onlinePionSelection->value),
        onlinePionSelection->source));
    }
    Logger::Info("{}: Pion map counted with the selection of the pairs.", GetName());
  }

  // -------------------------------------------------------------------------
  // Loading of what InitCorrections asked for: efficiency, signal loss, event loss.
  // -------------------------------------------------------------------------
  void LoadCorrections()
  {
    const AnalysisStructures::CorrectionsConfig& cfg = *corrections;
    const std::string& correctionsPath = cfg.file;
    std::unique_ptr<TFile> correctionsFile = RootIO::OpenOrThrow(correctionsPath, "READ", GetName());

    RootIO::PrintProvenance(correctionsFile.get(), globalCfgs.binningName,
                            std::format("corrections file '{}'", correctionsPath));

    RootIO::RequireMatchingBinningStamp(
      RootIO::FindPath(correctionsFile.get(), {globalCfgs.binningName}),
      RootIO::kStampMult, multBinning, std::format("corrections file '{}'", correctionsPath));

    // What a map corrects: its axis, and what an invalid bin of it does under on_invalid_correction_bin 'warn'.
    enum class MapOf { EventLoss,
                       Phi,
                       Associated };

    // LAMBDA HELPER 1: one map of the corrections file, 'name' in the directory 'path' (IONames, where MCTask
    // writes it). It returns the map with the analysis binning, or throws; a bin that is not positive is
    // on_invalid_correction_bin's.
    auto fetchHist = [&](MapOf of, const std::vector<std::string>& path, const std::string& name, bool apply,
                         const std::vector<double>& targetBinning) -> std::unique_ptr<TH1> {
      if (!apply)
        return nullptr;
      const char* axis = of == MapOf::EventLoss ? "multiplicity" : "pT";

      // By path from the file: a missing directory and a missing map are one message, naming the file and the path.
      std::unique_ptr<TH1> h = RootIO::GetUniqueOrThrow<TH1>(correctionsFile.get(), RootIO::ObjectPath(path, name), GetName());

      // Verified, NOT rebinned. An efficiency is a ratio, and merging bins of a
      // ratio sums them: two bins of 0.5 would give 1.0. A coarser efficiency can
      // only be built where numerator and denominator still exist, which is the
      // MC task - see 'rebinning_pt' in the MC configuration.
      const std::string diff = BinningUtils::Compare(targetBinning, BinningUtils::AxisEdges(h->GetXaxis()),
                                                     "analysis binning (data production)",
                                                     "correction map (MC production)");
      if (!diff.empty()) {
        throw std::runtime_error(std::format(
          "[FATAL] {}: '{}' in '{}' "
          "is not binned like the data being corrected:\n{}"
          "Re-run the production whose axis does not match; for a pT map, 'rebinning_pt' in the MC configuration "
          "sets the binning the efficiency is produced at.",
          GetName(), name, correctionsPath, diff));
      }

      // A bin at or below zero, or not a number, has no correct correction: it is not a correction of 1 (CLAUDE.md,
      // Physics). Every bin is looked at, since each is applied to a cell, and all of them are named.
      std::string invalid;
      for (int bin = 1; bin <= h->GetNbinsX(); ++bin)
        if (!(h->GetBinContent(bin) > 0.0))
          invalid += std::format(" {} at {} {};", h->GetBinContent(bin), axis, BinningUtils::BinLabel(targetBinning, bin - 1));
      if (invalid.empty())
        return h;

      if (cfg.onInvalidBin == AnalysisStructures::InvalidBinPolicy::Stop)
        throw std::runtime_error(std::format(
          "[FATAL] {}: '{}' in '{}' is{} and a correction that cannot be computed is not a correction of 1. The MC has "
          "nothing there: more MC statistics in that interval, or data and MC productions whose binning merges it.",
          GetName(), name, correctionsPath, invalid));

      // The event loss is skipped in those classes (GenerateSpectraAndTrends), the owner's choice of 28 October 2026;
      // the other maps are divided by as they are.
      const char* consequence =
        of == MapOf::EventLoss ? "not applied in those multiplicity classes, which are left without the event-loss correction"
        : of == MapOf::Phi     ? "applied as it is: the triggers and the pairs of those trigger-pT bins are divided by it, so no "
                                 "result of this task is right in the multiplicity classes the map applies to"
                               : "applied as it is: the cells it corrects are divided by it, and what they give is not a result";
      Logger::Warn("{}: '{}' in '{}' is{} (on_invalid_correction_bin: warn): {}", GetName(), name, correctionsPath,
                   invalid, consequence);
      // Next to the results as well, one line per map: the log is not kept with them, and they must say why.
      provenance["invalid_correction_bins"] += std::format("{}:{}\n", name, invalid);
      return h;
    };

    hEventLoss = fetchHist(MapOf::EventLoss, IONames::EventLossPath(globalCfgs.binningName), IONames::kEventLoss, cfg.eventLoss, multBinning);

    // LAMBDA HELPER 2: Clone and multiply (if both are needed) safely
    auto combineHists = [](const TH1* hEfficiency, const TH1* hSignalLoss, const std::string& name) -> std::unique_ptr<TH1> {
      if (!hEfficiency && !hSignalLoss)
        return nullptr;

      std::unique_ptr<TH1> hRes(static_cast<TH1*>(hEfficiency ? hEfficiency->Clone(name.c_str()) : hSignalLoss->Clone(name.c_str())));
      if (hEfficiency && hSignalLoss)
        hRes->Multiply(hSignalLoss);
      hRes->SetDirectory(0);

      return hRes;
    };

    correctionCollection.clear();

    for (const auto& [particle, pc] : cfg.perParticle) {
      AnalysisStructures::LoadedCorrections corr;
      corr.name = particle;
      corr.h1Corrections.resize(BinningUtils::NBins(multBinning));

      const std::vector<double>& targetBinning = PtBinningOf(particle, GetName());
      const MapOf of = particle == "Phi" ? MapOf::Phi : MapOf::Associated;
      // The efficiency is the map counted online (h4PiMCReco); the signal loss is not, and carries no selection
      if (particle == "Pi" && pc.efficiency) {
        CheckPionMapSelection(correctionsFile.get(), correctionsPath);
      }

      const bool applyEfficiencyBinned = pc.efficiency && pc.efficiency->mult == AnalysisStructures::MultDependence::Binned;
      const bool applyEfficiencyIntegrated = pc.efficiency && pc.efficiency->mult == AnalysisStructures::MultDependence::Integrated;

      std::unique_ptr<TH1> hEfficiencyIntegrated = fetchHist(of, IONames::CorrectionMapPath(globalCfgs.binningName, IONames::CorrectionMap::Efficiency), IONames::CorrectionMapName(particle, IONames::CorrectionMap::Efficiency, IONames::MultUnionTag(multBinning)), applyEfficiencyIntegrated, targetBinning);

      for (int multBin : BinningUtils::BinIndices(multBinning)) {
        const std::string multTag = IONames::MultTag(multBinning, multBin);
        std::unique_ptr<TH1> hEfficiencyBinned = fetchHist(of, IONames::CorrectionMapPath(globalCfgs.binningName, IONames::CorrectionMap::Efficiency), IONames::CorrectionMapName(particle, IONames::CorrectionMap::Efficiency, multTag), applyEfficiencyBinned, targetBinning);
        std::unique_ptr<TH1> hSignalLossBinned = fetchHist(of, IONames::CorrectionMapPath(globalCfgs.binningName, IONames::CorrectionMap::SignalLoss), IONames::CorrectionMapName(particle, IONames::CorrectionMap::SignalLoss, multTag),
                                                           pc.signalLoss.has_value(), // always per multiplicity class
                                                           targetBinning);

        corr.h1Corrections[multBin] = combineHists(hEfficiencyIntegrated ? hEfficiencyIntegrated.get() : hEfficiencyBinned.get(), hSignalLossBinned.get(),
                                                   std::format("h1Corrected_{}_{}", particle, multTag));
      }

      correctionCollection[particle] = std::move(corr);
    }

    Logger::Info("{}: Corrections loaded successfully.", GetName());
  }

  // -------------------------------------------------------------------------
  // Object naming for the cached projections.
  //
  // Bin RANGES are spelled out instead of bin indices. With an index, a cache
  // built from a different production would still contain "_ptBin7" and be read back
  // happily while meaning another pT interval; with the range in the name, the
  // lookup simply misses and the run stops. The name is the primary defence,
  // the binning stamp below only explains what went wrong.
  //
  // Both run modes go through here on purpose: they used to build the suffix
  // separately, with different conventions, so a cache produced by one was not
  // readable by the other.
  // -------------------------------------------------------------------------
  // Per (multiplicity, trigger pT, associated pT) cell.
  // One cell of the h5Phi{X}Data* containers, whose axes are (mult, pT(phi), pT(assoc), dy, dphi): its cut and its name.
  // No class: the union of the classes, the whole multiplicity axis.
  CorrelationCalculator::Cell DataCell(const AnalysisStructures::AssocParticleConfig& config, std::optional<int> multBin, int ptPhiBin, int ptAssocBin) const
  {
    return {{multBin ? HistogramUtils::AxisToCut::Bin(0, *multBin) : HistogramUtils::AxisToCut::Whole(0),
             HistogramUtils::AxisToCut::Bin(1, ptPhiBin), HistogramUtils::AxisToCut::Bin(2, ptAssocBin)},
            std::format("h1Phi{}Data{}", config.name, CellSuffix(config, multBin, ptPhiBin, ptAssocBin))};
  }

  std::string CellSuffix(const AnalysisStructures::AssocParticleConfig& config, std::optional<int> multBin, int ptPhiBin, int ptAssocBin) const
  {
    return std::format("_{}_ptPhi{}_{}",
                       IONames::MultTag(multBinning, multBin),
                       BinningUtils::BinLabel(ptPhiBinning, ptPhiBin),
                       IONames::PtTag(config.binning, ptAssocBin));
  }

  // For the objects accumulated over the trigger pT bins.
  std::string CellSuffix(const AnalysisStructures::AssocParticleConfig& config, int multBin, int ptAssocBin) const
  {
    return std::format("_{}", IONames::CellTag(multBinning, multBin, config.binning, ptAssocBin));
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
      TFile* cacheFile = projectionFiles[pIdx].get();
      const std::string ctx = std::format("{}, scheme '{}'", cacheFile->GetName(), globalCfgs.binningName);

      // Resolved for THIS particle, then merged into the common ones below.
      std::vector<double> mult;
      std::vector<double> ptPhi;
      const bool meIntegrated = particle.meMult == AnalysisStructures::MultDependence::Integrated;

      if (!useProjectionCache) {
        // h5Phi{X}Data* axes: (mult, pT(phi), pT(assoc), dy, dphi)
        const THnSparse* source = loadedDataCollection[pIdx].h5DataSignal.get();
        const std::string origin = source->GetName();

        mult = globalCfgs.ResolveMultBinning(source->GetAxis(0), origin);
        ptPhi = globalCfgs.ResolvePtBinning("Phi", source->GetAxis(1), origin);
        particle.binning = globalCfgs.ResolvePtBinning(particle.name, source->GetAxis(2), origin);

        TDirectory* schemeDir = RootIO::MakePath(cacheFile, {globalCfgs.binningName});

        // Projections from an earlier run under the SAME scheme name are still
        // there: if that run used a different binning, the ones whose bin range
        // no longer exists are not overwritten and would survive as garbage.
        const std::vector<double> previous = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampPtAssoc, ctx);
        if (!previous.empty()) {
          const std::string diff = BinningUtils::Compare(previous, particle.binning, "existing file", "current run");
          if (!diff.empty()) {
            throw std::runtime_error(std::format(
              "[FATAL] {}: {} already holds projections built with a different pT binning:\n{}"
              "Overwriting would leave projections of both binnings under the same scheme. Either give this "
              "binning a distinct 'binning_name', so the two live in separate directories, or delete the file.",
              GetName(), ctx, diff));
          }
        }

        RootIO::WriteBinningStamp(schemeDir, RootIO::kStampPtAssoc, particle.binning);
        RootIO::WriteBinningStamp(schemeDir, RootIO::kStampPtPhi, ptPhi);
        RootIO::WriteBinningStamp(schemeDir, RootIO::kStampMult, mult);
        // In the section, next to the mixed events it describes: Extract1D and Extract2D are built apart
        RootIO::WriteParameter(RootIO::MakePath(cacheFile, SectionPath()), RootIO::kStampMEMultIntegrated, meIntegrated);

        // The stamps say WITH WHAT BINNING these projections were built. They cannot
        // say WHAT THEY WERE BUILT FROM, and two productions can share a binning - so
        // a cache reused across them matches on every check there is and holds the
        // projections of the wrong input. Recording the config block is what makes
        // that answerable at all; ShowProvenance.C can also read it back, which on
        // these files it previously could not.
        //
        // Documentation and not a check, like everywhere else provenance is written:
        // nothing reads it on reuse yet. Turning it into a cache key - and with it
        // deciding automatically whether the cache is usable - is a separate step,
        // and one that must not land while files are being rebuilt. See DESIGN_NOTES.
        RootIO::WriteProvenance(cacheFile, globalCfgs.binningName,
                                provenance);
      } else {
        TDirectory* schemeDir = RootIO::FindPath(cacheFile, {globalCfgs.binningName});
        if (!schemeDir) {
          throw std::runtime_error(std::format(
            "[FATAL] {}: cache requested but scheme '{}' does not exist in {}. "
            "Run once with 'use_projection_cache': false.",
            GetName(), globalCfgs.binningName, cacheFile->GetName()));
        }

        particle.binning = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampPtAssoc, ctx);
        ptPhi = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampPtPhi, ctx);
        mult = RootIO::ReadBinningStamp(schemeDir, RootIO::kStampMult, ctx);

        if (particle.binning.empty() || ptPhi.empty() || mult.empty()) {
          throw std::runtime_error(std::format(
            "[FATAL] {}: {} carries no binning stamps, so the binning its projections "
            "were built with is unknown. Rebuild it with 'use_projection_cache': false.",
            GetName(), ctx));
        }

        // A cache section answers for the me_mult it was last built with. Without the stamp it predates the key and holds
        // the per-class mixed events only, which a run with the union's does not find (the names differ); without the
        // section, ProjectionCacheDir says so.
        if (const std::optional<bool> built =
              RootIO::TryReadParameter<bool>(RootIO::FindPath(cacheFile, SectionPath()), RootIO::kStampMEMultIntegrated, ctx, GetName());
            built && *built != meIntegrated)
          throw std::runtime_error(std::format(
            "[FATAL] {}: {} holds mixed events with {} = {}, and the me_mult of {} asks for {}. Rebuild it with "
            "'use_projection_cache': false.",
            GetName(), ctx, RootIO::kStampMEMultIntegrated, *built, particle.name, meIntegrated));

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
      for (auto& perMult : h1PhiAssocNoPtPhi[pIdx])
        perMult.resize(BinningUtils::NBins(assocParticles[pIdx].binning));
    }

    // 2. Initialize matrix for Mult Trends: [ParticleIndex][DeltaYIndex]
    hNormalisationRelError = std::make_unique<TH1D>(IONames::kNormalisationRelError.c_str(),
                                                    "Relative uncertainty of the per-trigger normalisation;Multiplicity Percentile (%);#sigma_{N}/N",
                                                    BinningUtils::NBins(multBinning), multBinning.data());
    hNormalisationRelError->SetDirectory(0);
    hNormalisationRelError->Sumw2(); // the bins hold numbers, not counts: errors 0, not sqrt(content)
    h1MultTrends.resize(assocParticles.size());
    if (applyExtrapolation) {
      h1MultTrendsExtrap.resize(assocParticles.size());
    }

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& p = assocParticles[pIdx];
      const bool doExtrapForThis = extrapolatedParticles.contains(p.name);

      const std::vector<std::string> spectraPath = SectionPath({p.name, IONames::kSpectraDir});

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];
        const std::string dyTitleStr = BinningUtils::FormatDeltaY(dyLimit);

        std::string hName = IONames::Trend(p.name, dyLimit);
        std::string hTitle = std::format("Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

        std::unique_ptr<TH1F> hTrend = std::make_unique<TH1F>(hName.c_str(), hTitle.c_str(), BinningUtils::NBins(multBinning), multBinning.data());
        hTrend->SetDirectory(0);
        h1MultTrends[pIdx].push_back(std::move(hTrend));

        if (doExtrapForThis) {
          std::string hExtrapName = IONames::TrendExtrapolated(p.name, dyLimit);
          std::string hExtrapTitle = std::format("Extrapolated Yield Trend {} |#Delta y| < {};Multiplicity Percentile (%);dN_{{{}}}/dy", p.name, dyTitleStr, p.name);

          std::unique_ptr<TH1F> hTrendExtrap = std::make_unique<TH1F>(hExtrapName.c_str(), hExtrapTitle.c_str(), BinningUtils::NBins(multBinning), multBinning.data());
          hTrendExtrap->SetDirectory(0);
          h1MultTrendsExtrap[pIdx].push_back(std::move(hTrendExtrap));
        }

        std::string cSpecName = IONames::SpectraCanvas(p.name, dyLimit);
        std::string cSpecTitle = std::format("Spectra {} |#Delta y| < {}", p.name, dyTitleStr);

        TCanvas* cSpec = canvasManager.CreateCanvas(cSpecName, cSpecTitle, spectraPath, 800, 600);
        cSpec->SetLogy();
        canvasManager.SetAutoYRange(cSpec);

        // The same spectra on an axis that reaches pT = 0, with the fitted model drawn
        // over each one. The measured canvas above starts at the first measured bin, so
        // the region the extrapolation is about - which is where a third of the yield
        // comes from - is precisely the part it cannot show.
        if (doExtrapForThis) {
          std::string cExtName = IONames::SpectraExtrapCanvas(p.name, dyLimit);
          std::string cExtTitle = std::format("Spectra with extrapolation {} |#Delta y| < {}", p.name, dyTitleStr);

          TCanvas* cExt = canvasManager.CreateCanvas(cExtName, cExtTitle, spectraPath, 800, 600);
          cExt->SetLogy();
          canvasManager.SetAutoYRange(cExt);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // Extrapolation function
  // -------------------------------------------------------------------------
  // 'overlay' is where the extended spectrum is drawn, one multiplicity bin per call,
  // and may be null. It is a parameter for the same reason 'targetDir' is: this
  // function knows the spectrum but not which particle or delta-y index it belongs to,
  // and the alternative - handing it the two indices so it can look the canvas up - is
  // giving it a way to reach objects that are none of its business.
  AnalysisStructures::ExtrapolationResult ExtrapolateSpectrum(TH1* hSpec, const AnalysisStructures::AssocParticleConfig& config, int multBin,
                                                              TDirectory* targetDir, TCanvas* overlay)
  {
    // 1. Fetch configuration for THIS particle and THIS multBin from JSON
    AnalysisStructures::ExtrapConfig eCfg = extrapConfigManager->GetConfig(config.name, IONames::MultTag(multBinning, multBin));
    eCfg.mass = config.mass; // Inject physical mass

    // 2. Generate configured TF1 model
    // Named after the spectrum by the factory (unique per particle, delta-y and class: see CreateModel).
    std::unique_ptr<TF1> extrapModel = ExtrapolationModelFactory::CreateModel(eCfg, hSpec->GetMaximum(), hSpec->GetName());

    // The separate fit of the high side, when the configuration asks for one.
    std::unique_ptr<TF1> highModel;
    if (eCfg.high)
      highModel = ExtrapolationModelFactory::CreateModel(eCfg.HighConfig(), hSpec->GetMaximum(),
                                                         std::format("high_{}", hSpec->GetName()));

    /*{
      // std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}", hSpec->GetName(), extrapFunction);
      std::string debugCanvasName = std::format("cDebug_InitialGuess_{}_{}_{}", hSpec->GetName(), eCfg.model, IONames::MultTag(multBinning, multBin));
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
    extrapolator.SetFitRange(eCfg.fitRange.min, eCfg.fitRange.max);
    // The limits especially: without this call the class kept its own 0..10 and
    // 'domain_range' looked like it controlled the integration while controlling
    // only the range of the TF1.
    extrapolator.SetExtrapolationLimits(eCfg.domainRange.min, eCfg.domainRange.max);
    extrapolator.SetFitOption(kFitOption);
    extrapolator.SetIntegrationPrecisions(kLoPrecision, kHiPrecision);
    if (highModel)
      extrapolator.SetHighModel(highModel.get(), eCfg.high->fitRange.min, eCfg.high->fitRange.max);

    // No SetSystematicSpectrum: nothing in this chain produces per-bin systematic
    // uncertainties yet, so the four variations stay switched off and the result
    // reports hasSystematics == false. This is the one line to change once a
    // systematics driver exists - see DESIGN_NOTES.md.

    AnalysisStructures::ExtrapolationResult res = extrapolator.Extrapolate();

    // 4. Report what happened. Reads nothing back into the result.
    ReportExtrapolation(hSpec, res, eCfg, extrapModel.get(), firstMeasuredPt);

    // 5. Create extended binning dynamically
    std::vector<double> extBinning;
    extBinning.push_back(0.0); // Lower limit for extrapolation
    for (double edge : config.binning) {
      extBinning.push_back(edge);
    }

    std::string extName = IONames::SpectrumExtended(hSpec->GetName());
    std::unique_ptr<TH1> hSpecExt = std::make_unique<TH1D>(extName.c_str(), extName.c_str(), BinningUtils::NBins(extBinning), extBinning.data());
    hSpecExt->SetDirectory(0);

    // 6. Copy measured contents (shifted by 1 bin to the right)
    for (int bin = 1; bin <= hSpec->GetNbinsX(); ++bin) {
      hSpecExt->SetBinContent(bin + 1, hSpec->GetBinContent(bin));
      hSpecExt->SetBinError(bin + 1, hSpec->GetBinError(bin));
    }

    // Leave the first bin empty (visual placeholder for the fit curve)
    hSpecExt->SetBinContent(1, 0.0);
    hSpecExt->SetBinError(1, 0.0);

    // The fitted curve, travelling with the spectrum it describes. Same colour as the
    // points, so that on the overlay below each curve is readable against its own
    // spectrum rather than against the one next to it.
    //
    // A copy, not Clone(). The models are TF1s built from C++ functions, and ROOT streams such
    // a TF1 as its values on GetNpx() points; Clone() goes through the streamer, so a clone is
    // already an interpolation of 100 points, in memory as in the file, and a SetNpx after it
    // changes nothing (TF1::Streamer, ROOT 6.36). The copy constructor keeps the function, so
    // SetNpx decides what the file gets. Measured on 20 October 2026, read back against the
    // live function: 100 points, 17 % off below 0.3 GeV/c and 2 % above (pi blast-wave), and
    // the integral over 0-0.2 GeV/c 6 % low; 1000 points, 0.16-0.18 % (by the points checked)
    // and 0.02 %. On the chain's own output: 0.26 % below 0.3 GeV/c, the integral 0.07 % low
    // (OldComparison/extrapolation/checks/savedCurves.cpp). The price is the file: a spectra
    // file goes from 0.46 to 1.7 MB, the canvases carrying the curves too.
    auto attachCurve = [&](const TF1* model, Style_t style, double from, double to) {
      auto curve = std::make_unique<TF1>(*model);
      curve->SetNpx(1000);
      curve->SetLineColor(globalCfgs.GetSpectraColor(multBin));
      curve->SetLineWidth(2);
      curve->SetLineStyle(style);
      curve->SetRange(from, to);
      hSpecExt->GetListOfFunctions()->Add(curve.release());
    };
    // With a high fit, each curve is drawn only on its side of where the high fit starts,
    // the high one dashed: drawn over the whole domain, the plot would show two tails on
    // each side and nothing would say which one entered the yield.
    const double split = eCfg.high ? eCfg.high->fitRange.min : eCfg.domainRange.max;
    attachCurve(extrapModel.get(), 1, eCfg.domainRange.min, split);
    if (highModel)
      attachCurve(highModel.get(), 2, split, eCfg.domainRange.max);

    targetDir->cd();
    hSpecExt->Write(nullptr, TObject::kOverwrite);

    // Drawn after the curve is attached, so DrawCopy carries it along: the copy
    // clones the function list too, which is what puts one curve per multiplicity bin
    // on this canvas without a second explicit draw.
    if (overlay) {
      overlay->cd();
      CanvasManager::DrawStyledCopy(hSpecExt.get(), multBin == 0 ? "" : "SAME", globalCfgs.GetSpectraColor(multBin));
    }

    return res;
  }

  // What the extrapolation of one spectrum has to say for itself. It computes nothing
  // that survives the call - which is the reason to give it a name of its own, since
  // that fact is invisible while it sits inline among the steps that do.
  //
  // 'model' must be the FITTED function, which it is: SpectrumExtrapolator fits in
  // place. It is passed rather than a precomputed number because the low-pT
  // cross-check below has to be taken HERE, by a different route from the one
  // Extrapolate() used - a number computed once and passed in would compare a value
  // with itself.
  static void ReportExtrapolation(const TH1* hSpec, const AnalysisStructures::ExtrapolationResult& res,
                                  const AnalysisStructures::ExtrapConfig& eCfg, TF1* model, double firstMeasuredPt)
  {
    const double rawIntegral = hSpec->Integral(1, hSpec->GetNbinsX(), "width");

    Logger::Debug("extrapolation of {}", hSpec->GetName());
    Logger::Debug("  -> Raw data integral:            {:g}", rawIntegral);
    Logger::Debug("  -> Yield after extrapolation:    {:g}", res.yield);
    Logger::Debug("  -> Extrapolated part:            {:g} ({:g}% of the yield)",
                  res.ExtrapolatedYield(), 100. * res.ExtrapolatedFraction());

    // Split by side, and printed next to an independent computation of the low one.
    // The two low numbers come by completely different routes - a sum over a histogram
    // of 0.01-wide bins, each filled with TF1::Integral, against one direct
    // TF1::Integral - so agreement says the whole extrapolation machinery works, and it
    // is how the silently-dropped extrapolation was found in the first place.
    //
    // They differ legitimately when the first declared bin is empty: the histogram
    // starts at the first NON-EMPTY edge while the direct integral starts at
    // config.binning[0]. If they differ and the first bin is filled, something is wrong.
    const double fitLowPtIntegral = firstMeasuredPt > 0.0 ? model->Integral(0.0, firstMeasuredPt) : 0.0;

    Logger::Debug("       low  {:g}   vs fitted function over [0, {:g}]: {:g}",
                  res.extrapolatedYieldLow, firstMeasuredPt, fitLowPtIntegral);
    Logger::Debug("       high {:g}", res.extrapolatedYieldHigh);

    // The four numbers to check against the reference if this ever has to be verified
    // again. Yield and mean are deterministic and must agree exactly; the two errors
    // are the RMS of 1000 toys and agree only to a few percent, so comparing them for
    // equality is chasing noise.
    Logger::Debug("  -> Yield:  {:g} +- {:g}   <pT>: {:g} +- {:g}",
                  res.yield, res.yieldStatErr, res.meanPt, res.meanPtStatErr);

    // A converged fit is not a good fit, and roughly a third of the yield above is an
    // integral of this function outside the measured range - so this line is not
    // decoration. It used to be printed only by YieldMean, which meant retiring that
    // path would have silently removed the only sign that the fit does not describe
    // the data. See DESIGN_NOTES.md, which still lists that as open.
    auto reportFit = [](const char* label, const AnalysisStructures::ExtrapFit& fit, const AnalysisStructures::FitQuality& quality) {
      Logger::Debug("  -> {} {} over [{:g}, {:g}]: chi2/ndf = {:g}/{} = {:g}", label, fit.model,
                    fit.fitRange.min, fit.fitRange.max, quality.chi2, quality.ndf, quality.ReducedChi2());
    };
    reportFit("Fit", eCfg, res.fit);
    if (eCfg.high)
      reportFit("High fit", *eCfg.high, *res.highFit);

    if (res.hasSystematics)
      Logger::Debug("  -> Systematics: yield +{:g} -{:g}   <pT> +{:g} -{:g}",
                    res.yieldSysHi, res.yieldSysLo, res.meanPtSysHi, res.meanPtSysLo);

    // The two being equal is not a coincidence worth leaving to the eye: it means
    // nothing was added, i.e. the extrapolation silently dropped out.
    if (res.yield == rawIntegral)
      Logger::Warn(
        "{}: the extrapolated yield equals the raw integral exactly - nothing was extrapolated. Check "
        "the fit above.",
        hSpec->GetName());
  }

  // -------------------------------------------------------------------------
  // Shared spectra construction, normalization, and extrapolation.
  // -------------------------------------------------------------------------
  void GenerateSpectraAndTrends(int multBin)
  {
    const auto [totalTriggerSignalPerMult, triggerError] = CountTriggers(multBin);
    double effectiveTriggers = totalTriggerSignalPerMult;
    double eventLossRelError = 0.0;

    // Normalize by the event loss correction if it is required. A class at or below zero is left without it, as before
    // 27 October 2026: only on_invalid_correction_bin 'warn' lets one through, and the loader has warned about it.
    if (hEventLoss) {
      const auto [eventLossCorr, eventLossError] = HistogramUtils::BinValueAndError(hEventLoss.get(), multBin);
      if (eventLossCorr > 0.0) {
        effectiveTriggers /= eventLossCorr;
        eventLossRelError = eventLossError / eventLossCorr;
      }
    }

    // The uncertainty of the normalisation: the trigger count and the event loss, in quadrature. One number
    // per class, since it scales every bin of every spectrum of the class alike (CLAUDE.md, Physics).
    const double triggerRelError = totalTriggerSignalPerMult > 0.0 ? triggerError / totalTriggerSignalPerMult : 0.0;
    const double normalisationRelError = std::hypot(triggerRelError, eventLossRelError);
    hNormalisationRelError->SetBinContent(BinningUtils::RootBin(multBin), normalisationRelError);

    // The single number every spectrum of this multiplicity class is divided by. A
    // per-trigger yield that comes out orders of magnitude off, or identical across
    // classes, is a statement about THIS number and about nothing further downstream -
    // but with the number invisible, the first place it could be noticed was the shape
    // of a canvas.
    //
    // Two classes sharing a value is the signature of trigger cells being read by the
    // wrong index; a value that does not fall with the multiplicity percentile is the
    // signature of the wrong container.
    Logger::Info("{}: mult {} -> triggers = {:g} +- {:g}, normalisation uncertainty {:g}%", GetName(),
                 BinningUtils::BinLabel(multBinning, multBin), totalTriggerSignalPerMult, triggerError,
                 100.0 * normalisationRelError);

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      const auto& config = assocParticles[pIdx];

      // Per particle, alongside the Trends written in Terminate(). Resolved inside
      // the loop and not once outside it, because the directory now depends on which
      // species this iteration is producing.
      TDirectory* targetSpectraDir = RootIO::MakePath(outputFile.get(), SectionPath({config.name, IONames::kSpectraDir}));
      const bool doExtrapForThis = extrapolatedParticles.contains(config.name);

      for (size_t yIdx = 0; yIdx < deltaYLimits.size(); ++yIdx) {
        double dyLimit = deltaYLimits[yIdx];

        std::string spectraName = IONames::Spectrum(config.name, dyLimit, multBinning, multBin);

        std::vector<const TH1*> views;
        views.reserve(h1PhiAssocNoPtPhi[pIdx][multBin].size());
        for (const auto& h : h1PhiAssocNoPtPhi[pIdx][multBin])
          views.push_back(h.get());

        // Construct the 1D spectrum from the accumulated Pt bins
        std::unique_ptr<TH1> h1Spectrum = HistogramUtils::ConstructSpectrum(views, config.binning, spectraName, dyLimit);

        // Normalize by the total number of triggers AND the Delta Y phase space
        if (totalTriggerSignalPerMult > 0.0) {
          h1Spectrum->Scale(1.0 / (effectiveTriggers * 2.0 * dyLimit));
        } else if (h1Spectrum->GetMaximum() != 0.0 || h1Spectrum->GetMinimum() != 0.0) {
          // Zero triggers with a NON-EMPTY spectrum is a contradiction, not a sparse
          // sample. This is a correlation analysis: the associated container is filled
          // per trigger, so every pair in that spectrum came from a phi the trigger
          // container says does not exist. The two disagree, and since the trigger is
          // read by (multiplicity, pT) bin index, the way they come to disagree is that
          // one of those indices addresses the wrong cell.
          //
          // Fatal, because the alternative is what used to happen: the scaling is
          // skipped and RAW COUNTS are left in an object whose axis title says
          // 1/N_trig dN/dpT dDeltaY. It reads as a spectrum, plots as a spectrum, and
          // is orders of magnitude away from one - which is how a trigger container
          // indexed the wrong way round survived until somebody looked at a canvas.
          throw std::runtime_error(std::format(
            "[FATAL] {}: '{}' has entries but the trigger count for multiplicity class {} is zero. "
            "In a correlation analysis the associated pairs exist only where a trigger did, so the "
            "two containers contradict each other - the trigger yields are being read from the "
            "wrong cells. Check that the trigger container and the associated data share the "
            "multiplicity and trigger-pT binnings.",
            GetName(), spectraName, BinningUtils::BinLabel(multBinning, multBin)));
        }
        // Both empty: nothing happened in this class, and nothing to say about it.

        // Purity correction hook: no-op unless overridden (CorrelationTask does).
        if (TH1* hPurity = GetPurityHist(config.name, multBin)) {
          h1Spectrum->Multiply(hPurity);
        }

        // Draw and Write
        std::string cSpecName = IONames::SpectraCanvas(config.name, dyLimit);
        canvasManager.RequireCanvas(cSpecName)->cd();
        CanvasManager::DrawStyledCopy(h1Spectrum.get(), multBin == 0 ? "" : "SAME", globalCfgs.GetSpectraColor(multBin));

        targetSpectraDir->cd();
        h1Spectrum->Write(nullptr, TObject::kOverwrite);

        HistogramUtils::FillTrendFromSpectrum(h1MultTrends[pIdx][yIdx].get(), h1Spectrum.get(), multBin);

        // Extrapolate and Fill Trend
        if (doExtrapForThis) {
          std::string cExtName = IONames::SpectraExtrapCanvas(config.name, dyLimit);
          TCanvas* overlay = canvasManager.GetCanvas(cExtName);
          AnalysisStructures::ExtrapolationResult res = ExtrapolateSpectrum(h1Spectrum.get(), config, multBin, targetSpectraDir, overlay);
          HistogramUtils::FillTrendFromExtrapolation(h1MultTrendsExtrap[pIdx][yIdx].get(), res, multBin);
        }
      }
    }
  }

  // -------------------------------------------------------------------------
  // The run with the 1D engine and no signal cache: CorrelationWPDGTask's Run(), and CorrelationTask's in run_mode
  // "legacy". The data-source-specific part comes from the GetTriggerSignal/GetTriggerBkgRatio hooks.
  // -------------------------------------------------------------------------
  void RunLegacy()
  {
    Logger::Info("{}: RUNNING CORRELATIONS (Legacy)...", GetName());
    CorrelationCalculator corrCalculator(CalculatorOptions());

    for (int multBin : BinningUtils::BinIndices(multBinning)) {
      AccumulateSignals(corrCalculator, multBin);
      GenerateSpectraAndTrends(multBin);
    }
  }

  // The corrected signal of every cell of one multiplicity class, summed over the trigger pT into h1PhiAssocNoPtPhi:
  // the loop of both run modes (RunLegacy, CorrelationTask::RunOptimized), written once so that a change to the cell,
  // its correction or its names is not made in one and forgotten in the other. The QA objects, which only the 2D
  // engine writes, go into the QA files, open only with do_more_qa.
  void AccumulateSignals(const CorrelationCalculator& corrCalculator, int multBin)
  {
    const std::vector<std::string> logicalPath = SectionPath();
    const bool withQA = !qaFiles.empty() && Engine2D();
    for (int ptPhiBin : BinningUtils::BinIndices(ptPhiBinning)) {
      double triggerBkgRatio = GetTriggerBkgRatio(multBin, ptPhiBin);
      double phiCorrection = Correction("Phi", multBin, ptPhiBin);

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        const auto& config = assocParticles[pIdx];
        const auto& data = loadedDataCollection[pIdx];

        TDirectory* targetDir = ProjectionCacheDir(pIdx, logicalPath);

        TDirectory* qaDir = withQA ? RootIO::MakePath(qaFiles.at(pIdx).get(), logicalPath) : nullptr;

        for (int ptAssocBin : BinningUtils::BinIndices(config.binning)) {
          // The same event's cell and the mixed event's, which differ in the class alone. With me_mult integrated the
          // union's mixed event is projected again for every class, the same object each time: a cost, not a difference.
          const CorrelationCalculator::Cell cell = DataCell(config, multBin, ptPhiBin, ptAssocBin);
          const CorrelationCalculator::Cell meCell = DataCell(config, config.MEMultBin(multBin), ptPhiBin, ptAssocBin);

          double assocCorrection = Correction(config.name, multBin, ptAssocBin);
          double totalCorrection = phiCorrection * assocCorrection;

          std::unique_ptr<TH1> h1FinalSignal = corrCalculator.ExtractCorrectedSignal(
            data, {.cell = cell, .meCell = meCell, .totalCorrection = totalCorrection, .triggerBkgRatio = triggerBkgRatio, .ioDir = targetDir, .qaDir = qaDir});

          if (ptPhiBin == 0) {
            std::string accumName = std::format("h1Phi{}DataSignal{}", config.name, CellSuffix(config, multBin, ptAssocBin));
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin].reset(static_cast<TH1*>(h1FinalSignal->Clone(accumName.c_str())));
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->SetDirectory(0);
          } else {
            h1PhiAssocNoPtPhi[pIdx][multBin][ptAssocBin]->Add(h1FinalSignal.get());
          }
        }
      }
    }
  }

  // The correction applied here to one particle in one (multiplicity class, pT bin) - the trigger pT for the phi -
  // and 1 where none is: efficiency x signal loss, the factor the counts are DIVIDED by. The one reader of the loaded corrections: the phi's divides both the triggers
  // (CountTriggers) and the pairs (totalCorrection), and the per-trigger yield is corrected only if the two use the same
  // number.
  double Correction(const std::string& particle, int multBin, int ptBin) const
  {
    auto it = correctionCollection.find(particle);
    const TH1* h = it == correctionCollection.end() ? nullptr : it->second.h1Corrections[multBin].get();
    return h ? h->GetBinContent(BinningUtils::RootBin(ptBin)) : 1.0;
  }

  // The efficiency-corrected trigger count of one multiplicity class, the sum over the trigger pT bins, with its
  // uncertainty from the phi fits: one independent fit per bin, so the errors add in quadrature. The uncertainty of
  // the phi efficiency is left out on purpose: the pairs of each trigger pT bin are divided by the same efficiency,
  // so in the per-trigger yield it largely cancels; what is left was measured: at most 0.01% of the pi and K0S
  // trends, 0.03% of the Xi, 0.06% of the Omega (DESIGN_NOTES.md section 13, point 3).
  // The whole error of the fit, as independent of the pairs: the conservative choice, until an open question is
  // answered (QUESTIONS_FOR_EXPERTS.md, "The uncertainty of the trigger count").
  std::pair<double, double> CountTriggers(int multBin)
  {
    double count = 0.0, variance = 0.0;
    for (int ptPhiBin : BinningUtils::BinIndices(ptPhiBinning)) {
      const auto [signal, error] = GetTriggerSignal(multBin, ptPhiBin);
      const double phiCorrection = Correction("Phi", multBin, ptPhiBin);
      count += signal / phiCorrection;
      variance += (error / phiCorrection) * (error / phiCorrection);
    }
    return {count, std::sqrt(variance)};
  }

  // Which of the two sections (Extract1D, Extract2D) this task writes into. Derived rather
  // than stored: it follows Engine2D(), the engine that runs and not the key, so the
  // directory says how the results were made. The same ternary used to be spelled out in
  // four places - four chances for one of them to be edited alone.
  std::string SectionDirName() const { return Engine2D() ? IONames::kExtract2D : IONames::kExtract1D; }

  // The files both correlation tasks write, opened in one way: PhiAssocSpectra.root, and per associated particle
  // the projection file (the cache) and, when asked, a QA file.
  void OpenOutputFiles(const rapidjson::Value& taskConfig, bool withQA)
  {
    // One key for both tasks (input_output_prefix, the WPDG spelling, was renamed on 30 October 2026): it names the
    // file whose section is cleared below.
    const std::string outputPrefix = JsonConfig::RequireString(taskConfig, "output_prefix", GetName());

    const std::string projectionsDir = JsonConfig::RequireString(taskConfig, "projections_dir", GetName());
    const std::string finalDir = JsonConfig::RequireString(taskConfig, "output_dir_final", GetName());
    outputFile = RootIO::OpenOrThrow(std::format("{}{}{}", finalDir, outputPrefix, IONames::kSpectraFile), "UPDATE", GetName());

    // UPDATE and not RECREATE here too, and for a different reason: the cache is
    // expensive to rebuild, so a scheme directory that already holds projections of
    // another binning must be REFUSED rather than silently replaced. That guard lives
    // in ResolveBinningAndCache and only has something to guard if the file survives.
    const char* projMode = ProjectionFilesReadOnly() ? "READ" : "UPDATE";
    for (const auto& p : assocParticles) {
      projectionFiles.push_back(
        RootIO::OpenOrThrow(std::format("{}{}{}", projectionsDir, outputPrefix, IONames::ProjectionFile(p.name)), projMode, GetName()));
      if (withQA)
        qaFiles.push_back(
          RootIO::OpenOrThrow(std::format("{}{}Phi{}QAHistograms.root", projectionsDir, outputPrefix, p.name), "RECREATE", GetName()));
    }
  }

  // The projection files are read, not written, when the projections come from them: the cache is not rebuilt.
  bool ProjectionFilesReadOnly() const { return useProjectionCache; }

  // The last step of Init, once the whole configuration has been read (the extrapolation classes included), so that a
  // mistake in it does not cost the previous production. This run rewrites its whole section directory, so whatever
  // a previous one left there - produced when the layout or the object names were different - would sit beside the
  // new objects with nothing to tell them apart. UPDATE and not RECREATE because only this subtree is ours: another
  // 'binning_name', or the other Extract1D/2D, belongs to somebody else and stays.
  void ClearOwnSection() { RootIO::ClearPath(outputFile.get(), SectionPath()); }

  // Where the associated particle of that name is in assocParticles, if it is one: the configuration names particles.
  std::optional<size_t> AssocIndex(std::string_view name) const
  {
    const auto it = std::ranges::find(assocParticles, name, &AnalysisStructures::AssocParticleConfig::name);
    if (it == assocParticles.end())
      return std::nullopt;
    return static_cast<size_t>(it - assocParticles.begin());
  }

  // The pT binning a correction or a purity of 'particle' is verified against: the trigger's for "Phi", the
  // particle's own for an associated one; any other name stops the run.
  const std::vector<double>& PtBinningOf(const std::string& particle, std::string_view errCtx) const
  {
    if (particle == "Phi")
      return ptPhiBinning;
    if (const std::optional<size_t> pIdx = AssocIndex(particle))
      return assocParticles[*pIdx].binning;
    throw std::runtime_error(std::format("[FATAL] {}: '{}' is neither Phi nor one of the associated_particles.", errCtx, particle));
  }

  // A correction or a purity names its particle: the trigger, "Phi", or one of the associated particles.
  void RequireKnownParticle(const std::string& particle, std::string_view errCtx) const { (void)PtBinningOf(particle, errCtx); }

  // The path of this task's section, {binning_name, section}, and of what is below it:
  // the layout spelled once, not at every call.
  std::vector<std::string> SectionPath(std::initializer_list<std::string> below = {}) const
  {
    std::vector<std::string> path{globalCfgs.binningName, SectionDirName()};
    path.insert(path.end(), below);
    return path;
  }

  // The directory of one particle's projections. With use_projection_cache on it is looked
  // up, not created, and it is missing when the cache was never built for this binning and
  // section (Extract1D and Extract2D are separate, see SectionDirName). The calculator, given
  // the null directory, could only say that none was provided: this says which file, which
  // directory, and what to do.
  TDirectory* ProjectionCacheDir(size_t pIdx, const std::vector<std::string>& logicalPath) const
  {
    TFile* file = projectionFiles[pIdx].get();
    if (!useProjectionCache)
      return RootIO::MakePath(file, logicalPath);
    if (TDirectory* dir = RootIO::FindPath(file, logicalPath))
      return dir;
    throw std::runtime_error(std::format(
      "[FATAL] {}: use_projection_cache is on, but '{}' has no '{}': the projections of {} were never cached "
      "for this binning and section ({}). Run once with use_projection_cache false to build the cache.",
      GetName(), RootIO::FileNameOf(file), RootIO::MakeDirPath(logicalPath), assocParticles[pIdx].name, SectionDirName()));
  }

  // Where an associated particle's container is in the O2Physics output: "<base><dirName>/h5Phi<name><kind>", with
  // 'kind' as the producer names it (DataSignal, DataMESideband, ClosureMCGen, ...). The rule changes when O2Physics
  // renames a directory, so it is written here once for both tasks.
  static std::string AssocContainerPath(std::string_view basePath, const AnalysisStructures::AssocParticleConfig& config,
                                        std::string_view kind)
  {
    return std::format("{}{}/h5Phi{}{}", basePath, config.dirName, config.name, kind);
  }

  // The containers a task reads per associated particle, each by its 'kind' in AssocContainerPath. The signal and,
  // with mixed events, its mixed event are always read; the sideband and its mixed event are a pair a task may leave
  // empty (the WPDG task has no sideband), and their pointers in LoadedAssocData then stay null.
  struct AssocContainerKinds {
    std::string_view signal;
    std::string_view sideband;
    std::string_view meSignal;
    std::string_view meSideband;
  };

  // loadedDataCollection, one entry per associated particle in their order. The containers come from the data file
  // and, when applyME, from the mixed-event file, each opened here with its base path; with use_projection_cache
  // nothing is opened or read, and the entry holds the name alone. (The WPDG task opens the data file once more, for
  // its trigger: read-only, and cheaper than two ways of handing a file over.)
  void LoadAssocData(const rapidjson::Value& taskConfig, const AssocContainerKinds& kinds)
  {
    const bool readContainers = !useProjectionCache;
    Logger::Info("{}: {}", GetName(), readContainers ? "Cache DISABLED. Loading heavy THnSparse data..." : "Cache ENABLED. Skipping THnSparse loading.");

    std::unique_ptr<TFile> inputFile, inputMEFile;
    std::string basePath, basePathME;
    if (readContainers) {
      // AssocInputs: the same event first, then the mixed event when it is applied
      const std::vector<AssocInput> inputs = AssocInputs();
      auto open = [&](const AssocInput& input, std::unique_ptr<TFile>& file, std::string& path) {
        file = RootIO::OpenOrThrow(JsonConfig::RequireString(taskConfig, input.fileKey, GetName()), "READ", GetName());
        path = JsonConfig::RequireString(taskConfig, input.basePathKey, GetName());
      };
      open(inputs.front(), inputFile, basePath);
      if (applyME)
        open(inputs.back(), inputMEFile, basePathME);
    }

    auto read = [&](TFile* file, std::string_view path, const AnalysisStructures::AssocParticleConfig& config, std::string_view kind) {
      return RootIO::GetUniqueOrThrow<THnSparseF>(file, AssocContainerPath(path, config, kind), GetName());
    };

    for (const auto& config : assocParticles) {
      AnalysisStructures::LoadedAssocData data;
      data.name = config.name;
      if (readContainers) {
        data.h5DataSignal = read(inputFile.get(), basePath, config, kinds.signal);
        // The others are cut with the signal's bin indices: read, then required to have its axes
        auto readLikeSignal = [&](TFile* file, std::string_view path, std::string_view kind) {
          auto container = read(file, path, config, kind);
          RequireSameAxesAsSignal(data.h5DataSignal.get(), inputFile->GetName(), container.get(), file->GetName());
          return container;
        };
        if (!kinds.sideband.empty())
          data.h5DataSideband = readLikeSignal(inputFile.get(), basePath, kinds.sideband);
        if (applyME) {
          data.h5DataMESignal = readLikeSignal(inputMEFile.get(), basePathME, kinds.meSignal);
          if (!kinds.meSideband.empty())
            data.h5DataMESideband = readLikeSignal(inputMEFile.get(), basePathME, kinds.meSideband);
        }
      }
      loadedDataCollection.push_back(std::move(data));
    }
  }

  // A per-class or multiplicity-integrated choice, spelled the same in every key that makes one
  static AnalysisStructures::MultDependence RequireMultDependence(const rapidjson::Value& node, const char* key, std::string_view errCtx)
  {
    return JsonConfig::RequireEnum<AnalysisStructures::MultDependence>(
      node, key, {{"binned", AnalysisStructures::MultDependence::Binned}, {"integrated", AnalysisStructures::MultDependence::Integrated}}, errCtx);
  }

  // The sideband and the mixed events are cut with the bin indices of the signal (DataCell) and divided by or
  // subtracted from it bin by bin; the binnings are resolved from the signal alone (ResolveBinningAndCache). A
  // container with other axes would be read from the wrong cells, and TH1::Divide does not stop on different edges.
  // The most likely cause is a mixed event of another production, which comes from its own file.
  static void RequireSameAxesAsSignal(const THnSparse* signal, std::string_view signalFile, const THnSparse* other, std::string_view otherFile)
  {
    if (other->GetNdimensions() != signal->GetNdimensions())
      throw std::runtime_error(std::format("[FATAL] '{}' in '{}' has {} axes and '{}' in '{}' {}: they are cut and combined bin by bin.",
                                           signal->GetName(), signalFile, signal->GetNdimensions(), other->GetName(), otherFile,
                                           other->GetNdimensions()));
    for (int axis = 0; axis < signal->GetNdimensions(); ++axis) {
      const std::string_view title = signal->GetAxis(axis)->GetTitle();
      const std::string what = title.empty() ? std::format("axis {}", axis) : std::format("axis {} ({})", axis, title);
      BinningUtils::RequireSameAxis(signal->GetAxis(axis), other->GetAxis(axis), std::format("{} {} in '{}'", signal->GetName(), what, signalFile),
                                    std::format("{} {} in '{}'", other->GetName(), what, otherFile));
    }
  }

  static double GetYieldScaleFactor(const std::string& name)
  {
    return (name == "K0S") ? 2.0 : 1.0;
  }
};

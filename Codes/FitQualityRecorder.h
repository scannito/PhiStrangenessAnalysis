#pragma once

#include "AnalysisDataStructures.h"
#include "Logger.h"

#include "TDirectory.h"
#include "TTree.h"

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The quality of every fit a task runs, one row per fit in a TTree written at {binning_name}/FitQuality of the
// task's output file (PhiFitTask, PurityTask). A table and not a TNamed per fact: the rows all have the same typed
// columns, the analyst asks it questions (FitQuality->Scan("section:ptLow:covQual", "covQual<3")), and a missing
// column is an error instead of a key absent from one row. Raw facts only, no verdict: what counts as a bad fit is
// Reasons below, one public function, used by the warnings now and by any reader that excludes cells later.
//
// Nothing here touches a fit: everything comes from what the fit already records (DynamicRooFitter::Diagnostics, its
// RooFitResult and, for the NLL and the start, DoFit). The tree is detached, filled in memory and written like a histogram, so it is
// owned here and not by whatever gDirectory was when it was made; a few thousand rows cost nothing.
//
// atLowerLimit and atUpperLimit are lists, and TTree::Scan shows one line per element: a row with no parameter at a
// limit has none, and does not appear in a Scan that asks for that column (measured: with it, a selection of 70 rows
// printed 0). "@atLowerLimit.size()" is a column of one value per row, and keeps them all.
class FitQualityRecorder
{
 public:
  static inline const std::string kTreeName{"FitQuality"};
  // The two binnings a fit can be at: PurityTask fits both when rebinning_pt merges bins.
  static inline const std::string kAnalysisBinning{"analysis"};
  static inline const std::string kSourceBinning{"source"};

  FitQualityRecorder() : fTree(std::make_unique<TTree>(kTreeName.c_str(), "Quality of every fit, one row per fit"))
  {
    fTree->SetDirectory(0);
    fTree->Branch("section", &fRow.section);
    fTree->Branch("binning", &fRow.binning);
    fTree->Branch("multLow", &fRow.multLow);
    fTree->Branch("multHigh", &fRow.multHigh);
    fTree->Branch("ptLow", &fRow.ptLow);
    fTree->Branch("ptHigh", &fRow.ptHigh);
    fTree->Branch("binOverride", &fRow.binOverride);
    fTree->Branch("counts", &fRow.fit.counts);
    fTree->Branch("status", &fRow.fit.status);
    fTree->Branch("covQual", &fRow.fit.covQual);
    fTree->Branch("edm", &fRow.fit.edm);
    fTree->Branch("minNll", &fRow.fit.minNll);
    fTree->Branch("numInvalidNLL", &fRow.fit.numInvalidNLL);
    fTree->Branch("atLowerLimit", &fRow.fit.atLowerLimit);
    fTree->Branch("atUpperLimit", &fRow.fit.atUpperLimit);
    fTree->Branch("upperIsShape", &fRow.fit.upperIsShape);
    fTree->Branch("start", &fRow.fit.start);
    fTree->Branch("nllConfig", &fRow.fit.nllConfig);
  }

  // The branches point into this object.
  FitQualityRecorder(const FitQualityRecorder&) = delete;
  FitQualityRecorder& operator=(const FitQualityRecorder&) = delete;

  // One fit: the fitConfig.json section, the binning (kAnalysisBinning or kSourceBinning), the cell as indices into
  // the two binnings, whose edges are read here so that no caller pairs an index with the wrong vector, and the
  // fit_setup.bins override that applied. Only the edges are recorded: the canvas and the override are named by
  // them, and a cut on an index across two productions is the mismatch the framework is built against.
  void Add(const std::string& section, const std::string& binning, const std::vector<double>& multBinning, int multBin,
           const std::vector<double>& ptBinning, int ptBin, const std::string& binOverride, const FitDiagnostics& fit)
  {
    fRow = {.section = section, .binning = binning, .multLow = multBinning[multBin],
            .multHigh = multBinning[multBin + 1], .ptLow = ptBinning[ptBin], .ptHigh = ptBinning[ptBin + 1],
            .binOverride = binOverride, .fit = fit};
    fTree->Fill();

    Tally& tally = fTallies[{section, binning}];
    ++tally.fits;
    const std::vector<std::string> reasons = Reasons(fit);
    tally.flagged += !reasons.empty();
    for (const std::string& reason : reasons)
      ++tally.reasons[reason];
  }

  // Writes the tree into 'binningDir', {binning_name} of the output file, and gives one warning per section and
  // binning that has flagged fits, not one per fit: with hundreds of cells the end-of-run summary would otherwise be
  // a list of fits. One call, so that the tree cannot be written without the warnings.
  void Write(TDirectory* binningDir, std::string_view who) const
  {
    if (!binningDir)
      throw std::runtime_error(std::format("[FATAL] {}: no directory to write the fit quality into.", who));
    binningDir->cd();
    fTree->Write(nullptr, TObject::kOverwrite);
    const std::string where = std::format("{}/{}", binningDir->GetPath(), kTreeName);
    Logger::Info("{}: the quality of {} fits written to {}", who, fTree->GetEntries(), where);

    for (const auto& [key, tally] : fTallies) {
      if (!tally.flagged)
        continue;
      std::string details;
      for (const auto& [reason, n] : tally.reasons)
        details += std::format("{}{} {}", details.empty() ? "" : ", ", reason, n);
      Logger::Warn("{}: {} of {} fits of '{}' ({} binning) flagged: {}; the rows are in {}", who, tally.flagged, tally.fits, key.first,
                   key.second, details, where);
    }
  }

  // Why a fit is flagged, empty if it is not: the flags that say something on their own (RooFitKnowledge, note 09).
  // An empty cell, a covariance that is not accurate, NaN evaluations of the likelihood, a parameter sitting at a
  // limit of its range. The status is not one: with the fit as it is today, status 3 is the outcome of most cells,
  // good and bad alike. Each reason is the label it is counted and printed under, next to its test.
  static std::vector<std::string> Reasons(const FitDiagnostics& fit)
  {
    if (fit.counts <= 0.0)
      return {"empty"}; // nothing else means anything without data
    std::vector<std::string> reasons;
    if (fit.covQual < 3)
      reasons.emplace_back("covQual < 3");
    if (fit.numInvalidNLL > 0)
      reasons.emplace_back("invalid NLL evaluations");
    for (const std::string& name : fit.atLowerLimit)
      reasons.push_back(name + " at its lower limit");
    for (const std::string& name : fit.atUpperLimit) // not one its model calls a shape there (the DSCB's tail n)
      if (std::ranges::find(fit.upperIsShape, name) == fit.upperIsShape.end())
        reasons.push_back(name + " at its upper limit");
    return reasons;
  }

 private:
  struct Row {
    std::string section; // the fitConfig.json section: "phi", or the purity_key
    std::string binning;
    double multLow{0.0}, multHigh{0.0}, ptLow{0.0}, ptHigh{0.0};
    std::string binOverride; // empty for none
    FitDiagnostics fit;
  };

  struct Tally {
    int fits{0}, flagged{0};
    std::map<std::string, int> reasons; // per label of Reasons
  };

  std::unique_ptr<TTree> fTree;
  Row fRow; // what the branches read at each Fill
  std::map<std::pair<std::string, std::string>, Tally> fTallies; // per (section, binning)
};

#pragma once

#include "AnalysisStructures.h"
#include "Logger.h"
#include "RootIOHelpers.h"

#include "TDirectory.h"
#include "TTree.h"

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
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
//
// The columns are listed once (Columns), for the writer and for Read, the reader: a reader with its own list
// would read a column added later as its default, and Reasons would judge it on that.
class FitQualityRecorder
{
 public:
  static inline const std::string kTreeName{"FitQuality"};
  // The two binnings a fit can be at: PurityTask fits both when rebinning_pt merges bins.
  static inline const std::string kAnalysisBinning{"analysis"};
  static inline const std::string kSourceBinning{"source"};

  // One row, one fit.
  struct Row {
    std::string section; // the fitConfig.json section: "phi", or the purity_key
    std::string binning;
    double multLow{0.0}, multHigh{0.0}, ptLow{0.0}, ptHigh{0.0};
    std::string binOverride; // empty for none
    std::string sigModel, bkgModel; // the models the fit used, after the override
    AnalysisStructures::FitDiagnostics fit;
  };

  FitQualityRecorder() : fTree(std::make_unique<TTree>(kTreeName.c_str(), "Quality of every fit, one row per fit"))
  {
    fTree->SetDirectory(0);
    Columns(fRow, [this](const char* name, auto* address) { fTree->Branch(name, address); });
  }

  // The branches point into this object.
  FitQualityRecorder(const FitQualityRecorder&) = delete;
  FitQualityRecorder& operator=(const FitQualityRecorder&) = delete;

  // One fit: the fitConfig.json section, the binning (kAnalysisBinning or kSourceBinning), the cell as indices into
  // the two binnings, whose edges are read here so that no caller pairs an index with the wrong vector, and the
  // configuration it ran with (the fit_setup.bins override that applied, the models). Only the edges are recorded: the
  // canvas and the override are named by them, and a cut on an index across two productions is the mismatch the
  // framework is built against.
  void Add(const std::string& section, const std::string& binning, const std::vector<double>& multBinning, int multBin,
           const std::vector<double>& ptBinning, int ptBin, const AnalysisStructures::FitConfig& cfg, const AnalysisStructures::FitDiagnostics& fit)
  {
    fRow = {.section = section, .binning = binning, .multLow = multBinning[multBin],
            .multHigh = multBinning[multBin + 1], .ptLow = ptBinning[ptBin], .ptHigh = ptBinning[ptBin + 1],
            .binOverride = cfg.binOverride, .sigModel = cfg.model.sigModel, .bkgModel = cfg.model.bkgModel, .fit = fit};
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

  // Every row of the FitQuality tree in 'binningDir', {binning_name} of a task's output file, in order. A column of
  // today's that the tree lacks, or holds with another type (a file written before the column was added or changed),
  // stops the read: as a default value it would change what Reasons says.
  static std::vector<Row> Read(TDirectory* binningDir, std::string_view who)
  {
    // Declared before the tree, so that the tree goes first: it points into them. An object column is read through a
    // pointer to it, which must outlive the reading: one holder per column.
    Row row;
    std::vector<std::shared_ptr<void>> holders;
    const auto tree = RootIO::GetUniqueOrThrow<TTree>(binningDir, kTreeName, who);
    const std::string where = std::format("the {} tree of '{}'", kTreeName, RootIO::FileNameOf(binningDir));
    Columns(row, [&](const char* name, auto* address) {
      if (!tree->GetBranch(name))
        throw std::runtime_error(std::format("[FATAL] {}: {} has no column '{}': it was written by an older version of the "
                                             "task; re-run it.", who, where, name));
      int code = 0;
      if constexpr (std::is_arithmetic_v<std::remove_pointer_t<decltype(address)>>) {
        code = tree->SetBranchAddress(name, address);
      } else {
        auto holder = std::make_shared<decltype(address)>(address);
        holders.push_back(holder);
        code = tree->SetBranchAddress(name, holder.get());
      }
      if (code < 0)
        throw std::runtime_error(std::format("[FATAL] {}: the column '{}' of {} is not of the type this version reads "
                                             "(SetBranchAddress code {}); re-run the task.", who, name, where, code));
    });
    std::vector<Row> rows;
    for (Long64_t entry = 0; entry < tree->GetEntries(); ++entry) {
      if (tree->GetEntry(entry) <= 0)
        throw std::runtime_error(std::format("[FATAL] {}: cannot read entry {} of {}.", who, entry, where));
      rows.push_back(row);
    }
    return rows;
  }

  // Why a fit is flagged, empty if it is not: the flags that say something on their own (RooFitKnowledge, note 09).
  // An empty cell, a covariance that is not accurate, NaN evaluations of the likelihood, a parameter sitting at a
  // limit of its range. The status is not one: with the fit as it is today, status 3 is the outcome of most cells,
  // good and bad alike. Each reason is the label it is counted and printed under, next to its test.
  static std::vector<std::string> Reasons(const AnalysisStructures::FitDiagnostics& fit)
  {
    if (fit.counts <= 0.0)
      return {"empty"}; // nothing else means anything without data
    std::vector<std::string> reasons;
    if (fit.covQual < 3)
      reasons.emplace_back("covQual < 3");
    if (fit.numInvalidNLL > 0)
      reasons.emplace_back("invalid NLL evaluations");
    for (const std::string& name : fit.atLowerLimit)
      reasons.push_back(std::format("{} at its lower limit", name));
    for (const std::string& name : fit.atUpperLimit) // not one its model calls a shape there (the DSCB's tail n)
      if (std::ranges::find(fit.upperIsShape, name) == fit.upperIsShape.end())
        reasons.push_back(std::format("{} at its upper limit", name));
    return reasons;
  }

 private:
  // The columns, name and member, in their order in the tree.
  template <class Visit>
  static void Columns(Row& row, Visit&& visit)
  {
    visit("section", &row.section);
    visit("binning", &row.binning);
    visit("multLow", &row.multLow);
    visit("multHigh", &row.multHigh);
    visit("ptLow", &row.ptLow);
    visit("ptHigh", &row.ptHigh);
    visit("binOverride", &row.binOverride);
    visit("sigModel", &row.sigModel);
    visit("bkgModel", &row.bkgModel);
    visit("counts", &row.fit.counts);
    visit("status", &row.fit.status);
    visit("covQual", &row.fit.covQual);
    visit("edm", &row.fit.edm);
    visit("minNll", &row.fit.minNll);
    visit("numInvalidNLL", &row.fit.numInvalidNLL);
    visit("numFloating", &row.fit.numFloating);
    visit("atLowerLimit", &row.fit.atLowerLimit);
    visit("atUpperLimit", &row.fit.atUpperLimit);
    visit("upperIsShape", &row.fit.upperIsShape);
    visit("start", &row.fit.start);
    visit("nllConfig", &row.fit.nllConfig);
  }

  struct Tally {
    int fits{0}, flagged{0};
    std::map<std::string, int> reasons; // per label of Reasons
  };

  std::unique_ptr<TTree> fTree;
  Row fRow; // what the branches read at each Fill
  std::map<std::pair<std::string, std::string>, Tally> fTallies; // per (section, binning)
};

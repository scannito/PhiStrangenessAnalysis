#pragma once

#include "AnalysisSettings.h"
#include "AnalysisUtils.h"
#include "BinningUtils.h"
#include "IAnalysisTask.h"
#include "JsonConfigHelpers.h"
#include "RootIOHelpers.h"
#include "RunEnvironment.h"

#include "TClass.h"
#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TKey.h"

#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Divides results produced by two separate runs of the chain: MC closure
// (corrected against generated), an old production against a new one, 2024 against
// 2026. This is the axis the framework did not have - 'yield_ratios' inside
// CorrelationTaskBase divides two DIFFERENT quantities produced TOGETHER, while
// everything here divides the SAME quantity produced APART.
//
// That difference is the whole reason this needs care. Two files produced apart can
// disagree about what their bins mean, and the objects are named by multiplicity
// INDEX: "_multBin3" is present on both sides whatever the two productions meant by
// it. Comparing the axes of the two histograms would not help either, because
// multiplicity is not one of their axes - it is in the name. So the binning stamps
// written by CorrelationTaskBase are not a formality here, they are the only thing
// standing between a name that matches and an interval that does not.
//
// The trends have the mirror problem: multiplicity IS their axis, but they are
// integrals over pT, so a disagreement in the associated-pT binning leaves their axes
// identical and their contents incomparable. Hence the per-particle stamp. Between
// the two, the only thing an axis comparison covers on its own is the pT of the
// spectra.
//
// Three targets: the spectra, the multiplicity trends, and the cross-species ratios.
// The last is often the most telling - a ratio of ratios, where what is common to
// both species has already cancelled once, so what survives is the difference
// between the two productions.
//
// No object name is ever built here. Each target lists what the numerator directory
// holds and divides whatever the denominator holds under the same name, so nothing
// in this file depends on how the producer spells its bins.
class ComparisonTask : public IAnalysisTask
{
 public:
  // Which family of objects to divide. Dispatched in Run(), the same shape as
  // CorrelationTask::RunMode: the two share the file handling and the checks and
  // differ only in what they iterate over.
  enum class Target { Spectra = 0,
                      Trends,
                      Ratios };

  std::string GetName() const override { return "comparison_task"; }

  void Init(const rapidjson::Value& taskConfig, const AnalysisSettings& globalSettings) override
  {
    std::cout << "[INFO] ComparisonTask: INITIALIZING..." << std::endl;

    globalCfgs = globalSettings;

    target = JsonConfig::OptionalEnum<Target>(taskConfig, "target", "ratios",
                                              {{"spectra", Target::Spectra},
                                               {"trends", Target::Trends},
                                               {"ratios", Target::Ratios}},
                                              GetName());

    // Where inside each input file to look. Defaulted rather than required because
    // both are fixed by whoever wrote the spectra, and a caller who has not changed
    // them should not have to repeat them.
    inputScheme = JsonConfig::OptionalString(taskConfig, "input_binning_name", globalCfgs.binningName, GetName());
    inputDirName = JsonConfig::OptionalString(taskConfig, "input_dir_name", "Extract1D", GetName());

    outputDir = JsonConfig::RequireString(taskConfig, "output_dir_final", GetName());
    outputPrefix = JsonConfig::OptionalString(taskConfig, "output_prefix", "", GetName());

    // One entry per ratio, numerator and denominator spelled out. 1-vs-1 is one
    // entry, 1-vs-2 is two entries sharing a denominator, 2-vs-2 is two entries.
    // Deliberately not a 'reference' plus a list of 'targets': that shorthand saves
    // typing in the common case and then has to grow a rule for every other one.
    int idx = 0;
    for (const auto& c : JsonConfig::RequireArray(taskConfig, "comparisons", GetName())) {
      // The index is in the context because a missing 'label' leaves nothing else to
      // identify the entry by.
      const std::string ctx = std::format("{} comparisons[{}]", GetName(), idx++);

      Comparison comp;
      comp.numeratorFile = JsonConfig::RequireString(c, "numerator", ctx);
      comp.denominatorFile = JsonConfig::RequireString(c, "denominator", ctx);
      comp.label = JsonConfig::OptionalString(c, "label", "comparison" + std::to_string(idx - 1), ctx);

      // Declared, never guessed. Plain division is right when the two inputs are
      // independent samples - an old production against a new one - and wrong for a
      // closure, where numerator and denominator come from the SAME events and are
      // strongly correlated: propagating their errors as independent inflates the
      // uncertainty on the ratio, which makes a closure that does not close look
      // compatible with unity. "B" is the binomial treatment used elsewhere in this
      // codebase when the numerator counts a subset of the denominator.
      //
      // The task cannot infer which case it is in, so it refuses to choose.
      comp.divideOption = JsonConfig::RequireString(c, "divide_option", ctx);

      comparisons.push_back(std::move(comp));
    }

    std::string outName = outputDir + outputPrefix + "Comparisons.root";
    fileOutput = RootIO::OpenOrThrow(outName, "RECREATE", GetName());

    provenance["produced_at"] = RunEnvironment::TimestampNow();
    provenance["config_block"] = JsonConfig::Serialize(taskConfig);
    const auto& runEnv = RunEnvironment::Facts();
    provenance.insert(runEnv.begin(), runEnv.end());

    std::cout << "[INFO] ComparisonTask: " << comparisons.size() << " comparison(s) configured." << std::endl;
  }

  void Run() override
  {
    switch (target) {
      case Target::Spectra:
        RunSpectra();
        break;
      case Target::Trends:
        RunTrends();
        break;
      case Target::Ratios:
        RunRatios();
        break;
      default:
        throw std::runtime_error("[FATAL] ComparisonTask: Unknown target!");
    }
  }

  void Terminate() override
  {
    std::cout << "[INFO] ComparisonTask: TERMINATING AND CLEANING UP..." << std::endl;

    RootIO::WriteProvenance(RootIO::GetOrCreatePath(fileOutput.get(), {"Provenance"}, false), provenance);

    if (fileOutput) {
      fileOutput->Close();
    }

    std::cout << "[INFO] ComparisonTask: DONE." << std::endl;
  }

 private:
  struct Comparison {
    std::string label;
    std::string numeratorFile;
    std::string denominatorFile;
    std::string divideOption;
  };

  // A pair of open input files, with their binnings already agreed upon.
  struct OpenPair {
    std::unique_ptr<TFile> num;
    std::unique_ptr<TFile> den;
    TDirectory* numDir{nullptr};
    TDirectory* denDir{nullptr};
    TDirectory* outDir{nullptr};
    std::vector<double> multBinning;
  };

  AnalysisSettings globalCfgs;
  Target target{Target::Ratios};

  std::string inputScheme;
  std::string inputDirName;
  std::string outputDir;
  std::string outputPrefix;

  std::vector<Comparison> comparisons;
  std::unique_ptr<TFile> fileOutput;
  std::map<std::string, std::string> provenance;

  // ---------------------------------------------------------------------------
  // Shared: open both inputs, refuse to go on unless they agree, and record where
  // they came from.
  // ---------------------------------------------------------------------------
  OpenPair OpenAndVerify(const Comparison& comp)
  {
    OpenPair pair;
    const std::string ctx = GetName() + " '" + comp.label + "'";

    pair.num = RootIO::OpenOrThrow(comp.numeratorFile, "READ", ctx);
    pair.den = RootIO::OpenOrThrow(comp.denominatorFile, "READ", ctx);

    TDirectory* numScheme = RootIO::GetOrCreatePath(pair.num.get(), {inputScheme}, true);
    TDirectory* denScheme = RootIO::GetOrCreatePath(pair.den.get(), {inputScheme}, true);
    if (!numScheme || !denScheme)
      throw std::runtime_error(std::format(
        "[FATAL] {}: no '{}' directory in '{}'. Either the inputs were produced under a different "
        "'binning_name', or 'input_binning_name' does not name the one they carry.",
        ctx, inputScheme, numScheme ? comp.denominatorFile : comp.numeratorFile));

    // The two productions must mean the same thing by their bins. Read from the
    // numerator and required of the denominator, so that a mismatch is reported once
    // with both binnings side by side.
    pair.multBinning = RootIO::ReadBinningStamp(numScheme, "binning_mult");
    if (pair.multBinning.empty())
      throw std::runtime_error(std::format(
        "[FATAL] {}: '{}' carries no 'binning_mult' stamp, so there is no way to tell whether its "
        "bins mean the same intervals as the other file's. It was produced before the stamps existed: "
        "re-run the task that wrote it.",
        ctx, comp.numeratorFile));

    RootIO::RequireMatchingBinningStamp(denScheme, "binning_mult", pair.multBinning,
                                        ctx + ", denominator '" + comp.denominatorFile + "'");

    // Trigger pT is integrated away in everything this task divides - spectra and
    // trends alike - so it is an axis of nothing, and two productions summing over
    // different trigger ranges would compare cleanly and mean different things.
    // Checked here rather than per target because it applies to both.
    const std::vector<double> ptPhi = RootIO::ReadBinningStamp(numScheme, "binning_ptPhi");
    if (!ptPhi.empty())
      RootIO::RequireMatchingBinningStamp(denScheme, "binning_ptPhi", ptPhi,
                                          ctx + ", denominator '" + comp.denominatorFile + "'");

    pair.numDir = RootIO::GetOrCreatePath(pair.num.get(), {inputScheme, inputDirName}, true);
    pair.denDir = RootIO::GetOrCreatePath(pair.den.get(), {inputScheme, inputDirName}, true);
    if (!pair.numDir || !pair.denDir)
      throw std::runtime_error(std::format("[FATAL] {}: no '{}' directory under '{}' in one of the two inputs.",
                                           ctx, inputDirName, inputScheme));

    pair.outDir = RootIO::GetOrCreatePath(fileOutput.get(), {comp.label});
    if (!pair.outDir)
      throw std::runtime_error(std::format("[FATAL] {}: cannot create the output directory '{}'.", ctx, comp.label));

    // What was divided, travelling with the result. Provenance never throws: a path
    // that moved is not a reason to fail a comparison that is otherwise fine.
    RootIO::WriteProvenance(RootIO::GetOrCreatePath(fileOutput.get(), {comp.label, "Provenance_numerator"}, false),
                            RootIO::ReadProvenance(RootIO::GetOrCreatePath(pair.num.get(), {inputScheme, "Provenance"}, true)));
    RootIO::WriteProvenance(RootIO::GetOrCreatePath(fileOutput.get(), {comp.label, "Provenance_denominator"}, false),
                            RootIO::ReadProvenance(RootIO::GetOrCreatePath(pair.den.get(), {inputScheme, "Provenance"}, true)));

    return pair;
  }

  // ---------------------------------------------------------------------------
  // The three targets. Everything above is shared; these only decide WHERE to look.
  //
  // None of them builds an object name. They list what the numerator directory
  // actually holds and divide whatever the denominator holds under the same name,
  // which has three consequences worth stating: the delta-y tags and multiplicity
  // bins need not be reconstructed, the same three lines serve all three targets,
  // and the task is unaffected by how the producer spells its bins - "_multBin3" or
  // "_mult10-15" alike, since it never writes a name, only reads one.
  // ---------------------------------------------------------------------------
  void RunSpectra()
  {
    ForEachComparison("spectra", [&](const OpenPair& pair, const Comparison& comp) {
      int done = 0;
      for (const auto& particle : ListParticles(pair.numDir))
        done += DivideDirectory(pair, comp, {particle, "Spectra"});
      return done;
    });
  }

  void RunTrends()
  {
    ForEachComparison("trends", [&](const OpenPair& pair, const Comparison& comp) {
      int done = 0;
      for (const auto& particle : ListParticles(pair.numDir)) {
        RequireSamePtAssoc(pair, comp, particle);
        done += DivideDirectory(pair, comp, {particle, "Trends"});
      }
      return done;
    });
  }

  // The cross-species ratios - K0S/pi, Xi/pi - which is what the previous generation
  // of this analysis mostly produced. A ratio of ratios is often the most telling
  // comparison there is: what is common to numerator and denominator has already
  // cancelled once, so what survives is the difference between the two productions.
  void RunRatios()
  {
    ForEachComparison("ratios", [&](const OpenPair& pair, const Comparison& comp) {
      // Every particle contributes to these, so the pT binning of all of them has to
      // agree, not just one - and they are trends, integrated over pT.
      for (const auto& particle : ListParticles(pair.numDir))
        RequireSamePtAssoc(pair, comp, particle);
      return DivideDirectory(pair, comp, {"Ratios"});
    });
  }

  // ---------------------------------------------------------------------------
  // Shared body of the three targets above.
  // ---------------------------------------------------------------------------
  template <typename F>
  void ForEachComparison(const char* what, F&& body)
  {
    for (const auto& comp : comparisons) {
      std::cout << "[INFO] " << GetName() << ": '" << comp.label << "' (" << what << ")" << std::endl;
      OpenPair pair = OpenAndVerify(comp);
      const int done = body(pair, comp);

      // Counted in terms of what was DIVIDED, not of what came out: with the
      // 'ratios' target the inputs are themselves ratios, so "N ratios written"
      // would leave a reader guessing which of the two it meant.
      std::cout << "  -> " << done << " " << what << " divided." << std::endl;

      if (done == 0)
        std::cerr << "[WARNING] " << GetName() << ": '" << comp.label << "' divided no " << what
                  << ". The two files share no object under '" << inputScheme << "/" << inputDirName
                  << "' for this target." << std::endl;
    }
  }

  // Divides every histogram the numerator holds in 'subPath' by the one of the same
  // name in the denominator. Canvases and anything that is not a histogram are
  // skipped, which is what keeps the binning stamps and the drawing canvases out.
  int DivideDirectory(const OpenPair& pair, const Comparison& comp, const std::vector<std::string>& subPath)
  {
    // Read-only on the inputs: navigate, never create. They are opened READ, so a
    // mkdir would fail anyway, but saying it here is what makes a missing directory
    // a plain "nothing to do" instead of an error.
    TDirectory* numSub = RootIO::GetOrCreatePath(pair.numDir, subPath, true);
    TDirectory* denSub = RootIO::GetOrCreatePath(pair.denDir, subPath, true);
    if (!numSub || !denSub)
      return 0;

    // From pair.outDir, which is already the directory of this comparison: the same
    // 'subPath' then means the same thing on all three sides, so the output mirrors
    // the input layout by construction rather than by two places agreeing.
    TDirectory* outSub = RootIO::GetOrCreatePath(pair.outDir, subPath);
    if (!outSub)
      return 0;

    int done = 0;
    for (const std::string& name : ListHistograms(numSub)) {
      const std::string ctx = GetName() + " '" + comp.label + "' " + RootIO::MakeDirPath(subPath);

      std::unique_ptr<TH1> num = RootIO::GetUniqueOrThrow<TH1>(numSub, name, ctx);
      std::unique_ptr<TH1> den = RootIO::GetUniqueOrWarn<TH1>(denSub, name, ctx);
      if (!den)
        continue; // present on one side only: the two productions may legitimately differ

      // The pT or multiplicity axis of the objects themselves. The stamps checked in
      // OpenAndVerify cover what is in the NAME; this covers what is in the object.
      BinningUtils::RequireSameAxis(num->GetXaxis(), den->GetXaxis(),
                                    "numerator " + name + " of " + comp.numeratorFile,
                                    "denominator " + name + " of " + comp.denominatorFile);

      std::unique_ptr<TH1> ratio = AnalysisUtils::MakeRatioHist(
        num.get(), den.get(), name + "_ratio",
        std::format("{};{};ratio", comp.label, num->GetXaxis()->GetTitle()),
        1.0, 1.0, comp.divideOption.c_str());

      outSub->cd();
      ratio->Write(nullptr, TObject::kOverwrite);
      ++done;
    }
    return done;
  }

  // The associated-pT binning of one particle, when it is not an axis of what is
  // being divided - see the note at the top of the class.
  void RequireSamePtAssoc(const OpenPair& pair, const Comparison& comp, const std::string& particle)
  {
    const std::vector<double> stamped = RootIO::ReadBinningStamp(RootIO::GetOrCreatePath(pair.numDir, {particle}, true), "binning_ptAssoc");
    if (stamped.empty())
      return; // produced before the stamp existed: warned about, not fatal

    RootIO::RequireMatchingBinningStamp(RootIO::GetOrCreatePath(pair.denDir, {particle}, true), "binning_ptAssoc", stamped,
                                        GetName() + " '" + comp.label + "', " + particle + ", denominator '" +
                                          comp.denominatorFile + "'");
  }

  // ---------------------------------------------------------------------------
  // Discovery: what the file holds decides what is compared, rather than a list in
  // the JSON that goes stale the first time a particle or a delta-y cut is added.
  // ---------------------------------------------------------------------------
  // A particle is a subdirectory that HOLDS per-particle results, which is what
  // distinguishes it from the "Ratios" sibling. Recognised by what it contains and
  // not by excluding the names of everything it is not: that exclusion would need a
  // new entry the first time another sibling appears - systematics, QA - and whoever
  // added it would have no reason to look here.
  static std::vector<std::string> ListParticles(TDirectory* dir)
  {
    std::vector<std::string> names;
    if (!dir || !dir->GetListOfKeys())
      return names;

    for (TObject* keyObj : *dir->GetListOfKeys()) {
      auto* key = static_cast<TKey*>(keyObj);
      if (std::string(key->GetClassName()) != "TDirectoryFile")
        continue;

      TDirectory* sub = dir->GetDirectory(key->GetName());
      if (sub && (sub->GetDirectory("Trends") || sub->GetDirectory("Spectra")))
        names.emplace_back(key->GetName());
    }
    return names;
  }

  // Histograms only. Filtering by class rather than by name is what keeps the
  // drawing canvases out - and the binning stamps, which are TH1D and would
  // otherwise be divided like data.
  static std::vector<std::string> ListHistograms(TDirectory* dir)
  {
    std::vector<std::string> names;
    if (!dir || !dir->GetListOfKeys())
      return names;

    for (TObject* keyObj : *dir->GetListOfKeys()) {
      auto* key = static_cast<TKey*>(keyObj);
      TClass* cl = TClass::GetClass(key->GetClassName());
      if (cl && cl->InheritsFrom(TH1::Class()) && !std::string(key->GetName()).starts_with("binning_"))
        names.emplace_back(key->GetName());
    }
    return names;
  }
};

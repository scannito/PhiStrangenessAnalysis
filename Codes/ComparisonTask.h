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

#include <algorithm>
#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <set>
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

      // Optional, and its presence switches the whole comparison into a different
      // mode: the denominator is a file this framework did not produce - results from
      // before it existed - so there is nothing to discover and nothing to verify.
      //
      // What is given up is stated plainly because it cannot be recovered: a foreign
      // file carries no binning stamps, so binning_mult, binning_ptPhi and
      // binning_ptAssoc are all unavailable. The only check left is RequireSameAxis on
      // the two objects, which compares the edges they actually carry - so what it
      // covers depends on what you declared. For a trend it is the multiplicity, which
      // is exactly what the stamps would have checked. For a spectrum it is the
      // associated pT, and the multiplicity interval stays unchecked because that one
      // is in the name. In this mode YOU assert that the two objects are comparable.
      //
      // Convert graphs to histograms first - Macros/convertGraphHist.C - taking the
      // binning from the new result. Then RequireSameAxis has something to say again:
      // it checks that the conversion landed where it was meant to.
      if (auto objs = JsonConfig::TryArray(c, "objects", ctx)) {
        int oIdx = 0;
        for (const auto& o : *objs) {
          const std::string octx = std::format("{} objects[{}]", ctx, oIdx++);

          ObjectPair pair;
          pair.numeratorPath = JsonConfig::RequireString(o, "numerator", octx);
          pair.denominatorPath = JsonConfig::RequireString(o, "denominator", octx);
          // Defaulted from the numerator, unchanged: the object being characterised is
          // the new one, and the leaf alone is the name because a ROOT object name
          // holding a '/' is read as a path by Write(). No suffix, for the reason given
          // where the discovered ratios are built.
          pair.outName = JsonConfig::OptionalString(o, "name", LeafName(pair.numeratorPath), octx);

          comp.objects.push_back(std::move(pair));
        }

        if (comp.objects.empty())
          throw std::runtime_error(std::format(
            "[FATAL] {}: 'objects' is present but empty. Remove it to compare by discovery, "
            "or list the pairs to compare.", ctx));

        // Declared pairs land flat in the comparison's directory, so two of them whose
        // numerators share a leaf name would collapse onto one output and the second
        // would overwrite the first without a word. The discovered mode cannot hit this
        // because its output mirrors the input directories. Checked here, where every
        // name is already known, rather than on the way out.
        std::set<std::string> seen;
        for (const ObjectPair& o : comp.objects)
          if (!seen.insert(o.outName).second)
            throw std::runtime_error(std::format(
              "[FATAL] {}: two declared pairs would both be written as '{}'. Names default to the "
              "leaf of the numerator path, which is not unique across directories - give at least "
              "one of them an explicit 'name'.", ctx, o.outName));
      }

      comparisons.push_back(std::move(comp));
    }

    // Before the two questions below, because both are vacuously true on an empty list
    // and would answer about comparisons that do not exist. A task configured to divide
    // nothing is a configuration error in its own right: it would run, report nothing
    // and write an output file holding only its own provenance.
    if (comparisons.empty())
      throw std::runtime_error(std::format(
        "[FATAL] {}: 'comparisons' is empty, so there is nothing to divide.", GetName()));

    // 'objects' says the denominator is foreign, and that is a property of the TASK
    // rather than of one entry: 'target' lives at task level, so a mixed list would
    // carry one target that selects something for half the comparisons and means
    // nothing for the other half. There is no correct way to write that, so it is
    // refused instead of resolved arbitrarily - put the two kinds in two task blocks.
    //
    // Which is also the answer if these comparisons ever stop being occasional: not a
    // richer 'objects', but converting the old results into this framework's layout
    // once. See DESIGN_NOTES.md.
    const bool anyDeclared = std::ranges::any_of(comparisons, [](const Comparison& c) { return !c.objects.empty(); });
    const bool allDeclared = std::ranges::all_of(comparisons, [](const Comparison& c) { return !c.objects.empty(); });

    if (anyDeclared && !allDeclared)
      throw std::runtime_error(std::format(
        "[FATAL] {}: some comparisons list 'objects' and some do not. Those are two different "
        "kinds of comparison - one discovers what to divide and verifies the binning stamps, the "
        "other divides what you named and can verify nothing beyond the axes - and 'target' is a "
        "task-level key that cannot mean both. Split them into two task blocks.", GetName()));

    // Presence in the JSON, not the value of the member: 'target' always HAS a value,
    // its default, so testing the member would refuse every declared comparison ever
    // written. What is refused is spelling it out, and the reason is that it would be
    // inert rather than wrong - it selects which family to DISCOVER, and here nothing
    // is discovered. A key that changes no behaviour is better absent than present and
    // silently doing nothing, which is the converse of the rule in CLAUDE.md.
    if (allDeclared && JsonConfig::OptionalMember(taskConfig, "target"))
      throw std::runtime_error(std::format(
        "[FATAL] {}: 'target' selects which family of objects to DISCOVER, and every comparison "
        "here lists its objects explicitly, so it selects nothing - it would be inert, not wrong. "
        "Remove it.", GetName()));

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
  // One object named on each side, for a denominator this framework did not write.
  struct ObjectPair {
    std::string numeratorPath;
    std::string denominatorPath;
    std::string outName;
  };

  struct Comparison {
    std::string label;
    std::string numeratorFile;
    std::string denominatorFile;
    std::string divideOption;

    // Empty means the normal mode: both files come from this chain, so the objects
    // are discovered and matched by name. Non-empty means the denominator is foreign
    // and every pair is spelled out - see the note on ObjectPair in Init.
    std::vector<ObjectPair> objects;
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

    pair.outDir = RootIO::GetOrCreatePath(fileOutput.get(), {comp.label});
    if (!pair.outDir)
      throw std::runtime_error(std::format("[FATAL] {}: cannot create the output directory '{}'.", ctx, comp.label));

    // What was divided, travelling with the result. Provenance never throws: a path
    // that moved is not a reason to fail a comparison that is otherwise fine.
    //
    // An input may legitimately carry none - a denominator this framework did not
    // write, or one produced before the records existed - and in that case the reason
    // is written down instead of leaving the directory empty. An empty directory reads
    // as "something failed while writing this"; a note reads as "there was nothing to
    // write", which is what actually happened. Both possible causes are named because
    // the task cannot tell them apart either, and the file name goes in because it is
    // the only thing that identifies an input nothing else is known about.
    const auto copyProvenance = [&](TFile* src, const char* dirName, const std::string& fileName) {
      auto facts = RootIO::ReadProvenance(RootIO::GetOrCreatePath(src, {inputScheme, "Provenance"}, true));
      if (facts.empty())
        facts["no_provenance"] = "'" + fileName + "' carries nothing under '" + inputScheme +
                                 "/Provenance': either it was not produced by this framework, "
                                 "or it predates the provenance records.";
      RootIO::WriteProvenance(RootIO::GetOrCreatePath(fileOutput.get(), {comp.label, dirName}, false), facts);
    };

    copyProvenance(pair.num.get(), "Provenance_numerator", comp.numeratorFile);
    copyProvenance(pair.den.get(), "Provenance_denominator", comp.denominatorFile);

    // A declared comparison stops here. Its denominator was not written by this
    // framework, so it has no scheme directory to navigate and no stamps to compare -
    // everything below would fail on a file that is not at fault. What remains is
    // RequireSameAxis on each divided pair, and the caller's word that the two objects
    // mean the same thing.
    if (!comp.objects.empty()) {
      std::cout << "  -> declared comparison: the denominator is not a product of this chain, "
                   "so no binning is verified beyond the axes of the objects themselves."
                << std::endl;
      return pair;
    }

    TDirectory* numScheme = RootIO::GetOrCreatePath(pair.num.get(), {inputScheme}, true);
    TDirectory* denScheme = RootIO::GetOrCreatePath(pair.den.get(), {inputScheme}, true);
    if (!numScheme || !denScheme)
      throw std::runtime_error(std::format(
        "[FATAL] {}: no '{}' directory in '{}'. Either the inputs were produced under a different "
        "'binning_name', or 'input_binning_name' does not name the one they carry. If the "
        "denominator was not produced by this chain at all, list the objects to divide under "
        "'objects' instead.",
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
      // 'what' names the target, and a declared comparison has none: 'target' would
      // still hold its default there, so reporting it would put "ratios" next to a
      // list of spectra you named yourself. The label follows what was actually
      // divided rather than a key that selected nothing.
      const bool declared = !comp.objects.empty();
      const char* divided = declared ? "declared pairs" : what;

      std::cout << "[INFO] " << GetName() << ": '" << comp.label << "' (" << divided << ")" << std::endl;
      OpenPair pair = OpenAndVerify(comp);
      const int done = declared ? DivideDeclared(pair, comp) : body(pair, comp);

      // Counted in terms of what was DIVIDED, not of what came out: with the
      // 'ratios' target the inputs are themselves ratios, so "N ratios written"
      // would leave a reader guessing which of the two it meant.
      std::cout << "  -> " << done << " " << divided << " divided." << std::endl;

      // Unreachable for a declared comparison - a missing object is fatal there and an
      // empty list is refused in Init - which is why the message may speak of discovery.
      if (done == 0)
        std::cerr << "[WARNING] " << GetName() << ": '" << comp.label << "' divided no " << what
                  << ". The two files share no object under '" << inputScheme << "/" << inputDirName
                  << "' for this target." << std::endl;
    }
  }

  // Divides the pairs named in the configuration, each by its full path from the root
  // of its own file. Nothing is discovered and nothing but the axes is checked - see
  // the note on 'objects' in Init.
  int DivideDeclared(const OpenPair& pair, const Comparison& comp)
  {
    int done = 0;
    for (const ObjectPair& obj : comp.objects) {
      const std::string ctx = GetName() + " '" + comp.label + "'";

      // Both fatal, unlike the discovered mode where a missing object means the two
      // productions legitimately differ: here every pair was written out by hand, so
      // one that does not resolve is a typo or a stale path, not a difference.
      std::unique_ptr<TH1> num = RootIO::GetUniqueOrThrow<TH1>(pair.num.get(), obj.numeratorPath, ctx);
      std::unique_ptr<TH1> den = RootIO::GetUniqueOrThrow<TH1>(pair.den.get(), obj.denominatorPath, ctx);

      // The only check left, and what it covers depends on what was declared - nothing
      // here restricts the pairs to one kind of object.
      //
      // For a trend the X axis IS the multiplicity, so this verifies the very binning
      // the stamps would have verified. For a spectrum the X axis is the associated pT,
      // so it verifies that instead - which is real, but leaves the multiplicity
      // interval unchecked, because that one lives in the name and a foreign file does
      // not spell names the same way. There you are asserting that the two objects
      // belong to the same interval, and nothing can check it for you: in this mode the
      // task holds two paths and two TH1, and guessing the kind from an axis title
      // would be a check that fails quietly the first time a title changes.
      BinningUtils::RequireSameAxis(num->GetXaxis(), den->GetXaxis(),
                                    "numerator " + obj.numeratorPath + " of " + comp.numeratorFile,
                                    "denominator " + obj.denominatorPath + " of " + comp.denominatorFile);

      std::unique_ptr<TH1> ratio = AnalysisUtils::MakeRatioHist(
        num.get(), den.get(), obj.outName,
        std::format("{};{};ratio", comp.label, num->GetXaxis()->GetTitle()),
        1.0, 1.0, comp.divideOption.c_str());

      pair.outDir->cd();
      ratio->Write(nullptr, TObject::kOverwrite);
      ++done;
    }
    return done;
  }

  // The leaf of a "a/b/c" path, so that a declared pair can name its output after the
  // numerator without the directories coming along.
  static std::string LeafName(const std::string& path)
  {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
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

      // The name is carried over unchanged, not suffixed. Nothing here needs
      // disambiguating - the numerator lives in another file, this whole file holds
      // nothing but ratios, and the directory already carries the comparison's label -
      // so a suffix would be a third copy of what the file and the path already say.
      //
      // It also keeps the names stable across a round trip: this output can itself be
      // the input of another comparison, and discovery matches by name. A suffix added
      // at every pass would make the second comparison find nothing.
      std::unique_ptr<TH1> ratio = AnalysisUtils::MakeRatioHist(
        num.get(), den.get(), name,
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

#pragma once

#include "BinningUtils.h"

#include "Rtypes.h"

#include <format>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Runtime configuration of the analysis.
//
// Note the asymmetry between the two kinds of member below. The colours and the
// scheme name are genuine choices: they exist nowhere else. The binnings are
// not - they must equal the axes of the input files, so they are declarations
// about the production being read, and the file always wins. Accordingly there
// is no getter returning them: they can only be compared against an axis.
struct AnalysisSettings {

  // ---------------------------------------------------------------------------
  // Output layout
  // ---------------------------------------------------------------------------

  // Identifies the binning scheme. Used as the top directory of every output
  // file, so several schemes can coexist in the same file.
  std::string binningName{"NewFullDifferential"};

  // ---------------------------------------------------------------------------
  // Declared binning
  //
  // Only ever compared against the axes found in the input files. The binning
  // actually used lives in the tasks, resolved from the containers they open.
  //
  // Deliberately empty by default. A built-in binning would not be a fallback:
  // nothing reads these values, so it could only ever serve as an expectation -
  // and an expectation nobody wrote is one that fails against every production
  // while blaming a JSON key that does not exist. Undeclared means unchecked,
  // and that is announced explicitly rather than defaulted away.
  // ---------------------------------------------------------------------------

  std::vector<double> binsMult;
  std::map<std::string, std::vector<double>> speciesPtBinning;

  // For reference, the values that used to be built in here. They are not stale:
  // they are the binning of the CORRELATION productions, and are still in use.
  // They cannot be a default precisely because of that - the MC productions are
  // finer (34 pT bins for K0S against 10), so whichever of the two were built in
  // would be a false alarm for every workflow of the other family. Declare the
  // one that matches the production in the JSON of that workflow instead.
  //
  //   binsMult{0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0}
  //
  //   speciesPtBinning{
  //     {"Phi", {0.4, 0.8, 1.4, 2.0, 2.8, 4.0, 6.0, 10.0}},
  //     {"K0S", {0.0, 0.3, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0}},
  //     {"Xi",  {0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0}},
  //     {"Pi",  {0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0}}}

  // ---------------------------------------------------------------------------
  // Presentation
  // ---------------------------------------------------------------------------

  std::vector<int> spectraColors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856};
  std::vector<int> multTrendColors = {kRed, kBlue, kGreen + 2};

  // ---------------------------------------------------------------------------
  // Resolution -- the only way the analysis obtains a binning
  //
  // Reads the edges from the axis of the container that is about to be used and
  // verifies them against the declaration, in a single call. Keeping the two
  // together is what makes it impossible to index one binning while labelling
  // another, and impossible to forget the check.
  // ---------------------------------------------------------------------------

  std::vector<double> ResolvePtBinning(const std::string& species, const TAxis* axis,
                                       std::string_view context) const
  {
    std::vector<double> found = BinningUtils::AxisEdges(axis);
    VerifyPtBinning(species, found, context);
    return found;
  }

  std::vector<double> ResolveMultBinning(const TAxis* axis, std::string_view context) const
  {
    std::vector<double> found = BinningUtils::AxisEdges(axis);
    VerifyMultBinning(found, context);
    return found;
  }

  // ---------------------------------------------------------------------------
  // Verification
  //
  // Source bins are addressed BY INDEX (h->ProjectionZ(n, k+1, k+1),
  // AxisToCut{.axis = a, .bins = {k+1, k+1}}), so the declared binning and the file's axis
  // must be identical, not merely rebin-compatible: a disagreement means every
  // index points at a different interval than intended.
  //
  // Checked whatever the origin of the declared values - a wrong default is
  // exactly as harmful as a stale JSON entry. Nothing declared, nothing checked.
  // ---------------------------------------------------------------------------

  void VerifyPtBinning(const std::string& species, std::span<const double> found,
                       std::string_view context) const
  {
    auto it = speciesPtBinning.find(species);
    if (it == speciesPtBinning.end()) {
      // Announced once per species: silence here would be indistinguishable
      // from a check that passed.
      if (undeclaredPtWarned.insert(species).second) {
        std::cerr << "[WARNING] AnalysisSettings: no expected pT binning declared for '" << species
                  << "'. The binning found in the file is used without verification (" << context << ")."
                  << std::endl;
      }
      return;
    }

    const std::string diff = BinningUtils::Compare(it->second, found, "configuration", "input file");
    if (!diff.empty()) {
      throw std::runtime_error(std::format(
        "[FATAL] AnalysisSettings: pT binning mismatch for '{}' ({}):\n{}"
        "Bins are addressed by index, so these must match exactly. Either 'global_binning.pt_binning' "
        "is stale, or this file comes from a different production than the ones it is combined with.",
        species, context, diff));
    }
  }

  void VerifyMultBinning(std::span<const double> found, std::string_view context) const
  {
    if (binsMult.empty()) {
      if (!undeclaredMultWarned) {
        undeclaredMultWarned = true;
        std::cerr << "[WARNING] AnalysisSettings: no expected multiplicity binning declared. The binning "
                     "found in the file is used without verification ("
                  << context << ")." << std::endl;
      }
      return;
    }

    const std::string diff = BinningUtils::Compare(binsMult, found, "configuration", "input file");
    if (!diff.empty()) {
      throw std::runtime_error(std::format(
        "[FATAL] AnalysisSettings: multiplicity binning mismatch ({}):\n{}"
        "Bins are addressed by index, so these must match exactly. Either 'global_binning.multiplicity' "
        "is stale, or this file comes from a different production than the ones it is combined with.",
        context, diff));
    }
  }

  // ---------------------------------------------------------------------------
  // Presentation accessors
  // ---------------------------------------------------------------------------

  int GetSpectraColor(int index) const { return spectraColors[index % spectraColors.size()]; }
  int GetMultTrendColor(int index) const { return multTrendColors[index % multTrendColors.size()]; }

 private:
  // Bookkeeping for the "nothing declared" warnings, so that each one is said
  // once and not on every container of every particle. Mutable because issuing
  // a warning does not change the configuration.
  mutable std::set<std::string> undeclaredPtWarned;
  mutable bool undeclaredMultWarned{false};
};

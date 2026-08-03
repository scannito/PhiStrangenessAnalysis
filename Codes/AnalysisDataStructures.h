#pragma once

#include "AnalysisConstants.h"
#include "BinningUtils.h"

#include "TCanvas.h"
#include "TH1.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct AssocParticleConfig {
  std::string name;            // e.g., "K0S", "Pi"
  std::string dirName;         // e.g., "phiK0S", "phiPi"
  std::vector<double> binning; // Resolved from the input containers, not here: at
                               // construction time no file is open yet
  double mass;                 // Follows from the name, so it is not a parameter -
                               // a caller cannot pair "K0S" with the wrong mass

  // The override is an optional rather than a string so that the struct owns both
  // the rule and the decision whether to apply it: the caller hands over what the
  // configuration said, including the fact that it said nothing.
  AssocParticleConfig(std::string name_, std::optional<std::string> dirNameOverride = std::nullopt)
    : name(std::move(name_)),
      dirName(dirNameOverride.value_or(DefaultDirName(name))),
      mass(AnalysisConstants::GetMass(name)) {}

  // How the O2 task names its pair directories: "phiK0S", "phiXi", "phiPi".
  // Unlike the mass this is only a default, because several species can share one
  // directory - as Lambda and AntiLambda would, the histogram name still carrying
  // the species while the folder is shared.
  static std::string DefaultDirName(const std::string& particleName) { return "phi" + particleName; }
};

struct LoadedCorrections {
  std::string name;
  std::vector<std::unique_ptr<TH1F>> h1Corrections;
  std::vector<std::unique_ptr<TH1F>> h1CorrectionsEffMultInt;
};

struct LoadedPurity {
  std::string name;
  std::vector<std::unique_ptr<TH1>> h1Purity;
};

struct ParticleTask {
  std::string name;                                     // e.g. "k0s", "pi_tpc"
  std::unique_ptr<TH3F> h3Source;                       // Pointer to the 3D source histogram in RAM
  std::vector<double> sourceBinning;                    // The pT axis of h3Source
  std::optional<std::vector<double>> rebinningPt;       // Set only when the configuration asks for a coarser
                                                        // binning. Same spelling as LoadedMC: "was a merge
                                                        // requested" must be one question with one answer,
                                                        // not something each task deduces from the binnings
                                                        // it happens to be holding
  std::vector<BinningUtils::BinRange> mappedSourceBins; // Source bins covered by each analysis bin.
                                                        // Derived, but stored: deriving it is a search, not
                                                        // a branch, and MapToSourceBins throws when an
                                                        // analysis edge is not an edge of the source - a
                                                        // diagnostic that belongs to construction, before
                                                        // any fit has run
  TFile* outputFile;                                    // Output file for this particle
  std::unique_ptr<TCanvas> canvas;                      // Summary canvas: one pad, all multiplicity bins
  std::unique_ptr<TCanvas> canvasSourceBinning;         // Same overlay at the source binning, non-null only
                                                        // when a merge was requested. Its own pad, because
                                                        // two binnings together would ruin the comparison
                                                        // between multiplicity bins it is drawn for
  std::unique_ptr<TH2F> h2PurityCCDB;                   // (multiplicity, pT) purity at the source binning,
                                                        // written on its own as "ccdb_object" so O2 can
                                                        // apply it candidate by candidate

  // mappedSourceBins and the two canvases are NOT parameters: they follow from the
  // binnings and from the name, so the caller cannot get them wrong or forget them,
  // and the object is never half-built. Note that member initialisers run in
  // declaration order, which is why they are declared after what they read.
  ParticleTask(std::string name_, std::unique_ptr<TH3F> h3Source_, std::vector<double> sourceBinning_,
               std::optional<std::vector<double>> rebinningPt_, TFile* outputFile_,
               std::unique_ptr<TH2F> h2PurityCCDB_)
    : name(std::move(name_)),
      h3Source(std::move(h3Source_)),
      sourceBinning(std::move(sourceBinning_)),
      rebinningPt(std::move(rebinningPt_)),
      mappedSourceBins(BinningUtils::MapToSourceBins(sourceBinning, AnalysisBinning())),
      outputFile(outputFile_),
      canvas(std::make_unique<TCanvas>(("canvas" + name + "Purity").c_str(), (name + " Purity").c_str(), 800, 600)),
      canvasSourceBinning(rebinningPt
                            ? std::make_unique<TCanvas>(("canvas" + name + "Purity_sourceBinning").c_str(),
                                                        (name + " Purity (source binning)").c_str(), 800, 600)
                            : nullptr),
      h2PurityCCDB(std::move(h2PurityCCDB_)) {}

  // The binning the analysis works at: the merged one when one was asked for, the
  // source one otherwise. Derived rather than stored, so it cannot fall out of
  // agreement with rebinningPt.
  const std::vector<double>& AnalysisBinning() const { return rebinningPt ? *rebinningPt : sourceBinning; }
};

template <size_t size>
struct ParticleConfig {
  std::string name;                     // e.g., "Phi", "K0S", "Pi"
  std::array<std::string, size> titles; // Es. {Gen, GenAssoc, Reco} o {Efficiency, SignalLoss}
};

struct LoadedAssocData {
  std::string name;
  std::unique_ptr<THnSparseF> h5DataSignal;
  std::unique_ptr<THnSparseF> h5DataSideband;
  std::unique_ptr<THnSparseF> h5DataMESignal;
  std::unique_ptr<THnSparseF> h5DataMESideband;
};

// Structure to hold the loaded MC data and canvases in RAM
struct LoadedMC {
  std::string name;
  std::unique_ptr<TH3F> h3MCGen;
  std::unique_ptr<THnSparseF> h4MCGenAssocReco;
  std::unique_ptr<THnSparseF> h4MCReco;
  std::optional<std::vector<double>> rebinningPt;
  std::unique_ptr<TCanvas> canvasEfficiency;
  std::unique_ptr<TCanvas> canvasSignalLoss;
  // Same two overlays at the source binning, non-null only when 'rebinning_pt' asks
  // for a merge: on their own pads, because a single one mixing the two binnings
  // would ruin the multiplicity-to-multiplicity comparison they are drawn for
  std::unique_ptr<TCanvas> canvasEfficiencySourceBinning;
  std::unique_ptr<TCanvas> canvasSignalLossSourceBinning;

  // All four follow from 'name' and 'rebinningPt', so the caller neither names them
  // nor decides which ones exist. Not a constructor because the histograms above
  // are loaded one at a time, with the axis checks in between: call this once name
  // and rebinningPt are set.
  void CreateCanvases()
  {
    auto make = [](const std::string& canvasName) {
      return std::make_unique<TCanvas>(canvasName.c_str(), canvasName.c_str(), 800, 600);
    };

    canvasEfficiency = make("c_" + name + "_Efficiency");
    canvasSignalLoss = make("c_" + name + "_SignalLoss");

    if (rebinningPt) {
      canvasEfficiencySourceBinning = make("c_" + name + "_Efficiency_sourceBinning");
      canvasSignalLossSourceBinning = make("c_" + name + "_SignalLoss_sourceBinning");
    }
  }
};

// ============================================================================
// MATHEMATICAL PARAMETER
// ============================================================================
// Unifies FitParam and ExtrapParam into a single, clean structure
struct MathParam {
  double val{0.0};
  double min{0.0};
  double max{0.0};
  bool isConstant{false};
};

// ============================================================================
// FIT CONFIGURATION STRUCTURES
// ============================================================================
struct ObservableConfig {
  std::string name;
  std::string title;
  double min;
  double max;
};

struct ModelConfig {
  std::string sigModel;
  std::string bkgModel;
  std::map<std::string, MathParam> params; // Uses the unified MathParam
};

struct IntegrationConfig {
  bool useFixedRange{true};
  std::pair<double, double> range{0.0, 0.0};
  double nSigma{3.0};
  bool snapToBin{false};
  bool calculatePurity{false};
  bool calculateSideband{false};
  bool sidebandFromFit{true};
  std::pair<double, double> sidebandRange{0.0, 0.0};
};

struct FitConfig {
  ObservableConfig obs;
  ModelConfig model;
  IntegrationConfig integration;
};

// ============================================================================
// EXTRAPOLATION CONFIGURATION STRUCTURE
// ============================================================================
struct ExtrapConfig {
  std::string model;
  std::pair<double, double> domainRange{0.0, 15.0};
  std::pair<double, double> fitRange{0.0, 0.0};
  std::map<std::string, MathParam> params; // Uses the unified MathParam
  double mass{0.0};                        // Injected dynamically at runtime by the Task
};

struct ExtrapolationResult {
  double yield{0.0};
  double yieldStatErr{0.0};
  double meanPt{0.0};
  double meanPtStatErr{0.0};
  double extrapolatedFraction{0.0};
};

struct YieldRatioConfig {
  std::string num;
  std::string den;
  std::string label;
};

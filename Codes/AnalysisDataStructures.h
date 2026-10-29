#pragma once

#include "AnalysisConstants.h"
#include "BinningUtils.h"

#include "TFile.h"
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
#include <variant>
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
  static std::string DefaultDirName(const std::string& particle) { return "phi" + particle; }
};

// One vector, already built with the multiplicity dependence the configuration chose for
// this particle. There used to be two, binned and multiplicity-integrated, with every
// consumer picking one through a flag that held for all particles at once.
//
// TH1 and not TH1F: MCTask writes TH1D, and asking for TH1F made LoadCorrections stop
// on every Corrections file it produces. The values are read with GetBinContent, so the
// class only decides the precision they are stored with.
struct LoadedCorrections {
  std::string name;
  std::vector<std::unique_ptr<TH1>> h1Corrections;
};

// ---------------------------------------------------------------------------
// The "corrections" block of a correlation task. See DESIGN_NOTES.md section 13.
// ---------------------------------------------------------------------------
// Per particle and not per correction, because the physics is: the multiplicity
// dependence of the efficiency differs between species (the K0S depends on the
// multiplicity class, the others do not show it), and some corrections exist only for
// some species. An optional is a variant of "absent" and the correction, so a
// correction that is not applied has no settings to disagree with - which is what the
// independent flags it replaces could not guarantee.

enum class MultDependence { Binned, Integrated };

struct EfficiencyCorrection {
  MultDependence mult; // No default: it changes a number, so the JSON says it
};

// Always per multiplicity class, so there is nothing to choose.
struct SignalLossCorrection {};

struct ParticleCorrections {
  std::optional<EfficiencyCorrection> efficiency;
  std::optional<SignalLossCorrection> signalLoss;
  // Λ feed-down and π primary fraction belong here as well, once implemented. Until then
  // the parser refuses them instead of carrying a field nobody reads.
};

// What a bin at or below zero, or not a number, in a map that is applied does ('on_invalid_correction_bin'). Warn: the
// map is applied as it is, as before 27 October 2026, and every such map is a counted warning naming its bins; Stop:
// the run stops at loading.
enum class InvalidBinPolicy { Warn, Stop };

struct CorrectionsConfig {
  std::string file;
  std::map<std::string, ParticleCorrections> perParticle; // "Phi" or an associated species; absent = not corrected here
  bool eventLoss{false};                                  // Per event, not per particle
  InvalidBinPolicy onInvalidBin;                          // Set by InitCorrections from the required key
};

struct LoadedPurity {
  std::string name;
  std::vector<std::unique_ptr<TH1>> h1Purity;
};

struct ParticleTask {
  std::string purityKey;                                // e.g. "k0s", "pi_tpc"
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
  std::unique_ptr<TH2F> h2PurityCCDB;                   // (multiplicity, pT) purity at the source binning,
                                                        // written on its own as "ccdb_object" so O2 can
                                                        // apply it candidate by candidate

  // mappedSourceBins is NOT a parameter: it follows from the binnings and from the
  // name, so the caller cannot get it wrong or forget it, and the object is never
  // half-built. Note that member initialisers run in declaration order, which is why
  // it is declared after what it reads.
  ParticleTask(std::string purityKey_, std::unique_ptr<TH3F> h3Source_, std::vector<double> sourceBinning_,
               std::optional<std::vector<double>> rebinningPt_, std::unique_ptr<TH2F> h2PurityCCDB_)
    : purityKey(std::move(purityKey_)),
      h3Source(std::move(h3Source_)),
      sourceBinning(std::move(sourceBinning_)),
      rebinningPt(std::move(rebinningPt_)),
      mappedSourceBins(BinningUtils::MapToSourceBins(sourceBinning, AnalysisBinning())),
      h2PurityCCDB(std::move(h2PurityCCDB_))
  {
  }

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

// Structure to hold the loaded MC data in RAM
struct LoadedMC {
  std::string name;
  std::unique_ptr<TH3F> h3MCGen;
  std::unique_ptr<THnSparseF> h4MCGenAssocReco;
  std::unique_ptr<THnSparseF> h4MCReco;
  std::optional<std::vector<double>> rebinningPt;
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
  // Initialised so that a default-constructed FitConfig holds no indeterminate value.
  // No sensible default exists for the range, and FitConfigManager requires both keys.
  double min{0.0};
  double max{0.0};
};

struct ModelConfig {
  std::string sigModel;
  std::string bkgModel;
  std::map<std::string, MathParam> params; // Uses the unified MathParam
};

struct FixedRange {
  double min{0.0};
  double max{0.0};
};

struct NSigma {
  double n{3.0};
};

struct SidebandConfig {
  std::pair<double, double> range{0.0, 0.0};
  bool fromFit{true};
};

struct IntegrationConfig {
  std::variant<FixedRange, NSigma> signalWindow{FixedRange{0.0, 0.0}};
  bool snapToBin{false};
  bool calculatePurity{false};
  std::optional<SidebandConfig> sideband{std::nullopt};
};

struct FitConfig {
  ObservableConfig obs;
  ModelConfig model;
  IntegrationConfig integration;
  // The fit_setup.bins key that was applied over "default", empty for none: said by the manager that applied it, so
  // that a record of the fit names what was really used instead of rebuilding the key from indices.
  std::string binOverride;
};

// The raw facts of one fit (DynamicRooFitter::Diagnostics): from the RooFitResult of the winning start, and from
// DoFit the likelihood without offset and which start won. No verdict: what counts as a bad fit is
// FitQualityRecorder's, in one place.
struct FitDiagnostics {
  int status{-1};
  int covQual{-1};
  double edm{0.0};
  double minNll{0.0};
  int numInvalidNLL{0};
  double counts{0.0};               // what the likelihood saw: the sum of the binned data over the observable range
  // Floating parameters at a limit of their range, one list per end: within 1e-5 of the range (RooFitKnowledge,
  // note 09), a yield at its lower end below one count (DynamicRooFitter::DiagnosticsOf says why).
  std::vector<std::string> atLowerLimit;
  std::vector<std::string> atUpperLimit;
  std::vector<std::string> upperIsShape; // parameters whose model calls their upper limit a shape (see the DSCB's)
  std::string start;                // the DoFit start that won: "config", "yields" or "staged"
  double nllConfig{0.0};            // the NLL the "config" start reached (minNll is the winner's): never below it
};

// ============================================================================
// EXTRAPOLATION CONFIGURATION STRUCTURE
// ============================================================================
// One fit of a spectrum: what extrapConfig.json says per fit, the main one and "high".
struct ExtrapFit {
  std::string model;
  std::pair<double, double> fitRange{0.0, 0.0};
  std::map<std::string, MathParam> params; // Uses the unified MathParam
};

// The main fit, and what belongs to the species rather than to a fit.
struct ExtrapConfig : ExtrapFit {
  std::pair<double, double> domainRange{0.0, 15.0};
  double mass{0.0}; // Injected dynamically at runtime by the Task

  // When set, the part above the last measured point comes from this fit instead of the
  // main one, which then serves the part below the first point only. The two sides are
  // constrained by different data - the first bins and the last - and one function fitted
  // to both has to compromise between them; for the pi spectra it describes neither end
  // (DESIGN_NOTES.md section 6).
  std::optional<ExtrapFit> high;

  // The high fit as a configuration the model factory can build: this one with the fit
  // replaced, so that whatever belongs to the species comes along, including what is added
  // to it later. Only for a configuration that has a high fit.
  ExtrapConfig HighConfig() const
  {
    ExtrapConfig c = *this;
    static_cast<ExtrapFit&>(c) = *high;
    c.high.reset();
    return c;
  }
};

// The quality of one fit, so that the main and the high fit report it the same way.
struct FitQuality {
  double chi2{0.0};
  int ndf{0};

  // No threshold and no warning attached: what counts as an acceptable chi2 is a
  // physics judgement, and inventing a cut here would only hide it. It is printed
  // on every extrapolation instead.
  double ReducedChi2() const { return ndf > 0 ? chi2 / ndf : 0.0; }
};

struct ExtrapolationResult {
  double yield{0.0};
  double yieldStatErr{0.0};
  double meanPt{0.0};
  double meanPtStatErr{0.0};
  // Split by side. They are separate measurements of separate things: the low part
  // is what the fit says lies below the first measured point, the high part what lies
  // above the last, and the two are constrained by different data - the first bins for
  // one, the last for the other. Summed, a large low part hides a runaway high tail
  // and the other way round.
  //
  // The low part is also the one directly checkable: it is the same number as the
  // integral of the fitted function below the first measured point, computed by a
  // completely different route.
  double extrapolatedYieldLow{0.0};
  double extrapolatedYieldHigh{0.0};

  // Derived, so it cannot contradict the number it comes from.
  double ExtrapolatedYield() const { return extrapolatedYieldLow + extrapolatedYieldHigh; }
  double ExtrapolatedFraction() const { return yield > 0. ? ExtrapolatedYield() / yield : 0.0; }

  // Fit quality. Here because a converged fit is not a good fit, and roughly a
  // third of the yield is an integral of this function outside the measured range:
  // the yield alone gives no way to tell a description from an extrapolation of
  // nonsense. YieldMean printed it and SpectrumExtrapolator did not, so retiring
  // that path would have quietly removed the only warning there was.
  FitQuality fit;
  std::optional<FitQuality> highFit; // Only with a high fit (ExtrapConfig::high)

  // Systematic uncertainty from propagating the spectrum's own systematic band
  // through the extrapolation - NOT a systematic on the choice of extrapolation
  // model, which is a separate contribution and is not computed anywhere yet.
  //
  // Asymmetric because the two sides come from different variations and there is
  // no reason for them to be equal. Zero unless 'hasSystematics' is set: the
  // extrapolator only computes them when it is given a systematic spectrum, and a
  // systematic of exactly zero would otherwise be indistinguishable from one that
  // was never computed.
  double yieldSysHi{0.0};
  double yieldSysLo{0.0};
  double meanPtSysHi{0.0};
  double meanPtSysLo{0.0};
  bool hasSystematics{false};
};

struct YieldRatioConfig {
  std::string num;
  std::string den;
  std::string label;
};

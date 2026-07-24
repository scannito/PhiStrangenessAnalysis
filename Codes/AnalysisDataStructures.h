#pragma once

#include "AnalysisConstants.h"

#include "TCanvas.h"
#include "TH1.h"
#include "TH1F.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct AssocParticleConfig {
  std::string name;    // e.g., "K0S", "Pi"
  std::string dirName; // e.g., "phiK0S", "phiPi"
  int nBinPt;
  std::vector<double> binning;
  double mass;
};

struct LoadedCorrections {
  std::string name;
  std::vector<TH1F*> h1Corrections{nullptr};
  std::vector<TH1F*> h1CorrectionsEffMultInt{nullptr};
};

struct LoadedPurity {
  std::string name;
  std::vector<TH1*> h1Purity{nullptr};
};

struct ParticleTask {
  std::string name;            // e.g. "k0s", "pi_tpc"
  TH3F* h3Source;              // Pointer to the 3D source histogram in RAM
  int nBinPt;                  // Number of Pt bins
  std::vector<double> binning; // Binning for the final spectrum
  TFile* outputFile;           // Output file for this particle
  TCanvas* canvas;             // Summary canvas for plotting
};

template <size_t size>
struct ParticleConfig {
  std::string name;                     // e.g., "Phi", "K0S", "Pi"
  std::array<std::string, size> titles; // Es. {Gen, GenAssoc, Reco} o {Efficiency, SignalLoss}
};

struct LoadedAssocData {
  std::string name;
  THnSparseF* h5DataSignal{nullptr};
  THnSparseF* h5DataSideband{nullptr};
  THnSparseF* h5DataMESignal{nullptr};
  THnSparseF* h5DataMESideband{nullptr};
};

// Structure to hold the loaded MC data and canvases in RAM
struct LoadedMC {
  std::string name;
  TH3F* h3MCGen{nullptr};
  THnSparseF* h4MCGenAssocReco{nullptr};
  THnSparseF* h4MCReco{nullptr};

  std::optional<std::vector<double>> rebinningPt;

  TCanvas* canvasEfficiency{nullptr};
  TCanvas* canvasSignalLoss{nullptr};
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

struct YieldRatioConfig {
  std::string num;
  std::string den;
  std::string label;
};

#pragma once

#include "AnalysisConstants.h"

#include "TH1.h"
#include "TH1F.h"
#include "THnSparse.h"

#include <array>
#include <string>
#include <vector>

struct AssocParticleConfig {
  std::string name;    // e.g., "K0S", "Pi"
  std::string dirName; // e.g., "phiK0S", "phiPi"
  int nBinPt;
  // int effIndex; // Index in the corrections array (0 is Phi, 1 is K0S, 2 is Pi)
  std::vector<double> binning;
  double mass;
};

struct LoadedCorrections {
  std::string name;
  // std::array<TH1F*, AnalysisConstants::nBinMult> h1Corrections{nullptr};
  std::vector<TH1F*> h1Corrections{nullptr};
  // std::array<TH1F*, AnalysisConstants::nBinMult> h1CorrectionsEffMultInt{nullptr};
  std::vector<TH1F*> h1CorrectionsEffMultInt{nullptr};
};

struct LoadedPurity {
  std::string name;
  // std::array<TH1*, AnalysisConstants::nBinMult> h1Purity{nullptr};
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

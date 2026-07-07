#pragma once

#include "Rtypes.h" // Required for ROOT color definitions like kOrange

#include <array>
#include <utility>
#include <vector>

// Wrap everything in a namespace to avoid polluting the global scope
namespace AnalysisConstants
{
// --- Physics Constants ---
inline const std::map<std::string, double> particleMass{
  {"Phi", 1.019461}, // PDG mass of Phi meson in GeV/c^2
  {"K", 0.49367},    // PDG mass of charged kaon in GeV/c^2
  {"K0S", 0.497611}, // PDG mass of K0S in GeV/c^2
  {"Pi", 0.139570},  // PDG mass of charged pion in GeV/c^2
};

inline double GetMass(const std::string& name)
{
  auto it = particleMass.find(name);
  if (it == particleMass.end())
    throw std::runtime_error("[FATAL] AnalysisConstants: Unknown particle '" + name + "'");
  return it->second;
}

/*inline constexpr double kaonMass{0.49367}; // PDG mass of chaged kaon in GeV/c^2
inline constexpr double k0sMass{0.497611}; // PDG mass of K0S in GeV/c^2
inline constexpr double piMass{0.139570};  // PDG mass of charged pion in GeV/c^2*/

/*// --- Binning Dimensions ---
inline constexpr int nBinMult{10};
inline constexpr int nBinPtPhi{7};
inline constexpr int nBinPtK0S{9};
inline constexpr int nBinPtPi{10};*/
inline constexpr int nBinZVtx{100};
inline constexpr int nBinY{20};

/*// --- Binning Arrays ---
// Note: 'inline const' is required for objects like std::vector to avoid
// Multiple Definition Errors when included in multiple .cpp files.
inline const std::vector<double> binsMult{0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
inline const std::vector<double> binspTK0S{0.1, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0};
inline const std::vector<double> binspTPi{0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};*/

/*inline constexpr int nBinMult{1};
inline constexpr int nBinPtPhi{1};
inline constexpr int nBinPtK0S{1};
inline constexpr int nBinPtPi{1};
inline constexpr int nBinZVtx{100};
inline constexpr int nBinY{20};

inline const std::vector<double> binsMult{0.0, 100.0};
inline const std::vector<double> binspTK0S{0.1, 6.0};
inline const std::vector<double> binspTPi{0.2, 3.0};*/

// --- Analysis Ranges ---
inline const std::pair<double, double> phiMassSignalRange{1.0095, 1.029};
inline const std::pair<double, double> phiMassSidebandRange{1.1, 1.2};

// --- Plotting Styles ---
// inline constexpr std::array<int, 10> spectraColors{634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856};
// inline const std::vector<int> multTrendColors{kRed, kBlue, kGreen + 2};
} // namespace AnalysisConstants

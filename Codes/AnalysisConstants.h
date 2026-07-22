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

inline constexpr int nBinZVtx{100};
inline constexpr int nBinY{20};

// --- Analysis Ranges ---
inline const std::pair<double, double> phiMassSignalRange{1., 1.05};
inline const std::pair<double, double> phiMassSidebandRange{1.06, 1.08};
} // namespace AnalysisConstants

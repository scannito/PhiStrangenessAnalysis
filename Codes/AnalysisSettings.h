#pragma once

#include "Rtypes.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

struct AnalysisSettings {
  std::string binningName{"NewFullDifferential"}; // Name of the binning scheme (used for I/O directory naming)

  // Default historical values (fallback if JSON does not specify them)
  std::vector<double> binsMult{0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
  int nBinMult = GetBinCount(binsMult);

  std::map<std::string, std::vector<double>> speciesPtBinning{
    {"Phi", {0.4, 0.8, 1.4, 2.0, 2.8, 4.0, 6.0, 10.0}},
    {"K0S", {0.0, 0.3, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0}},
    {"Xi", {0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0}},
    {"Pi", {0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0}}};

  // Color palettes
  std::vector<int> spectraColors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856};
  std::vector<int> multTrendColors = {kRed, kBlue, kGreen + 2};

  void UpdateBinCounts()
  {
    nBinMult = GetBinCount(binsMult);
  }

  const std::vector<double>& GetPtBinning(const std::string& species) const
  {
    auto it = speciesPtBinning.find(species);
    if (it == speciesPtBinning.end())
      throw std::runtime_error("[FATAL] AnalysisSettings: No pT binning defined for species '" + species + "'");
    return it->second;
  }

  int GetNBinPt(const std::string& species) const { return GetBinCount(GetPtBinning(species)); }

  int GetSpectraColor(int index) const { return spectraColors[index % spectraColors.size()]; }
  int GetMultTrendColor(int index) const { return multTrendColors[index % multTrendColors.size()]; }

 private:
  static int GetBinCount(const std::vector<double>& vec) { return vec.empty() ? 0 : static_cast<int>(vec.size()) - 1; }
};

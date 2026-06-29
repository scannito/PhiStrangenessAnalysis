#pragma once

#include "Rtypes.h"

#include <vector>

struct AnalysisSettings {
  // Default historical values (fallback if JSON does not specify them)
  std::vector<double> binsMult{0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
  std::vector<double> binspTPhi{0.4, 0.8, 1.4, 2.0, 2.8, 4.0, 6.0, 10.0};
  std::vector<double> binspTK0S{0.1, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0};
  std::vector<double> binspTPi{0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};

  int nBinMult = GetBinCount(binsMult);
  int nBinPtPhi = GetBinCount(binspTPhi);
  int nBinPtK0S = GetBinCount(binspTK0S);
  int nBinPtPi = GetBinCount(binspTPi);

  // Safe color palettes
  std::vector<int> spectraColors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856};
  std::vector<int> multTrendColors = {kRed, kBlue, kGreen + 2};

  AnalysisSettings() = default;

  // Called by WorkflowManager AFTER overriding the vectors from JSON
  void UpdateBinCounts()
  {
    nBinMult = GetBinCount(binsMult);
    nBinPtPhi = GetBinCount(binspTPhi);
    nBinPtK0S = GetBinCount(binspTK0S);
    nBinPtPi = GetBinCount(binspTPi);
  }

  int GetSpectraColor(int index) const
  {
    return spectraColors[index % spectraColors.size()];
  }

  int GetMultTrendColor(int index) const
  {
    return multTrendColors[index % multTrendColors.size()];
  }

 private:
  static int GetBinCount(const std::vector<double>& vec)
  {
    return vec.empty() ? 0 : vec.size() - 1;
  }
};

#pragma once

#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace AnalysisUtils
{
// -----------------------------------------------------------------------------
// Math & Projection Utilities
// -----------------------------------------------------------------------------

// Computes the integral and error in a specific range for 1D histograms
inline std::pair<double, double> IntegralAndErrorPair(TH1* h1, double x1, double x2, Option_t* option = "")
{
  double integral{0.0};
  double error{0.0};
  double epsilon{0.00001};

  int binX1 = h1->GetXaxis()->FindBin(x1 + epsilon);
  int binX2 = h1->GetXaxis()->FindBin(x2 - epsilon);

  integral = h1->IntegralAndError(binX1, binX2, error, option);

  return std::make_pair(integral, error);
}

// Computes the integral and error in a specific range for 2D histograms
inline std::pair<double, double> IntegralAndErrorPair(TH2* h2, double x1, double x2, double y1, double y2, Option_t* option = "")
{
  double integral{0.0};
  double error{0.0};
  double epsilon{0.00001};

  int binX1 = h2->GetXaxis()->FindBin(x1 + epsilon);
  int binX2 = h2->GetXaxis()->FindBin(x2 - epsilon);
  int binY1 = h2->GetYaxis()->FindBin(y1 + epsilon);
  int binY2 = h2->GetYaxis()->FindBin(y2 - epsilon);

  integral = h2->IntegralAndError(binX1, binX2, binY1, binY2, error, option);

  return std::make_pair(integral, error);
}

// Struct required by the projection utility
struct AxisToCut {
  int axis;
  int binLow;
  int binUp;
};

// Projects a THnSparse into a lower-dimensional histogram
template <typename THType>
inline THType* projectTHnSparse(THnSparse* hnSparse,
                                const std::vector<AxisToCut>& axesToBeCut,
                                const std::vector<int>& axesToProject,
                                const std::string& histName)
{
  if (!hnSparse)
    return nullptr;

  for (const auto& axisToCut : axesToBeCut) {
    hnSparse->GetAxis(axisToCut.axis)->SetRange(axisToCut.binLow, axisToCut.binUp);
  }

  THType* hProjection = nullptr;

  if constexpr (std::is_base_of_v<TH3, THType>) {
    hProjection = hnSparse->Projection(axesToProject[0], axesToProject[1], axesToProject[2]);
  } else if constexpr (std::is_base_of_v<TH2, THType>) {
    hProjection = hnSparse->Projection(axesToProject[0], axesToProject[1]);
  } else if constexpr (std::is_base_of_v<TH1, THType>) {
    hProjection = hnSparse->Projection(axesToProject[0]);
  }

  if (hProjection) {
    hProjection->SetName(histName.c_str());
    hProjection->SetDirectory(0);
    hProjection->Sumw2();
  }

  return hProjection;
}

// -----------------------------------------------------------------------------
// pT and Multiplicity Trend Utilities
// -----------------------------------------------------------------------------

// Construct pT spectrum from a container of histograms
template <typename Container>
TH1* constructSpectrum(const Container& hContainer,
                       const std::vector<double>& binsVec,
                       const std::string& histName,
                       double absLimToIntegrate)
{
  if (binsVec.size() - 1 != hContainer.size()) {
    throw std::runtime_error("Size of histogram container must be equal to number of bins - 1");
  }

  TH1* hSpectrum = new TH1D(histName.c_str(), "; p_{T} (GeV/#it{c}); 1/N_{trig} d^{2}N/dp_{T}d#Deltay", binsVec.size() - 1, binsVec.data());

  for (size_t i{0}; i < hContainer.size(); i++) {
    auto [binContent, binError] = IntegralAndErrorPair(hContainer[i], -absLimToIntegrate, absLimToIntegrate);
    double normalizationFactor = (binsVec[i + 1] - binsVec[i]);

    hSpectrum->SetBinContent(i + 1, binContent / normalizationFactor);
    hSpectrum->SetBinError(i + 1, binError / normalizationFactor);
  }

  hSpectrum->SetDirectory(0);

  return hSpectrum;
}

// Construct multiplicity trends from pT spectra
void constructMultTrend(TH1* hMultTrend,
                        TH1* hPtSpectrum,
                        int i,
                        bool applyExtrapolation = false,
                        double totalYield = 0.0,
                        double totalError = 0.0
                        /*double extrapolatedYield = 0.0,
                        double extrapolatedError = 0.0*/
)
{
  /*auto [content, error] = IntegralAndErrorPair(hPtSpectrum, hPtSpectrum->GetXaxis()->GetXmin(), hPtSpectrum->GetXaxis()->GetXmax(), "width");

  if (applyExtrapolation)
  {
      content += extrapolatedYield;
      error = std::sqrt((error * error) + (extrapolatedError * extrapolatedError));
  }*/

  double content = 0.0;
  double error = 0.0;

  if (applyExtrapolation) {
    content = totalYield;
    error = totalError;
  } else {
    auto pair = IntegralAndErrorPair(hPtSpectrum, hPtSpectrum->GetXaxis()->GetXmin(), hPtSpectrum->GetXaxis()->GetXmax(), "width");
    content = pair.first;
    error = pair.second;
  }

  hMultTrend->SetBinContent(i + 1, content);
  hMultTrend->SetBinError(i + 1, error);
}

// -----------------------------------------------------------------------------
// Graphic Utilities
// -----------------------------------------------------------------------------

// Applies the standard visual style to 1D histograms
inline void SetHistogramStyle(TH1* h1, int color)
{
  h1->SetMarkerStyle(20);
  h1->SetMarkerColor(color);
  h1->SetMarkerSize(1.5);
  h1->SetLineColor(color);
  h1->SetLineWidth(2);
  h1->SetFillStyle(3001);
  h1->SetFillColor(color);
  h1->GetYaxis()->SetTitleSize(0.045);
  h1->GetYaxis()->SetTitleOffset(1.0);
  h1->GetYaxis()->SetLabelSize(0.045);
}

// ------------------------------------------------------------------------------
// TFile navigation utility
// ------------------------------------------------------------------------------

inline TDirectory* GetOrCreatePath(TDirectory* baseDir, const std::vector<std::string>& pathNodes, bool isReadOnly = false)
{
  if (!baseDir)
    return nullptr;

  TDirectory* currentDir = baseDir;

  for (const auto& node : pathNodes) {
    if (node.empty())
      continue;

    // Search for the subdirectory in the current level
    TDirectory* nextDir = currentDir->GetDirectory(node.c_str());
    if (!nextDir) {
      if (isReadOnly) {
        // In read-only mode, do not create anything; fail silently (Cache Miss)
        return nullptr;
      } else {
        nextDir = currentDir->mkdir(node.c_str());
      }

      // Safety check (e.g., disk full, file closed, permission denied)
      if (!nextDir) {
        std::cerr << "[ERROR] AnalysisUtils::GetOrCreatePath - Failed to create TDirectory '" << node << "'." << std::endl;
        return nullptr;
      }
    }

    // Move down one level for the next iteration
    currentDir = nextDir;
  }

  return currentDir;
}
} // namespace AnalysisUtils

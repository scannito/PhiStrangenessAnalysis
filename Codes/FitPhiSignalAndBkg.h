#pragma once

#include "AnalysisConstants.h"
#include "AnalysisUtils.h"

#include "TF1.h"
#include "TH1.h"
#include "TMath.h"
#include "TMatrixDSym.h"

#include <cmath>
#include <format>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace PhiFitModels
{
inline double Voigt(double* x, double* par)
{
  double mass = x[0];

  return par[0] * TMath::Voigt(mass - par[1], par[2], par[3]);
}

inline double BkgSourav(double* x, double* par)
{
  double mass = x[0];

  double arg = mass - 2 * AnalysisConstants::GetMass("K");
  if (arg < 0.0) {
    arg = 0.0;
  }

  return par[0] + par[1] * mass + par[2] * std::sqrt(arg);
}

inline double VoigtBkgSourav(double* x, double* par)
{
  return Voigt(x, &par[0]) + BkgSourav(x, &par[4]);
}

inline double BkgMattia(double* x, double* par)
{
  double mass = x[0];

  return par[0] * std::pow(mass - 2 * AnalysisConstants::GetMass("K"), par[1]) * std::exp(par[2] * (mass - 2 * AnalysisConstants::GetMass("K")) + par[3] * std::pow(mass - 2 * AnalysisConstants::GetMass("K"), 2) + par[4] * std::pow(mass - 2 * AnalysisConstants::GetMass("K"), 3));
}

inline double VoigtBkgMattia(double* x, double* par)
{
  return Voigt(x, &par[0]) + BkgMattia(x, &par[4]);
}

// Integration ranges of THIS fitter, not of the analysis: they are hardcoded
// only because this path is not configuration-driven. DynamicRooFitter takes the
// equivalent values from 'integration_range' and 'sideband_range' in the fit
// config, and if the two fitters are ever unified these move into FitConfig and
// disappear from here.
inline const std::pair<double, double> kPhiSignalRange{1.0, 1.05};
inline const std::pair<double, double> kPhiSidebandRange{1.06, 1.08};

} // namespace PhiFitModels

template <bool wSidebandFit>
class FitPhiSignalAndBkg
{
 public:
  FitPhiSignalAndBkg(TH1* h,
                     TF1* fitFunc,
                     int indexFirstBkgParam,
                     std::pair<double, double> signalRegion = PhiFitModels::kPhiSignalRange,
                     std::pair<double, double> sidebandRegion = PhiFitModels::kPhiSidebandRange)
    : h1(h), fitFunction(fitFunc)
  {
    // This class decomposes into Voigt (4 parameters) + BkgSourav (3), and those two
    // shapes are written below rather than derived from what was passed: whatever
    // 'fitFunc' holds, the pieces integrated afterwards are always those two. Handing
    // it VoigtBkgMattia would fit correctly and then decompose with the WRONG
    // background, and the yields would come out wrong with nothing to show for it.
    //
    // A TF1 cannot be asked which formula it carries, so the parameter layout is the
    // closest available proxy for the shape - and it catches the case that matters,
    // since VoigtBkgMattia has 9 parameters and not 7.
    //
    // Note what this makes explicit: 'indexFirstBkgParam' has exactly one legal value.
    // It is an argument that cannot vary, which is the definition of a parameter that
    // should not be one.
    if (fitFunction->GetNpar() != kNPars || indexFirstBkgParam != kNSignalPars) {
      throw std::runtime_error(std::format(
        "[FATAL] FitPhiSignalAndBkg: this class only decomposes Voigt + BkgSourav, which is "
        "{} parameters with the background starting at index {}. It was given {} parameters "
        "with indexFirstBkgParam = {}. For any other model use DynamicRooFitter, which builds "
        "the components from the configuration instead of assuming them.",
        kNPars, kNSignalPars, fitFunction->GetNpar(), indexFirstBkgParam));
    }

    double binWidth = h1->GetXaxis()->GetBinWidth(1);

    TFitResultPtr fitResult = h1->Fit(fitFunction, "RS");
    TMatrixDSym covMatrix = fitResult->GetCovarianceMatrix();

    // Owned here until they are handed to the histogram below. They used to be raw
    // 'new TF1' whose clones went to the histogram while the originals were never
    // deleted - nBinMult x nBinPt of them per run, all sharing two names in ROOT's
    // global function list.
    std::unique_ptr<TF1> signalFunction = std::make_unique<TF1>("Voigt", PhiFitModels::Voigt, signalRegion.first, signalRegion.second, kNSignalPars);
    signalFunction->SetLineColor(kBlue);
    std::unique_ptr<TF1> bkgFunction = std::make_unique<TF1>("Bkg", PhiFitModels::BkgSourav, signalRegion.first, signalRegion.second, kNBkgPars);
    bkgFunction->SetLineColor(kGreen + 2);

    TMatrixDSym covSignal(indexFirstBkgParam);
    TMatrixDSym covBkg(fitFunction->GetNpar() - indexFirstBkgParam);

    for (int i = 0; i < indexFirstBkgParam; i++) {
      signalFunction->SetParameter(i, fitFunction->GetParameter(i));
      for (int j = 0; j < indexFirstBkgParam; j++) {
        covSignal(i, j) = covMatrix(i, j);
      }
    }

    for (int i = indexFirstBkgParam; i < fitFunction->GetNpar(); i++) {
      bkgFunction->SetParameter(i - indexFirstBkgParam, fitFunction->GetParameter(i));
      for (int j = indexFirstBkgParam; j < fitFunction->GetNpar(); j++) {
        covBkg(i - indexFirstBkgParam, j - indexFirstBkgParam) = covMatrix(i, j);
      }
    }

    signalIntegralAndError.first = signalFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
    signalIntegralAndError.second = signalFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covSignal.GetMatrixArray()) / binWidth;

    bkgIntegralAndErrorInSigRegion.first = bkgFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
    bkgIntegralAndErrorInSigRegion.second = bkgFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covBkg.GetMatrixArray()) / binWidth;

    if constexpr (wSidebandFit) {
      bkgIntegralAndErrorInSideRegion.first = bkgFunction->Integral(sidebandRegion.first, sidebandRegion.second) / binWidth;
      bkgIntegralAndErrorInSideRegion.second = bkgFunction->IntegralError(sidebandRegion.first, sidebandRegion.second, fitFunction->GetParameters(), covBkg.GetMatrixArray()) / binWidth;
    } else {
      bkgIntegralAndErrorInSideRegion = AnalysisUtils::IntegralAndErrorPair(h1, sidebandRegion.first, sidebandRegion.second);
    }

    // Handed over last, once nothing here needs them any more. TH1 owns what goes into
    // its function list and deletes it with itself, so releasing is the transfer: the
    // caller gets the same two curves drawn with the histogram as before, and nothing
    // is left behind. Previously these were clones and the originals were the leak.
    h1->GetListOfFunctions()->Add(signalFunction.release());
    h1->GetListOfFunctions()->Add(bkgFunction.release());
  }

  std::pair<double, double> GetSignalAndError() const { return signalIntegralAndError; }
  std::pair<double, double> GetBkgInSigRegionAndError() const { return bkgIntegralAndErrorInSigRegion; }
  std::pair<double, double> GetBkgInSideRegionAndError() const { return bkgIntegralAndErrorInSideRegion; }

 private:
  // The shape this class can decompose, spelled out because the decomposition below
  // assumes it rather than reading it: Voigt has 4 parameters, BkgSourav 3.
  static constexpr int kNSignalPars = 4;
  static constexpr int kNBkgPars = 3;
  static constexpr int kNPars = kNSignalPars + kNBkgPars;

  TH1* h1{nullptr};
  TF1* fitFunction{nullptr};

  std::pair<double, double> signalIntegralAndError{0.0, 0.0};
  std::pair<double, double> bkgIntegralAndErrorInSigRegion{0.0, 0.0};
  std::pair<double, double> bkgIntegralAndErrorInSideRegion{0.0, 0.0};
};

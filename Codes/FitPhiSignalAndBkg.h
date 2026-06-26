#include "AnalysisConstants.h"
#include "AnalysisUtils.h"

inline double Voigt(double* x, double* par)
{
  double mass = x[0];

  return par[0] * TMath::Voigt(mass - par[1], par[2], par[3]);
}

inline double BkgSourav(double* x, double* par)
{
  double mass = x[0];

  double arg = mass - 2 * AnalysisConstants::kaonMass;
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

  return par[0] * std::pow(mass - 2 * AnalysisConstants::kaonMass, par[1]) * std::exp(par[2] * (mass - 2 * AnalysisConstants::kaonMass) + par[3] * std::pow(mass - 2 * AnalysisConstants::kaonMass, 2) + par[4] * std::pow(mass - 2 * AnalysisConstants::kaonMass, 3));
}

inline double VoigtBkgMattia(double* x, double* par)
{
  return Voigt(x, &par[0]) + BkgMattia(x, &par[4]);
}

template <bool wSidebandFit>
class FitPhiSignalAndBkg
{
 public:
  FitPhiSignalAndBkg(TH1* h,
                     TF1* fitFunc,
                     int indexFirstBkgParam,
                     std::pair<double, double> signalRegion,
                     std::pair<double, double> sidebandRegion)
    : h1(h), fitFunction(fitFunc)
  {
    double binWidth = h1->GetXaxis()->GetBinWidth(1);

    TFitResultPtr fitResult = h1->Fit(fitFunction, "RS");
    TMatrixDSym covMatrix = fitResult->GetCovarianceMatrix();

    // h1->GetListOfFunctions()->Add(fitFunction->Clone());

    TF1* signalFunction = new TF1("Voigt", Voigt, signalRegion.first, signalRegion.second, 4);
    signalFunction->SetLineColor(kBlue);
    TF1* bkgFunction = new TF1("Bkg", BkgSourav, signalRegion.first, signalRegion.second, 3);
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

    h1->GetListOfFunctions()->Add(signalFunction->Clone());
    h1->GetListOfFunctions()->Add(bkgFunction->Clone());

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

    std::cout << "Signal Integral: " << signalIntegralAndError.first << " +/- " << signalIntegralAndError.second << std::endl;
    std::cout << "Bkg Integral in Signal Region: " << bkgIntegralAndErrorInSigRegion.first << " +/- " << bkgIntegralAndErrorInSigRegion.second << std::endl;
    std::cout << "Bkg Integral in Sideband Region: " << bkgIntegralAndErrorInSideRegion.first << " +/- " << bkgIntegralAndErrorInSideRegion.second << std::endl;
  }

  double GetSignal() const { return signalIntegralAndError.first; }
  double GetSignalError() const { return signalIntegralAndError.second; }
  std::pair<double, double> GetSignalAndError() const { return signalIntegralAndError; }

  double GetBkgInSigRegion() const { return bkgIntegralAndErrorInSigRegion.first; }
  double GetBkgInSigRegionError() const { return bkgIntegralAndErrorInSigRegion.second; }
  std::pair<double, double> GetBkgInSigRegionAndError() const { return bkgIntegralAndErrorInSigRegion; }

  double GetBkgInSideRegion() const { return bkgIntegralAndErrorInSideRegion.first; }
  double getBkgInSideRegionError() const { return bkgIntegralAndErrorInSideRegion.second; }
  std::pair<double, double> GetBkgInSideRegionAndError() const { return bkgIntegralAndErrorInSideRegion; }

 private:
  TH1* h1{nullptr};
  TF1* fitFunction{nullptr};

  std::pair<double, double> signalIntegralAndError{0.0, 0.0};
  std::pair<double, double> bkgIntegralAndErrorInSigRegion{0.0, 0.0};
  std::pair<double, double> bkgIntegralAndErrorInSideRegion{0.0, 0.0};
};

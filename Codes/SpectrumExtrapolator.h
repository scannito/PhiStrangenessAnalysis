#pragma once

#include "TF1.h"
#include "TH1.h"
#include "TMath.h"
#include "TROOT.h"
#include "TRandom3.h"
#include "TVirtualFitter.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class SpectrumExtrapolator
{
 public:
  struct ExtrapolationResult {
    double yield{0.0};
    double yieldStatErr{0.0};
    double meanPt{0.0};
    double meanPtStatErr{0.0};
    double extrapolatedFraction{0.0};
  };

  // Constructor: Takes the measured spectrum and the initialized fit function (e.g., Levy-Tsallis)
  SpectrumExtrapolator(TH1* measuredSpectrum, TF1* fitModel)
    : fMeasuredSpectrum(measuredSpectrum)
  {
    if (!fMeasuredSpectrum || !fitModel) {
      throw std::invalid_argument("[SpectrumExtrapolator] Invalid input pointers.");
    }

    // Clone the fit model to avoid polluting the global namespace or the user's object
    fFitModel = static_cast<TF1*>(fitModel->Clone(Form("%s_clone", fitModel->GetName())));
    fRandomGen.SetSeed(0); // Randomize seed
  }

  // Configuration Setters
  void SetExtrapolationLimits(double minPt, double maxPt)
  {
    fMinPt = minPt;
    fMaxPt = maxPt;
  }

  void SetFitRange(double minFit, double maxFit)
  {
    fMinFit = minFit;
    fMaxFit = maxFit;
  }

  void SetIntegrationPrecisions(double lowPrecision, double highPrecision)
  {
    fLoPrecision = lowPrecision;
    fHiPrecision = highPrecision;
  }

  void SetToyMCTrials(int nTrialsCoarse, int nTrialsFine)
  {
    fTrialsCoarse = nTrialsCoarse;
    fTrialsFine = nTrialsFine;
  }

  // Main execution method
  ExtrapolationResult CalculateYieldAndMean()
  {
    ExtrapolationResult res;

    // 1. Fit the measured data
    TVirtualFitter::SetMaxIterations(1000000);
    int fitres;
    int trials = 0;
    do {
      fitres = fMeasuredSpectrum->Fit(fFitModel, "R0V", "", fMinFit, fMaxFit);
      trials++;
      if (trials > 10) {
        std::cerr << "[WARNING] SpectrumExtrapolator: Fit did not converge after 10 trials!" << std::endl;
        break;
      }
    } while (fitres != 0);

    // 2. Extrapolate standard values
    auto hlo = CreateLowExtrapolationHisto(fMeasuredSpectrum, fFitModel);
    auto hhi = CreateHighExtrapolationHisto(fMeasuredSpectrum, fFitModel);

    double integral = 0.0, mean = 0.0, extra = 0.0;
    Integrate(fMeasuredSpectrum, hlo.get(), hhi.get(), integral, mean, extra);

    res.yield = integral;
    res.meanPt = mean;
    res.extrapolatedFraction = (integral > 0) ? (extra / integral) : 0.0;

    // 3. Statistical Error Evaluation via Toy MC
    // 3a. Coarse phase (to find the bounds for the histograms)
    std::unique_ptr<TH1F> hIntegral_tmp = std::make_unique<TH1F>("hInt_tmp", "", 1000, 0.5 * integral, 1.5 * integral);
    std::unique_ptr<TH1F> hMean_tmp = std::make_unique<TH1F>("hMean_tmp", "", 1000, 0.5 * mean, 1.5 * mean);

    for (int irnd = 0; irnd < fTrialsCoarse; irnd++) {
      auto hrnd = ReturnRandomHisto(fMeasuredSpectrum);
      auto hrndlo = ReturnCoherentRandomHisto(hlo.get());
      auto hrndhi = ReturnCoherentRandomHisto(hhi.get());

      double tmpInt, tmpMean, tmpExt;
      Integrate(hrnd.get(), hrndlo.get(), hrndhi.get(), tmpInt, tmpMean, tmpExt);

      hIntegral_tmp->Fill(tmpInt);
      hMean_tmp->Fill(tmpMean);
    }

    // 3b. Fine phase (actual evaluation)
    std::unique_ptr<TH1F> hIntegral = std::make_unique<TH1F>("hInt", "", 100,
                                                             hIntegral_tmp->GetMean() - 5. * hIntegral_tmp->GetRMS(),
                                                             hIntegral_tmp->GetMean() + 5. * hIntegral_tmp->GetRMS());

    std::unique_ptr<TH1F> hMean = std::make_unique<TH1F>("hMean", "", 100,
                                                         hMean_tmp->GetMean() - 5. * hMean_tmp->GetRMS(),
                                                         hMean_tmp->GetMean() + 5. * hMean_tmp->GetRMS());

    for (int irnd = 0; irnd < fTrialsFine; irnd++) {
      auto hrnd = ReturnRandomHisto(fMeasuredSpectrum);
      auto hrndlo = ReturnCoherentRandomHisto(hlo.get());
      auto hrndhi = ReturnCoherentRandomHisto(hhi.get());

      double tmpInt, tmpMean, tmpExt;
      Integrate(hrnd.get(), hrndlo.get(), hrndhi.get(), tmpInt, tmpMean, tmpExt);

      hIntegral->Fill(tmpInt);
      hMean->Fill(tmpMean);
    }

    // 4. Extract Gaussian widths as the statistical uncertainties
    TF1* gaus = static_cast<TF1*>(gROOT->GetFunction("gaus"));

    if (hIntegral->GetEntries() > 0) {
      hIntegral->Fit(gaus, "0Q");
      res.yieldStatErr = res.yield * gaus->GetParameter(2) / gaus->GetParameter(1);
    }

    if (hMean->GetEntries() > 0) {
      hMean->Fit(gaus, "0Q");
      res.meanPtStatErr = res.meanPt * gaus->GetParameter(2) / gaus->GetParameter(1);
    }

    return res;
  }

  // Systematic variations (Caller takes ownership of the returned pointers)
  TH1* GetExtremeHighHisto() const
  {
    TH1* hout = static_cast<TH1*>(fMeasuredSpectrum->Clone(Form("%s_extremehigh", fMeasuredSpectrum->GetName())));
    for (int ibin = 0; ibin < fMeasuredSpectrum->GetNbinsX(); ibin++) {
      if (fMeasuredSpectrum->GetBinError(ibin + 1) <= 0.)
        continue;
      hout->SetBinContent(ibin + 1, fMeasuredSpectrum->GetBinContent(ibin + 1) + fMeasuredSpectrum->GetBinError(ibin + 1));
    }
    return hout;
  }

  TH1* GetExtremeLowHisto() const
  {
    TH1* hout = static_cast<TH1*>(fMeasuredSpectrum->Clone(Form("%s_extremelow", fMeasuredSpectrum->GetName())));
    for (int ibin = 0; ibin < fMeasuredSpectrum->GetNbinsX(); ibin++) {
      if (fMeasuredSpectrum->GetBinError(ibin + 1) <= 0.)
        continue;
      hout->SetBinContent(ibin + 1, fMeasuredSpectrum->GetBinContent(ibin + 1) - fMeasuredSpectrum->GetBinError(ibin + 1));
    }
    return hout;
  }

  TH1* GetExtremeSoftHisto() const
  {
    return ReturnExtremeHisto(fMeasuredSpectrum, -1.0);
  }

  TH1* GetExtremeHardHisto() const
  {
    return ReturnExtremeHisto(fMeasuredSpectrum, 1.0);
  }

 private:
  TH1* fMeasuredSpectrum{nullptr};
  TF1* fFitModel{nullptr}; // Cloned internally to avoid modifying the user's original object

  TRandom3 fRandomGen;

  // Extrapolation Ranges
  double fMinPt{0.0};
  double fMaxPt{10.0};
  double fMinFit{0.0};
  double fMaxFit{10.0};

  // Integration parameters
  double fLoPrecision{0.01};
  double fHiPrecision{0.1};

  // Toy MC parameters
  int fTrialsCoarse{100};
  int fTrialsFine{1000};

  // Internal Utility Methods
  void Integrate(TH1* hdata, TH1* hlo, TH1* hhi, double& integral, double& mean, double& extra) const
  {
    double I = 0., IX = 0., E = 0.;

    // Data integration
    for (int ibin = 0; ibin < hdata->GetNbinsX(); ibin++) {
      double cent = hdata->GetBinCenter(ibin + 1);
      double width = hdata->GetBinWidth(ibin + 1);
      double cont = width * hdata->GetBinContent(ibin + 1);
      if (hdata->GetBinError(ibin + 1) <= 0.)
        continue;
      I += cont;
      IX += cont * cent;
    }

    // Low integration
    if (hlo) {
      for (int ibin = 0; ibin < hlo->GetNbinsX(); ibin++) {
        double cent = hlo->GetBinCenter(ibin + 1);
        double width = hlo->GetBinWidth(ibin + 1);
        double cont = width * hlo->GetBinContent(ibin + 1);
        if (hlo->GetBinError(ibin + 1) <= 0.)
          continue;
        I += cont;
        IX += cont * cent;
        E += cont;
      }
    }

    // High integration
    if (hhi) {
      for (int ibin = 0; ibin < hhi->GetNbinsX(); ibin++) {
        double cent = hhi->GetBinCenter(ibin + 1);
        double width = hhi->GetBinWidth(ibin + 1);
        double cont = width * hhi->GetBinContent(ibin + 1);
        if (hhi->GetBinError(ibin + 1) <= 0.)
          continue;
        I += cont;
        IX += cont * cent;
        E += cont;
      }
    }

    integral = I;
    mean = (I > 0) ? (IX / I) : 0.0;
    extra = E;
  }

  std::unique_ptr<TH1> CreateLowExtrapolationHisto(TH1* h, TF1* f) const
  {
    int binlo = 1;
    double lo = fMinPt;
    for (int ibin = 1; ibin <= h->GetNbinsX(); ibin++) {
      if (h->GetBinContent(ibin) != 0.) {
        binlo = ibin;
        lo = h->GetBinLowEdge(ibin);
        break;
      }
    }

    int nbins = static_cast<int>((lo - fMinPt) / fLoPrecision);
    if (nbins < 1)
      return nullptr;

    auto hlo = std::make_unique<TH1F>("hlo", "", nbins, fMinPt, lo);

    for (int ibin = 0; ibin < hlo->GetNbinsX(); ibin++) {
      double width = hlo->GetBinWidth(ibin + 1);
      double cont = f->Integral(hlo->GetBinLowEdge(ibin + 1), hlo->GetBinLowEdge(ibin + 2), 1.e-6);
      double err = f->IntegralError(hlo->GetBinLowEdge(ibin + 1), hlo->GetBinLowEdge(ibin + 2), nullptr, nullptr, 1.e-6);

      hlo->SetBinContent(ibin + 1, cont / width);
      hlo->SetBinError(ibin + 1, err / width);
    }
    return hlo;
  }

  std::unique_ptr<TH1> CreateHighExtrapolationHisto(TH1* h, TF1* f) const
  {
    int binhi = h->GetNbinsX();
    double hi = fMaxPt;
    for (int ibin = h->GetNbinsX(); ibin > 0; ibin--) {
      if (h->GetBinContent(ibin) != 0.) {
        binhi = ibin + 1;
        hi = h->GetBinLowEdge(ibin + 1);
        break;
      }
    }

    if (fMaxPt < hi) {
      std::cerr << "[WARNING] Extrapolation max < highest non-empty bin." << std::endl;
      return nullptr;
    }

    int nbins = static_cast<int>((fMaxPt - hi) / fHiPrecision);
    if (nbins < 1)
      return nullptr;

    auto hhi = std::make_unique<TH1F>("hhi", "", nbins, hi, fMaxPt);

    for (int ibin = 0; ibin < hhi->GetNbinsX(); ibin++) {
      double width = hhi->GetBinWidth(ibin + 1);
      double cont = f->Integral(hhi->GetBinLowEdge(ibin + 1), hhi->GetBinLowEdge(ibin + 2), 1.e-6);
      double err = f->IntegralError(hhi->GetBinLowEdge(ibin + 1), hhi->GetBinLowEdge(ibin + 2), nullptr, nullptr, 1.e-6);

      hhi->SetBinContent(ibin + 1, cont / width);
      hhi->SetBinError(ibin + 1, err / width);
    }
    return hhi;
  }

  std::unique_ptr<TH1> ReturnRandomHisto(TH1* hin)
  {
    if (!hin)
      return nullptr;
    auto hout = std::unique_ptr<TH1>(static_cast<TH1*>(hin->Clone("hout_rnd")));
    hout->Reset();

    for (int ibin = 0; ibin < hin->GetNbinsX(); ibin++) {
      if (hin->GetBinError(ibin + 1) <= 0.)
        continue;
      double cont = hin->GetBinContent(ibin + 1);
      double err = hin->GetBinError(ibin + 1);
      hout->SetBinContent(ibin + 1, fRandomGen.Gaus(cont, err));
      hout->SetBinError(ibin + 1, err);
    }
    return hout;
  }

  std::unique_ptr<TH1> ReturnCoherentRandomHisto(TH1* hin)
  {
    if (!hin)
      return nullptr;
    auto hout = std::unique_ptr<TH1>(static_cast<TH1*>(hin->Clone("hout_cohrnd")));
    hout->Reset();

    double cohe = fRandomGen.Gaus(0., 1.);
    for (int ibin = 0; ibin < hin->GetNbinsX(); ibin++) {
      if (hin->GetBinError(ibin + 1) <= 0.)
        continue;
      double cont = hin->GetBinContent(ibin + 1);
      double err = hin->GetBinError(ibin + 1);
      hout->SetBinContent(ibin + 1, cont + cohe * err);
      hout->SetBinError(ibin + 1, err);
    }
    return hout;
  }

  TH1* ReturnExtremeHisto(TH1* hin, float sign) const
  {
    double ptlow = 0.0, pthigh = 0.0;
    for (int ibin = 0; ibin < hin->GetNbinsX(); ibin++) {
      if (hin->GetBinError(ibin + 1) > 0.) {
        ptlow = hin->GetBinLowEdge(ibin + 1);
        break;
      }
    }
    for (int ibin = hin->GetNbinsX(); ibin >= 0; ibin--) {
      if (hin->GetBinError(ibin + 1) > 0.) {
        pthigh = hin->GetBinLowEdge(ibin + 2);
        break;
      }
    }

    double mean = hin->GetMean();
    double maxdiff = 0.;
    TH1* hmax = nullptr;

    for (int inode = 0; inode < hin->GetNbinsX(); inode++) {
      double ptnode = hin->GetBinCenter(inode + 1);
      std::unique_ptr<TH1> hout(static_cast<TH1*>(hin->Clone("tmp_extreme")));

      for (int ibin = 0; ibin < hin->GetNbinsX(); ibin++) {
        if (hin->GetBinError(ibin + 1) <= 0.)
          continue;
        double val = hin->GetBinContent(ibin + 1);
        double err = hin->GetBinError(ibin + 1);
        double cen = hin->GetBinCenter(ibin + 1);

        if (cen < ptnode)
          err *= -1. + (cen - ptlow) / (ptnode - ptlow);
        else
          err *= (cen - ptnode) / (pthigh - ptnode);

        hout->SetBinContent(ibin + 1, val + sign * err);
      }

      double diff = TMath::Abs(mean - hout->GetMean());
      if (diff > maxdiff) {
        if (hmax)
          delete hmax;
        hmax = static_cast<TH1*>(hout->Clone(Form("%s_extreme_sys", hin->GetName())));
        maxdiff = diff;
      }
    }
    return hmax; // The caller must take ownership of this pointer!
  }
};

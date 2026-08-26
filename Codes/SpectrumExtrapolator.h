#pragma once

#include "AnalysisDataStructures.h"

#include "TF1.h"
#include "TH1.h"
#include "TMath.h"
#include "TROOT.h"
#include "TRandom3.h"
#include "TString.h"
#include "TVirtualFitter.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Replaces the ALICE-standard YieldMean, kept as OldCodes/YieldMean.h. The two were
// run side by side until they agreed on yield, mean pT and chi2 to every printed
// digit; DESIGN_NOTES.md records the numbers and the seven differences that had to
// be closed to get there.
//
// That history is what the comments below marked "as YieldMean" are for: each one
// is a value that looks arbitrary and is not, because it was matched to the
// reference. Changing one changes published-style numbers, so it wants a measured
// reason rather than a tidier-looking one. The reference is still in the repository
// if a claim here ever needs checking.
class SpectrumExtrapolator
{
 public:
  // Takes the measured spectrum and the initialised fit function (e.g. Levy-Tsallis).
  //
  // NOTE: 'fitModel' IS FITTED IN PLACE, as YieldMean did. It used to be cloned,
  // to protect the caller's object, but the caller creates it fresh for each
  // spectrum and wants the fitted parameters back - it prints them and writes the
  // curve alongside the spectrum. With a clone it silently got the initial guess
  // instead, in the debug output and in the saved plots. The clone also leaked (no
  // destructor) and, since every TF1 registers in gROOT's global function list,
  // added one more object under the same name per spectrum - the same trap as the
  // TCanvas one recorded in CLAUDE.md.
  SpectrumExtrapolator(TH1* measuredSpectrum, TF1* fitModel)
    : fMeasuredSpectrum(measuredSpectrum), fFitModel(fitModel)
  {
    if (!fMeasuredSpectrum || !fFitModel) {
      throw std::invalid_argument("[SpectrumExtrapolator] Invalid input pointers.");
    }

    // Was SetSeed(0), which in ROOT does not mean "seed zero": it draws one from
    // TUUID. The toy MC - and with it the quoted statistical error - therefore
    // changed on every run of the same analysis over the same files. A published
    // number that moves when you re-run it is a problem on its own.
    fRandomGen.SetSeed(kDefaultSeed);
  }

  // No FittedModel() accessor: the fit happens in place, so the caller already
  // holds the fitted function - it is the TF1 it passed in.
  bool FitConverged() const { return fFitConverged; }

  // Configuration Setters
  void SetExtrapolationLimits(double minPt, double maxPt)
  {
    fMinPt = minPt;
    fMaxPt = maxPt;
  }

  // Settable because it was a parameter of YieldMean rather than a constant in it,
  // and the default is the value it was called with - so a caller that says nothing
  // reproduces the reference, and diverging takes a deliberate call.
  //
  // "0QI": 0 do not draw, Q quiet, and I compare the INTEGRAL of the function over
  // each bin with the bin content rather than its value at the centre. On a
  // steeply falling spectrum with wide bins the two differ, and the extrapolated
  // yield is the integral of the fitted function below the first measured point.
  void SetFitOption(std::string option) { fFitOption = std::move(option); }

  // Passing 0 restores ROOT's non-reproducible behaviour, which is a legitimate
  // thing to want - checking that a result does not depend on the stream - but it
  // has to be asked for.
  void SetRandomSeed(unsigned int seed) { fRandomGen.SetSeed(seed); }

  // The systematic band of the spectrum.
  //
  // CONTRACT: this is the SPECTRUM, not the errors. Bin contents are the measured
  // values, the same ones as the spectrum passed to the constructor; bin ERRORS are
  // the systematic uncertainty. Handing over a histogram whose contents are the
  // systematic errors compiles, runs, and produces wrong numbers in silence -
  // MakeTotalErrorSpectrum only reads the errors, but the four variations read the
  // contents too, and would end up computing "systematic plus systematic".
  //
  // Not checked, deliberately. Comparing the contents against the measured spectrum
  // needs a tolerance, and the right tolerance depends on how the systematic band is
  // produced - by a driver that does not exist yet. A check written now would be a
  // guess, and the first false alarm would widen it into uselessness. It belongs
  // with the producer. The bin count IS checked, in ComputeSystematics, because that
  // is structural and holds whoever produces it.
  //
  // Optional, and nullptr is the normal state today: nothing in this chain produces
  // systematic uncertainties (DESIGN_NOTES.md). Without it the four variations are
  // skipped, the central value is fitted with statistical errors only, and the
  // result reports hasSystematics == false - rather than four zeros that would read
  // as "measured and found to be negligible".
  void SetSystematicSpectrum(TH1* sysSpectrum) { fSysSpectrum = sysSpectrum; }

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

  // Does everything the class exists for: fits the spectrum, integrates it together
  // with the extrapolated regions, estimates the statistical uncertainty by toy MC
  // and, if a systematic spectrum was supplied, propagates the systematic band
  // through all of it.
  ExtrapolationResult Extrapolate()
  {
    ExtrapolationResult res;

    // The domain has to cover the fit range, as in YieldMean. Integrating only up
    // to fMaxPt while the fit was performed up to a larger fMaxFit would drop the
    // measured region between the two from the yield - silently, since both
    // numbers are legitimate on their own. Widening rather than throwing because
    // it is what the reference does, and the configurations rely on it.
    const double minPt = std::min(fMinPt, fMinFit);
    const double maxPt = std::max(fMaxPt, fMaxFit);

    // 1. The central value, from a fit weighted with the TOTAL error.
    //
    // This is why YieldMean fits twice. The measurement is the fit that accounts
    // for everything known about the uncertainty of each point, so the central
    // value comes from stat (+) sys in quadrature. The toy MC below must instead
    // start from a fit weighted with the statistical error ALONE, or the
    // "statistical" uncertainty it returns would carry the systematic inside it.
    //
    // Without a systematic spectrum the two are the same histogram and the second
    // fit is the first repeated - which is the state this class was written in, and
    // the reason a single fit was correct until now.
    std::unique_ptr<TH1> hTotal = MakeTotalErrorSpectrum();
    TH1* hCentral = hTotal ? hTotal.get() : fMeasuredSpectrum;

    fFitConverged = FitOrWarn(hCentral);

    res.chi2 = fFitModel->GetChisquare();
    res.ndf = fFitModel->GetNDF();

    // 2. Extrapolate standard values
    auto hlo = CreateLowExtrapolationHisto(hCentral, fFitModel, minPt);
    auto hhi = CreateHighExtrapolationHisto(hCentral, fFitModel, maxPt);

    double integral = 0.0, mean = 0.0, extraLow = 0.0, extraHigh = 0.0;
    Integrate(hCentral, hlo.get(), hhi.get(), integral, mean, extraLow, extraHigh);

    res.yield = integral;
    res.meanPt = mean;
    res.extrapolatedYieldLow = extraLow;
    res.extrapolatedYieldHigh = extraHigh;

    // Independent of the fit status, because a zero contribution from a region
    // that exists is the observable symptom whatever caused it. Checked here and
    // not left to the reader of the log: 'extra == 0' is indistinguishable from a
    // legitimately tiny extrapolation unless you compare the yield against the raw
    // integral by eye, which is how this went unnoticed.
    // Per side, now that the two are separate: a region that exists and contributes
    // exactly zero is the observable symptom, and checking the sum would let one side
    // hide behind the other.
    const auto warnIfEmpty = [this](const char* side, const TH1* region, double contribution) {
      if (region && contribution == 0.0)
        std::cerr << "[WARNING] SpectrumExtrapolator: the " << side << " extrapolation region of '"
                  << fMeasuredSpectrum->GetName()
                  << "' was built but contributed exactly zero, so every one of its bins was skipped "
                     "for having a non-positive error. This is what a failed fit looks like downstream."
                  << std::endl;
    };
    warnIfEmpty("low", hlo.get(), extraLow);
    warnIfEmpty("high", hhi.get(), extraHigh);

    // 3. Statistical Error Evaluation via Toy MC
    //
    // Refit on the statistical spectrum, and rebuild the extrapolation histograms
    // from it: the toys sample these, so their errors have to be the statistical
    // ones. Skipped when there is no systematic spectrum, because hCentral is then
    // fMeasuredSpectrum and this would refit the same data a second time - which is
    // what the reference does, harmlessly, and what produced its duplicated 'hlo'
    // and 'hhi' warnings.
    if (hTotal) {
      FitOrWarn(fMeasuredSpectrum);
      hlo = CreateLowExtrapolationHisto(fMeasuredSpectrum, fFitModel, minPt);
      hhi = CreateHighExtrapolationHisto(fMeasuredSpectrum, fFitModel, maxPt);
    }

    // 3a. Coarse phase (to find the bounds for the histograms)
    // 0.75 to 1.25, as YieldMean: with 1000 bins either way, a wider window means
    // wider bins and a coarser sampled distribution, which moves the uncertainty
    // read off it. A constant in the reference too, so it is matched here rather
    // than exposed.
    std::unique_ptr<TH1F> hIntegral_tmp = std::make_unique<TH1F>("hInt_tmp", "", 1000, 0.75 * integral, 1.25 * integral);
    std::unique_ptr<TH1F> hMean_tmp = std::make_unique<TH1F>("hMean_tmp", "", 1000, 0.75 * mean, 1.25 * mean);
    hIntegral_tmp->SetDirectory(nullptr);
    hMean_tmp->SetDirectory(nullptr);

    for (int irnd = 0; irnd < fTrialsCoarse; irnd++) {
      auto hrnd = ReturnRandomHisto(fMeasuredSpectrum);
      auto hrndlo = ReturnCoherentRandomHisto(hlo.get());
      auto hrndhi = ReturnCoherentRandomHisto(hhi.get());

      // The extrapolated parts are discarded here: what the toys sample is the total
      // and the mean, and the two sides carry no information the sum does not.
      double tmpInt = 0.0, tmpMean = 0.0, tmpLow = 0.0, tmpHigh = 0.0;
      Integrate(hrnd.get(), hrndlo.get(), hrndhi.get(), tmpInt, tmpMean, tmpLow, tmpHigh);

      hIntegral_tmp->Fill(tmpInt);
      hMean_tmp->Fill(tmpMean);
    }

    // 3b. Fine phase (actual evaluation)
    // 10 RMS, as YieldMean. This window is not cosmetic: the RMS of these
    // histograms IS the quoted statistical uncertainty, so a window that clips the
    // tails returns a smaller error. At 5, which is what this class used to have,
    // it was clipping them.
    std::unique_ptr<TH1F> hIntegral = std::make_unique<TH1F>("hInt", "", 100,
                                                             hIntegral_tmp->GetMean() - 10. * hIntegral_tmp->GetRMS(),
                                                             hIntegral_tmp->GetMean() + 10. * hIntegral_tmp->GetRMS());

    std::unique_ptr<TH1F> hMean = std::make_unique<TH1F>("hMean", "", 100,
                                                         hMean_tmp->GetMean() - 10. * hMean_tmp->GetRMS(),
                                                         hMean_tmp->GetMean() + 10. * hMean_tmp->GetRMS());
    hIntegral->SetDirectory(nullptr);
    hMean->SetDirectory(nullptr);

    for (int irnd = 0; irnd < fTrialsFine; irnd++) {
      auto hrnd = ReturnRandomHisto(fMeasuredSpectrum);
      auto hrndlo = ReturnCoherentRandomHisto(hlo.get());
      auto hrndhi = ReturnCoherentRandomHisto(hhi.get());

      // The extrapolated parts are discarded here: what the toys sample is the total
      // and the mean, and the two sides carry no information the sum does not.
      double tmpInt = 0.0, tmpMean = 0.0, tmpLow = 0.0, tmpHigh = 0.0;
      Integrate(hrnd.get(), hrndlo.get(), hrndhi.get(), tmpInt, tmpMean, tmpLow, tmpHigh);

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

    // 5. Systematic uncertainty, only if a systematic spectrum was supplied
    ComputeSystematics(res, minPt, maxPt);

    return res;
  }

 private:
  TH1* fMeasuredSpectrum{nullptr};
  TF1* fFitModel{nullptr};    // Owned by the caller, and fitted in place - see the constructor
  TH1* fSysSpectrum{nullptr}; // Owned by the caller, optional - see SetSystematicSpectrum

  // TRandom3's own default, and also where gRandom - which YieldMean used - starts.
  // Matching that stream exactly was never the goal and is not possible anyway:
  // gRandom is shared and keeps advancing from one spectrum to the next, while this
  // generator is re-seeded per instance. What matters is that the quoted statistical
  // error does not move between two runs over the same files, which it did while
  // this was SetSeed(0).
  static constexpr unsigned int kDefaultSeed = 4357;

  bool fFitConverged{true};

  TRandom3 fRandomGen;

  // Extrapolation Ranges
  double fMinPt{0.0};
  double fMaxPt{10.0};
  double fMinFit{0.0};
  double fMaxFit{10.0};

  // Integration parameters
  std::string fFitOption{"0QI"};

  double fLoPrecision{0.01};
  double fHiPrecision{0.1};

  // Toy MC parameters
  int fTrialsCoarse{100};
  int fTrialsFine{1000};

  // The four systematic variations of YieldMean, all built from the SYSTEMATIC
  // spectrum and never from the measured one. They used to read fMeasuredSpectrum,
  // whose errors are statistical: wired up that way they would have returned the
  // statistical error propagated coherently, under the name of a systematic.
  //
  // Shifted: every bin moved by +-sigma together. A coherent normalisation-like
  // move, which changes the yield and leaves the shape - hence the mean - almost
  // untouched.
  std::unique_ptr<TH1> ShiftedHisto(const TH1* src, double sign, const char* suffix) const
  {
    std::unique_ptr<TH1> hout(static_cast<TH1*>(src->Clone(Form("%s_%s", src->GetName(), suffix))));
    hout->SetDirectory(nullptr);
    for (int ibin = 0; ibin < src->GetNbinsX(); ibin++) {
      if (src->GetBinError(ibin + 1) <= 0.)
        continue;
      hout->SetBinContent(ibin + 1, src->GetBinContent(ibin + 1) + sign * src->GetBinError(ibin + 1));
    }
    return hout;
  }

  // stat (+) sys in quadrature, as YieldMean builds htot. Null when no systematic
  // spectrum was supplied, which is how the caller tells the two regimes apart
  // without asking twice.
  std::unique_ptr<TH1> MakeTotalErrorSpectrum() const
  {
    if (!fSysSpectrum)
      return nullptr;

    std::unique_ptr<TH1> hTotal(static_cast<TH1*>(fMeasuredSpectrum->Clone(
      Form("%s_statplussys", fMeasuredSpectrum->GetName()))));
    hTotal->SetDirectory(nullptr);

    for (int ibin = 1; ibin <= fMeasuredSpectrum->GetNbinsX(); ibin++) {
      const double stat = fMeasuredSpectrum->GetBinError(ibin);
      const double sys = fSysSpectrum->GetBinError(ibin);
      hTotal->SetBinError(ibin, TMath::Sqrt(stat * stat + sys * sys));
    }

    return hTotal;
  }

  // Propagates the systematic band of the spectrum through the extrapolation, as
  // YieldMean does: refit each extreme, rebuild its extrapolation histograms,
  // re-integrate, and take the absolute shift of the central value.
  //
  // The pairing is the reference's and it is deliberate: the yield uncertainty
  // comes from the SHIFTED pair and the mean from the TILTED one. A coherent
  // +-sigma shift moves the integral while barely touching the shape, and a tilt
  // does the opposite, so each pair is read where it is sensitive. Taking all four
  // numbers from all four variations would not be more conservative, only noisier.
  void ComputeSystematics(ExtrapolationResult& res, double minPt, double maxPt)
  {
    if (!fSysSpectrum)
      return;

    if (fSysSpectrum->GetNbinsX() != fMeasuredSpectrum->GetNbinsX())
      throw std::runtime_error(
        "[FATAL] SpectrumExtrapolator: the systematic spectrum has a different number of bins "
        "than the measured one, so bin i does not mean the same interval in the two.");

    // The model is fitted in place and the caller keeps using it afterwards - it
    // integrates it and writes the curve next to the spectrum. Four more fits are
    // about to run, so the central parameters are saved and restored below;
    // otherwise what the caller draws would be the last systematic variation.
    std::vector<double> centralPars(fFitModel->GetNpar());
    fFitModel->GetParameters(centralPars.data());

    // Yield and mean of one variation, refitted exactly as the central value was.
    // Falls back to the central numbers when the variation could not be built, so
    // that the resulting shift is zero rather than the distance from nothing.
    auto integrateVariation = [&](const std::unique_ptr<TH1>& hvar, double& yield, double& meanPt) {
      yield = res.yield;
      meanPt = res.meanPt;
      if (!hvar)
        return;

      FitOrWarn(hvar.get());
      auto hlo = CreateLowExtrapolationHisto(hvar.get(), fFitModel, minPt);
      auto hhi = CreateHighExtrapolationHisto(hvar.get(), fFitModel, maxPt);

      double extraLow = 0.0, extraHigh = 0.0;
      Integrate(hvar.get(), hlo.get(), hhi.get(), yield, meanPt, extraLow, extraHigh);
    };

    double varYield = 0.0, varMean = 0.0;

    // high, hard, low, soft - the order of the reference, not grouped by type. It
    // matters because all four refit the same TF1, so each starts from the previous
    // one's converged parameters: reordering them changes the starting points, and
    // with them the results, for no stated reason.
    integrateVariation(ShiftedHisto(fSysSpectrum, +1.0, "extremehigh"), varYield, varMean);
    res.yieldSysHi = TMath::Abs(varYield - res.yield);

    integrateVariation(TiltedHisto(fSysSpectrum, +1.0), varYield, varMean);
    res.meanPtSysHi = TMath::Abs(varMean - res.meanPt);

    integrateVariation(ShiftedHisto(fSysSpectrum, -1.0, "extremelow"), varYield, varMean);
    res.yieldSysLo = TMath::Abs(varYield - res.yield);

    integrateVariation(TiltedHisto(fSysSpectrum, -1.0), varYield, varMean);
    res.meanPtSysLo = TMath::Abs(varMean - res.meanPt);

    res.hasSystematics = true;

    // Parameters only: the covariance matrix left in the global fitter belongs to
    // the last variation. Nothing reads it after this point, and res.chi2 was taken
    // from the central fit long before, but anything added here that calls
    // TF1::IntegralError would silently use the wrong one.
    fFitModel->SetParameters(centralPars.data());
  }

  // The fit procedure, in one place: the systematic variations have to be fitted
  // exactly as the central value is, and duplicating the loop is how the two would
  // come to differ. Returns whether it converged.
  bool FitOrWarn(TH1* h)
  {
    // Enough iterations that minimisation is not stopped by MAX_CALLS.
    TVirtualFitter::SetMaxIterations(1000000);

    int fitres = 0;
    int trials = 0;
    do {
      fitres = h->Fit(fFitModel, fFitOption.c_str(), "", fMinFit, fMaxFit);
      trials++;
      if (trials > 10) {
        // Not just "the fit is bad": a failed fit leaves no usable covariance
        // matrix, so TF1::IntegralError returns 0 for every bin of hlo/hhi, and
        // the 'error <= 0' guard in Integrate - which exists to skip empty bins -
        // then drops the whole extrapolation. The yield that comes out is the raw
        // data integral, looks perfectly plausible, and is not extrapolated.
        std::cerr << "[WARNING] SpectrumExtrapolator: the fit of '" << h->GetName()
                  << "' with '" << fFitModel->GetName() << "' did not converge in 10 trials over ["
                  << fMinFit << ", " << fMaxFit << "]. The covariance matrix is unusable, so the "
                                                   "extrapolation will contribute nothing and the yield below is the raw integral, "
                                                   "NOT an extrapolated one. Do not use it."
                  << std::endl;
        return false;
      }
    } while (fitres != 0);

    return true;
  }

  // Internal Utility Methods
  void Integrate(TH1* hdata, TH1* hlo, TH1* hhi, double& integral, double& mean,
                 double& extraLow, double& extraHigh) const
  {
    double I = 0., IX = 0.;
    extraLow = 0.0;
    extraHigh = 0.0;

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
        extraLow += cont;
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
        extraHigh += cont;
      }
    }

    integral = I;
    mean = (I > 0) ? (IX / I) : 0.0;
  }

  // minPt, not fMinPt: the caller has already widened the domain to cover the
  // fit range, and reading the member here would quietly undo that.
  std::unique_ptr<TH1> CreateLowExtrapolationHisto(TH1* h, TF1* f, double minPt) const
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

    int nbins = static_cast<int>((lo - minPt) / fLoPrecision);
    if (nbins < 1)
      return nullptr;

    auto hlo = std::make_unique<TH1F>("hlo", "", nbins, minPt, lo);
    // Detached: these are scratch, and gDirectory here is the physics output
    // file. Attached they get written into it and warn on the next bin.
    hlo->SetDirectory(nullptr);

    for (int ibin = 0; ibin < hlo->GetNbinsX(); ibin++) {
      double width = hlo->GetBinWidth(ibin + 1);
      double cont = f->Integral(hlo->GetBinLowEdge(ibin + 1), hlo->GetBinLowEdge(ibin + 2), 1.e-6);
      double err = f->IntegralError(hlo->GetBinLowEdge(ibin + 1), hlo->GetBinLowEdge(ibin + 2), nullptr, nullptr, 1.e-6);

      hlo->SetBinContent(ibin + 1, cont / width);
      hlo->SetBinError(ibin + 1, err / width);
    }
    return hlo;
  }

  std::unique_ptr<TH1> CreateHighExtrapolationHisto(TH1* h, TF1* f, double maxPt) const
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

    int nbins = static_cast<int>((maxPt - hi) / fHiPrecision);
    if (nbins < 1)
      return nullptr;

    auto hhi = std::make_unique<TH1F>("hhi", "", nbins, hi, maxPt);
    hhi->SetDirectory(nullptr);

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
    // Clone inherits the directory of the original, and the measured spectrum
    // lives in the output file: 1100 toy iterations under one name is what the
    // "Replacing existing TH1" warnings were.
    hout->SetDirectory(nullptr);
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
    // Clone inherits the directory of the original, and the measured spectrum
    // lives in the output file: 1100 toy iterations under one name is what the
    // "Replacing existing TH1" warnings were.
    hout->SetDirectory(nullptr);
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

  // Tilted: the error is applied with a pT-dependent sign that pivots around a
  // node, and the node scanned for is the one that moves the mean the most. A
  // shape-like move, which is why YieldMean takes the mean from this pair and the
  // yield from the shifted one.
  std::unique_ptr<TH1> TiltedHisto(const TH1* hin, double sign) const
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

    const double mean = hin->GetMean();
    double maxdiff = 0.;
    std::unique_ptr<TH1> hmax;

    for (int inode = 0; inode < hin->GetNbinsX(); inode++) {
      const double ptnode = hin->GetBinCenter(inode + 1);
      std::unique_ptr<TH1> hout(static_cast<TH1*>(hin->Clone("tmp_extreme")));
      hout->SetDirectory(nullptr);

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

      const double diff = TMath::Abs(mean - hout->GetMean());
      if (diff > maxdiff) {
        hmax.reset(static_cast<TH1*>(hout->Clone(Form("%s_tilted", hin->GetName()))));
        hmax->SetDirectory(nullptr);
        maxdiff = diff;
      }
    }

    // Null when no node moves the mean at all, e.g. a spectrum with no errors set.
    return hmax;
  }
};

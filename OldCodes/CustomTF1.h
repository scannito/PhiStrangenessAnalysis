#ifndef CUSTOMTF1_H
#define CUSTOMTF1_H

#include "TF1.h"
#include "TMath.h"
#include "TObject.h"

class CustomTF1 : public TObject
{
 public:
  CustomTF1() : fLastFunc(0), fLineWidth(1) {}
  ~CustomTF1()
  {
    if (fLastFunc)
      delete fLastFunc;
  }

  // Levi Tsallis
  TF1* GetLeviTsallis(Double_t mass, Double_t n, Double_t T, Double_t norm, const char* name = "fLeviTsallis")
  {
    fLastFunc = new TF1(name, LevyTsallis, 0.0, 10, 4);
    fLastFunc->SetParNames("mass", "n", "T", "norm");
    fLastFunc->SetParameters(mass, n, T, norm);
    fLastFunc->FixParameter(0, mass);
    fLastFunc->SetParLimits(1, 2, 30);
    fLastFunc->SetParLimits(2, 0.1, 10);
    fLastFunc->SetLineWidth(fLineWidth);
    return fLastFunc;
  }

  // Bose Einstein
  TF1* GetBoseEinstein(Double_t mass, Double_t T, Double_t norm, const char* name = "fBoseEinstein")
  {
    fLastFunc = new TF1(name, BoseEinstein, 0.0, 10, 3);
    fLastFunc->SetParNames("mass", "T", "norm");
    fLastFunc->SetParameters(mass, T, norm);
    fLastFunc->FixParameter(0, mass);
    fLastFunc->SetParLimits(1, 0.01, 10);
    fLastFunc->SetLineWidth(fLineWidth);
    return fLastFunc;
  }

  // Blast Wave
  TF1* GetBlastWave(Double_t mass, Double_t beta, Double_t temp, Double_t n, Double_t norm, const char* name)
  {
    fLastFunc = new TF1(name, BlastWave, 0.0, 10, 5);
    fLastFunc->SetParNames("mass", "#beta", "temp", "n", "norm");
    fLastFunc->SetParameters(mass, beta, temp, n, norm);
    fLastFunc->FixParameter(0, mass);
    fLastFunc->SetParLimits(1, 0.01, 0.99);
    fLastFunc->SetParLimits(2, 0.01, 1.);
    fLastFunc->SetParLimits(3, 0.01, 50.);
    fLastFunc->SetLineWidth(fLineWidth);
    return fLastFunc;
  }

  TF1* GetMtExponential(Double_t mass, Double_t temp, Double_t norm, const char* name)
  {
    fLastFunc = new TF1(name, MtExponential, 0.0, 10, 3);
    fLastFunc->SetParNames("mass", "norm", "T");
    fLastFunc->SetParameters(mass, norm, temp);
    fLastFunc->FixParameter(0, mass);
    fLastFunc->SetParLimits(1, 0.01, 10);

    fLastFunc->SetLineWidth(fLineWidth);
    return fLastFunc;
  }

  TF1* GetPtExponential(Double_t temp, Double_t norm, const char* name)
  {
    fLastFunc = new TF1(name, PtExponential, 0.0, 10, 2);
    fLastFunc->SetParNames("norm", "T");
    fLastFunc->SetParameters(norm, temp);
    fLastFunc->SetParLimits(1, 0.01, 10);
    fLastFunc->SetLineWidth(fLineWidth);
    return fLastFunc;
  }

 protected:
  static Double_t LevyTsallis(Double_t* x, Double_t* p)
  {
    /* dN/dpt */

    double pt = x[0];
    double mass = p[0];
    double mt = TMath::Sqrt(pt * pt + mass * mass);
    double n = p[1];
    double C = p[2];
    double norm = p[3];

    double part1 = (n - 1.) * (n - 2.);
    double part2 = n * C * (n * C + mass * (n - 2.));
    double part3 = part1 / part2;
    double part4 = 1. + (mt - mass) / n / C;
    double part5 = TMath::Power(part4, -n);
    return pt * norm * part3 * part5;
  }

  static Double_t BoseEinstein(Double_t* x, Double_t* p)
  {
    double pt = x[0];
    double mass = p[0];
    double T = p[1];
    double norm = p[2];

    return norm * pt * 1. / (TMath::Exp(TMath::Sqrt(pt * pt + mass * mass) / T) - 1);
  }

  static Double_t BlastWaveIntegrand(Double_t* x, Double_t* p)
  {
    // integrand for boltzman-gibbs blast wave
    // x[0] -> r (radius)
    // p[0] -> mass
    // p[1] -> pT (transverse momentum)
    // p[2] -> beta_max (surface velocity)
    // p[3] -> T (freezout temperature)
    // p[4] -> n (velocity profile)

    double r = x[0];
    double mass = p[0];
    double pT = p[1];
    double beta_max = p[2];
    double temp = p[3];
    double n = p[4];

    // Keep beta within reasonable limits
    double beta = beta_max * TMath::Power(r, n);
    if (beta > 0.9999999999999999)
      beta = 0.9999999999999999;
    double rho0 = TMath::ATanH(beta);
    double arg00 = pT * TMath::SinH(rho0) / temp;
    if (arg00 > 700.)
      arg00 = 700.; // avoid FPE
    double mT = TMath::Sqrt(mass * mass + pT * pT);
    double arg01 = mT * TMath::CosH(rho0) / temp;

    return r * mT * TMath::BesselI0(arg00) * TMath::BesselK1(arg01);
  }

  static Double_t BlastWave(Double_t* x, Double_t* p)
  {
    /* dN/dpt */

    Double_t pt = x[0];
    Double_t mass = p[0];
    Double_t beta = p[1];
    Double_t temp = p[2];
    Double_t n = p[3];
    Double_t norm = p[4];

    static TF1* fBlastWaveIntegrand = 0;
    if (!fBlastWaveIntegrand)
      fBlastWaveIntegrand = new TF1("fBGBlastWave_Integrand", BlastWaveIntegrand, 0., 1., 5);
    fBlastWaveIntegrand->SetParameters(mass, pt, beta, temp, n);
    Double_t integral = fBlastWaveIntegrand->Integral(0., 1.);
    return norm * pt * integral;
  }

  static Double_t MtExponential(Double_t* x, Double_t* p)
  {
    double pt = x[0];
    double mass = p[0];
    double temp = p[1];
    double norm = p[2];

    return norm * pt * TMath::Exp(-TMath::Sqrt(pt * pt + mass * mass) / temp);
  }

  static Double_t PtExponential(Double_t* x, Double_t* p)
  {
    double pt = x[0];
    double temp = p[0];
    double norm = p[1];

    return norm * pt * TMath::Exp(-pt / temp);
  }

 private:
  TF1* fLastFunc;
  Width_t fLineWidth;

  ClassDef(CustomTF1, 1)
};

#endif

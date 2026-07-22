#pragma once

#include "ExtrapConfigManager.h"

#include "Math/Functor.h"
#include "Math/Integrator.h"
#include "TF1.h"
#include "TMath.h"

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

class ExtrapolationModelFactory
{
 public:
  // Purely static Factory
  ExtrapolationModelFactory() = delete;

  // MAIN FACTORY METHOD
  // Creates a dynamically configured TF1 based on the ExtrapConfig structure
  static std::unique_ptr<TF1> CreateModel(const ExtrapConfig& config, double histMaxVal)
  {
    std::unique_ptr<TF1> f1;

    double dMin = config.domainRange.first;
    double dMax = config.domainRange.second;

    if (config.model == "LevyTsallis") {
      f1 = std::make_unique<TF1>("fLevyTsallis", LevyTsallisFunc, dMin, dMax, 4);
      ConfigureTF1(f1.get(), config, histMaxVal, {"mass", "n", "T", "norm"});
    } else if (config.model == "BoseEinstein") {
      f1 = std::make_unique<TF1>("fBoseEinstein", BoseEinsteinFunc, dMin, dMax, 3);
      ConfigureTF1(f1.get(), config, histMaxVal, {"mass", "T", "norm"});
    } else if (config.model == "BlastWave") {
      f1 = std::make_unique<TF1>("fBlastWave", BlastWaveFunc, dMin, dMax, 5);
      ConfigureTF1(f1.get(), config, histMaxVal, {"mass", "beta", "temp", "n", "norm"});
    } else if (config.model == "MtExponential") {
      f1 = std::make_unique<TF1>("fMtExp", MtExponentialFunc, dMin, dMax, 3);
      ConfigureTF1(f1.get(), config, histMaxVal, {"mass", "T", "norm"});
    } else if (config.model == "PtExponential") {
      f1 = std::make_unique<TF1>("fPtExp", PtExponentialFunc, dMin, dMax, 2);
      ConfigureTF1(f1.get(), config, histMaxVal, {"T", "norm"}); // No mass parameter required for PtExponential
    } else {
      throw std::runtime_error("[FATAL] ExtrapolationModelFactory: Unknown model requested -> " + config.model);
    }

    return f1;
  }

  // --- Levy-Tsallis ---
  static std::unique_ptr<TF1> CreateLevyTsallis(double mass, double n, double T, double norm, const std::string& name = "fLevyTsallis")
  {
    auto f = std::make_unique<TF1>(name.c_str(), LevyTsallisFunc, 0.0, 10.0, 4);
    f->SetParNames("mass", "n", "T", "norm");
    f->SetParameters(mass, n, T, norm);
    f->FixParameter(0, mass);
    f->SetParLimits(1, 2.1, 30.0);
    f->SetParLimits(2, 0.01, 10.0);
    // f->SetParLimits(3, norm * 0.01, norm * 10.0);
    f->SetParLimits(3, 1e-6, norm * 1000.0);

    f->SetParError(1, 0.5);
    f->SetParError(2, 0.05);
    f->SetParError(3, norm * 0.1);
    return f;
  }

  // --- Bose-Einstein ---
  static std::unique_ptr<TF1> CreateBoseEinstein(double mass, double T, double norm, const std::string& name = "fBoseEinstein")
  {
    auto f = std::make_unique<TF1>(name.c_str(), BoseEinsteinFunc, 0.0, 10.0, 3);
    f->SetParNames("mass", "T", "norm");
    f->SetParameters(mass, T, norm);
    f->FixParameter(0, mass);
    f->SetParLimits(1, 0.01, 10.0);
    f->SetParLimits(2, norm * 0.01, norm * 10.0);
    return f;
  }

  // --- Blast-Wave ---
  static std::unique_ptr<TF1> CreateBlastWave(double mass, double beta, double temp, double n, double norm, const std::string& name = "fBlastWave")
  {
    auto f = std::make_unique<TF1>(name.c_str(), BlastWaveFunc, 0.0, 10.0, 5);
    f->SetParNames("mass", "#beta", "temp", "n", "norm");
    f->SetParameters(mass, beta, temp, n, norm);
    f->FixParameter(0, mass);
    f->SetParLimits(1, 0.01, 0.99);
    f->SetParLimits(2, 0.01, 1.0);
    f->SetParLimits(3, 0.01, 50.0);
    f->SetParLimits(4, norm * 0.01, norm * 10.0);
    return f;
  }

  // --- mT Exponential ---
  static std::unique_ptr<TF1> CreateMtExponential(double mass, double temp, double norm, const std::string& name = "fMtExp")
  {
    auto f = std::make_unique<TF1>(name.c_str(), MtExponentialFunc, 0.0, 10.0, 3);
    f->SetParNames("mass", "T", "norm");
    f->SetParameters(mass, temp, norm);
    f->FixParameter(0, mass);
    f->SetParLimits(1, 0.01, 10.0);
    f->SetParLimits(2, norm * 0.01, norm * 10.0);
    return f;
  }

  // --- pT Exponential ---
  static std::unique_ptr<TF1> CreatePtExponential(double temp, double norm, const std::string& name = "fPtExp")
  {
    auto f = std::make_unique<TF1>(name.c_str(), PtExponentialFunc, 0.0, 10.0, 2);
    f->SetParNames("T", "norm");
    f->SetParameters(temp, norm);
    f->SetParLimits(0, 0.01, 10.0);
    f->SetParLimits(1, norm * 0.01, norm * 10.0);
    return f;
  }

 private:
  // AUTOMATED PARAMETER INJECTION HELPER
  static void ConfigureTF1(TF1* f1, const ExtrapConfig& config, double histMaxVal, const std::vector<std::string>& paramNames)
  {
    for (size_t i = 0; i < paramNames.size(); ++i) {
      const std::string& name = paramNames[i];

      // 1. Mass is a fixed physical property, injected directly from the analysis config
      if (name == "mass") {
        f1->FixParameter(i, config.mass);
        f1->SetParName(i, "mass");
        continue;
      }

      // 2. Dynamic shape and normalization parameters are read from the JSON setup
      if (config.params.count(name)) {
        const auto& param = config.params.at(name);

        // Auto-scale ONLY the normalization parameter based on the histogram's maximum value
        double scale = (name == "norm") ? histMaxVal : 1.0;

        double scaledVal = param.val * scale;
        double scaledMin = param.min * scale;
        double scaledMax = param.max * scale;

        f1->SetParName(i, name.c_str());

        // Handle fixed vs free parameters dynamically based on the JSON parsing (isConstant flag)
        if (param.isConstant) {
          f1->FixParameter(i, scaledVal);
        } else {
          f1->SetParameter(i, scaledVal);
          f1->SetParError(i, std::abs(scaledVal) * 0.05 + 0.001);

          // Apply parameter limits only if a valid range is provided in JSON (min < max)
          // E.g., a JSON array like [2.0, 0.0, 0.0] leaves the parameter unbounded
          if (scaledMin < scaledMax) {
            f1->SetParLimits(i, scaledMin, scaledMax);
          }
        }
      } else {
        std::cerr << "[WARNING] ExtrapolationModelFactory: Parameter '" << name
                  << "' missing in JSON config for model '" << config.model << "'!" << std::endl;
      }
    }
  }

  // INNER MATHEMATICAL IMPLEMENTATIONS
  static double LevyTsallisFunc(double* x, double* p)
  {
    double pt = x[0], mass = p[0], n = p[1], C = p[2], norm = p[3];
    double mt = std::sqrt(pt * pt + mass * mass);

    double part1 = (n - 1.) * (n - 2.);
    double part2 = n * C * (n * C + mass * (n - 2.));
    double part3 = part1 / part2;
    double part4 = 1. + (mt - mass) / (n * C);
    double part5 = std::pow(part4, -n);

    return pt * norm * part3 * part5;
  }

  static double BoseEinsteinFunc(double* x, double* p)
  {
    double pt = x[0], mass = p[0], T = p[1], norm = p[2];
    double mt = std::sqrt(pt * pt + mass * mass);
    return norm * pt / (std::exp(mt / T) - 1.0);
  }

  static double BlastWaveFunc(double* x, double* p)
  {
    double pt = x[0], mass = p[0], beta_max = p[1], temp = p[2], n = p[3], norm = p[4];

    auto integrand = [&](double r) {
      double beta = beta_max * std::pow(r, n);
      if (beta > 0.999999)
        beta = 0.999999;

      double rho0 = std::atanh(beta);
      double arg00 = pt * std::sinh(rho0) / temp;
      if (arg00 > 700.)
        arg00 = 700.;

      double mT = std::sqrt(mass * mass + pt * pt);
      double arg01 = mT * std::cosh(rho0) / temp;

      return r * mT * TMath::BesselI0(arg00) * TMath::BesselK1(arg01);
    };

    ROOT::Math::Functor1D func(integrand);
    ROOT::Math::Integrator ig(func, ROOT::Math::IntegrationOneDim::kADAPTIVE, 1.e-6, 1.e-6);

    return norm * pt * ig.Integral(0.0, 1.0);
  }

  static double MtExponentialFunc(double* x, double* p)
  {
    double pt = x[0], mass = p[0], temp = p[1], norm = p[2];
    double mt = std::sqrt(pt * pt + mass * mass);
    return norm * pt * std::exp(-mt / temp);
  }

  static double PtExponentialFunc(double* x, double* p)
  {
    double pt = x[0], temp = p[0], norm = p[1];
    return norm * pt * std::exp(-pt / temp);
  }
};

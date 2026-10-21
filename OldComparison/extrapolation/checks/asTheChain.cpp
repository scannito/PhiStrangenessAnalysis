// Whether the study extrapolates as the chain does. Every K0S, Xi and pi spectrum of the file (|Delta y| < 1, every
// class) is extrapolated again with Chain::Extrapolate and the configuration the file records in its provenance,
// and the yield and its statistical error are compared with the extrapolated trend the chain wrote next to it.
// Chain.h copies the options of CorrelationTaskBase::ExtrapolateSpectrum; this is what fails when the two drift
// apart, and when the study is built differently enough to give other numbers.
//
// The trend is a TH1F, so the comparison is at float precision: PASS if every value agrees within 1e-6 relative.
//
// Usage: asTheChain <PhiAssocSpectra.root> <binning>
#include "../Chain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

int main(int argc, char** argv)
{
  if (argc != 3)
    Chain::Fail(std::format("usage: {} <PhiAssocSpectra.root> <binning>", argv[0]));
  Chain::Source source(argv[1], argv[2]);
  const auto& classes = source.Classes();
  constexpr double kTolerance = 1e-6;

  double worst = 0.0;
  int compared = 0;
  printf("%-4s %-7s %14s %14s %10s   %12s %12s %10s\n", "", "class", "yield", "chain", "rel. diff", "stat. error", "chain", "rel. diff");
  for (const char* species : {"K0S", "Xi", "Pi"}) {
    const std::string trendPath = std::format("{}/{}/{}/{}/{}", argv[2], OutputNames::kExtract1D, species, OutputNames::kTrendsDir,
                                              OutputNames::TrendExtrapolated(species, 1.0));
    auto* trend = dynamic_cast<TH1*>(source.File().Get(trendPath.c_str()));
    if (!trend) {
      printf("%-4s no extrapolated trend in the file (%s): skipped\n", species, trendPath.c_str());
      continue;
    }
    if (trend->GetNbinsX() != static_cast<int>(classes.size()))
      Chain::Fail(std::format("{} has {} bins, the file's multiplicity binning {}", trendPath, trend->GetNbinsX(), classes.size()));
    for (size_t m = 0; m < classes.size(); ++m) {
      auto h = source.Spectrum(species, m, std::format("h_{}_{}", species, m));
      const ExtrapolationResult res = Chain::Extrapolate(h.get(), source.Configured(species, m), std::format("{}_{}", species, m)).res;
      const double chainYield = trend->GetBinContent(m + 1), chainError = trend->GetBinError(m + 1);
      const double dYield = std::abs(res.yield / chainYield - 1), dError = std::abs(res.yieldStatErr / chainError - 1);
      worst = std::max({worst, dYield, dError});
      ++compared;
      printf("%-4s %-7s %14.8g %14.8g %10.2e   %12.6g %12.6g %10.2e\n", species, classes[m].c_str(), res.yield, chainYield, dYield,
             res.yieldStatErr, chainError, dError);
    }
  }
  const bool pass = compared > 0 && worst < kTolerance;
  printf("%s: %d yields and errors, largest relative difference %.2e (tolerance %g)\n", pass ? "PASS" : "FAIL", compared, worst, kTolerance);
  return pass ? 0 : 2;
}

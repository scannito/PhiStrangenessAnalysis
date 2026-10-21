// What the statistical error of an extrapolated yield depends on (DESIGN_HISTORY.md, "The toy fits and the
// saved curves"). Two parts:
//
//  1. ROOT: a predefined "gaus" fitted to the same toy histogram after two different fits. With the shared
//     gROOT "gaus" the result depends on the previous fit, because TH1::Fit takes Minuit's initial step sizes
//     from the parameter errors the function carries (HFitImpl.cxx); with those errors reset, or with a TF1 of
//     its own, it does not. This part shows the mechanism, and its first row is expected to differ.
//  2. The framework: the statistical error of the K0S, extrapolated with the file's configuration right after a
//     pi extrapolated in two different ways. PASS if the two errors are equal to the last digit; before the fix
//     of 21 October 2026 they differed by 1.5e-13.
//
// Usage: toyGaus <PhiAssocSpectra.root> <binning>
#include "../Chain.h"

#include "TF1.h"
#include "TH1D.h"
#include "TROOT.h"
#include "TRandom3.h"

#include <cstdio>
#include <format>
#include <memory>

namespace
{
std::unique_ptr<TH1D> Gaussian(const char* name, double mean, double sigma, int seed)
{
  auto h = std::make_unique<TH1D>(name, "", 100, mean - 10 * sigma, mean + 10 * sigma);
  h->SetDirectory(0);
  TRandom3 random(seed);
  for (int i = 0; i < 1000; ++i)
    h->Fill(random.Gaus(mean, sigma));
  return h;
}

// A "gaus" of its own, out of gROOT's list, on the histogram's range.
std::unique_ptr<TF1> OwnGaus(const TH1* h, const char* name)
{
  return std::make_unique<TF1>(name, "gaus", h->GetXaxis()->GetXmin(), h->GetXaxis()->GetXmax(), TF1::EAddToList::kNo);
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 3)
    Chain::Fail(std::format("usage: {} <PhiAssocSpectra.root> <binning>", argv[0]));

  printf("1. ROOT: sigma of the same toy distribution, fitted after two different fits\n");
  auto target = Gaussian("target", 1.0, 0.014, 1); // shaped like the toys of a K0S yield
  auto previousA = Gaussian("previousA", 28.0, 0.5, 2);
  auto previousB = Gaussian("previousB", 6.0, 0.08, 3);
  TF1* shared = dynamic_cast<TF1*>(gROOT->GetFunction("gaus"));

  // Fits 'previous' with one function, then 'target' with another (or the same), and returns target's sigma.
  auto sigmaAfter = [&](TH1* previous, TF1* forPrevious, TF1* forTarget, bool resetErrors) {
    previous->Fit(forPrevious, "0Q");
    if (resetErrors)
      for (int i = 0; i < forPrevious->GetNpar(); ++i)
        forPrevious->SetParError(i, 0.0);
    target->Fit(forTarget, "0Q");
    return forTarget->GetParameter(2);
  };
  struct Mode {
    const char* label;
    bool ownFunctions; // a TF1 of its own for each fit, instead of the shared one
    bool resetErrors;  // the shared one, with the errors the previous fit left set to zero
  };
  for (const Mode& mode : {Mode{"shared gROOT gaus", false, false}, Mode{"shared, errors reset first", false, true},
                           Mode{"a TF1 of its own", true, false}}) {
    printf("   %-28s", mode.label);
    for (TH1D* previous : {previousA.get(), previousB.get()}) {
      auto own = OwnGaus(previous, "ownPrevious");
      auto ownTarget = OwnGaus(target.get(), "ownTarget");
      const double sigma = mode.ownFunctions ? sigmaAfter(previous, own.get(), ownTarget.get(), false)
                                             : sigmaAfter(previous, shared, shared, mode.resetErrors);
      printf("  after %s: %.17g", previous->GetName(), sigma);
    }
    printf("\n");
  }

  printf("2. framework: K0S statistical error after a pi extrapolated two ways\n");
  Chain::Source source(argv[1], argv[2]);
  double errors[2];
  int i = 0;
  for (double piUpperEdge : {1.0, 3.0}) {
    auto pi = source.Spectrum("Pi", 2, "pi");
    Chain::Extrapolate(pi.get(), source.PiConfig("LevyTsallis", {0.2, piUpperEdge}, Chain::kLevyPi), "pi");
    auto k0s = source.Spectrum("K0S", 3, "k0s");
    errors[i] = Chain::Extrapolate(k0s.get(), source.Configured("K0S", 3), "k0s").res.yieldStatErr;
    printf("   after a pi fitted up to %.1f GeV/c: %.17g\n", piUpperEdge, errors[i]);
    ++i;
  }
  const bool pass = errors[0] == errors[1];
  printf("   %s: the K0S error %s on the pi extrapolated before it\n", pass ? "PASS" : "FAIL", pass ? "does not depend" : "DEPENDS");
  return pass ? 0 : 2;
}

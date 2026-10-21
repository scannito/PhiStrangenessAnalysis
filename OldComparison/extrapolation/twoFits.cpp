// One fit for both sides of the pi spectra against a low fit plus a separate high fit (extrapConfig "high"), on the
// domain and with the mass of the file's pi configuration, extrapolated as the chain does it (Chain::Extrapolate).
//
// Cases: the Levy-Tsallis over 0.2-1.0 GeV/c (the pi block until 20 October 2026), the blast-wave over 0.2-3.0
// (the pi block since), and three with a high fit of their own: Levy-Tsallis 0.2-1.0 with a second Levy-Tsallis
// over 1.0-3.0 or 0.8-3.0, and blast-wave 0.2-1.0 with a Levy-Tsallis over 1.0-3.0.
//
// Prints five tables:
//  1. per case and class: convergence, chi2/ndf of each fit, low-pT and high-pT parts of the yield, yield;
//  2. the yield of every case relative to the first;
//  3. data / fit in the bins above 1 GeV/c, for every case (each bin against the fit that serves it);
//  4. the blast-wave 0.2-3.0 against the two Levy-Tsallis 0.2-1.0 + 1.0-3.0: chi2/ndf of the fits (the two summed),
//     and chi2 over the first three bins of the fit range, which decide the low-pT part;
//  5. the blast-wave function itself at 3.5-4.5 GeV/c, where it has a numerical kink in the highest class.
// Writes, for drawTwoFits.C: the spectra (data_<class>, titled with the class), the curves f_<case>_<class>_main and
// _high, each over the side it serves and titled with the case, and data / fit from the live functions
// (ratio_<case>_<class>).
//
// Usage: twoFits <PhiAssocSpectra.root> <binning> <output.root>
#include "Chain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>
#include <vector>

namespace
{
struct Case {
  std::string key;   // how drawTwoFits.C and the comparisons below ask for it
  std::string label; // what the tables and the figure call it
  ExtrapConfig cfg;
};

// chi2 of 'n' bins from the one where 'from' lies, each against the average of 'f' over it.
double Chi2OfBins(const TH1* h, TF1* f, double from, int n)
{
  double chi2 = 0.0;
  const int first = h->GetXaxis()->FindFixBin(from + 1e-9);
  for (int b = first; b < first + n && b <= h->GetNbinsX(); ++b)
    chi2 += std::pow((h->GetBinContent(b) - Chain::BinAverage(f, h, b)) / h->GetBinError(b), 2);
  return chi2;
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 4)
    Chain::Fail(std::format("usage: {} <PhiAssocSpectra.root> <binning> <output.root>", argv[0]));
  Chain::Source source(argv[1], argv[2]);
  const std::vector<std::string>& classes = source.Classes();
  const size_t nClasses = classes.size();

  auto withHigh = [](ExtrapConfig cfg, std::pair<double, double> highRange) {
    cfg.high = ExtrapFit{"LevyTsallis", highRange, Chain::kLevyPi};
    return cfg;
  };
  const ExtrapConfig levy = source.PiConfig("LevyTsallis", {0.2, 1.0}, Chain::kLevyPi);
  const std::vector<Case> cases = {
    {"levy", "Levy-Tsallis 0.2-1 (until 20 Oct)", levy},
    {"blastwave", "Blast-wave 0.2-3 (since 20 Oct)", source.PiConfig("BlastWave", {0.2, 3.0}, Chain::kBlastWavePi)},
    {"levy+levy1.0", "Levy-Tsallis 0.2-1 + 1.0-3", withHigh(levy, {1.0, 3.0})},
    {"levy+levy0.8", "Levy-Tsallis 0.2-1 + 0.8-3", withHigh(levy, {0.8, 3.0})},
    {"blastwave+levy1.0", "Blast-wave 0.2-1 + Levy-Tsallis 1.0-3",
     withHigh(source.PiConfig("BlastWave", {0.2, 1.0}, Chain::kBlastWavePi), {1.0, 3.0})},
  };
  auto index = [&](const std::string& key) {
    const auto it = std::find_if(cases.begin(), cases.end(), [&](const Case& c) { return c.key == key; });
    if (it == cases.end())
      Chain::Fail("no case " + key);
    return static_cast<size_t>(it - cases.begin());
  };
  const size_t blastWave = index("blastwave"), twoLevy = index("levy+levy1.0"); // compared in tables 4 and 5
  if (!cases[twoLevy].cfg.high)
    Chain::Fail("the case compared as two fits has no high fit");

  std::vector<std::unique_ptr<TH1>> spectra;
  for (size_t m = 0; m < nClasses; ++m)
    spectra.push_back(source.Spectrum("Pi", m, std::format("data_{}", m)));

  // Every extrapolation, kept for the tables that compare them.
  std::vector<std::vector<Chain::Extrapolation>> results(cases.size());
  printf("%-38s %-7s conv  chi2/ndf main   chi2/ndf high   low%%   high%%   yield\n", "case", "class");
  for (size_t c = 0; c < cases.size(); ++c) {
    for (size_t m = 0; m < nClasses; ++m) {
      auto h = source.Spectrum("Pi", m, std::format("h_{}_{}", cases[c].key, m));
      results[c].push_back(Chain::Extrapolate(h.get(), cases[c].cfg, std::format("{}_{}", cases[c].key, m)));
      const ExtrapolationResult& res = results[c].back().res;
      printf("%-38s %-7s %-4s  %6.1f/%-3d       %6.1f/%-3d    %6.2f  %6.2f   %.4f\n", cases[c].label.c_str(), classes[m].c_str(),
             results[c].back().converged ? "yes" : "NO", res.fit.chi2, res.fit.ndf, res.highFit ? res.highFit->chi2 : 0.0,
             res.highFit ? res.highFit->ndf : 0, 100 * res.extrapolatedYieldLow / res.yield, 100 * res.extrapolatedYieldHigh / res.yield, res.yield);
    }
  }

  printf("\nyield relative to '%s':\n%-38s", cases[0].label.c_str(), "case");
  for (const auto& cls : classes)
    printf(" %7s", cls.c_str());
  printf("\n");
  for (size_t c = 1; c < cases.size(); ++c) {
    printf("%-38s", cases[c].label.c_str());
    for (size_t m = 0; m < nClasses; ++m)
      printf(" %+6.1f%%", 100 * (results[c][m].res.yield / results[0][m].res.yield - 1));
    printf("\n");
  }

  std::vector<int> highBins; // the bins above 1 GeV/c, the same in every class (one pi binning per file)
  for (int b = 1; b <= spectra.front()->GetNbinsX(); ++b)
    if (spectra.front()->GetBinLowEdge(b) >= 1.0 - 1e-9)
      highBins.push_back(b);
  printf("\ndata / fit in the bins above 1 GeV/c (each against the fit that serves it):\n%-38s %-7s", "case", "class");
  for (int b : highBins)
    printf("  %7s", std::format("{}-{}", BinningUtils::FormatEdge(spectra.front()->GetBinLowEdge(b)),
                                BinningUtils::FormatEdge(spectra.front()->GetBinLowEdge(b + 1))).c_str());
  printf("\n");
  for (size_t c = 0; c < cases.size(); ++c) {
    for (size_t m = 0; m < nClasses; ++m) {
      const TH1* h = spectra[m].get();
      printf("%-38s %-7s", cases[c].label.c_str(), classes[m].c_str());
      for (int b : highBins)
        printf("  %7.3f", h->GetBinContent(b) / Chain::BinAverage(Chain::Serving(results[c][m], cases[c].cfg, h->GetBinLowEdge(b)), h, b));
      printf("\n");
    }
  }

  const double fitFrom = cases[blastWave].cfg.fitRange.first;
  printf("\n'%s' against '%s':\n", cases[blastWave].label.c_str(), cases[twoLevy].label.c_str());
  printf("%-7s | chi2/ndf of the fits            | chi2, first 3 bins from %g GeV/c\n", "class", fitFrom);
  for (size_t m = 0; m < nClasses; ++m) {
    const FitQuality& bw = results[blastWave][m].res.fit;
    const FitQuality& low = results[twoLevy][m].res.fit;
    const FitQuality& high = *results[twoLevy][m].res.highFit;
    const double twoChi2 = low.chi2 + high.chi2;
    const int twoNdf = low.ndf + high.ndf;
    printf("%-7s | %6.1f/%d = %5.1f  %6.1f/%d = %5.1f | %6.1f  %6.1f\n", classes[m].c_str(), bw.chi2, bw.ndf, bw.ReducedChi2(), twoChi2, twoNdf,
           twoChi2 / twoNdf, Chi2OfBins(spectra[m].get(), results[blastWave][m].main.get(), fitFrom, 3),
           Chi2OfBins(spectra[m].get(), results[twoLevy][m].main.get(), fitFrom, 3));
  }
  printf("(free parameters: %d against %d + %d)\n", results[blastWave][0].main->GetNumberFreeParameters(),
         results[twoLevy][0].main->GetNumberFreeParameters(), results[twoLevy][0].high->GetNumberFreeParameters());

  printf("\nthe '%s' function itself at 3.5, 3.7, 3.9, 4.1, 4.5 GeV/c, and the ratio of each step:\n", cases[blastWave].label.c_str());
  for (size_t m = 0; m < nClasses; ++m) {
    TF1* bw = results[blastWave][m].main.get();
    printf("%-7s", classes[m].c_str());
    double previous = 0.0;
    for (double x : {3.5, 3.7, 3.9, 4.1, 4.5}) {
      const double v = bw->Eval(x);
      printf("  %.3g", v);
      if (previous > 0)
        printf(" (%.2f)", v / previous);
      previous = v;
    }
    printf("\n");
  }

  TFile out(argv[3], "RECREATE");
  for (size_t m = 0; m < nClasses; ++m) {
    spectra[m]->SetTitle(classes[m].c_str());
    spectra[m]->Write();
    for (size_t c = 0; c < cases.size(); ++c) {
      Chain::Extrapolation& e = results[c][m];
      const ExtrapConfig& cfg = cases[c].cfg;
      Chain::DataOverFit(spectra[m].get(), e, cfg, std::format("ratio_{}_{}", cases[c].key, m))->Write();
      Chain::WriteCurve(e.main.get(), cfg.domainRange.first, Chain::Split(cfg), cases[c].label);
      if (e.high)
        Chain::WriteCurve(e.high.get(), Chain::Split(cfg), cfg.domainRange.second, cases[c].label);
    }
  }
  return 0;
}

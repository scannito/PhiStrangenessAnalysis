// Every model of ExtrapolationModelFactory fitted to the pi spectra of a PhiAssocSpectra file, over two ranges:
// 0.2-1.0 GeV/c (the pi fit_range until 20 October 2026) and 0.2-3.0 (all the data), on the domain and with the
// mass of the file's pi configuration, extrapolated as the chain does it (Chain::Extrapolate).
//
// Prints, per model, range and multiplicity class: convergence, chi2/ndf, the low-pT part (below the first
// measured edge) and the high-pT part (above the last) as fractions of the yield, and the yield.
// Writes, for drawModels.C: the spectra (data_<class>, titled with the class), the fitted curves
// (f_<model>_<upper edge>_<class>_main, titled with model and range) and data / fit from the live functions
// (ratio_<model>_<upper edge>_<class>).
//
// Usage: modelScan <PhiAssocSpectra.root> <binning> <output.root>
// The spectra do not depend on the extrapolation configuration, so any current output will do.
#include "Chain.h"

#include <cstdio>
#include <format>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char** argv)
{
  if (argc != 4)
    Chain::Fail(std::format("usage: {} <PhiAssocSpectra.root> <binning> <output.root>", argv[0]));

  // The Levy-Tsallis and the blast-wave start where the pi blocks did, before and after the change; the others
  // where a first trial converged in every class. A start that fails shows as "NO" in the convergence column.
  struct Model {
    std::string name;
    std::map<std::string, MathParam> params;
  };
  const std::vector<Model> models = {
    {"LevyTsallis", Chain::kLevyPi},
    {"BoseEinstein", {{"T", {0.2, 0.02, 1.0}}, {"norm", {10.0, 0.0, 1e5}}}},
    {"MtExponential", {{"T", {0.2, 0.02, 1.0}}, {"norm", {15.0, 0.0, 1e5}}}},
    {"PtExponential", {{"T", {0.2, 0.02, 1.0}}, {"norm", {15.0, 0.0, 1e5}}}},
    {"BlastWave", Chain::kBlastWavePi},
  };
  const std::vector<std::pair<double, double>> ranges = {{0.2, 1.0}, {0.2, 3.0}};

  Chain::Source source(argv[1], argv[2]);
  const std::vector<std::string>& classes = source.Classes();
  TFile out(argv[3], "RECREATE");
  for (size_t m = 0; m < classes.size(); ++m) {
    auto h = source.Spectrum("Pi", m, std::format("data_{}", m));
    h->SetTitle(classes[m].c_str());
    out.cd();
    h->Write();
  }

  printf("%-14s %-9s %-7s conv  chi2/ndf     low%%   high%%   yield\n", "model", "fit", "class");
  for (const auto& model : models) {
    for (const auto& range : ranges) {
      const ExtrapConfig cfg = source.PiConfig(model.name, range, model.params);
      for (size_t m = 0; m < classes.size(); ++m) {
        const std::string tag = std::format("{}_{:g}_{}", model.name, range.second, m);
        auto h = source.Spectrum("Pi", m, "h_" + tag);
        Chain::Extrapolation e = Chain::Extrapolate(h.get(), cfg, tag);
        const ExtrapolationResult& res = e.res;
        printf("%-14s %3.1f-%-5.1f %-7s %-4s  %8.2f   %6.2f  %6.2f   %.4f\n", model.name.c_str(), range.first, range.second, classes[m].c_str(),
               e.converged ? "yes" : "NO", res.fit.ReducedChi2(), 100 * res.extrapolatedYieldLow / res.yield,
               100 * res.extrapolatedYieldHigh / res.yield, res.yield);

        out.cd();
        Chain::DataOverFit(h.get(), e, cfg, "ratio_" + tag)->Write();
        Chain::WriteCurve(e.main.get(), cfg.domainRange.first, cfg.domainRange.second,
                          std::format("{} {:g}-{:g}", model.name, range.first, range.second));
      }
    }
  }
  return 0;
}

// What the programs of this folder take from the chain, in one place. Most of it they read from the file they
// study (Source): its spectra by the names of OutputNames, its multiplicity classes from its binning stamp, the
// extrapolation configuration it was made with from its provenance. What they cannot read is copied here once:
// the options of CorrelationTaskBase::ExtrapolateSpectrum, which checks/asTheChain.cpp verifies by remaking the
// file's own extrapolated yields, and the pi starting points before and after 20 October 2026, for the study's
// own cases.
#pragma once

#include "AnalysisConstants.h"
#include "BinningUtils.h"
#include "ExtrapConfigManager.h"
#include "ExtrapolationModelFactory.h"
#include "OutputNames.h"
#include "RootIOHelpers.h"
#include "SpectrumExtrapolator.h"

#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

namespace Chain
{
// The options of CorrelationTaskBase::ExtrapolateSpectrum, local constants there: copied, and checked by
// checks/asTheChain.cpp, which fails if extrapolating with them does not give the file's yields.
inline const std::string kFitOption{"0QI"};
constexpr double kLoPrecision = 0.01;
constexpr double kHiPrecision = 0.1;

// The pi blocks of extrapConfig.json until and since 20 October 2026, as starting points for the study's cases.
// History, not the configuration: what the file was made with comes from Source::Configured.
inline const std::map<std::string, MathParam> kLevyPi = {{"n", {5.0, 2.0, 20.0}}, {"T", {0.3, 0.05, 0.8}}, {"norm", {1.5, 0.0, 1000.0}}};
inline const std::map<std::string, MathParam> kBlastWavePi = {
  {"beta", {0.6, 0.05, 0.99}}, {"temp", {0.12, 0.03, 0.4}}, {"n", {1.0, 0.1, 5.0}}, {"norm", {1.0, 0.0, 1e6}}};

[[noreturn]] inline void Fail(const std::string& message)
{
  fprintf(stderr, "[FATAL] %s\n", message.c_str());
  std::exit(1);
}

// A PhiAssocSpectra file under one binning, and what it says about itself.
class Source
{
 public:
  Source(const std::string& path, const std::string& binning) : fFile(path.c_str()), fBinning(binning)
  {
    if (fFile.IsZombie())
      Fail(std::format("cannot open {}", path));
    TDirectory* base = fFile.GetDirectory(fBinning.c_str());
    if (!base)
      Fail(std::format("no binning '{}' in {}", fBinning, path));

    // The spectra are addressed by index, so the labels come from the file the index refers to.
    const std::vector<double> edges = RootIO::ReadBinningStamp(base, RootIO::kStampMult);
    if (edges.size() < 2)
      Fail(std::format("no {} stamp under {} in {}", RootIO::kStampMult, fBinning, path));
    for (size_t i = 0; i + 1 < edges.size(); ++i)
      fClasses.push_back(std::format("{}-{}", BinningUtils::FormatEdge(edges[i]), BinningUtils::FormatEdge(edges[i + 1])));

    // The extrapolation configuration the file was made with, the whole extrapConfig.json as text. The manager
    // reads a file, so it gets a copy of it.
    const auto facts = RootIO::ReadProvenance(&fFile, fBinning);
    const auto text = facts.find("extrap_config");
    if (text == facts.end())
      Fail(std::format("no extrap_config in the provenance of {} in {}", fBinning, path));
    const std::filesystem::path copy = std::filesystem::temp_directory_path() / std::format("extrapConfig_{}.json", ::getpid());
    std::ofstream(copy) << text->second;
    fConfigs = std::make_unique<ExtrapConfigManager>(copy.string());
    std::filesystem::remove(copy);
  }

  TFile& File() { return fFile; }
  const std::vector<std::string>& Classes() const { return fClasses; }

  // Where the chain writes the measured spectrum of 'species' in class 'multBin', |Delta y| < 1.
  std::string SpectrumPath(const std::string& species, int multBin, bool extended = false) const
  {
    const std::string name = OutputNames::Spectrum(species, 1.0, multBin);
    return std::format("{}/{}/{}/{}/{}", fBinning, OutputNames::kExtract1D, species, OutputNames::kSpectraDir,
                       extended ? OutputNames::SpectrumExtended(name) : name);
  }

  // That spectrum, detached, under 'name'.
  std::unique_ptr<TH1> Spectrum(const std::string& species, int multBin, const std::string& name)
  {
    auto* h = dynamic_cast<TH1*>(fFile.Get(SpectrumPath(species, multBin).c_str()));
    if (!h)
      Fail(std::format("no {} in {}", SpectrumPath(species, multBin), fFile.GetName()));
    std::unique_ptr<TH1> copy(static_cast<TH1*>(h->Clone(name.c_str())));
    copy->SetDirectory(0);
    return copy;
  }

  // The configuration the chain used for 'species' in class 'multBin', with the mass it injects.
  ExtrapConfig Configured(const std::string& species, int multBin) const
  {
    ExtrapConfig cfg = fConfigs->GetConfig(species, multBin);
    cfg.mass = AnalysisConstants::GetMass(species);
    return cfg;
  }

  // A pi fit of the study's own, on the domain and with the mass of the configured pi.
  ExtrapConfig PiConfig(const std::string& model, std::pair<double, double> fitRange, const std::map<std::string, MathParam>& params) const
  {
    ExtrapConfig cfg = Configured("Pi", 0);
    cfg.model = model;
    cfg.fitRange = fitRange;
    cfg.params = params;
    cfg.high.reset();
    return cfg;
  }

 private:
  TFile fFile;
  std::string fBinning;
  std::vector<std::string> fClasses;
  std::unique_ptr<ExtrapConfigManager> fConfigs;
};

// One extrapolation, as the chain does it: the models built by the factory from the configuration (the high one
// through HighConfig), named after 'name', fitted in place by SpectrumExtrapolator with the chain's options.
struct Extrapolation {
  std::unique_ptr<TF1> main, high; // the fitted functions, live
  ExtrapolationResult res;
  bool converged = false;
};

inline Extrapolation Extrapolate(TH1* h, const ExtrapConfig& cfg, const std::string& name)
{
  Extrapolation out;
  out.main = ExtrapolationModelFactory::CreateModel(cfg, h->GetMaximum());
  out.main->SetName(std::format("f_{}_main", name).c_str());
  SpectrumExtrapolator extrapolator(h, out.main.get());
  extrapolator.SetFitRange(cfg.fitRange.first, cfg.fitRange.second);
  extrapolator.SetExtrapolationLimits(cfg.domainRange.first, cfg.domainRange.second);
  extrapolator.SetFitOption(kFitOption);
  extrapolator.SetIntegrationPrecisions(kLoPrecision, kHiPrecision);
  if (cfg.high) {
    out.high = ExtrapolationModelFactory::CreateModel(cfg.HighConfig(), h->GetMaximum());
    out.high->SetName(std::format("f_{}_high", name).c_str());
    extrapolator.SetHighModel(out.high.get(), cfg.high->fitRange.first, cfg.high->fitRange.second);
  }
  out.res = extrapolator.Extrapolate();
  out.converged = extrapolator.FitConverged();
  return out;
}

// Where the high fit takes over, as the chain draws its curves (CorrelationTaskBase, attachCurve): from where the
// high fit starts; without one, the main fit serves the whole domain.
inline double Split(const ExtrapConfig& cfg) { return cfg.high ? cfg.high->fitRange.first : cfg.domainRange.second; }

// The fit that serves the bin starting at 'lowEdge'.
inline TF1* Serving(const Extrapolation& e, const ExtrapConfig& cfg, double lowEdge)
{
  return (e.high && lowEdge >= Split(cfg) - 1e-9) ? e.high.get() : e.main.get();
}

// The average of 'f' over bin 'b' of 'h': what the "I" fit option compares the bin with.
inline double BinAverage(TF1* f, const TH1* h, int b)
{
  const double a = h->GetBinLowEdge(b), w = h->GetBinWidth(b);
  return f->Integral(a, a + w) / w;
}

// data / fit per bin, each bin against the average of the fit that serves it, from the live functions: what the
// figures draw, so that they show the numbers of the tables and not those of read-back curves.
inline std::unique_ptr<TH1> DataOverFit(const TH1* h, const Extrapolation& e, const ExtrapConfig& cfg, const std::string& name)
{
  std::unique_ptr<TH1> ratio(static_cast<TH1*>(h->Clone(name.c_str())));
  ratio->SetDirectory(0);
  ratio->Reset();
  for (int b = 1; b <= h->GetNbinsX(); ++b) {
    const double v = BinAverage(Serving(e, cfg, h->GetBinLowEdge(b)), h, b);
    if (v > 0 && h->GetBinContent(b) > 0) {
      ratio->SetBinContent(b, h->GetBinContent(b) / v);
      ratio->SetBinError(b, h->GetBinError(b) / v);
    }
  }
  return ratio;
}

// A curve as it is written next to a figure: 1000 points over [from, to], titled with its label. Read back, a TF1
// built from a C++ function is an interpolation of its saved points (checks/savedCurves.cpp).
inline void WriteCurve(TF1* f, double from, double to, const std::string& label)
{
  f->SetRange(from, to);
  f->SetNpx(1000);
  f->SetTitle(label.c_str());
  f->Write();
}
} // namespace Chain

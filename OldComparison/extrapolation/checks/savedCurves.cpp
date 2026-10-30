// What a fitted curve looks like once written to a file (DESIGN_HISTORY.md, "The toy fits and the saved
// curves"). The models of ExtrapolationModelFactory are TF1s built from C++ functions, which ROOT streams as
// their values on GetNpx() points and reads back as an interpolation of them. Two parts:
//
//  1. ROOT: the pi and K0S models of the file's configuration, written and read back as a Clone() and as a copy,
//     for 100 to 2000 points; the worst |read back / live - 1| below and above 0.3 GeV/c, and the integral over
//     0-0.2 GeV/c. A Clone() goes through the streamer, so it is already a 100-point interpolation and SetNpx on
//     it changes nothing: its rows are the same for every Npx. A copy keeps the function, and SetNpx decides.
//  2. The framework: the curves attached to the pi and K0S spectra of the file, against the live function of the
//     configuration the file was made with, given the parameters saved with each curve. PASS if they read back
//     within 1 % below 0.3 GeV/c (since 21 October 2026; 23 % before), with where the largest deviation is.
//
// Usage: savedCurves <PhiAssocSpectra.root> <binning> <scratch.root>
#include "../Chain.h"

#include "TList.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <memory>

namespace
{
constexpr double kSplit = 0.3; // GeV/c: below, the spectra turn over and an interpolation does worst
constexpr double kFar = 3.5;   // GeV/c: above, beyond every measured point

struct Deviation {
  double belowSplit = 0.0, aboveSplit = 0.0, aboveAt = 0.0, belowFar = 0.0, lowIntegral = 0.0;
};

// Read back against live, over the range the curve was saved on, on points that do not fall on its saved grid.
Deviation Compare(TF1* readBack, TF1* live)
{
  Deviation d;
  for (double x = readBack->GetXmin() + 0.0037; x < readBack->GetXmax(); x += 0.0071) {
    const double r = std::abs(readBack->Eval(x) / live->Eval(x) - 1);
    if (x < kSplit) {
      d.belowSplit = std::max(d.belowSplit, r);
      continue;
    }
    if (r > d.aboveSplit) {
      d.aboveSplit = r;
      d.aboveAt = x;
    }
    if (x < kFar)
      d.belowFar = std::max(d.belowFar, r);
  }
  d.lowIntegral = readBack->Integral(0, 0.2) / live->Integral(0, 0.2) - 1;
  return d;
}
} // namespace

int main(int argc, char** argv)
{
  if (argc != 4)
    Chain::Fail(std::format("usage: {} <PhiAssocSpectra.root> <binning> <scratch.root>", argv[0]));
  Chain::Source source(argv[1], argv[2]);

  printf("1. ROOT: a curve written and read back (worst deviation below %g GeV/c, above it; integral over 0-0.2 GeV/c)\n", kSplit);
  for (const char* species : {"Pi", "K0S"}) {
    auto live = ExtrapolationModelFactory::CreateModel(source.Configured(species, 3), 30.0, species);
    for (const bool copy : {false, true}) {
      for (int npx : {100, 200, 500, 1000, 2000}) {
        std::unique_ptr<TF1> curve(copy ? new TF1(*live) : static_cast<TF1*>(live->Clone("c")));
        curve->SetNpx(npx);
        {
          TFile out(argv[3], "RECREATE");
          curve->Write("c");
        }
        TFile in(argv[3]);
        auto* readBack = dynamic_cast<TF1*>(in.Get("c"));
        if (!readBack)
          Chain::Fail(std::format("the curve written to {} does not read back", argv[3]));
        const Deviation d = Compare(readBack, live.get());
        printf("   %-4s %-13s %-6s Npx %4d: %.2e, %.2e; integral %+.2e\n", species, live->GetName(), copy ? "copy" : "Clone", npx, d.belowSplit,
               d.aboveSplit, d.lowIntegral);
      }
    }
  }

  printf("2. framework: the curves attached to the spectra (class 3) of the file\n");
  bool pass = true;
  for (const char* species : {"Pi", "K0S"}) {
    const std::string path = source.SpectrumPath(species, 3, true);
    auto* h = dynamic_cast<TH1*>(source.File().Get(path.c_str()));
    auto* saved = h ? dynamic_cast<TF1*>(h->GetListOfFunctions()->At(0)) : nullptr;
    if (!saved)
      Chain::Fail(std::format("no curve attached to {}", path));
    auto live = ExtrapolationModelFactory::CreateModel(source.Configured(species, 3), 1.0, species);
    if (saved->GetNpar() != live->GetNpar())
      Chain::Fail(std::format("the curve on {} has {} parameters, the configured {} {}", path, saved->GetNpar(), live->GetName(), live->GetNpar()));
    live->SetParameters(saved->GetParameters());
    const Deviation d = Compare(saved, live.get());
    const bool ok = d.belowSplit < 0.01;
    pass = pass && ok;
    printf("   %-4s %s, %d points: %.2e below %g GeV/c; above, %.2e at %.2f GeV/c (%.2e below %g); integral %+.2e  %s\n", species, saved->GetName(),
           saved->GetNpx(), d.belowSplit, kSplit, d.aboveSplit, d.aboveAt, d.belowFar, kFar, d.lowIntegral, ok ? "ok" : "NOT OK");
  }
  printf("   %s\n", pass ? "PASS" : "FAIL");
  return pass ? 0 : 2;
}

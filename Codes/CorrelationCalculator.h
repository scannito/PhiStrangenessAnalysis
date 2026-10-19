#pragma once

#include "AnalysisDataStructures.h"
#include "HistogramUtils.h"
#include "Logger.h"
#include "RootIOHelpers.h"

#include "TDirectory.h"
#include "TF1.h"
#include "TH1.h"
#include "TH2.h"
#include "THnSparse.h"

#include <format>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class CorrelationCalculator
{
 public:
  enum class AxisTarget { DeltaPhi_X = 0,
                          DeltaY_Y };

  // How the mixed event is normalised before it divides the same event: a choice that changes
  // the numbers, so the configuration declares it (me_normalization). The two do not measure
  // the same thing in the 2D engine: see NormalizeME2D.
  enum class MENormalization { BinsAtZero, // the bins at zero: Delta y = 0 (1D), (Delta phi, Delta y) = (0, 0) (2D)
                               Fit };      // a0 of a0 + a1|x| + a2 x^2 fitted near Delta y = 0

  // What the calculator does, fixed for a run. Built with designated initialisers, so the
  // call site says which switch is which; what it does not write is off.
  struct Options {
    bool mixedEvents{false}; // divide by the mixed event
    bool fromCache{false};   // read the projections from ioDir instead of projecting the THnSparse
    bool engine2D{false};    // the 2D engine (Extract2D) instead of the 1D one (Extract1D)
    bool moreQA{false};      // write the 2D and 1D QA objects into qaDir (2D engine only)
    MENormalization meNorm{MENormalization::BinsAtZero}; // always set from me_normalization by the tasks
    AxisTarget targetAxis{AxisTarget::DeltaY_Y};         // the same for every cell of a run; Delta phi only with the 2D engine
  };

  // One extraction: the cell to cut, the corrections to apply, where to read and write.
  struct Request {
    const std::vector<HistogramUtils::AxisToCut>& axesToCut;
    double totalEff;
    double triggerBkgRatio;
    std::string histNameBase;
    TDirectory* ioDir{nullptr};                                // projections cache, written or read
    TDirectory* qaDir{nullptr};                                // QA objects (2D engine with moreQA)
    std::optional<std::pair<double, double>> orthogonalCut{};  // range on the other axis (2D engine)
  };

  explicit CorrelationCalculator(Options options) : opt(options) {}

  // Which projections each engine can make, stated once. The 1D engine projects on Delta y
  // only: Delta phi there needs a normalisation of the mixed event that is still to be
  // decided (DESIGN_NOTES.md section 6). Asked by the tasks at Init
  // (CorrelationTaskBase::RequireEngineSettingsSupported), which stop before loading any
  // data and can name the keys to change; an engine asked for an axis it does not support
  // would otherwise answer with a Delta y projection.
  static bool Supports(bool engine2D, AxisTarget axis) { return engine2D || axis == AxisTarget::DeltaY_Y; }

  // The corrected, sideband-subtracted correlation of one cell, projected on targetAxis.
  std::unique_ptr<TH1> ExtractCorrectedSignal(const LoadedAssocData& data, const Request& req) const
  {
    return opt.engine2D ? Extract2D(data, req) : Extract1D(data, req);
  }

 private:
  Options opt;

  constexpr static int kAxisDeltaY = 3;   // Axis index for Delta y in the THnSparse
  constexpr static int kAxisDeltaPhi = 4; // Axis index for Delta Phi in the THnSparse

  // The four inputs of an extraction - signal and sideband, same event and mixed event - with
  // what follows from the engine, which is the histogram type: TH2 for the 2D engine, TH1 for
  // the 1D one. The names (under which the four are projected, written to the cache and read
  // back, so the two sides cannot drift apart) and the axes to project are derived from H
  // here, once; a 1D input with the 2D suffix cannot be written. The names are fixed at
  // construction and given to the histograms, not a second copy of what they hold.
  template <typename H>
  struct Inputs {
    static constexpr bool k2D = std::is_base_of_v<TH2, H>;

    explicit Inputs(const std::string& base)
      : nameSig(base + "Signal" + Suffix()), nameSb(base + "Sideband" + Suffix()), nameMeSig(base + "MESignal" + Suffix()), nameMeSb(base + "MESideband" + Suffix()) {}

    const std::string nameSig, nameSb, nameMeSig, nameMeSb;
    std::unique_ptr<H> sig, sb, meSig, meSb;

    // Y = Delta y, X = Delta phi in the 2D projection.
    static std::vector<int> ProjAxes() { return k2D ? std::vector<int>{kAxisDeltaY, kAxisDeltaPhi} : std::vector<int>{kAxisDeltaY}; }

   private:
    static const char* Suffix() { return k2D ? "2D" : ""; }
  };

  // Shared by the two engines: the four inputs, read from the cache or projected. Only freshly
  // projected histograms are written to the cache; what was read from it is already there.
  template <typename H>
  Inputs<H> LoadOrProject(const LoadedAssocData& data, const Request& req) const
  {
    return opt.fromCache ? ReadFromCache<H>(req) : ProjectAndCache<H>(data, req);
  }

  // Reads only. The sideband may be missing (a warning, not an error); with mixed events, a
  // sideband comes with its mixed event, the rule ProjectAndCache follows, so a sideband without
  // it means a damaged cache and stops the run (the cell would subtract a sideband never divided
  // by the mixed event from a signal that was). A
  // missing cache directory is the caller's to report (CorrelationTaskBase::ProjectionCacheDir,
  // which names the file and what to do); a null ioDir reaching here stops in RootIO.
  template <typename H>
  Inputs<H> ReadFromCache(const Request& req) const
  {
    Inputs<H> in(req.histNameBase);
    in.sig = RootIO::GetUniqueOrThrow<H>(req.ioDir, in.nameSig, "CorrelationCalculator");
    in.sb = RootIO::GetUniqueOrWarn<H>(req.ioDir, in.nameSb, "CorrelationCalculator");
    if (opt.mixedEvents) {
      in.meSig = RootIO::GetUniqueOrThrow<H>(req.ioDir, in.nameMeSig, "CorrelationCalculator");
      if (in.sb)
        in.meSb = RootIO::GetUniqueOrThrow<H>(req.ioDir, in.nameMeSb, "CorrelationCalculator");
    }
    return in;
  }

  // Projects the THnSparse and, when ioDir is given, writes the result to the cache: the only
  // place the cache is written. The sideband is optional; with mixed events, a sideband comes
  // with its mixed event (the loaders require the two together), and one without stops here.
  template <typename H>
  Inputs<H> ProjectAndCache(const LoadedAssocData& data, const Request& req) const
  {
    const std::vector<int> projAxes = Inputs<H>::ProjAxes();
    Inputs<H> in(req.histNameBase);
    if (!data.h5DataSignal)
      throw std::runtime_error(std::format("[FATAL] CorrelationCalculator: missing signal THnSparse for {}", data.name));
    in.sig = HistogramUtils::ProjectTHnSparse<H>(data.h5DataSignal.get(), req.axesToCut, projAxes, in.nameSig);
    if (data.h5DataSideband)
      in.sb = HistogramUtils::ProjectTHnSparse<H>(data.h5DataSideband.get(), req.axesToCut, projAxes, in.nameSb);

    if (opt.mixedEvents) {
      if (!data.h5DataMESignal)
        throw std::runtime_error(std::format("[FATAL] CorrelationCalculator: missing mixed-event signal THnSparse for {}", data.name));
      in.meSig = HistogramUtils::ProjectTHnSparse<H>(data.h5DataMESignal.get(), req.axesToCut, projAxes, in.nameMeSig);
      if (in.sb) {
        if (!data.h5DataMESideband)
          throw std::runtime_error(std::format("[FATAL] CorrelationCalculator: a sideband for {} without its mixed-event sideband THnSparse", data.name));
        in.meSb = HistogramUtils::ProjectTHnSparse<H>(data.h5DataMESideband.get(), req.axesToCut, projAxes, in.nameMeSb);
      }
    }

    if (req.ioDir) {
      req.ioDir->cd();
      in.sig->Write(nullptr, TObject::kOverwrite);
      if (in.sb)
        in.sb->Write(nullptr, TObject::kOverwrite);
      if (opt.mixedEvents) {
        in.meSig->Write(nullptr, TObject::kOverwrite);
        if (in.meSb)
          in.meSb->Write(nullptr, TObject::kOverwrite);
      }
    }
    return in;
  }

  // The normalisation of the mixed event, chosen by opt.meNorm, is the same idea in the two
  // engines: the mixed event is scaled to 1 at zero separation.

  // 1D. BinsAtZero: the bins at Delta y = 0 (GetZeroAvg). With an even, symmetric binning,
  // which is what every configuration has had, it equals the integral of the two bins around
  // zero divided by 2 that this engine used to compute, number for number; with zero at the
  // centre of a bin that integral took three bins and would have normalised to about 1.5.
  // Fit: a0 of the fit near zero, on a copy so that the fit is not attached to the histogram.
  void NormalizeME1D(TH1* me) const
  {
    double norm = 0.0;
    if (opt.meNorm == MENormalization::Fit) {
      std::unique_ptr<TH1> copy(static_cast<TH1*>(me->Clone("tempME1D_fit")));
      copy->SetDirectory(0);
      norm = GetZeroAvgFromFit(copy.get(), me->GetName());
    } else {
      norm = GetZeroAvg(me);
    }
    ScaleToOneAtZero(me, norm);
  }

  // 2D. Fit: a0 of the fit of the Delta y projection near zero, divided by the number of
  // Delta phi bins, i.e. the value per bin at Delta y = 0 AVERAGED OVER Delta phi.
  // BinsAtZero: the bins at (Delta phi, Delta y) = (0, 0), i.e. the value AT Delta phi = 0.
  // They agree only if the mixed event is flat in Delta phi.
  void NormalizeME2D(TH2* me, const char* tempName) const
  {
    double norm = 0.0;
    if (opt.meNorm == MENormalization::Fit) {
      std::unique_ptr<TH1> projY(static_cast<TH1*>(me->ProjectionY(tempName)));
      projY->SetDirectory(0);
      const double a0 = GetZeroAvgFromFit(projY.get(), me->GetName());
      const double nBinsPhi = me->GetNbinsX();
      norm = a0 / nBinsPhi;
    } else {
      norm = GetZeroZeroAvg(me);
    }
    ScaleToOneAtZero(me, norm);
  }

  // A normalisation that cannot be computed (an empty mixed event at zero, in a cell with few
  // pairs) leaves the mixed event as it is, so the same event is divided by raw mixed-event
  // counts and the cell is off by orders of magnitude. Kept as it was, but no longer silent
  // (CLAUDE.md: a correction that cannot be computed is not a correction of 1).
  static void ScaleToOneAtZero(TH1* me, double norm)
  {
    if (norm > 0) {
      me->Scale(1.0 / norm);
      return;
    }
    // One constant text, so the end-of-run summary counts it once with (xN) instead of filling
    // its list of distinct warnings cell by cell; which cell is in the Info line before it.
    Logger::Info("CorrelationCalculator: mixed event '{}' at zero: {}", me->GetName(), norm);
    Logger::Warn("CorrelationCalculator: a mixed event is not positive at zero and is left unnormalised; its cell is "
                 "divided by raw mixed-event counts (the cells are in the Info lines above each)");
  }

  // The content at x = 0: the bin around zero, or the mean of the two bins that meet at zero.
  static double GetZeroAvg(TH1* h)
  {
    if (!h)
      return 0.0;
    int bx_left = h->GetXaxis()->FindBin(-1e-6);
    int bx_right = h->GetXaxis()->FindBin(1e-6);

    // If zero falls exactly in the center of a single bin
    if (bx_left == bx_right) {
      return h->GetBinContent(bx_left);
    }

    // If zero falls on the intersection of 2 bins
    return (h->GetBinContent(bx_left) + h->GetBinContent(bx_right)) / 2.0;
  }

  // The content at (0, 0): the bin around it, or the mean of the bins that meet there.
  static double GetZeroZeroAvg(TH2* h)
  {
    if (!h)
      return 0.0;
    int bx_left = h->GetXaxis()->FindBin(-1e-6);
    int bx_right = h->GetXaxis()->FindBin(1e-6);
    int by_down = h->GetYaxis()->FindBin(-1e-6);
    int by_up = h->GetYaxis()->FindBin(1e-6);

    // If zero falls exactly in the center of a single bin
    if (bx_left == bx_right && by_down == by_up) {
      return h->GetBinContent(bx_left, by_down);
    }

    // Otherwise the mean of the bins that meet at zero: two (on an edge in one axis) or four.
    // GetBinContent of the same bin twice counts it twice, so the mean over four is right in
    // both cases.
    return (h->GetBinContent(bx_left, by_down) + h->GetBinContent(bx_left, by_up) +
            h->GetBinContent(bx_right, by_down) + h->GetBinContent(bx_right, by_up)) /
           4.0;
  }

  // Constant term of a fit near zero. The range (0.5) and the shape are fixed here: with the fit
  // available to both engines they may become configurable (DESIGN_NOTES.md section 6).
  double GetZeroAvgFromFit(TH1* h, const std::string& meName, double fitRange = 0.5) const
  {
    if (!h)
      return 0.0;

    // Fit a constant in the range [-fitRange, fitRange]
    std::unique_ptr<TF1> fitFunc = std::make_unique<TF1>("fitFunc", "[0] + [1]*abs(x) + [2]*x*x", -fitRange, fitRange);
    // R = use range, Q = quiet mode. A non-zero status is reported, not interpreted: it is the
    // minimiser's code, and some non-zero values leave a usable minimum. a0 is used as before.
    const int status = h->Fit(fitFunc.get(), "RQ");
    if (status != 0) {
      Logger::Info("CorrelationCalculator: fit normalising mixed event '{}': status {}", meName, status);
      Logger::Warn("CorrelationCalculator: a fit normalising a mixed event returned a non-zero status (see TH1::Fit; "
                   "0 is a successful fit); its a0 is used as it is (the cells are in the Info lines above each)");
    }

    return fitFunc->GetParameter(0);
  }

  // =========================================================================
  // UNIVERSAL 2D ENGINE
  // =========================================================================
  std::unique_ptr<TH1> Extract2D(const LoadedAssocData& data, const Request& req) const
  {
    // Short names for the body below, which is the engine as it was.
    const std::string& histNameBase = req.histNameBase;
    const double totalEff = req.totalEff;
    const double triggerBkgRatio = req.triggerBkgRatio;
    TDirectory* qaDir = req.qaDir;
    const AxisTarget targetAxis = opt.targetAxis;
    const auto& orthogonalCut = req.orthogonalCut;
    const bool applyME = opt.mixedEvents;
    const bool doMoreQA = opt.moreQA;

    // 1. Loading / Projection. Y = Delta y, X = Delta phi.
    Inputs<TH2> in = LoadOrProject<TH2>(data, req);
    std::unique_ptr<TH2>& h2Signal = in.sig;
    std::unique_ptr<TH2>& h2Sideband = in.sb;
    std::unique_ptr<TH2>& h2MESignal = in.meSig;
    std::unique_ptr<TH2>& h2MESideband = in.meSb;

    // 2. ME normalization (if requested)
    if (applyME) {
      NormalizeME2D(h2MESignal.get(), "tempMEProjY_Sig");
      if (h2MESideband)
        NormalizeME2D(h2MESideband.get(), "tempMEProjY_Sb");
    }

    // 3. Efficiency Correction
    h2Signal->Scale(1.0 / totalEff);
    // Sideband Scaling
    if (h2Sideband)
      h2Sideband->Scale(triggerBkgRatio / totalEff);

    // 4. 2D Division
    if (applyME && h2MESignal)
      h2Signal->Divide(h2MESignal.get());

    if (h2Sideband) {
      if (applyME && h2MESideband)
        h2Sideband->Divide(h2MESideband.get());
    }

    std::unique_ptr<TH1> h1Final1D;

    // 5. Uncut QA (if requested)
    if (qaDir && doMoreQA) {
      qaDir->cd();
      h2Signal->Write((histNameBase + "Signal2D_MECorrected_Uncut").c_str());
      if (h2Sideband)
        h2Sideband->Write((histNameBase + "Sideband2D_MECorrected_Uncut").c_str());

      // QA: X axis (Delta Phi)
      std::unique_ptr<TH1> h1QA_X_Sig(h2Signal->ProjectionX((histNameBase + "Signal1D_dPhi_Uncut").c_str()));
      h1QA_X_Sig->SetDirectory(0);
      std::unique_ptr<TH1> h1QA_X_Sb = h2Sideband ? std::unique_ptr<TH1>(h2Sideband->ProjectionX((histNameBase + "Sideband1D_dPhi_Uncut").c_str())) : nullptr;
      if (h1QA_X_Sb)
        h1QA_X_Sb->SetDirectory(0);
      std::unique_ptr<TH1> h1QA_X_Final(static_cast<TH1*>(h1QA_X_Sig->Clone((histNameBase + "Final_dPhi_Uncut").c_str())));
      h1QA_X_Final->SetDirectory(0);
      if (h1QA_X_Sb && triggerBkgRatio > 0)
        h1QA_X_Final->Add(h1QA_X_Sb.get(), -1);

      // QA: Y axis (Delta y)
      std::unique_ptr<TH1> h1QA_Y_Sig(h2Signal->ProjectionY((histNameBase + "Signal1D_dy_Uncut").c_str()));
      h1QA_Y_Sig->SetDirectory(0);
      std::unique_ptr<TH1> h1QA_Y_Sb = h2Sideband ? std::unique_ptr<TH1>(h2Sideband->ProjectionY((histNameBase + "Sideband1D_dy_Uncut").c_str())) : nullptr;
      if (h1QA_Y_Sb)
        h1QA_Y_Sb->SetDirectory(0);
      std::unique_ptr<TH1> h1QA_Y_Final(static_cast<TH1*>(h1QA_Y_Sig->Clone((histNameBase + "Final_dy_Uncut").c_str())));
      h1QA_Y_Final->SetDirectory(0);
      if (h1QA_Y_Sb && triggerBkgRatio > 0)
        h1QA_Y_Final->Add(h1QA_Y_Sb.get(), -1);

      if (!orthogonalCut) {
        TH1* targetQA = (targetAxis == AxisTarget::DeltaPhi_X) ? h1QA_X_Final.get() : h1QA_Y_Final.get();

        h1Final1D.reset(static_cast<TH1*>(targetQA->Clone((histNameBase + "Final" + ((targetAxis == AxisTarget::DeltaPhi_X) ? "_dPhi" : "_dy")).c_str())));
        h1Final1D->SetDirectory(0);
      }

      h1QA_X_Sig->Write();
      if (h1QA_X_Sb)
        h1QA_X_Sb->Write();
      h1QA_X_Final->Write();

      h1QA_Y_Sig->Write();
      if (h1QA_Y_Sb)
        h1QA_Y_Sb->Write();
      h1QA_Y_Final->Write();
    }

    if (!h1Final1D) {
      // 6. Orthogonal Cut (If requested)
      if (orthogonalCut) {
        auto [cutMin, cutMax] = *orthogonalCut;
        if (targetAxis == AxisTarget::DeltaPhi_X) {
          // If projecting onto X, cut the Y axis (Delta y)
          int binMin = h2Signal->GetYaxis()->FindBin(cutMin + 1e-6);
          int binMax = h2Signal->GetYaxis()->FindBin(cutMax - 1e-6);
          h2Signal->GetYaxis()->SetRange(binMin, binMax);
          if (h2Sideband)
            h2Sideband->GetYaxis()->SetRange(binMin, binMax);
        } else {
          // If projecting onto Y, cut the X axis (Delta Phi)
          int binMin = h2Signal->GetXaxis()->FindBin(cutMin + 1e-6);
          int binMax = h2Signal->GetXaxis()->FindBin(cutMax - 1e-6);
          h2Signal->GetXaxis()->SetRange(binMin, binMax);
          if (h2Sideband)
            h2Sideband->GetXaxis()->SetRange(binMin, binMax);
        }
      }

      std::unique_ptr<TH1> h1Sig1D = (targetAxis == AxisTarget::DeltaPhi_X)
                                       ? std::unique_ptr<TH1>(h2Signal->ProjectionX((histNameBase + "Signal1D_dPhi").c_str()))
                                       : std::unique_ptr<TH1>(h2Signal->ProjectionY((histNameBase + "Signal1D_dy").c_str()));
      h1Sig1D->SetDirectory(0);

      std::unique_ptr<TH1> h1Side1D;
      if (h2Sideband) {
        h1Side1D = (targetAxis == AxisTarget::DeltaPhi_X)
                     ? std::unique_ptr<TH1>(h2Sideband->ProjectionX((histNameBase + "Sideband1D_dPhi").c_str()))
                     : std::unique_ptr<TH1>(h2Sideband->ProjectionY((histNameBase + "Sideband1D_dy").c_str()));
        h1Side1D->SetDirectory(0);
      }

      h1Final1D.reset(static_cast<TH1*>(h1Sig1D->Clone((histNameBase + "Final" + ((targetAxis == AxisTarget::DeltaPhi_X) ? "_dPhi" : "_dy")).c_str())));
      h1Final1D->SetDirectory(0);
      if (h1Side1D && triggerBkgRatio > 0) {
        h1Final1D->Add(h1Side1D.get(), -1);
      }

      if (qaDir && doMoreQA && orthogonalCut) {
        qaDir->cd();
        h2Signal->Write((histNameBase + "Signal2D_MECorrected_Cut").c_str());
        if (h2Sideband)
          h2Sideband->Write((histNameBase + "Sideband2D_MECorrected_Cut").c_str());
        h1Sig1D->Write();
        if (h1Side1D)
          h1Side1D->Write();
        h1Final1D->Write();
      }
    }

    return h1Final1D;
  }

  // =========================================================================
  // 1D ENGINE (used if opt.engine2D is false): projects on Delta y only, see ExtractCorrectedSignal
  // =========================================================================
  std::unique_ptr<TH1> Extract1D(const LoadedAssocData& data, const Request& req) const
  {
    // 1. Load from cache or project THnSparse on the fly
    Inputs<TH1> in = LoadOrProject<TH1>(data, req);

    // 2. Efficiency Correction
    in.sig->Scale(1.0 / req.totalEff);

    // 3. Mixed Event Correction
    if (opt.mixedEvents) {
      NormalizeME1D(in.meSig.get());
      in.sig->Divide(in.meSig.get());
    }

    if (in.sb) {
      if (opt.mixedEvents && in.meSb) {
        NormalizeME1D(in.meSb.get());
        in.sb->Divide(in.meSb.get());
      }
      in.sb->Scale(req.triggerBkgRatio / req.totalEff);
    }

    // 4. Final Subtraction
    std::unique_ptr<TH1> h1FinalSignal(static_cast<TH1*>(in.sig->Clone(req.histNameBase.c_str())));
    h1FinalSignal->SetDirectory(0);
    if (in.sb && req.triggerBkgRatio > 0)
      h1FinalSignal->Add(in.sb.get(), -1);

    return h1FinalSignal;
  }
};

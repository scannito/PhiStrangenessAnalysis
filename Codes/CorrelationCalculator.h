#pragma once

#include "AnalysisDataStructures.h"
#include "HistogramUtils.h"
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
#include <utility>
#include <vector>

class CorrelationCalculator
{
 public:
  enum class AxisTarget { DeltaPhi_X = 0,
                          DeltaY_Y };

  // What the calculator does, fixed for a run. Built with designated initialisers, so the
  // call site says which switch is which; what it does not write is off.
  struct Options {
    bool mixedEvents{false}; // divide by the mixed event
    bool fromCache{false};   // read the projections from ioDir instead of projecting the THnSparse
    bool engine2D{false};    // the 2D engine (Extract2D) instead of the 1D one (Extract1D)
    bool moreQA{false};      // write the 2D and 1D QA objects into qaDir (2D engine only)
  };

  // One extraction: the cell to cut, the corrections to apply, where to read and write.
  struct Request {
    const std::vector<HistogramUtils::AxisToCut>& axesToCut;
    double totalEff;
    double triggerBkgRatio;
    std::string histNameBase;
    TDirectory* ioDir{nullptr};                                // projections cache, written or read
    TDirectory* qaDir{nullptr};                                // QA objects (2D engine with moreQA)
    AxisTarget targetAxis{AxisTarget::DeltaY_Y};               // Delta phi only with the 2D engine
    std::optional<std::pair<double, double>> orthogonalCut{};  // range on the other axis (2D engine)
  };

  explicit CorrelationCalculator(Options options) : opt(options) {}

  // The corrected, sideband-subtracted correlation of one cell, projected on targetAxis.
  std::unique_ptr<TH1> ExtractCorrectedSignal(const LoadedAssocData& data, const Request& req) const
  {
    // The 1D engine projects on Delta y only. Delta phi there needs a normalisation of the
    // mixed event that is still to be decided (DESIGN_NOTES.md section 6), so it is refused
    // rather than answered with a Delta y projection. The tasks refuse it at Init already;
    // this is the guarantee for any other caller.
    if (!opt.engine2D && req.targetAxis == AxisTarget::DeltaPhi_X)
      throw std::runtime_error("[FATAL] CorrelationCalculator: a Delta phi projection needs the 2D engine; the 1D engine projects on Delta y only.");

    return opt.engine2D ? Extract2D(data, req) : Extract1D(data, req);
  }

 private:
  Options opt;

  constexpr static int kAxisDeltaY = 3;   // Axis index for Delta y in the THnSparse
  constexpr static int kAxisDeltaPhi = 4; // Axis index for Delta Phi in the THnSparse

  // The four inputs of an extraction: signal and sideband, same event and mixed event.
  template <typename H>
  struct Inputs {
    std::unique_ptr<H> sig, sb, meSig, meSb;
  };

  // Shared by the two engines: read the four inputs from the cache, or project them from the
  // THnSparse and write them to the cache. The sideband and its mixed event are optional;
  // the mixed-event sideband is projected only when there is a sideband to divide.
  template <typename H>
  Inputs<H> LoadOrProject(const LoadedAssocData& data, const Request& req, const std::vector<int>& projAxes,
                          const std::string& dim) const
  {
    const std::string nameSig = req.histNameBase + "Signal" + dim;
    const std::string nameSb = req.histNameBase + "Sideband" + dim;
    const std::string nameMESig = req.histNameBase + "MESignal" + dim;
    const std::string nameMESb = req.histNameBase + "MESideband" + dim;
    Inputs<H> in;

    if (opt.fromCache) {
      if (!req.ioDir)
        throw std::runtime_error("[FATAL] CorrelationCalculator: cache mode requested but no ioDir provided!");
      in.sig = RootIO::GetUniqueOrThrow<H>(req.ioDir, nameSig, "CorrelationCalculator");
      in.sb = RootIO::GetUniqueOrWarn<H>(req.ioDir, nameSb, "CorrelationCalculator");
      if (opt.mixedEvents) {
        in.meSig = RootIO::GetUniqueOrThrow<H>(req.ioDir, nameMESig, "CorrelationCalculator");
        in.meSb = RootIO::GetUniqueOrWarn<H>(req.ioDir, nameMESb, "CorrelationCalculator");
      }
      return in;
    }

    if (!data.h5DataSignal)
      throw std::runtime_error(std::format("[FATAL] CorrelationCalculator: missing signal THnSparse for {}", data.name));
    in.sig = HistogramUtils::ProjectTHnSparse<H>(data.h5DataSignal.get(), req.axesToCut, projAxes, nameSig);
    if (data.h5DataSideband)
      in.sb = HistogramUtils::ProjectTHnSparse<H>(data.h5DataSideband.get(), req.axesToCut, projAxes, nameSb);

    if (opt.mixedEvents) {
      if (!data.h5DataMESignal)
        throw std::runtime_error(std::format("[FATAL] CorrelationCalculator: missing mixed-event signal THnSparse for {}", data.name));
      in.meSig = HistogramUtils::ProjectTHnSparse<H>(data.h5DataMESignal.get(), req.axesToCut, projAxes, nameMESig);
      if (data.h5DataMESideband && in.sb)
        in.meSb = HistogramUtils::ProjectTHnSparse<H>(data.h5DataMESideband.get(), req.axesToCut, projAxes, nameMESb);
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

  // The two engines normalise the mixed event differently, a physics choice and not an
  // accident: see the table in DESIGN_NOTES.md section 6.

  // 1D: scaled so that the integral of the two bins around Delta y = 0 is 2, about 1 per bin.
  static void NormalizeME1D(TH1* me)
  {
    const double width = me->GetXaxis()->GetBinWidth(me->GetXaxis()->FindBin(0.0));
    const double norm = HistogramUtils::IntegralAndErrorPair(me, -width, width).first;
    if (norm > 0)
      me->Scale(2.0 / norm);
  }

  // 2D: scaled so that its average per Delta phi bin at Delta y = 0, from a fit of the
  // Delta y projection near zero, is 1.
  void NormalizeME2D(TH2* me, const char* tempName) const
  {
    std::unique_ptr<TH1> projY(static_cast<TH1*>(me->ProjectionY(tempName)));
    projY->SetDirectory(0);
    const double a0 = GetZeroAvgFromFit(projY.get());
    const double nBinsPhi = me->GetNbinsX();
    const double norm = a0 / nBinsPhi;
    if (norm > 0)
      me->Scale(1.0 / norm);
  }

  // Constant term of a fit near zero of the 2D mixed event's Delta y projection.
  double GetZeroAvgFromFit(TH1* h, double fitRange = 0.5) const
  {
    if (!h)
      return 0.0;

    // Fit a constant in the range [-fitRange, fitRange]
    std::unique_ptr<TF1> fitFunc = std::make_unique<TF1>("fitFunc", "[0] + [1]*abs(x) + [2]*x*x", -fitRange, fitRange);
    h->Fit(fitFunc.get(), "RQ"); // R = use range, Q = quiet mode

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
    const AxisTarget targetAxis = req.targetAxis;
    const auto& orthogonalCut = req.orthogonalCut;
    const bool applyME = opt.mixedEvents;
    const bool doMoreQA = opt.moreQA;

    // 1. Loading / Projection. Y = Delta y, X = Delta phi.
    Inputs<TH2> in = LoadOrProject<TH2>(data, req, {kAxisDeltaY, kAxisDeltaPhi}, "2D");
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
    Inputs<TH1> in = LoadOrProject<TH1>(data, req, {kAxisDeltaY}, "");

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

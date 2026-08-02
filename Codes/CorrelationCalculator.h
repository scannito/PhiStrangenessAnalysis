#pragma once

#include "AnalysisDataStructures.h"
#include "AnalysisUtils.h"
#include "RootIOHelpers.h"

#include "TDirectory.h"
#include "TF1.h"
#include "TH1.h"
#include "TH2.h"
#include "THnSparse.h"

#include <iostream>
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

  CorrelationCalculator(bool useMixedEvents, bool useCache, bool use2D, bool doMoreQA)
    : applyME(useMixedEvents), useCacheMode(useCache), use2DME(use2D), doMoreQA(doMoreQA) {}

  // =========================================================================
  // Public Wrappers (Called by the Task)
  // =========================================================================

  // Used to extract the signal for the final spectra (Projects onto Delta y)
  std::unique_ptr<TH1> ExtractCorrectedSignal(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                                              double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                                              TDirectory* ioDir = nullptr, TDirectory* qaDir = nullptr, AxisTarget targetAxis = AxisTarget::DeltaY_Y,
                                              std::optional<std::pair<double, double>> orthogonalCut = std::nullopt) const
  {
    if (use2DME) {
      return Extract2D(data, axesToCut, totalEff, triggerBkgRatio, histNameBase, ioDir, qaDir, targetAxis, orthogonalCut);
    } else {
      return Extract1D(data, axesToCut, totalEff, triggerBkgRatio, histNameBase, ioDir);
    }
  }

 private:
  bool applyME{true}, useCacheMode{false}, use2DME{false}, doMoreQA{false};

  constexpr static int kAxisDeltaY = 3;   // Axis index for Delta y in the THnSparse
  constexpr static int kAxisDeltaPhi = 4; // Axis index for Delta Phi in the THnSparse

  // =========================================================================
  // Helper: Safe calculation of the average around (0,0)
  // =========================================================================
  double GetZeroZeroAvg(TH2* h) const
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

    // If zero falls on the intersection of 4 bins
    return (h->GetBinContent(bx_left, by_down) + h->GetBinContent(bx_left, by_up) +
            h->GetBinContent(bx_right, by_down) + h->GetBinContent(bx_right, by_up)) /
           4.0;
  }

  double GetZeroAvg(TH1* h) const
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
  std::unique_ptr<TH1> Extract2D(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                                 double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                                 TDirectory* ioDir, TDirectory* qaDir, AxisTarget targetAxis,
                                 std::optional<std::pair<double, double>> orthogonalCut = std::nullopt) const
  {
    std::unique_ptr<TH2> h2Signal;
    std::unique_ptr<TH2> h2Sideband;
    std::unique_ptr<TH2> h2MESignal;
    std::unique_ptr<TH2> h2MESideband;

    std::string nameSig2D = histNameBase + "Signal2D";
    std::string nameSb2D = histNameBase + "Sideband2D";
    std::string nameMESig2D = histNameBase + "MESignal2D";
    std::string nameMESb2D = histNameBase + "MESideband2D";

    // 1. Loading / Projection
    if (useCacheMode) {
      if (!ioDir)
        throw std::runtime_error("[FATAL] Cache mode requested but no ioDir provided!");

      h2Signal = RootIO::GetUniqueOrThrow<TH2>(ioDir, nameSig2D, "CorrelationTask");
      h2Sideband = RootIO::GetUniqueOrWarn<TH2>(ioDir, nameSb2D, "CorrelationTask");

      if (applyME) {
        h2MESignal = RootIO::GetUniqueOrThrow<TH2>(ioDir, nameMESig2D, "CorrelationTask");
        h2MESideband = RootIO::GetUniqueOrWarn<TH2>(ioDir, nameMESb2D, "CorrelationTask");
      }
    } else {
      std::vector<int> projAxes = {kAxisDeltaY, kAxisDeltaPhi}; // Y = Delta y, X = Delta Phi

      if (!data.h5DataSignal)
        throw std::runtime_error("[FATAL] No signal data provided for correlation calculation!");
      h2Signal = AnalysisUtils::ProjectTHnSparse<TH2>(data.h5DataSignal.get(), axesToCut, projAxes, nameSig2D);
      if (data.h5DataSideband)
        h2Sideband = AnalysisUtils::ProjectTHnSparse<TH2>(data.h5DataSideband.get(), axesToCut, projAxes, nameSb2D);

      if (applyME) {
        if (!data.h5DataMESignal)
          throw std::runtime_error("[FATAL] No ME signal data provided for correlation calculation!");
        h2MESignal = AnalysisUtils::ProjectTHnSparse<TH2>(data.h5DataMESignal.get(), axesToCut, projAxes, nameMESig2D);
        if (data.h5DataMESideband && h2Sideband) {
          h2MESideband = AnalysisUtils::ProjectTHnSparse<TH2>(data.h5DataMESideband.get(), axesToCut, projAxes, nameMESb2D);
        }
      }

      if (ioDir) {
        ioDir->cd();
        h2Signal->Write(nullptr, TObject::kOverwrite);
        if (h2Sideband)
          h2Sideband->Write(nullptr, TObject::kOverwrite);
        if (applyME) {
          h2MESignal->Write(nullptr, TObject::kOverwrite);
          if (h2MESideband)
            h2MESideband->Write(nullptr, TObject::kOverwrite);
        }
      }
    }

    // 2. ME normalization (if requested)
    if (applyME) {
      std::unique_ptr<TH1> h1MEProjY(static_cast<TH1*>(h2MESignal->ProjectionY("tempMEProjY_Sig")));
      h1MEProjY->SetDirectory(0);

      double a0_Sig = GetZeroAvgFromFit(h1MEProjY.get());
      double nBinsPhi_Sig = h2MESignal->GetNbinsX();
      double normMESig = a0_Sig / nBinsPhi_Sig;

      if (normMESig > 0)
        h2MESignal->Scale(1.0 / normMESig);

      if (h2MESideband) {
        std::unique_ptr<TH1> h1MEProjY_Sb(static_cast<TH1*>(h2MESideband->ProjectionY("tempMEProjY_Sb")));
        h1MEProjY_Sb->SetDirectory(0);

        double a0_Sb = GetZeroAvgFromFit(h1MEProjY_Sb.get());
        double nBinsPhi_Sb = h2MESideband->GetNbinsX();
        double normMESb = a0_Sb / nBinsPhi_Sb;

        if (normMESb > 0)
          h2MESideband->Scale(1.0 / normMESb);
      }
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
  // ORIGINAL 1D ENGINE (Untouched, used if use2DME = false)
  // =========================================================================
  std::unique_ptr<TH1> Extract1D(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                                 double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                                 TDirectory* ioDir) const
  {
    std::unique_ptr<TH1> h1Signal;
    std::unique_ptr<TH1> h1Sideband;
    std::unique_ptr<TH1> h1MESignal;
    std::unique_ptr<TH1> h1MESideband;

    // 1. Load from cache or project THnSparse on the fly
    if (useCacheMode) {
      if (!ioDir)
        throw std::runtime_error("[FATAL] Cache mode requested but no ioDir provided!");

      h1Signal = RootIO::GetUniqueOrThrow<TH1>(ioDir, histNameBase + "Signal", "CorrelationTask");
      h1Sideband = RootIO::GetUniqueOrWarn<TH1>(ioDir, histNameBase + "Sideband", "CorrelationTask");

      if (applyME) {
        h1MESignal = RootIO::GetUniqueOrThrow<TH1>(ioDir, histNameBase + "MESignal", "CorrelationTask");
        h1MESideband = RootIO::GetUniqueOrWarn<TH1>(ioDir, histNameBase + "MESideband", "CorrelationTask");
      }
    } else {
      if (!data.h5DataSignal)
        throw std::runtime_error("[FATAL] Missing Signal THnSparse for " + data.name);
      h1Signal = AnalysisUtils::ProjectTHnSparse<TH1>(data.h5DataSignal.get(), axesToCut, {3}, histNameBase + "Signal");
      if (data.h5DataSideband)
        h1Sideband = AnalysisUtils::ProjectTHnSparse<TH1>(data.h5DataSideband.get(), axesToCut, {3}, histNameBase + "Sideband");

      if (applyME) {
        if (!data.h5DataMESignal)
          throw std::runtime_error("[FATAL] Missing ME Signal THnSparse for " + data.name);
        h1MESignal = AnalysisUtils::ProjectTHnSparse<TH1>(data.h5DataMESignal.get(), axesToCut, {3}, histNameBase + "MESignal");
        if (data.h5DataMESideband && h1Sideband)
          h1MESideband = AnalysisUtils::ProjectTHnSparse<TH1>(data.h5DataMESideband.get(), axesToCut, {3}, histNameBase + "MESideband");
      }

      if (ioDir) {
        ioDir->cd();
        h1Signal->Write(nullptr, TObject::kOverwrite);
        if (h1Sideband)
          h1Sideband->Write(nullptr, TObject::kOverwrite);
        if (applyME) {
          h1MESignal->Write(nullptr, TObject::kOverwrite);
          if (h1MESideband)
            h1MESideband->Write(nullptr, TObject::kOverwrite);
        }
      }
    }

    // 2. Efficiency Correction
    h1Signal->Scale(1.0 / totalEff);

    // 3. Mixed Event Correction
    if (applyME) {
      double width = h1MESignal->GetXaxis()->GetBinWidth(h1MESignal->GetXaxis()->FindBin(0.0));
      auto [normMESignal, errSig] = AnalysisUtils::IntegralAndErrorPair(h1MESignal.get(), -width, width);
      if (normMESignal > 0)
        h1MESignal->Scale(2.0 / normMESignal);
      h1Signal->Divide(h1MESignal.get());
    }

    if (h1Sideband) {
      if (applyME && h1MESideband) {
        double width = h1MESideband->GetXaxis()->GetBinWidth(h1MESideband->GetXaxis()->FindBin(0.0));
        auto [normMESideband, errSide] = AnalysisUtils::IntegralAndErrorPair(h1MESideband.get(), -width, width);
        if (normMESideband > 0)
          h1MESideband->Scale(2.0 / normMESideband);
        h1Sideband->Divide(h1MESideband.get());
      }
      h1Sideband->Scale(triggerBkgRatio / totalEff);
    }

    // 4. Final Subtraction
    std::unique_ptr<TH1> h1FinalSignal(static_cast<TH1*>(h1Signal->Clone(histNameBase.c_str())));
    h1FinalSignal->SetDirectory(0);
    if (h1Sideband && triggerBkgRatio > 0)
      h1FinalSignal->Add(h1Sideband.get(), -1);

    return h1FinalSignal;
  }
};

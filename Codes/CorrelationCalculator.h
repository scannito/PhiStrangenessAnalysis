#pragma once

#include "AnalysisDataStructures.h"
#include "AnalysisUtils.h"

#include "TDirectory.h"
#include "TH1.h"
#include "TH2.h"
#include "THnSparse.h"

#include <iostream>
#include <optional>
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
  TH1* ExtractCorrectedSignal(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
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

  /*// Used to extract the QA (Projects onto Delta Phi with optional cuts on Delta y)
  TH1* ExtractDeltaPhi(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                       double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                       TDirectory* ioDir = nullptr, TDirectory* qaDir = nullptr,
                       bool applyOrthogonalCut = false, double cutMin = -1.0, double cutMax = 1.0) const
  {
    return Process2DExtraction(data, axesToCut, totalEff, triggerBkgRatio, histNameBase, ioDir, qaDir,
                               AxisTarget::DeltaPhi_X, applyOrthogonalCut, cutMin, cutMax);
  }*/

 private:
  bool applyME{true}, useCacheMode{false}, use2DME{false}, doMoreQA{false};

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
    TF1* fitFunc = new TF1("fitFunc", "[0] + [1]*abs(x) + [2]*x*x", -fitRange, fitRange);
    h->Fit(fitFunc, "RQ"); // R = use range, Q = quiet mode

    double fitValue = fitFunc->GetParameter(0);
    delete fitFunc;
    return fitValue;
  }

  // =========================================================================
  // UNIVERSAL 2D ENGINE
  // =========================================================================
  TH1* Extract2D(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                 double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                 TDirectory* ioDir, TDirectory* qaDir, AxisTarget targetAxis,
                 std::optional<std::pair<double, double>> orthogonalCut = std::nullopt) const
  {
    TH2* h2Signal{nullptr};
    TH2* h2Sideband{nullptr};
    TH2* h2MESignal{nullptr};
    TH2* h2MESideband{nullptr};

    std::string nameSig2D = histNameBase + "Signal2D";
    std::string nameSb2D = histNameBase + "Sideband2D";
    std::string nameMESig2D = histNameBase + "MESignal2D";
    std::string nameMESb2D = histNameBase + "MESideband2D";

    // 1. Loading / Projection (Axes 3 = Delta y, 4 = Delta Phi)
    if (useCacheMode) {
      if (!ioDir)
        throw std::runtime_error("[FATAL] Cache mode requested but no ioDir provided!");

      h2Signal = static_cast<TH2*>(ioDir->Get(nameSig2D.c_str()));
      if (!h2Signal)
        throw std::runtime_error("[FATAL] Cache missing: " + nameSig2D);
      h2Signal->SetDirectory(0);

      h2Sideband = static_cast<TH2*>(ioDir->Get(nameSb2D.c_str()));
      if (h2Sideband)
        h2Sideband->SetDirectory(0);

      if (applyME) {
        h2MESignal = static_cast<TH2*>(ioDir->Get(nameMESig2D.c_str()));
        if (h2MESignal)
          h2MESignal->SetDirectory(0);
        h2MESideband = static_cast<TH2*>(ioDir->Get(nameMESb2D.c_str()));
        if (h2MESideband)
          h2MESideband->SetDirectory(0);
      }
    } else {
      std::vector<int> projAxes = {3, 4}; // Y = Delta y, X = Delta Phi
      h2Signal = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataSignal, axesToCut, projAxes, nameSig2D);
      if (data.h5DataSideband)
        h2Sideband = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataSideband, axesToCut, projAxes, nameSb2D);

      if (applyME) {
        h2MESignal = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataMESignal, axesToCut, projAxes, nameMESig2D);
        if (data.h5DataMESideband && h2Sideband) {
          h2MESideband = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataMESideband, axesToCut, projAxes, nameMESb2D);
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
      if (h2MESignal) {
        TH1* h1MEProjY = h2MESignal->ProjectionY("tempMEProjY_Sig");
        double a0_Sig = GetZeroAvgFromFit(h1MEProjY);
        double nBinsPhi_Sig = h2MESignal->GetNbinsX();
        double normMESig = a0_Sig / nBinsPhi_Sig;

        if (normMESig > 0)
          h2MESignal->Scale(1.0 / normMESig);
        delete h1MEProjY;
      }

      if (h2MESideband) {
        TH1* h1MEProjY_Sb = h2MESideband->ProjectionY("tempMEProjY_Sb");
        double a0_Sb = GetZeroAvgFromFit(h1MEProjY_Sb);
        double nBinsPhi_Sb = h2MESideband->GetNbinsX();
        double normMESb = a0_Sb / nBinsPhi_Sb;

        if (normMESb > 0)
          h2MESideband->Scale(1.0 / normMESb);
        delete h1MEProjY_Sb;
      }

      /*double normMESig = GetZeroZeroAvg(h2MESignal);
      if (normMESig > 0)
        h2MESignal->Scale(1.0 / normMESig);
      double normMESb = GetZeroZeroAvg(h2MESideband);
      if (normMESb > 0)
        h2MESideband->Scale(1.0 / normMESb);*/
    }

    // 3. Efficiency Correction
    h2Signal->Scale(1.0 / totalEff);
    // Sideband Scaling
    h2Sideband->Scale(triggerBkgRatio / totalEff);

    // 4. 2D Division
    if (applyME && h2MESignal)
      h2Signal->Divide(h2MESignal);

    if (h2Sideband) {
      if (applyME && h2MESideband)
        h2Sideband->Divide(h2MESideband);

      // Sideband Scaling
      // h2Sideband->Scale(triggerBkgRatio / totalEff);
    }

    TH1* h1Final1D{nullptr};

    // 5. Uncut QA (if requested)
    if (qaDir && doMoreQA) {
      qaDir->cd();
      h2Signal->Write((histNameBase + "Signal2D_MECorrected_Uncut").c_str());
      if (h2Sideband)
        h2Sideband->Write((histNameBase + "Sideband2D_MECorrected_Uncut").c_str());

      // QA: X axis (Delta Phi)
      TH1* h1QA_X_Sig = h2Signal->ProjectionX((histNameBase + "Signal1D_dPhi_Uncut").c_str());
      TH1* h1QA_X_Sb = h2Sideband ? h2Sideband->ProjectionX((histNameBase + "Sideband1D_dPhi_Uncut").c_str()) : nullptr;
      TH1* h1QA_X_Final = static_cast<TH1*>(h1QA_X_Sig->Clone((histNameBase + "Final_dPhi_Uncut").c_str()));
      if (h1QA_X_Sb && triggerBkgRatio > 0)
        h1QA_X_Final->Add(h1QA_X_Sb, -1);

      // QA: Y axis (Delta y)
      TH1* h1QA_Y_Sig = h2Signal->ProjectionY((histNameBase + "Signal1D_dy_Uncut").c_str());
      TH1* h1QA_Y_Sb = h2Sideband ? h2Sideband->ProjectionY((histNameBase + "Sideband1D_dy_Uncut").c_str()) : nullptr;
      TH1* h1QA_Y_Final = static_cast<TH1*>(h1QA_Y_Sig->Clone((histNameBase + "Final_dy_Uncut").c_str()));
      if (h1QA_Y_Sb && triggerBkgRatio > 0)
        h1QA_Y_Final->Add(h1QA_Y_Sb, -1);

      if (!orthogonalCut) {
        TH1* targetQA = (targetAxis == AxisTarget::DeltaPhi_X) ? h1QA_X_Final : h1QA_Y_Final;
        h1Final1D = static_cast<TH1*>(targetQA->Clone((histNameBase + "Final" + ((targetAxis == AxisTarget::DeltaPhi_X) ? "_dPhi" : "_dy")).c_str()));
        h1Final1D->SetDirectory(0);
      }

      h1QA_X_Sig->Write();
      if (h1QA_X_Sb)
        h1QA_X_Sb->Write();
      h1QA_X_Final->Write();
      delete h1QA_X_Sig;
      if (h1QA_X_Sb)
        delete h1QA_X_Sb;
      delete h1QA_X_Final;

      h1QA_Y_Sig->Write();
      if (h1QA_Y_Sb)
        h1QA_Y_Sb->Write();
      h1QA_Y_Final->Write();
      delete h1QA_Y_Sig;
      if (h1QA_Y_Sb)
        delete h1QA_Y_Sb;
      delete h1QA_Y_Final;
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

      TH1* h1Sig1D = (targetAxis == AxisTarget::DeltaPhi_X)
                       ? h2Signal->ProjectionX((histNameBase + "Signal1D_dPhi").c_str())
                       : h2Signal->ProjectionY((histNameBase + "Signal1D_dy").c_str());
      h1Sig1D->SetDirectory(0);

      TH1* h1Side1D = nullptr;
      if (h2Sideband) {
        h1Side1D = (targetAxis == AxisTarget::DeltaPhi_X)
                     ? h2Sideband->ProjectionX((histNameBase + "Sideband1D_dPhi").c_str())
                     : h2Sideband->ProjectionY((histNameBase + "Sideband1D_dy").c_str());
        h1Side1D->SetDirectory(0);
      }

      h1Final1D = static_cast<TH1*>(h1Sig1D->Clone((histNameBase + "Final" + ((targetAxis == AxisTarget::DeltaPhi_X) ? "_dPhi" : "_dy")).c_str()));
      h1Final1D->SetDirectory(0);
      if (h1Side1D && triggerBkgRatio > 0) {
        h1Final1D->Add(h1Side1D, -1);
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

      delete h1Sig1D;
      if (h1Side1D)
        delete h1Side1D;
    }

    delete h2Signal;
    if (h2Sideband)
      delete h2Sideband;
    if (applyME) {
      delete h2MESignal;
      if (h2MESideband)
        delete h2MESideband;
    }

    return h1Final1D;
  }

  // =========================================================================
  // ORIGINAL 1D ENGINE (Untouched, used if use2DME = false)
  // =========================================================================
  TH1* Extract1D(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                 double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                 TDirectory* ioDir) const
  {
    TH1* h1Signal{nullptr};
    TH1* h1Sideband{nullptr};
    TH1* h1MESignal{nullptr};
    TH1* h1MESideband{nullptr};

    // 1. Load from cache or project THnSparse on the fly
    if (useCacheMode) {
      if (!ioDir)
        throw std::runtime_error("[FATAL] Cache mode requested but no ioDir provided!");

      h1Signal = static_cast<TH1*>(ioDir->Get((histNameBase + "Signal").c_str()));
      if (!h1Signal)
        throw std::runtime_error("[FATAL] Cache missing: " + histNameBase + "Signal");
      h1Signal->SetDirectory(0);

      h1Sideband = static_cast<TH1*>(ioDir->Get((histNameBase + "Sideband").c_str()));
      if (h1Sideband)
        h1Sideband->SetDirectory(0);

      if (applyME) {
        h1MESignal = static_cast<TH1*>(ioDir->Get((histNameBase + "MESignal").c_str()));
        if (!h1MESignal)
          throw std::runtime_error("[FATAL] Cache missing: " + histNameBase + "MESignal");
        h1MESignal->SetDirectory(0);

        h1MESideband = static_cast<TH1*>(ioDir->Get((histNameBase + "MESideband").c_str()));
        if (h1MESideband)
          h1MESideband->SetDirectory(0);
      }
    } else {
      if (!data.h5DataSignal)
        throw std::runtime_error("[FATAL] Missing Signal THnSparse for " + data.name);

      h1Signal = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataSignal, axesToCut, {3}, histNameBase + "Signal");
      if (data.h5DataSideband)
        h1Sideband = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataSideband, axesToCut, {3}, histNameBase + "Sideband");

      if (applyME) {
        if (!data.h5DataMESignal)
          throw std::runtime_error("[FATAL] Missing ME Signal THnSparse for " + data.name);
        h1MESignal = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataMESignal, axesToCut, {3}, histNameBase + "MESignal");
        if (data.h5DataMESideband && h1Sideband)
          h1MESideband = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataMESideband, axesToCut, {3}, histNameBase + "MESideband");
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
      auto [normMESignal, errSig] = AnalysisUtils::IntegralAndErrorPair(h1MESignal, -width, width);
      if (normMESignal > 0)
        h1MESignal->Scale(2.0 / normMESignal);
      h1Signal->Divide(h1MESignal);
    }

    if (h1Sideband) {
      if (applyME && h1MESideband) {
        double width = h1MESideband->GetXaxis()->GetBinWidth(h1MESideband->GetXaxis()->FindBin(0.0));
        auto [normMESideband, errSide] = AnalysisUtils::IntegralAndErrorPair(h1MESideband, -width, width);
        if (normMESideband > 0)
          h1MESideband->Scale(2.0 / normMESideband);
        h1Sideband->Divide(h1MESideband);
      }
      h1Sideband->Scale(triggerBkgRatio / totalEff);
    }

    // 4. Final Subtraction
    TH1* h1FinalSignal = static_cast<TH1*>(h1Signal->Clone(histNameBase.c_str()));
    h1FinalSignal->SetDirectory(0);
    if (h1Sideband && triggerBkgRatio > 0)
      h1FinalSignal->Add(h1Sideband, -1);

    // 5. Cleanup
    delete h1Signal;
    if (h1Sideband)
      delete h1Sideband;
    if (applyME) {
      delete h1MESignal;
      if (h1MESideband)
        delete h1MESideband;
    }

    return h1FinalSignal;
  }
};

#pragma once

#include "AnalysisDataStructures.h"
#include "AnalysisUtils.h"

#include "TH1.h"
#include "TH2.h"
#include "THnSparse.h"

#include <iostream>
#include <string>
#include <vector>

class CorrelationCalculator
{
 public:
  CorrelationCalculator(bool useMixedEvents, bool useCache, bool use2D) : applyME(useMixedEvents), useCacheMode(useCache), use2DME(use2D) {}

  // Public wrapper that routes to the correct extraction logic
  TH1* ExtractCorrectedSignal(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                              double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                              TDirectory* ioDir = nullptr) const
  {
    if (use2DME) {
      return Extract2D(data, axesToCut, totalEff, triggerBkgRatio, histNameBase, ioDir);
    } else {
      return Extract1D(data, axesToCut, totalEff, triggerBkgRatio, histNameBase, ioDir);
    }
  }

  /*// Returns the fully corrected 1D histogram of the associated signal.
  // Accepts an optional TDirectory* to save intermediate steps to disk.
  TH1* ExtractCorrectedSignal(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                              double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                              TDirectory* ioDir = nullptr) const
  {
    TH1* h1Signal{nullptr};
    TH1* h1Sideband{nullptr};
    TH1* h1MESignal{nullptr};
    TH1* h1MESideband{nullptr};

    // 1. Histogram retrieval: either from cache or projecting the THnSparse on the fly
    if (useCacheMode) { // In cache mode, we expect the histograms to be already projected and stored in the provided TDirectory
      if (!ioDir) {
        std::cerr << "\n[FATAL ERROR] CorrelationCalculator: Cache mode requested but no ioDir provided!\n"
                  << std::endl;
        std::exit(1);
      }

      h1Signal = static_cast<TH1*>(ioDir->Get((histNameBase + "Signal").c_str()));
      if (!h1Signal) {
        std::cerr << "\n[FATAL] Cache missing: " << histNameBase << "Signal" << std::endl;
        std::exit(1);
      }
      h1Signal->SetDirectory(0);

      h1Sideband = static_cast<TH1*>(ioDir->Get((histNameBase + "Sideband").c_str()));
      if (h1Sideband)
        h1Sideband->SetDirectory(0);

      if (applyME) {
        h1MESignal = static_cast<TH1*>(ioDir->Get((histNameBase + "MESignal").c_str()));
        if (!h1MESignal) {
          std::cerr << "\n[FATAL] Cache missing: " << histNameBase << "MESignal" << std::endl;
          std::exit(1);
        }
        h1MESignal->SetDirectory(0);

        h1MESideband = static_cast<TH1*>(ioDir->Get((histNameBase + "MESideband").c_str()));
        if (h1MESideband)
          h1MESideband->SetDirectory(0);
      }
    } else { // If not using cache, we need to project the THnSparse on the fly
      if (!data.h5DataSignal || data.h5DataSignal->IsZombie()) {
        throw std::runtime_error("[FATAL] CorrelationCalculator: Mandatory Signal THnSparse is missing for " + data.name);
      }
      h1Signal = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataSignal, axesToCut, {3}, histNameBase + "Signal");

      // Project Sideband ONLY if the sparse exists
      if (data.h5DataSideband) {
        h1Sideband = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataSideband, axesToCut, {3}, histNameBase + "Sideband");
      }

      if (applyME) {
        if (!data.h5DataMESignal || data.h5DataMESignal->IsZombie()) {
          throw std::runtime_error("[FATAL] CorrelationCalculator: ME is enabled, but mandatory ME Signal THnSparse is missing for " + data.name);
        }
        h1MESignal = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataMESignal, axesToCut, {3}, histNameBase + "MESignal");

        if (data.h5DataMESideband && h1Sideband) {
          h1MESideband = AnalysisUtils::projectTHnSparse<TH1>(data.h5DataMESideband, axesToCut, {3}, histNameBase + "MESideband");
        }
      }

      // --- SAVE RAW DATA ---
      if (ioDir) {
        ioDir->cd();
        h1Signal->Write();
        if (h1Sideband)
          h1Sideband->Write();
        if (applyME) {
          h1MESignal->Write();
          if (h1MESideband)
            h1MESideband->Write();
        }
      }
    }

    // 2. Efficiency Correction
    h1Signal->Scale(1.0 / totalEff);

    // 3. Mixed Event Correction (if enabled)
    if (applyME) {
      auto [normMESignal, errSig] = AnalysisUtils::IntegralAndErrorPair(h1MESignal, -0.1, 0.1);
      if (normMESignal > 0)
        h1MESignal->Scale(2.0 / normMESignal);
      h1Signal->Divide(h1MESignal);
    }

    if (h1Sideband) {
      if (applyME && h1MESideband) {
        auto [normMESideband, errSide] = AnalysisUtils::IntegralAndErrorPair(h1MESideband, -0.1, 0.1);
        if (normMESideband > 0)
          h1MESideband->Scale(2.0 / normMESideband);
        h1Sideband->Divide(h1MESideband);
      }

      // 4. Background Subtraction (Sideband Scaling & Subtraction)
      h1Sideband->Scale(triggerBkgRatio / totalEff);

      // --- SAVE SCALED SIDEBAND ---
      if (ioDir && !useCacheMode) {
        ioDir->cd();
        h1Sideband->Write((std::string(h1Sideband->GetName()) + "_Scaled").c_str());
      }
    }

    // 5. Create final histogram: Signal - Scaled Sideband
    TH1* h1FinalSignal = static_cast<TH1*>(h1Signal->Clone(histNameBase.c_str()));
    h1FinalSignal->SetDirectory(0);
    if (h1Sideband && triggerBkgRatio > 0)
      h1FinalSignal->Add(h1Sideband, -1);

    // 6. Memory cleanup for temporary objects
    delete h1Signal;
    if (h1Sideband)
      delete h1Sideband;
    if (applyME) {
      delete h1MESignal;
      if (h1MESideband)
        delete h1MESideband;
    }

    return h1FinalSignal;
  }*/

  // New 2D extraction -> 1D Projection along Delta Phi (X Axis)
  TH1* ExtractDeltaPhi(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                       double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                       TDirectory* ioDir = nullptr,
                       bool applyDyCut = false, double dyMin = -1.0, double dyMax = 1.0) const
  {
    TH2* h2Signal{nullptr};
    TH2* h2Sideband{nullptr};
    TH2* h2MESignal{nullptr};
    TH2* h2MESideband{nullptr};

    std::string nameSig2D = histNameBase + "Signal2D";
    std::string nameSb2D = histNameBase + "Sideband2D";
    std::string nameMESig2D = histNameBase + "MESignal2D";
    std::string nameMESb2D = histNameBase + "MESideband2D";

    // 1. Extraction or Projection from THnSparse (Raw 2D)
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
      std::vector<int> projAxes = {3, 4}; // 3 = Delta Y, 4 = Delta Phi

      h2Signal = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataSignal, axesToCut, projAxes, nameSig2D);
      if (data.h5DataSideband)
        h2Sideband = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataSideband, axesToCut, projAxes, nameSb2D);

      if (applyME) {
        h2MESignal = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataMESignal, axesToCut, projAxes, nameMESig2D);
        if (data.h5DataMESideband && h2Sideband)
          h2MESideband = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataMESideband, axesToCut, projAxes, nameMESb2D);
      }

      // Save Raw 2D histograms
      if (ioDir) {
        ioDir->cd();
        h2Signal->Write();
        if (h2Sideband)
          h2Sideband->Write();
        /*if (applyME) {
          if (h2MESignal)
            h2MESignal->Write();
          if (h2MESideband)
            h2MESideband->Write();
        }*/
      }
    }

    // 2. Efficiency Correction
    h2Signal->Scale(1.0 / totalEff);

    // 3. 2D Division: SE / ME to create Corrected 2D histograms
    if (applyME && h2MESignal) {
      double widthX = h2MESignal->GetXaxis()->GetBinWidth(h2MESignal->GetXaxis()->FindBin(0.0));
      double widthY = h2MESignal->GetYaxis()->GetBinWidth(h2MESignal->GetYaxis()->FindBin(0.0));
      auto [normMESignal, errSig] = AnalysisUtils::IntegralAndErrorPair(h2MESignal, -widthX, widthX, -widthY, widthY);
      if (normMESignal > 0)
        h2MESignal->Scale(4.0 / normMESignal);
      if (ioDir && !useCacheMode) {
        ioDir->cd();
        h2MESignal->Write();
      }

      h2Signal->Divide(h2MESignal); // <-- Now h2Signal is the CORRECTED 2D SIGNAL
      if (ioDir && !useCacheMode) {
        ioDir->cd();
        h2Signal->Write((histNameBase + "Signal2D_MECorrected").c_str());
      }
    }

    if (h2Sideband) {
      if (applyME && h2MESideband) {
        double widthX = h2MESideband->GetXaxis()->GetBinWidth(h2MESideband->GetXaxis()->FindBin(0.0));
        double widthY = h2MESideband->GetYaxis()->GetBinWidth(h2MESideband->GetYaxis()->FindBin(0.0));
        auto [normMESideband, errSide] = AnalysisUtils::IntegralAndErrorPair(h2MESideband, -widthX, widthX, -widthY, widthY);
        if (normMESideband > 0)
          h2MESideband->Scale(4.0 / normMESideband);
        if (ioDir && !useCacheMode) {
          ioDir->cd();
          h2MESideband->Write();
        }

        h2Sideband->Divide(h2MESideband); // <-- Now h2Sideband is the CORRECTED 2D SIDEBAND
        if (ioDir && !useCacheMode) {
          ioDir->cd();
          h2Sideband->Write((histNameBase + "Sideband2D_MECorrected").c_str());
        }
      }

      // Sideband scaling
      h2Sideband->Scale(triggerBkgRatio / totalEff);
      if (ioDir && !useCacheMode) {
        ioDir->cd();
        h2Sideband->Write((histNameBase + "Sideband2D_MECorrected_Scaled").c_str());
      }
    }

    // Restrict Delta Y range (Y Axis) before projection
    if (applyDyCut) {
      int binMin = h2Signal->GetYaxis()->FindBin(dyMin + 1e-6);
      int binMax = h2Signal->GetYaxis()->FindBin(dyMax - 1e-6);
      h2Signal->GetYaxis()->SetRange(binMin, binMax);
      if (h2Sideband)
        h2Sideband->GetYaxis()->SetRange(binMin, binMax);
    }

    // 4. Projection onto X Axis (Delta Phi) for both cases
    TH1* h1Signal1D = h2Signal->ProjectionX((histNameBase + "Signal_1D_dPhi").c_str());
    h1Signal1D->SetDirectory(0);

    TH1* h1Sideband1D{nullptr};
    if (h2Sideband) {
      h1Sideband1D = h2Sideband->ProjectionX((histNameBase + "Sideband_1D_dPhi_Scaled").c_str());
      h1Sideband1D->SetDirectory(0);
    }

    // -> SAVE INTERMEDIATE 1D PROJECTIONS <-
    if (ioDir && !useCacheMode) {
      ioDir->cd();
      h1Signal1D->Write();
      if (h1Sideband1D)
        h1Sideband1D->Write();
    }

    // 5. Final 1D Subtraction
    TH1* h1FinalSignal1D = static_cast<TH1*>(h1Signal1D->Clone((histNameBase + "Final_dPhi").c_str()));
    h1FinalSignal1D->SetDirectory(0);
    if (h1Sideband1D && triggerBkgRatio > 0)
      h1FinalSignal1D->Add(h1Sideband1D, -1);
    if (ioDir && !useCacheMode)
      h1FinalSignal1D->Write();

    // 6. Memory Cleanup
    delete h2Signal;
    if (h2Sideband)
      delete h2Sideband;
    if (applyME) {
      if (h2MESignal)
        delete h2MESignal;
      if (h2MESideband)
        delete h2MESideband;
    }
    delete h1Signal1D;
    if (h1Sideband1D)
      delete h1Sideband1D;

    return h1FinalSignal1D;
  }

 private:
  bool applyME{true}, useCacheMode{false}, use2DME{false};

  // Standard 1D extraction (original logic)
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
        h1Signal->Write();
        if (h1Sideband)
          h1Sideband->Write();
        if (applyME) {
          h1MESignal->Write();
          if (h1MESideband)
            h1MESideband->Write();
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

  // New 2D extraction -> Projection to 1D
  TH1* Extract2D(const LoadedAssocData& data, const std::vector<AnalysisUtils::AxisToCut>& axesToCut,
                 double totalEff, double triggerBkgRatio, const std::string& histNameBase,
                 TDirectory* ioDir) const
  {
    TH2* h2Signal{nullptr};
    TH2* h2Sideband{nullptr};
    TH2* h2MESignal{nullptr};
    TH2* h2MESideband{nullptr};

    // Cache names have "2D" appended to prevent loading 1D caches into TH2 pointers
    std::string nameSig2D = histNameBase + "Signal2D";
    std::string nameSb2D = histNameBase + "Sideband2D";
    std::string nameMESig2D = histNameBase + "MESignal2D";
    std::string nameMESb2D = histNameBase + "MESideband2D";

    // 1. Load from cache or project THnSparse
    if (useCacheMode) {
      if (!ioDir)
        throw std::runtime_error("[FATAL] Cache mode requested but no ioDir provided!");

      h2Signal = static_cast<TH2*>(ioDir->Get(nameSig2D.c_str()));
      if (!h2Signal)
        throw std::runtime_error("[FATAL] Cache missing: " + nameSig2D + ". Please run with 'use_projection_cache': false.");
      h2Signal->SetDirectory(0);

      h2Sideband = static_cast<TH2*>(ioDir->Get(nameSb2D.c_str()));
      if (h2Sideband)
        h2Sideband->SetDirectory(0);

      if (applyME) {
        h2MESignal = static_cast<TH2*>(ioDir->Get(nameMESig2D.c_str()));
        if (!h2MESignal)
          throw std::runtime_error("[FATAL] Cache missing: " + nameMESig2D);
        h2MESignal->SetDirectory(0);

        h2MESideband = static_cast<TH2*>(ioDir->Get(nameMESb2D.c_str()));
        if (h2MESideband)
          h2MESideband->SetDirectory(0);
      }
    } else {
      // Projecting on {3, 4}. Axis 3 (Delta Y) becomes Y, Axis 4 (Delta Phi) becomes X.
      std::vector<int> projAxes = {3, 4};

      if (!data.h5DataSignal)
        throw std::runtime_error("[FATAL] Missing Signal THnSparse for " + data.name);

      h2Signal = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataSignal, axesToCut, projAxes, nameSig2D);
      if (data.h5DataSideband)
        h2Sideband = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataSideband, axesToCut, projAxes, nameSb2D);

      if (applyME) {
        if (!data.h5DataMESignal)
          throw std::runtime_error("[FATAL] Missing ME Signal THnSparse for " + data.name);
        h2MESignal = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataMESignal, axesToCut, projAxes, nameMESig2D);
        if (data.h5DataMESideband && h2Sideband)
          h2MESideband = AnalysisUtils::projectTHnSparse<TH2>(data.h5DataMESideband, axesToCut, projAxes, nameMESb2D);
      }

      if (ioDir) {
        ioDir->cd();
        h2Signal->Write();
        if (h2Sideband)
          h2Sideband->Write();
        if (applyME) {
          h2MESignal->Write();
          if (h2MESideband)
            h2MESideband->Write();
        }
      }
    }

    // 2. Efficiency Correction (2D)
    h2Signal->Scale(1.0 / totalEff);

    // 3. Mixed Event Normalization & Division (2D)
    if (applyME) {
      double widthX = h2MESignal->GetXaxis()->GetBinWidth(h2MESignal->GetXaxis()->FindBin(0.0));
      double widthY = h2MESignal->GetYaxis()->GetBinWidth(h2MESignal->GetYaxis()->FindBin(0.0));
      auto [normMESignal, errSig] = AnalysisUtils::IntegralAndErrorPair(h2MESignal, -widthX, widthX, -widthY, widthY);
      if (normMESignal > 0) {
        h2MESignal->Scale(4.0 / normMESignal);
      }

      h2Signal->Divide(h2MESignal);
    }

    if (h2Sideband) {
      if (applyME && h2MESideband) {
        double widthX = h2MESideband->GetXaxis()->GetBinWidth(h2MESideband->GetXaxis()->FindBin(0.0));
        double widthY = h2MESideband->GetYaxis()->GetBinWidth(h2MESideband->GetYaxis()->FindBin(0.0));
        auto [normMESideband, errSide] = AnalysisUtils::IntegralAndErrorPair(h2MESideband, -widthX, widthX, -widthY, widthY);
        if (normMESideband > 0)
          h2MESideband->Scale(4.0 / normMESideband);
        h2Sideband->Divide(h2MESideband);
      }
      h2Sideband->Scale(triggerBkgRatio / totalEff);
    }

    // 4. Background Subtraction (2D)
    TH2* h2FinalSignal = static_cast<TH2*>(h2Signal->Clone((histNameBase + "Final2DTemp").c_str()));
    if (h2Sideband && triggerBkgRatio > 0) {
      h2FinalSignal->Add(h2Sideband, -1);
    }

    // 5. Final Projection to 1D: Delta Y is the Y axis
    TH1* h1FinalSignal1D = h2FinalSignal->ProjectionY(histNameBase.c_str());
    h1FinalSignal1D->SetDirectory(0); // Disconnect from current file directory

    // 6. Cleanup of temporary 2D histograms
    delete h2Signal;
    delete h2FinalSignal;
    if (h2Sideband)
      delete h2Sideband;
    if (applyME) {
      delete h2MESignal;
      if (h2MESideband)
        delete h2MESideband;
    }

    return h1FinalSignal1D;
  }
};

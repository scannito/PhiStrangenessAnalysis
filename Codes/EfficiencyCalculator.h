#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisSettings.h"
#include "AnalysisUtils.h"

#include "TCanvas.h"
#include "TH1.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

class EfficiencyCalculator
{
 public:
  // Enum to specify the type of correction to compute
  enum class ParticleCorrectionMode {
    EfficiencyOnly = 0,
    SignalLossOnly,
    Combined
  };

  // Purely static class
  EfficiencyCalculator() = delete;

  // Computes the Global Event Selection Efficiency (Event Loss)
  // Returns a 1D histogram. Caller owns the pointer and must delete it.
  static TH1* ComputeEventEfficiency(TH1* hEventMultGenAssocReco, TH1* hEventMultGen)
  {
    // Safety check
    if (!hEventMultGenAssocReco || !hEventMultGen) {
      return nullptr;
    }

    TH1* hEventLoss = static_cast<TH1*>(hEventMultGenAssocReco->Clone("hEventLoss"));
    hEventLoss->SetTitle("Event Selection Efficiency;Multiplicity Percentile (%);Efficiency");
    hEventLoss->SetDirectory(0);

    hEventLoss->Divide(hEventMultGenAssocReco, hEventMultGen, 1.0, 1.0, "B");

    return hEventLoss;
  }

  // Computes the 3D Total Efficiency map (Efficiency * Signal Loss)
  // NOTE: The caller becomes the owner of the returned TH3 pointer and must delete it.
  static TH3* Compute3DTotalMap(const LoadedMC& data, ParticleCorrectionMode mode = ParticleCorrectionMode::EfficiencyOnly)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};

    // Project 3D histograms safely from the THnSparse
    TH3* h3MCGenAssocReco = AnalysisUtils::projectTHnSparse<TH3>(data.h4MCGenAssocReco, {axisToCutZVtx}, {1, 2, 3}, "h3" + data.name + "MCGenAssocRecoTemp");

    TH3* h3MCReco = AnalysisUtils::projectTHnSparse<TH3>(data.h4MCReco, {axisToCutZVtx}, {1, 2, 3}, "h3" + data.name + "MCRecoTemp");

    // Calculate 3D Efficiency (Reco / GenAssocReco)
    TH3* h3Efficiency = static_cast<TH3*>(h3MCReco->Clone(("h3" + data.name + "EfficiencyTemp").c_str()));
    h3Efficiency->Divide(h3MCReco, h3MCGenAssocReco, 1.0, 1.0, "B");

    // Calculate 3D Signal Loss (GenAssocReco / Gen)
    TH3* h3SignalLoss = static_cast<TH3*>(h3MCGenAssocReco->Clone(("h3" + data.name + "SignalLossTemp").c_str()));
    h3SignalLoss->Divide(h3MCGenAssocReco, data.h3MCGen, 1.0, 1.0, "B");

    // Compute total 3D correction map: Efficiency, Signal Loss or Efficiency * Signal Loss
    TH3* h3TotalMap{nullptr};
    switch (mode) {
      case ParticleCorrectionMode::EfficiencyOnly:
        h3TotalMap = static_cast<TH3*>(h3Efficiency->Clone("ccdb_object"));
        break;

      case ParticleCorrectionMode::SignalLossOnly:
        h3TotalMap = static_cast<TH3*>(h3SignalLoss->Clone("ccdb_object"));
        break;

      case ParticleCorrectionMode::Combined:
        h3TotalMap = static_cast<TH3*>(h3Efficiency->Clone("ccdb_object"));
        h3TotalMap->Multiply(h3SignalLoss);
        break;
    }

    if (h3TotalMap)
      h3TotalMap->SetDirectory(0);

    // Clean up temporaries to prevent memory leaks
    delete h3MCGenAssocReco;
    delete h3MCReco;
    delete h3Efficiency;
    delete h3SignalLoss;

    return h3TotalMap;
  }

  // Computes the 2D Total Efficiency map (Efficiency * Signal Loss) integrated over multiplicity
  // NOTE: The caller becomes the owner of the returned TH2 pointer and must delete it.
  static TH2* Compute2DTotalMapMultIntegrated(const LoadedMC& data, const AnalysisSettings& cfg, ParticleCorrectionMode mode = ParticleCorrectionMode::EfficiencyOnly)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};
    AnalysisUtils::AxisToCut axisToCutMult{1, 1, cfg.nBinMult};

    // Project Generator level 2D
    std::string h2GenName = "h2" + data.name + "MCGen_multInt_temp";
    TH2* h2MCGen = static_cast<TH2D*>(data.h3MCGen->Project3D("zy"));
    h2MCGen->SetDirectory(0);

    // Project 2D histograms safely from the THnSparse
    TH2* h2MCGenAssocReco = AnalysisUtils::projectTHnSparse<TH2>(data.h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult}, {3, 2}, "h2" + data.name + "MCGenAssocRecoTemp");

    TH2* h2MCReco = AnalysisUtils::projectTHnSparse<TH2>(data.h4MCReco, {axisToCutZVtx, axisToCutMult}, {3, 2}, "h2" + data.name + "MCRecoTemp");

    // Calculate 2D Efficiency (Reco / GenAssocReco)
    TH2* h2Efficiency = static_cast<TH2*>(h2MCReco->Clone(("h2" + data.name + "EfficiencyTemp").c_str()));
    h2Efficiency->Divide(h2MCReco, h2MCGenAssocReco, 1.0, 1.0, "B");

    // Calculate 2D Signal Loss (GenAssocReco / Gen)
    TH2* h2SignalLoss = static_cast<TH2*>(h2MCGenAssocReco->Clone(("h2" + data.name + "SignalLossTemp").c_str()));
    h2SignalLoss->Divide(h2MCGenAssocReco, h2MCGen, 1.0, 1.0, "B");

    // Compute total 2D correction map: Efficiency, Signal Loss or Efficiency * Signal Loss
    TH2* h2TotalMapMultInt{nullptr};
    switch (mode) {
      case ParticleCorrectionMode::EfficiencyOnly:
        h2TotalMapMultInt = static_cast<TH2*>(h2Efficiency->Clone("ccdb_object"));
        break;

      case ParticleCorrectionMode::SignalLossOnly:
        h2TotalMapMultInt = static_cast<TH2*>(h2SignalLoss->Clone("ccdb_object"));
        break;

      case ParticleCorrectionMode::Combined:
        h2TotalMapMultInt = static_cast<TH2*>(h2Efficiency->Clone("ccdb_object"));
        h2TotalMapMultInt->Multiply(h2SignalLoss);
        break;
    }

    if (h2TotalMapMultInt)
      h2TotalMapMultInt->SetDirectory(0);

    // Clean up temporaries to prevent memory leaks
    delete h2MCGenAssocReco;
    delete h2MCReco;
    delete h2Efficiency;
    delete h2SignalLoss;

    return h2TotalMapMultInt;
  }

  // Builds a "twin" TH2 where each bin content is the bin ERROR of the source
  // histogram (instead of its value). Useful for spotting low-statistics regions.
  // Caller owns the returned pointer and must delete it.
  static TH2* BuildErrorMap(const TH2* h2Source, const std::string& name)
  {
    if (!h2Source)
      return nullptr;

    TH2* h2Error = static_cast<TH2*>(h2Source->Clone(name.c_str()));
    h2Error->Reset();
    h2Error->SetDirectory(0);

    for (int ix = 1; ix <= h2Source->GetNbinsX(); ++ix) {
      for (int iy = 1; iy <= h2Source->GetNbinsY(); ++iy) {
        h2Error->SetBinContent(ix, iy, h2Source->GetBinError(ix, iy));
      }
    }

    return h2Error;
  }

  // Same as BuildErrorMap, but returns the RELATIVE error (error / content) per
  // bin. Bins with zero content are left at 0 to avoid division by zero.
  // Caller owns the returned pointer and must delete it.
  static TH2* BuildRelativeErrorMap(const TH2* h2Source, const std::string& name)
  {
    if (!h2Source)
      return nullptr;

    TH2* h2RelError = static_cast<TH2*>(h2Source->Clone(name.c_str()));
    h2RelError->Reset();
    h2RelError->SetDirectory(0);

    for (int ix = 1; ix <= h2Source->GetNbinsX(); ++ix) {
      for (int iy = 1; iy <= h2Source->GetNbinsY(); ++iy) {
        double content = h2Source->GetBinContent(ix, iy);
        double error = h2Source->GetBinError(ix, iy);
        if (content != 0.0)
          h2RelError->SetBinContent(ix, iy, error / content);
      }
    }

    return h2RelError;
  }

  // Computes 1D Efficiency and Signal Loss for a specific multiplicity bin
  // Returns a pair: {Efficiency 1D, Signal Loss 1D}. Caller owns the pointers.
  static std::pair<TH1*, TH1*> Compute1DMaps(const LoadedMC& data, const AnalysisSettings& cfg, int multBin)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};
    AnalysisUtils::AxisToCut axisToCutMult{1, multBin + 1, multBin + 1};
    AnalysisUtils::AxisToCut axisToCutY{3, 1, AnalysisConstants::nBinY};

    // Project Generator level 1D
    std::string h1GenName = "h1" + data.name + "MCGen_multBin" + std::to_string(multBin) + "_temp";
    TH1* h1MCGen = data.h3MCGen->ProjectionY(h1GenName.c_str(), multBin + 1, multBin + 1, 1, AnalysisConstants::nBinY);
    h1MCGen->SetDirectory(0);

    // Project Sparse levels 1D
    TH1* h1MCGenAssocReco = AnalysisUtils::projectTHnSparse<TH1>(data.h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1AssocTemp");

    TH1* h1MCReco = AnalysisUtils::projectTHnSparse<TH1>(data.h4MCReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1RecoTemp");

    // 1D Efficiency Calculation
    std::string h1EffName = "h1" + data.name + "Efficiency_multBin" + std::to_string(multBin);
    TH1* h1Efficiency1D = static_cast<TH1*>(h1MCReco->Clone(h1EffName.c_str()));
    h1Efficiency1D->SetDirectory(0);
    h1Efficiency1D->Divide(h1MCReco, h1MCGenAssocReco, 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1Efficiency1D, cfg.GetSpectraColor(multBin));

    // 1D Signal Loss Calculation
    std::string h1SigName = "h1" + data.name + "SigLoss_multBin" + std::to_string(multBin);
    TH1* h1SignalLoss1D = static_cast<TH1*>(h1MCGenAssocReco->Clone(h1SigName.c_str()));
    h1SignalLoss1D->SetDirectory(0);
    h1SignalLoss1D->Divide(h1MCGenAssocReco, h1MCGen, 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1SignalLoss1D, cfg.GetSpectraColor(multBin));

    // Clean up temporaries
    delete h1MCGen;
    delete h1MCGenAssocReco;
    delete h1MCReco;

    return {h1Efficiency1D, h1SignalLoss1D};
  }

  // Computes 1D Efficiency and Signal Loss integrated over ALL multiplicity bins
  // Returns a pair: {Efficiency 1D, Signal Loss 1D}. Caller owns the pointers.
  static std::pair<TH1*, TH1*> Compute1DMapsMultIntegrated(const LoadedMC& data, const AnalysisSettings& cfg)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};
    AnalysisUtils::AxisToCut axisToCutMult{1, 1, cfg.nBinMult};
    AnalysisUtils::AxisToCut axisToCutY{3, 1, AnalysisConstants::nBinY};

    // Project Generator level 1D (Integrated)
    std::string h1GenName = "h1" + data.name + "MCGen_Integrated_temp";
    TH1* h1MCGen = data.h3MCGen->ProjectionY(h1GenName.c_str(), 1, cfg.nBinMult, 1, AnalysisConstants::nBinY);
    h1MCGen->SetDirectory(0);

    // Project Sparse levels 1D (Integrated)
    TH1* h1MCGenAssocReco = AnalysisUtils::projectTHnSparse<TH1>(data.h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1AssocTemp_Int");

    TH1* h1MCReco = AnalysisUtils::projectTHnSparse<TH1>(data.h4MCReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1RecoTemp_Int");

    // 1D Efficiency Calculation
    std::string h1EffName = "h1" + data.name + "Efficiency_multIntegrated";
    TH1* h1Efficiency1D = static_cast<TH1*>(h1MCReco->Clone(h1EffName.c_str()));
    h1Efficiency1D->SetDirectory(0);
    h1Efficiency1D->Divide(h1MCReco, h1MCGenAssocReco, 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1Efficiency1D, kBlack);

    // 1D Signal Loss Calculation
    std::string h1SigName = "h1" + data.name + "SigLoss_multIntegrated";
    TH1* h1SignalLoss1D = static_cast<TH1*>(h1MCGenAssocReco->Clone(h1SigName.c_str()));
    h1SignalLoss1D->SetDirectory(0);
    h1SignalLoss1D->Divide(h1MCGenAssocReco, h1MCGen, 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1SignalLoss1D, kBlack);

    // Clean up temporaries
    delete h1MCGen;
    delete h1MCGenAssocReco;
    delete h1MCReco;

    return {h1Efficiency1D, h1SignalLoss1D};
  }
};

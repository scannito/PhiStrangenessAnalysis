#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "AnalysisUtils.h"

#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <array>
#include <memory>
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
  static std::unique_ptr<TH1> ComputeEventEfficiency(const TH1* hEventMultGenAssocReco, const TH1* hEventMultGen)
  {
    // Safety check
    if (!hEventMultGenAssocReco || !hEventMultGen) {
      return nullptr;
    }

    std::unique_ptr<TH1> hEventLoss(static_cast<TH1*>(hEventMultGenAssocReco->Clone("hEventLoss")));
    hEventLoss->SetTitle("Event Selection Efficiency;Multiplicity Percentile (%);Efficiency");
    hEventLoss->SetDirectory(0);

    hEventLoss->Divide(hEventMultGenAssocReco, hEventMultGen, 1.0, 1.0, "B");

    return hEventLoss;
  }

  // Computes the 3D Total Efficiency map (Efficiency * Signal Loss)
  static std::unique_ptr<TH3> Compute3DTotalMap(const LoadedMC& data, ParticleCorrectionMode mode = ParticleCorrectionMode::EfficiencyOnly)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};

    // Project 3D histograms safely from the THnSparse
    std::unique_ptr<TH3> h3MCGenAssocReco = AnalysisUtils::ProjectTHnSparse<TH3>(data.h4MCGenAssocReco.get(), {axisToCutZVtx}, {1, 2, 3}, "h3" + data.name + "MCGenAssocRecoTemp");
    std::unique_ptr<TH3> h3MCReco = AnalysisUtils::ProjectTHnSparse<TH3>(data.h4MCReco.get(), {axisToCutZVtx}, {1, 2, 3}, "h3" + data.name + "MCRecoTemp");

    // Calculate 3D Efficiency (Reco / GenAssocReco)
    std::unique_ptr<TH3> h3Efficiency(static_cast<TH3*>(h3MCReco->Clone(("h3" + data.name + "EfficiencyTemp").c_str())));
    h3Efficiency->Divide(h3MCReco.get(), h3MCGenAssocReco.get(), 1.0, 1.0, "B");

    // Calculate 3D Signal Loss (GenAssocReco / Gen)
    std::unique_ptr<TH3> h3SignalLoss(static_cast<TH3*>(h3MCGenAssocReco->Clone(("h3" + data.name + "SignalLossTemp").c_str())));
    h3SignalLoss->Divide(h3MCGenAssocReco.get(), data.h3MCGen.get(), 1.0, 1.0, "B");

    // Compute total 3D correction map: Efficiency, Signal Loss or Efficiency * Signal Loss
    std::unique_ptr<TH3> h3TotalMap;
    switch (mode) {
      case ParticleCorrectionMode::EfficiencyOnly:
        h3TotalMap = std::unique_ptr<TH3>(static_cast<TH3*>(h3Efficiency->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::SignalLossOnly:
        h3TotalMap = std::unique_ptr<TH3>(static_cast<TH3*>(h3SignalLoss->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::Combined:
        h3TotalMap = std::unique_ptr<TH3>(static_cast<TH3*>(h3Efficiency->Clone("ccdb_object")));
        h3TotalMap->Multiply(h3SignalLoss.get());
        break;
    }

    if (h3TotalMap)
      h3TotalMap->SetDirectory(0);

    return h3TotalMap;
  }

  // Computes the 2D Total Efficiency map (Efficiency * Signal Loss) integrated over multiplicity
  static std::unique_ptr<TH2> Compute2DTotalMapMultIntegrated(const LoadedMC& data, ParticleCorrectionMode mode = ParticleCorrectionMode::EfficiencyOnly)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};
    // "Integrated" means the whole axis: take its extent from the container
    // itself rather than from a configured bin count.
    AnalysisUtils::AxisToCut axisToCutMult{1, 1, data.h4MCReco->GetAxis(1)->GetNbins()};

    // Project Generator level 2D
    std::string h2GenName = "h2" + data.name + "MCGen_multInt_temp";
    std::unique_ptr<TH2> h2MCGen(static_cast<TH2D*>(data.h3MCGen->Project3D("zy")));
    h2MCGen->SetDirectory(0);

    // Project 2D histograms safely from the THnSparse
    std::unique_ptr<TH2> h2MCGenAssocReco = AnalysisUtils::ProjectTHnSparse<TH2>(data.h4MCGenAssocReco.get(), {axisToCutZVtx, axisToCutMult}, {3, 2}, "h2" + data.name + "MCGenAssocRecoTemp");
    std::unique_ptr<TH2> h2MCReco = AnalysisUtils::ProjectTHnSparse<TH2>(data.h4MCReco.get(), {axisToCutZVtx, axisToCutMult}, {3, 2}, "h2" + data.name + "MCRecoTemp");

    // Calculate 2D Efficiency (Reco / GenAssocReco)
    std::unique_ptr<TH2> h2Efficiency(static_cast<TH2*>(h2MCReco->Clone(("h2" + data.name + "EfficiencyTemp").c_str())));
    h2Efficiency->Divide(h2MCReco.get(), h2MCGenAssocReco.get(), 1.0, 1.0, "B");

    // Calculate 2D Signal Loss (GenAssocReco / Gen)
    std::unique_ptr<TH2> h2SignalLoss(static_cast<TH2*>(h2MCGenAssocReco->Clone(("h2" + data.name + "SignalLossTemp").c_str())));
    h2SignalLoss->Divide(h2MCGenAssocReco.get(), h2MCGen.get(), 1.0, 1.0, "B");

    // Compute total 2D correction map: Efficiency, Signal Loss or Efficiency * Signal Loss
    std::unique_ptr<TH2> h2TotalMapMultInt;
    switch (mode) {
      case ParticleCorrectionMode::EfficiencyOnly:
        h2TotalMapMultInt = std::unique_ptr<TH2>(static_cast<TH2*>(h2Efficiency->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::SignalLossOnly:
        h2TotalMapMultInt = std::unique_ptr<TH2>(static_cast<TH2*>(h2SignalLoss->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::Combined:
        h2TotalMapMultInt = std::unique_ptr<TH2>(static_cast<TH2*>(h2Efficiency->Clone("ccdb_object")));
        h2TotalMapMultInt->Multiply(h2SignalLoss.get());
        break;
    }

    if (h2TotalMapMultInt)
      h2TotalMapMultInt->SetDirectory(0);

    return h2TotalMapMultInt;
  }

  // Builds a "twin" TH2 where each bin content is the bin ERROR of the source
  // histogram (instead of its value). Useful for spotting low-statistics regions.
  static std::unique_ptr<TH2> BuildErrorMap(const TH2* h2Source, const std::string& name)
  {
    if (!h2Source)
      return nullptr;

    std::unique_ptr<TH2> h2Error(static_cast<TH2*>(h2Source->Clone(name.c_str())));
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
  static std::unique_ptr<TH2> BuildRelativeErrorMap(const TH2* h2Source, const std::string& name)
  {
    if (!h2Source)
      return nullptr;

    std::unique_ptr<TH2> h2RelError(static_cast<TH2*>(h2Source->Clone(name.c_str())));
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
  // Returns a pair: {Efficiency 1D, Signal Loss 1D}.
  static std::pair<std::unique_ptr<TH1>, std::unique_ptr<TH1>> Compute1DMaps(const LoadedMC& data, int multBin, int color)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};
    AnalysisUtils::AxisToCut axisToCutMult{1, multBin + 1, multBin + 1};
    AnalysisUtils::AxisToCut axisToCutY{3, 1, AnalysisConstants::nBinY};

    // Project Generator level 1D
    std::string h1GenName = "h1" + data.name + "MCGen_multBin" + std::to_string(multBin) + "_temp";
    std::unique_ptr<TH1> h1MCGen(data.h3MCGen->ProjectionY(h1GenName.c_str(), multBin + 1, multBin + 1, 1, AnalysisConstants::nBinY));
    h1MCGen->SetDirectory(0);

    // Project Sparse levels 1D
    std::unique_ptr<TH1> h1MCGenAssocReco = AnalysisUtils::ProjectTHnSparse<TH1>(data.h4MCGenAssocReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1AssocTemp");
    std::unique_ptr<TH1> h1MCReco = AnalysisUtils::ProjectTHnSparse<TH1>(data.h4MCReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1RecoTemp");

    // 1D Efficiency Calculation
    std::string h1EffName = "h1" + data.name + "Efficiency_multBin" + std::to_string(multBin);
    std::unique_ptr<TH1> h1Efficiency1D(static_cast<TH1*>(h1MCReco->Clone(h1EffName.c_str())));
    h1Efficiency1D->SetDirectory(0);
    h1Efficiency1D->Divide(h1MCReco.get(), h1MCGenAssocReco.get(), 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1Efficiency1D.get(), color);

    // 1D Signal Loss Calculation
    std::string h1SigName = "h1" + data.name + "SigLoss_multBin" + std::to_string(multBin);
    std::unique_ptr<TH1> h1SignalLoss1D(static_cast<TH1*>(h1MCGenAssocReco->Clone(h1SigName.c_str())));
    h1SignalLoss1D->SetDirectory(0);
    h1SignalLoss1D->Divide(h1MCGenAssocReco.get(), h1MCGen.get(), 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1SignalLoss1D.get(), color);

    return {std::move(h1Efficiency1D), std::move(h1SignalLoss1D)};
  }

  // Computes 1D Efficiency and Signal Loss integrated over ALL multiplicity bins
  // Returns a pair: {Efficiency 1D, Signal Loss 1D}.
  static std::pair<std::unique_ptr<TH1>, std::unique_ptr<TH1>> Compute1DMapsMultIntegrated(const LoadedMC& data)
  {
    AnalysisUtils::AxisToCut axisToCutZVtx{0, 1, AnalysisConstants::nBinZVtx};
    // "Integrated" means the whole axis: take its extent from the containers
    // themselves rather than from a configured bin count.
    AnalysisUtils::AxisToCut axisToCutMult{1, 1, data.h4MCReco->GetAxis(1)->GetNbins()};
    AnalysisUtils::AxisToCut axisToCutY{3, 1, AnalysisConstants::nBinY};

    // Project Generator level 1D (Integrated)
    std::string h1GenName = "h1" + data.name + "MCGen_Integrated_temp";
    std::unique_ptr<TH1> h1MCGen(data.h3MCGen->ProjectionY(h1GenName.c_str(), 1, data.h3MCGen->GetXaxis()->GetNbins(),
                                                           1, AnalysisConstants::nBinY));
    h1MCGen->SetDirectory(0);

    // Project Sparse levels 1D (Integrated)
    std::unique_ptr<TH1> h1MCGenAssocReco = AnalysisUtils::ProjectTHnSparse<TH1>(data.h4MCGenAssocReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1AssocTemp_Int");
    std::unique_ptr<TH1> h1MCReco = AnalysisUtils::ProjectTHnSparse<TH1>(data.h4MCReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1RecoTemp_Int");

    // 1D Efficiency Calculation
    std::string h1EffName = "h1" + data.name + "Efficiency_multIntegrated";
    std::unique_ptr<TH1> h1Efficiency1D(static_cast<TH1*>(h1MCReco->Clone(h1EffName.c_str())));
    h1Efficiency1D->SetDirectory(0);
    h1Efficiency1D->Divide(h1MCReco.get(), h1MCGenAssocReco.get(), 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1Efficiency1D.get(), kBlack);

    // 1D Signal Loss Calculation
    std::string h1SigName = "h1" + data.name + "SigLoss_multIntegrated";
    std::unique_ptr<TH1> h1SignalLoss1D(static_cast<TH1*>(h1MCGenAssocReco->Clone(h1SigName.c_str())));
    h1SignalLoss1D->SetDirectory(0);
    h1SignalLoss1D->Divide(h1MCGenAssocReco.get(), h1MCGen.get(), 1.0, 1.0, "B");
    AnalysisUtils::SetHistogramStyle(h1SignalLoss1D.get(), kBlack);

    return {std::move(h1Efficiency1D), std::move(h1SignalLoss1D)};
  }
};

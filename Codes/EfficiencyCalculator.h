#pragma once

#include "AnalysisConstants.h"
#include "AnalysisDataStructures.h"
#include "BinningUtils.h"
#include "OutputNames.h"
#include "CanvasManager.h"
#include "HistogramUtils.h"

#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TH3F.h"
#include "THnSparse.h"

#include <array>
#include <format>
#include <memory>
#include <span>
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
  static std::unique_ptr<TH1> ComputeEventLoss(const TH1* hEventMultGenAssocReco, const TH1* hEventMultGen)
  {
    // Safety check
    if (!hEventMultGenAssocReco || !hEventMultGen) {
      return nullptr;
    }

    std::unique_ptr<TH1> hEventLoss(static_cast<TH1*>(hEventMultGenAssocReco->Clone(OutputNames::kEventLoss.c_str())));
    hEventLoss->SetTitle("Event Selection Efficiency;Multiplicity Percentile (%);Efficiency");
    hEventLoss->SetDirectory(0);

    hEventLoss->Divide(hEventMultGenAssocReco, hEventMultGen, 1.0, 1.0, "B");

    return hEventLoss;
  }

  // Computes the 3D Total Efficiency map (Efficiency * Signal Loss)
  static std::unique_ptr<TH3> Compute3DCorrectionMap(const LoadedMC& data, ParticleCorrectionMode mode = ParticleCorrectionMode::EfficiencyOnly)
  {
    HistogramUtils::AxisToCut axisToCutZVtx{.axis = 0, .bins = {1, data.h4MCReco->GetAxis(0)->GetNbins()}};

    // Project 3D histograms safely from the THnSparse
    std::unique_ptr<TH3> h3MCGenAssocReco = HistogramUtils::ProjectTHnSparse<TH3>(data.h4MCGenAssocReco.get(), {axisToCutZVtx}, {1, 2, 3}, "h3" + data.name + "MCGenAssocRecoTemp");
    std::unique_ptr<TH3> h3MCReco = HistogramUtils::ProjectTHnSparse<TH3>(data.h4MCReco.get(), {axisToCutZVtx}, {1, 2, 3}, "h3" + data.name + "MCRecoTemp");

    // Calculate 3D Efficiency (Reco / GenAssocReco)
    std::unique_ptr<TH3> h3Efficiency(static_cast<TH3*>(h3MCReco->Clone(("h3" + data.name + "EfficiencyTemp").c_str())));
    h3Efficiency->Divide(h3MCReco.get(), h3MCGenAssocReco.get(), 1.0, 1.0, "B");

    // Calculate 3D Signal Loss (GenAssocReco / Gen)
    std::unique_ptr<TH3> h3SignalLoss(static_cast<TH3*>(h3MCGenAssocReco->Clone(("h3" + data.name + "SignalLossTemp").c_str())));
    h3SignalLoss->Divide(h3MCGenAssocReco.get(), data.h3MCGen.get(), 1.0, 1.0, "B");

    // Compute total 3D correction map: Efficiency, Signal Loss or Efficiency * Signal Loss
    std::unique_ptr<TH3> h3CorrectionMap;
    switch (mode) {
      case ParticleCorrectionMode::EfficiencyOnly:
        h3CorrectionMap = std::unique_ptr<TH3>(static_cast<TH3*>(h3Efficiency->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::SignalLossOnly:
        h3CorrectionMap = std::unique_ptr<TH3>(static_cast<TH3*>(h3SignalLoss->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::Combined:
        h3CorrectionMap = std::unique_ptr<TH3>(static_cast<TH3*>(h3Efficiency->Clone("ccdb_object")));
        h3CorrectionMap->Multiply(h3SignalLoss.get());
        break;
    }

    if (h3CorrectionMap)
      h3CorrectionMap->SetDirectory(0);

    return h3CorrectionMap;
  }

  // Computes the 2D Total Efficiency map (Efficiency * Signal Loss) integrated over multiplicity
  static std::unique_ptr<TH2> Compute2DCorrectionMapMultIntegrated(const LoadedMC& data, ParticleCorrectionMode mode = ParticleCorrectionMode::EfficiencyOnly)
  {
    HistogramUtils::AxisToCut axisToCutZVtx{.axis = 0, .bins = {1, data.h4MCReco->GetAxis(0)->GetNbins()}};
    // "Integrated" means the whole axis: take its extent from the container
    // itself rather than from a configured bin count.
    HistogramUtils::AxisToCut axisToCutMult{.axis = 1, .bins = {1, data.h4MCReco->GetAxis(1)->GetNbins()}};

    // Project Generator level 2D
    std::string h2GenName = "h2" + data.name + "MCGen_multInt_temp";
    std::unique_ptr<TH2> h2MCGen(static_cast<TH2D*>(data.h3MCGen->Project3D("zy")));
    h2MCGen->SetDirectory(0);

    // Project 2D histograms safely from the THnSparse
    std::unique_ptr<TH2> h2MCGenAssocReco = HistogramUtils::ProjectTHnSparse<TH2>(data.h4MCGenAssocReco.get(), {axisToCutZVtx, axisToCutMult}, {3, 2}, "h2" + data.name + "MCGenAssocRecoTemp");
    std::unique_ptr<TH2> h2MCReco = HistogramUtils::ProjectTHnSparse<TH2>(data.h4MCReco.get(), {axisToCutZVtx, axisToCutMult}, {3, 2}, "h2" + data.name + "MCRecoTemp");

    // Calculate 2D Efficiency (Reco / GenAssocReco)
    std::unique_ptr<TH2> h2Efficiency(static_cast<TH2*>(h2MCReco->Clone(("h2" + data.name + "EfficiencyTemp").c_str())));
    h2Efficiency->Divide(h2MCReco.get(), h2MCGenAssocReco.get(), 1.0, 1.0, "B");

    // Calculate 2D Signal Loss (GenAssocReco / Gen)
    std::unique_ptr<TH2> h2SignalLoss(static_cast<TH2*>(h2MCGenAssocReco->Clone(("h2" + data.name + "SignalLossTemp").c_str())));
    h2SignalLoss->Divide(h2MCGenAssocReco.get(), h2MCGen.get(), 1.0, 1.0, "B");

    // Compute total 2D correction map: Efficiency, Signal Loss or Efficiency * Signal Loss
    std::unique_ptr<TH2> h2CorrectionMapMultIntegrated;
    switch (mode) {
      case ParticleCorrectionMode::EfficiencyOnly:
        h2CorrectionMapMultIntegrated = std::unique_ptr<TH2>(static_cast<TH2*>(h2Efficiency->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::SignalLossOnly:
        h2CorrectionMapMultIntegrated = std::unique_ptr<TH2>(static_cast<TH2*>(h2SignalLoss->Clone("ccdb_object")));
        break;

      case ParticleCorrectionMode::Combined:
        h2CorrectionMapMultIntegrated = std::unique_ptr<TH2>(static_cast<TH2*>(h2Efficiency->Clone("ccdb_object")));
        h2CorrectionMapMultIntegrated->Multiply(h2SignalLoss.get());
        break;
    }

    if (h2CorrectionMapMultIntegrated)
      h2CorrectionMapMultIntegrated->SetDirectory(0);

    return h2CorrectionMapMultIntegrated;
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

    for (int binX = 1; binX <= h2Source->GetNbinsX(); ++binX) {
      for (int binY = 1; binY <= h2Source->GetNbinsY(); ++binY) {
        h2Error->SetBinContent(binX, binY, h2Source->GetBinError(binX, binY));
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

    for (int binX = 1; binX <= h2Source->GetNbinsX(); ++binX) {
      for (int binY = 1; binY <= h2Source->GetNbinsY(); ++binY) {
        double content = h2Source->GetBinContent(binX, binY);
        double error = h2Source->GetBinError(binX, binY);
        if (content != 0.0)
          h2RelError->SetBinContent(binX, binY, error / content);
      }
    }

    return h2RelError;
  }

  // Computes 1D Efficiency and Signal Loss for a specific multiplicity bin
  // Rebins the COUNTS, before any division. Merging bins of a ratio would sum
  // the ratios - two bins of efficiency 0.5 would give 1.0 - so the only correct
  // coarse efficiency is sum(reco) / sum(genAssocReco) over the merged interval,
  // which is also the efficiency weighted by the generated spectrum.
  //
  // Nothing happens when no target binning is configured for this particle.
  static void RebinCountsIfRequested(const LoadedMC& data, std::unique_ptr<TH1>& h1MCGen,
                                     std::unique_ptr<TH1>& h1MCGenAssocReco, std::unique_ptr<TH1>& h1MCReco)
  {
    if (!data.rebinningPt)
      return;

    const std::vector<double>& target = data.rebinningPt.value();
    const std::string ctx = std::format("EfficiencyCalculator: '{}' rebinning_pt", data.name);

    h1MCGen = HistogramUtils::RebinToTargetBinning(std::move(h1MCGen), target, ctx);
    h1MCGenAssocReco = HistogramUtils::RebinToTargetBinning(std::move(h1MCGenAssocReco), target, ctx);
    h1MCReco = HistogramUtils::RebinToTargetBinning(std::move(h1MCReco), target, ctx);
  }

  // The maps the analysis uses, plus - only when 'rebinning_pt' merged the counts
  // - the same maps at the binning of the input histograms. Those are not used
  // by the analysis: they show the shape the merge averages over, which is the
  // only way to judge whether merging those bins was reasonable.
  //
  // Note the asymmetry: the *SourceBinning members are null exactly when there
  // was no merge, and that is the case in which 'efficiency' and 'signalLoss'
  // ARE at the source binning. So "the maps at the source binning" is
  //     efficiencySourceBinning ? efficiencySourceBinning : efficiency
  // and never just the first of the two.
  struct Maps1D {
    std::unique_ptr<TH1> efficiency;              // analysis binning: merged when requested,
    std::unique_ptr<TH1> signalLoss;              // otherwise the source binning itself
    std::unique_ptr<TH1> efficiencySourceBinning; // null unless 'rebinning_pt' is set
    std::unique_ptr<TH1> signalLossSourceBinning;
  };

  // The two divisions, shared by the per-multiplicity and the integrated case.
  // Takes the counts by raw pointer because it only reads them.
  static std::pair<std::unique_ptr<TH1>, std::unique_ptr<TH1>>
    DivideCounts(TH1* h1MCGen, TH1* h1MCGenAssocReco, TH1* h1MCReco,
                 const std::string& particle, const std::string& multTag, int color)
  {
    // "B" on both: the reconstructed are a subset of the generated-associated,
    // which are a subset of the generated, so the two ratios are binomial.
    std::unique_ptr<TH1> efficiency = HistogramUtils::MakeRatioHist(
      h1MCReco, h1MCGenAssocReco, OutputNames::Efficiency(particle, multTag),
      ";p_{T} (GeV/#it{c});Acc #times #epsilon", 1.0, 1.0, "B");
    CanvasManager::SetHistogramStyle(efficiency.get(), color);

    std::unique_ptr<TH1> signalLoss = HistogramUtils::MakeRatioHist(
      h1MCGenAssocReco, h1MCGen, OutputNames::SignalLoss(particle, multTag),
      ";p_{T} (GeV/#it{c});Signal loss", 1.0, 1.0, "B");
    CanvasManager::SetHistogramStyle(signalLoss.get(), color);

    return {std::move(efficiency), std::move(signalLoss)};
  }

  // Divides at the source binning first when a merge is requested, then merges
  // counts and divides again. Order matters: DivideCounts only reads, but
  // RebinCountsIfRequested replaces the histograms.
  static Maps1D BuildMaps1D(const LoadedMC& data, std::unique_ptr<TH1>& h1MCGen,
                            std::unique_ptr<TH1>& h1MCGenAssocReco, std::unique_ptr<TH1>& h1MCReco,
                            const std::string& multTag, int color)
  {
    Maps1D maps;

    if (data.rebinningPt) {
      auto [effFine, lossFine] = DivideCounts(h1MCGen.get(), h1MCGenAssocReco.get(), h1MCReco.get(),
                                              data.name, multTag + "_sourceBinning", color);
      maps.efficiencySourceBinning = std::move(effFine);
      maps.signalLossSourceBinning = std::move(lossFine);
    }

    RebinCountsIfRequested(data, h1MCGen, h1MCGenAssocReco, h1MCReco);

    auto [efficiency, signalLoss] = DivideCounts(h1MCGen.get(), h1MCGenAssocReco.get(), h1MCReco.get(),
                                                 data.name, multTag, color);
    maps.efficiency = std::move(efficiency);
    maps.signalLoss = std::move(signalLoss);

    return maps;
  }

  // Returns the maps for one multiplicity bin, named by its interval in 'multBinning':
  // CorrelationTaskBase::LoadCorrections reads them back by that name.
  static Maps1D Compute1DMaps(const LoadedMC& data, std::span<const double> multBinning, int multBin, int color)
  {
    HistogramUtils::AxisToCut axisToCutZVtx{.axis = 0, .bins = {1, data.h4MCReco->GetAxis(0)->GetNbins()}};
    HistogramUtils::AxisToCut axisToCutMult{.axis = 1, .bins = {multBin + 1, multBin + 1}};
    HistogramUtils::AxisToCut axisToCutY{.axis = 3, .bins = {1, data.h4MCReco->GetAxis(3)->GetNbins()}};

    // Project Generator level 1D
    const std::string multTag = OutputNames::MultTag(multBinning, multBin);
    std::string h1GenName = std::format("h1{}MCGen_{}_temp", data.name, multTag);
    std::unique_ptr<TH1> h1MCGen(data.h3MCGen->ProjectionY(h1GenName.c_str(), multBin + 1, multBin + 1, 1, data.h3MCGen->GetZaxis()->GetNbins()));
    h1MCGen->SetDirectory(0);

    // Project Sparse levels 1D
    std::unique_ptr<TH1> h1MCGenAssocReco = HistogramUtils::ProjectTHnSparse<TH1>(data.h4MCGenAssocReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1AssocTemp");
    std::unique_ptr<TH1> h1MCReco = HistogramUtils::ProjectTHnSparse<TH1>(data.h4MCReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1RecoTemp");

    return BuildMaps1D(data, h1MCGen, h1MCGenAssocReco, h1MCReco, multTag, color);
  }

  // Same, integrated over ALL multiplicity bins.
  static Maps1D Compute1DMapsMultIntegrated(const LoadedMC& data)
  {
    HistogramUtils::AxisToCut axisToCutZVtx{.axis = 0, .bins = {1, data.h4MCReco->GetAxis(0)->GetNbins()}};
    // "Integrated" means the whole axis: take its extent from the containers
    // themselves rather than from a configured bin count.
    HistogramUtils::AxisToCut axisToCutMult{.axis = 1, .bins = {1, data.h4MCReco->GetAxis(1)->GetNbins()}};
    HistogramUtils::AxisToCut axisToCutY{.axis = 3, .bins = {1, data.h4MCReco->GetAxis(3)->GetNbins()}};

    // Project Generator level 1D (Integrated)
    std::string h1GenName = "h1" + data.name + "MCGen_Integrated_temp";
    std::unique_ptr<TH1> h1MCGen(data.h3MCGen->ProjectionY(h1GenName.c_str(), 1, data.h3MCGen->GetXaxis()->GetNbins(),
                                                           1, data.h3MCGen->GetZaxis()->GetNbins()));
    h1MCGen->SetDirectory(0);

    // Project Sparse levels 1D (Integrated)
    std::unique_ptr<TH1> h1MCGenAssocReco = HistogramUtils::ProjectTHnSparse<TH1>(data.h4MCGenAssocReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1AssocTemp_Int");
    std::unique_ptr<TH1> h1MCReco = HistogramUtils::ProjectTHnSparse<TH1>(data.h4MCReco.get(), {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, "h1RecoTemp_Int");

    return BuildMaps1D(data, h1MCGen, h1MCGenAssocReco, h1MCReco, OutputNames::kMultIntegrated, kBlack);
  }
};

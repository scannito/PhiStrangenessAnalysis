#include "FitAssocSignalAndBkg.h"
#include "FitConfigManager.h"

#include "RooAddPdf.h"
#include "RooArgList.h"
#include "RooCBShape.h"
#include "RooCrystalBall.h"
#include "RooDataSet.h"
#include "RooExponential.h"
#include "RooFitResult.h"
#include "RooFormulaVar.h"
#include "RooGaussian.h"
#include "RooGenericPdf.h"
#include "RooPlot.h"
#include "RooPolynomial.h"
#include "RooProdPdf.h"
#include "RooProduct.h"
#include "RooVoigtian.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TFitResult.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"
#include "TMath.h"
#include "TMatrixDSym.h"
#include "TStyle.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace RooFit;

enum PartType {
  kPhi = 0,
  kK0S,
  kPi,
  kNParticles
};

static constexpr int nBinMult{10}, nBinPtPhi{7}, nBinPtK0S{9}, nBinPtPi{10}, nBinZVtx{100}, nBinY{20};
static constexpr double kaonMass{0.493677};
static const std::vector<double> binsMult{0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
static const std::vector<double> binspTK0S{0.1, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0};
static const std::vector<double> binspTPi{0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};
static const std::pair<double, double> phiMassSignalRange{1.0095, 1.029};
static const std::pair<double, double> phiMassSidebandRange{1.1, 1.2};
static const std::array<int, 10> spectraColors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856};

struct AxisToCut {
  int axis;
  int binLow;
  int binUp;
};

template <size_t size>
struct ParticleConfig {
  std::string name;                     // Es. "Phi", "K0S", "Pi"
  std::array<std::string, size> titles; // Es. {Gen, GenAssoc, Reco} o {Efficiency, SignalLoss}
};

struct AssocParticleConfig {
  std::string name;            // Es: "K0S", "Pi"
  std::string dirName;         // Es: "phiK0S", "phiPi"
  int nBinPt;                  // Numero bin Pt (nBinPtK0S o nBinPtPi)
  int effIndex;                // Indice nella collezione efficienze (1 per K0S, 2 per Pi)
  std::vector<double> binning; // Binning per lo spettro finale
};

struct LoadedAssocData {
  THnSparseF* h5DataSignal{nullptr};
  THnSparseF* h5DataSideband{nullptr};
  THnSparseF* h5DataMESignal{nullptr};
  THnSparseF* h5DataMESideband{nullptr};
};

template <size_t size>
struct LoadedCorrections {
  std::string name;
  std::array<TH1F*, size> h1Corrections;
};

struct LoadedMc {
  std::string name;
  TH3F* h3MCGen{nullptr};
  THnSparseF* h4MCGenAssocReco{nullptr};
  THnSparseF* h4MCReco{nullptr};
};

std::pair<double, double> operator/(const std::pair<double, double>& pair, double divide)
{
  return {pair.first / divide, pair.second / divide};
}

template <typename THType>
THType* projectTHnSparse(THnSparse* hnSparse,
                         const std::vector<AxisToCut>& axesToBeCut,
                         const std::vector<int>& axesToProject,
                         const std::string& histName)
{
  if (!hnSparse)
    return nullptr;

  for (const auto& axisToCut : axesToBeCut) {
    hnSparse->GetAxis(axisToCut.axis)->SetRange(axisToCut.binLow, axisToCut.binUp);
  }

  THType* hProjection = nullptr;

  if constexpr (std::is_base_of_v<TH3, THType>) {
    hProjection = hnSparse->Projection(axesToProject[0], axesToProject[1], axesToProject[2]);
  } else if constexpr (std::is_base_of_v<TH2, THType>) {
    hProjection = hnSparse->Projection(axesToProject[0], axesToProject[1]);
  } else if constexpr (std::is_base_of_v<TH1, THType>) {
    hProjection = hnSparse->Projection(axesToProject[0]);
  }

  hProjection->SetName(histName.c_str());
  hProjection->SetDirectory(0);
  hProjection->Sumw2();

  return hProjection;
}

std::pair<double, double> IntegralAndErrorPair(TH1* h1, double x1, double x2, Option_t* option = "")
{
  double integral{0.0};
  double error{0.0};
  double epsilon{0.00001};

  integral = h1->IntegralAndError(h1->GetXaxis()->FindBin(x1 + epsilon), h1->GetXaxis()->FindBin(x2 - epsilon), error, option);

  return std::make_pair(integral, error);
}

double Voigt(double* x, double* par)
{
  double mass = x[0];

  return par[0] * TMath::Voigt(mass - par[1], par[2], par[3]);
}

double BkgSourav(double* x, double* par)
{
  double mass = x[0];

  return par[0] + par[1] * mass + par[2] * std::sqrt(mass - 2 * kaonMass);
}

double BkgMattia(double* x, double* par)
{
  double mass = x[0];

  return par[0] * std::pow(mass - 2 * kaonMass, par[1]) * std::exp(par[2] * (mass - 2 * kaonMass) + par[3] * std::pow(mass - 2 * kaonMass, 2) + par[4] * std::pow(mass - 2 * kaonMass, 3));
}

double VoigtBkgSourav(double* x, double* par)
{
  return Voigt(x, &par[0]) + BkgSourav(x, &par[4]);
}

double VoigtBkgMattia(double* x, double* par)
{
  return Voigt(x, &par[0]) + BkgMattia(x, &par[4]);
}

template <bool wSidebandFit>
class FitPhiSignalAndBkg
{
 public:
  FitPhiSignalAndBkg(TH1* h,
                     TF1* fitFunc,
                     int indexFirstBkgParam,
                     std::pair<double, double> signalRegion,
                     std::pair<double, double> sidebandRegion)
    : h1(h), fitFunction(fitFunc)
  {
    double binWidth = h1->GetXaxis()->GetBinWidth(1);

    TFitResultPtr fitResult = h1->Fit(fitFunction, "RSN");
    TMatrixDSym covMatrix = fitResult->GetCovarianceMatrix();

    h1->GetListOfFunctions()->Add(fitFunction->Clone());

    TF1* signalFunction = new TF1("Voigt", Voigt, signalRegion.first, signalRegion.second, 4);
    signalFunction->SetLineColor(kBlue);
    TF1* bkgFunction = new TF1("Bkg", BkgSourav, signalRegion.first, signalRegion.second, 3);
    bkgFunction->SetLineColor(kGreen + 2);

    TMatrixDSym covSignal(indexFirstBkgParam);
    TMatrixDSym covBkg(fitFunction->GetNpar() - indexFirstBkgParam);

    for (int i = 0; i < indexFirstBkgParam; i++) {
      signalFunction->SetParameter(i, fitFunction->GetParameter(i));
      for (int j = 0; j < indexFirstBkgParam; j++) {
        covSignal(i, j) = covMatrix(i, j);
      }
    }

    for (int i = indexFirstBkgParam; i < fitFunction->GetNpar(); i++) {
      bkgFunction->SetParameter(i - indexFirstBkgParam, fitFunction->GetParameter(i));
      for (int j = indexFirstBkgParam; j < fitFunction->GetNpar(); j++) {
        covBkg(i - indexFirstBkgParam, j - indexFirstBkgParam) = covMatrix(i, j);
      }
    }

    h1->GetListOfFunctions()->Add(signalFunction->Clone());
    h1->GetListOfFunctions()->Add(bkgFunction->Clone());

    signalIntegralAndError.first = signalFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
    signalIntegralAndError.second = signalFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covSignal.GetMatrixArray()) / binWidth;

    bkgIntegralAndErrorInSigRegion.first = bkgFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
    bkgIntegralAndErrorInSigRegion.second = bkgFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covBkg.GetMatrixArray()) / binWidth;

    if constexpr (wSidebandFit) {
      bkgIntegralAndErrorInSideRegion.first = bkgFunction->Integral(sidebandRegion.first, sidebandRegion.second) / binWidth;
      bkgIntegralAndErrorInSideRegion.second = bkgFunction->IntegralError(sidebandRegion.first, sidebandRegion.second, fitFunction->GetParameters(), covBkg.GetMatrixArray()) / binWidth;
    } else {
      bkgIntegralAndErrorInSideRegion = IntegralAndErrorPair(h1, sidebandRegion.first, sidebandRegion.second);
    }

    std::cout << "Signal Integral: " << signalIntegralAndError.first << " +/- " << signalIntegralAndError.second << std::endl;
    std::cout << "Bkg Integral in Signal Region: " << bkgIntegralAndErrorInSigRegion.first << " +/- " << bkgIntegralAndErrorInSigRegion.second << std::endl;
    std::cout << "Bkg Integral in Sideband Region: " << bkgIntegralAndErrorInSideRegion.first << " +/- " << bkgIntegralAndErrorInSideRegion.second << std::endl;
  }

  double getSignal() const { return signalIntegralAndError.first; }
  double getSignalError() const { return signalIntegralAndError.second; }
  std::pair<double, double> getSignalAndError() const { return signalIntegralAndError; }

  double getBkgInSigRegion() const { return bkgIntegralAndErrorInSigRegion.first; }
  double getBkgInSigRegionError() const { return bkgIntegralAndErrorInSigRegion.second; }
  std::pair<double, double> getBkgInSigRegionAndError() const { return bkgIntegralAndErrorInSigRegion; }

  double getBkgInSideRegion() const { return bkgIntegralAndErrorInSideRegion.first; }
  double getBkgInSideRegionError() const { return bkgIntegralAndErrorInSideRegion.second; }
  std::pair<double, double> getBkgInSideRegionAndError() const { return bkgIntegralAndErrorInSideRegion; }

 private:
  TH1* h1 = nullptr;
  TF1* fitFunction = nullptr;

  std::pair<double, double> signalIntegralAndError{0.0, 0.0};
  std::pair<double, double> bkgIntegralAndErrorInSigRegion{0.0, 0.0};
  std::pair<double, double> bkgIntegralAndErrorInSideRegion{0.0, 0.0};
};

template <typename Container>
TH1* constructSpectrum(const Container& hContainer,
                       const std::vector<double>& binsVec,
                       const std::string& histName,
                       double absLimToIntegrate)
{
  if (binsVec.size() - 1 != hContainer.size()) {
    throw std::runtime_error("Size of histogram container must be equal to number of bins - 1");
  }

  TH1* hSpectrum = new TH1D(histName.c_str(), histName.c_str(), binsVec.size() - 1, binsVec.data());

  for (size_t i{0}; i < hContainer.size(); i++) {
    auto [binContent, binError] = IntegralAndErrorPair(hContainer[i], -absLimToIntegrate, absLimToIntegrate);
    double normalizationFactor = (binsVec[i + 1] - binsVec[i]);

    hSpectrum->SetBinContent(i + 1, binContent / normalizationFactor);
    hSpectrum->SetBinError(i + 1, binError / normalizationFactor);
  }

  hSpectrum->SetDirectory(0);

  return hSpectrum;
}

/*template <bool wExtrapolation>
void constructMultTrend(TH1 *hMultTrend,
                        TH1 *hPtSpectrum)*/
template <bool wExtrapolation>
void constructMultTrend(TH1* hMultTrend,
                        TH1* hPtSpectrum,
                        int i)
{
  // static int i{nBinMult};

  /*double content{0.0};
  for (int k{1}; k <= hPtSpectrum->GetNbinsX(); k++)
  {
      content += hPtSpectrum->GetBinContent(k) * hPtSpectrum->GetBinWidth(k);
  }*/

  auto [content, error] = IntegralAndErrorPair(hPtSpectrum, hPtSpectrum->GetXaxis()->GetXmin(), hPtSpectrum->GetXaxis()->GetXmax(), "width");
  if constexpr (wExtrapolation) {
    content += 0.0;
  }

  hMultTrend->SetBinContent(i + 1, content);
  hMultTrend->SetBinError(i + 1, error);

  // i--;
}

void SetHistogramStyle(TH1* h1, int color)
{
  h1->SetMarkerStyle(20);
  h1->SetMarkerColor(color);
  h1->SetMarkerSize(1.5);
  h1->SetLineColor(color);
  h1->SetLineWidth(2);
  h1->SetFillStyle(3001);
  h1->SetFillColor(color);
  // h1->GetXaxis()->SetLabelOffset(0.5);
  h1->GetYaxis()->SetTitleSize(0.045);
  h1->GetYaxis()->SetTitleOffset(1.0);
  h1->GetYaxis()->SetLabelSize(0.045);
  // h1->GetYaxis()->SetRangeUser(1e-3, 1e1);
  // h1->GetYaxis()->SetRangeUser(0.4e-5, 1.3e-1);
}

/// @brief ////////////
/// @param applyME
/// @param applyEfficiency
void AnalysisData(bool applyME = false, bool applyEfficiency = false)
{
  std::string basePathData = "phi-strangeness-correlation_id44940/phiStrangenessCorrelation/";
  std::string basePathDataME = "phi-strangeness-correlation_id46273/phiStrangenessCorrelation/";

  std::vector<AssocParticleConfig> assocParticles = {
    {"K0S", "phiK0S", nBinPtK0S, 1, binspTK0S}, // effIndex 1 perché 0 è Phi
    {"Pi", "phiPi", nBinPtPi, 2, binspTPi}};

  std::vector<LoadedAssocData> loadedDataCollection;
  loadedDataCollection.reserve(assocParticles.size());

  // TFile *fileDataInput = new TFile("../DataFile/Data/AnalysisResults_136.root");
  // TFile *fileDataInput = new TFile("../DataFile/Data/AnalysisResults_136_test.root");
  TFile* fileDataInput = new TFile("../DataFile/Data/AnalysisResults_136_2marzo.root");
  // TFile *fileDataMEInput = new TFile("../DataFile/DataME/AnalysisResultsME_136.root");
  // TFile *fileDataMEInput = new TFile("../DataFile/DataME/AnalysisResultsME_136_test.root");
  TFile* fileDataMEInput = new TFile("../DataFile/Data/AnalysisResults_136_2marzo.root");

  TH3F* h3PhiData = static_cast<TH3F*>(fileDataInput->Get((basePathData + "phi/h3PhiData").c_str()));
  h3PhiData->SetDirectory(0);

  auto loadAssocData = [&](const AssocParticleConfig& config) -> LoadedAssocData {
    LoadedAssocData data;

    std::string baseData = basePathData + config.dirName + "/h5Phi" + config.name;
    std::string baseDataME = basePathDataME + config.dirName + "/h5Phi" + config.name;

    data.h5DataSignal = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSignal").c_str()));
    data.h5DataSideband = static_cast<THnSparseF*>(fileDataInput->Get((baseData + "DataSideband").c_str()));

    data.h5DataMESignal = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESignal").c_str()));
    data.h5DataMESideband = static_cast<THnSparseF*>(fileDataMEInput->Get((baseDataME + "DataMESideband").c_str()));

    return data;
  };

  for (const auto& p : assocParticles) {
    loadedDataCollection.push_back(loadAssocData(p));
  }

  fileDataInput->Close();
  fileDataMEInput->Close();

  std::vector<ParticleConfig<2>> particles = {
    {"Phi", {"h1PhiEfficiency", "h1PhiSigLoss"}},
    {"K0S", {"h1K0SEfficiency", "h1K0SSigLoss"}},
    {"Pi", {"h1PiEfficiency", "h1PiSigLoss"}}};

  std::vector<LoadedCorrections<nBinMult>> correctionCollection;

  if (applyEfficiency) {
    TFile* fileEffInput = new TFile("../DataFile/Corrections/Corrections.root");

    correctionCollection.reserve(particles.size());

    auto loadCorrections = [&](const ParticleConfig<2>& config) -> LoadedCorrections<nBinMult> {
      LoadedCorrections<nBinMult> corrections;

      corrections.name = config.name;

      for (int i{0}; i < nBinMult; i++) {
        TH1F* h1Efficiency = static_cast<TH1F*>(fileEffInput->Get((config.titles[0] + "_multBin" + std::to_string(i)).c_str()));
        TH1F* h1SignalLoss = static_cast<TH1F*>(fileEffInput->Get((config.titles[1] + "_multBin" + std::to_string(i)).c_str()));
        corrections.h1Corrections[i] = static_cast<TH1F*>(h1Efficiency->Clone());
        corrections.h1Corrections[i]->Multiply(h1Efficiency, h1SignalLoss);
        corrections.h1Corrections[i]->SetDirectory(0);
      }

      return corrections;
    };

    for (const auto& p : particles) {
      correctionCollection.push_back(loadCorrections(p));
    }

    fileEffInput->Close();
  }

  std::vector<std::array<std::vector<TH1*>, nBinMult>> h1PhiAssocNoPtPhi(assocParticles.size());
  for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
    for (int i = 0; i < nBinMult; ++i) {
      h1PhiAssocNoPtPhi[pIdx][i].resize(assocParticles[pIdx].nBinPt, nullptr);
    }
  }

  TFile* filePhiDataOutput = new TFile("../DataFile/Output/PhiDataHistograms.root", "RECREATE");
  std::vector<TFile*> outputFiles;
  std::vector<TCanvas*> spectraCanvases;
  for (const auto& p : assocParticles) {
    outputFiles.push_back(new TFile(("../DataFile/Output/Phi" + p.name + "DataHistograms.root").c_str(), "RECREATE"));
    spectraCanvases.push_back(new TCanvas(("canvasSpectra" + p.name).c_str(), ("canvasSpectra" + p.name).c_str(), 800, 600));
  }

  std::vector<TH1*> h1MultTrends1, h1MultTrends05, h1MultTrends01;
  for (const auto& p : assocParticles) {
    TH1* h1 = new TH1F(("multTrend1" + p.name).c_str(), ("multTrend1" + p.name).c_str(), nBinMult, binsMult.data());
    h1->SetDirectory(0);
    h1MultTrends1.push_back(h1);

    TH1* h05 = static_cast<TH1F*>(h1->Clone(("multTrend05" + p.name).c_str()));
    h05->SetDirectory(0);
    h1MultTrends05.push_back(h05);

    TH1* h01 = static_cast<TH1F*>(h1->Clone(("multTrend01" + p.name).c_str()));
    h01->SetDirectory(0);
    h1MultTrends01.push_back(h01);
  }

  for (int i{0}; i < nBinMult; i++) {
    AxisToCut axisToCutMult{0, i + 1, i + 1};

    double totalTriggerSignalPerMult{0.0};

    for (int j{0}; j < nBinPtPhi; j++) {
      AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

      std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
      TH1* h1PhiData = static_cast<TH1D*>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1));
      h1PhiData->SetDirectory(0);

      TF1* fitVoigtBkgSourav = new TF1("fitVoigtBkgSourav", VoigtBkgSourav, 0.995, 1.06, 7);
      // fitVoigtBkgSourav->SetParameter(0, 2);
      fitVoigtBkgSourav->SetParameter(1, 1.019);
      fitVoigtBkgSourav->SetParameter(2, 0.001);
      fitVoigtBkgSourav->FixParameter(3, 0.00426);
      fitVoigtBkgSourav->SetNpx(400);
      fitVoigtBkgSourav->SetLineColor(kRed);

      /*TF1 *fitVoigtBkgMattia = new TF1("fitVoigtBkgMattia", VoigtBkgMattia, 0.995, 1.2, 9);
      fitVoigtBkgMattia->SetParameter(0, 10);
      fitVoigtBkgMattia->SetParameter(1, 1.019);
      fitVoigtBkgMattia->SetParameter(2, 0.001);
      fitVoigtBkgMattia->FixParameter(3, 0.00426);
      fitVoigtBkgMattia->SetNpx(400);
      fitVoigtBkgMattia->SetLineColor(kBlue);*/

      FitPhiSignalAndBkg<false> fitPhiSignalAndBkg{h1PhiData, fitVoigtBkgSourav, 4, phiMassSignalRange, phiMassSidebandRange};
      double triggerSignal = fitPhiSignalAndBkg.getSignal();
      double triggerBkgRatio = fitPhiSignalAndBkg.getBkgInSigRegion() / fitPhiSignalAndBkg.getBkgInSideRegion();

      double phiEff = applyEfficiency ? correctionCollection[0].h1Corrections[i]->GetBinContent(j + 1) : 1.0;
      totalTriggerSignalPerMult += triggerSignal / phiEff;

      filePhiDataOutput->cd();
      h1PhiData->Write();

      delete fitVoigtBkgSourav;

      auto processAssocParticle = [&](const AssocParticleConfig& config, const LoadedAssocData& data, int pIndex) {
        for (int k{0}; k < config.nBinPt; k++) {
          AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};

          std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

          TH1* h1Signal = projectTHnSparse<TH1>(data.h5DataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataSignal" + suffix);
          TH1* h1Sideband = projectTHnSparse<TH1>(data.h5DataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataSideband" + suffix);
          TH1* h1MESignal = projectTHnSparse<TH1>(data.h5DataMESignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataMESignal" + suffix);
          TH1* h1MESideband = projectTHnSparse<TH1>(data.h5DataMESideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataMESideband" + suffix);

          outputFiles[pIndex]->cd();
          h1Signal->Write();
          h1Sideband->Write();
          h1MESignal->Write();
          h1MESideband->Write();

          double assocEff = applyEfficiency ? correctionCollection[config.effIndex].h1Corrections[i]->GetBinContent(k + 1) : 1.0;
          double totalEff = phiEff * assocEff;

          // Correzione Signal
          h1Signal->Scale(1.0 / totalEff);
          auto [normMESignal, errnormMESignal] = IntegralAndErrorPair(h1MESignal, -0.1, 0.1) / 2;
          if (normMESignal > 0) {
            h1MESignal->Scale(1.0 / normMESignal);
          }
          if (applyME) {
            h1Signal->Divide(h1MESignal);
          }

          // Correzione Sideband
          auto [normMESideband, errnormMESideband] = IntegralAndErrorPair(h1MESideband, -0.1, 0.1) / 2;
          if (normMESideband > 0) {
            h1MESideband->Scale(1.0 / normMESideband);
          }
          if (applyME) {
            h1Sideband->Divide(h1MESideband);
          }
          h1Sideband->Scale(triggerBkgRatio / totalEff);
          h1Sideband->Write((std::string(h1Sideband->GetName()) + "_Scaled").c_str());

          h1Signal->Add(h1Sideband, -1);

          if (j == 0) {
            std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_pt" + config.name + "Bin" + std::to_string(k);
            h1PhiAssocNoPtPhi[pIndex][i][k] = static_cast<TH1*>(h1Signal->Clone(accumName.c_str()));
            h1PhiAssocNoPtPhi[pIndex][i][k]->SetDirectory(0);
          } else {
            h1PhiAssocNoPtPhi[pIndex][i][k]->Add(h1Signal);
          }
        }
      };

      for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
        processAssocParticle(assocParticles[pIdx], loadedDataCollection[pIdx], pIdx);
      }
    }

    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx) {
      auto& config = assocParticles[pIdx];

      std::string spectraName = "h1SpectrumPhi" + config.name + "_multBin" + std::to_string(i);
      TH1* h1Spectrum = constructSpectrum(h1PhiAssocNoPtPhi[pIdx][i], config.binning, spectraName, 1.0);
      h1Spectrum->Scale(1.0 / totalTriggerSignalPerMult);
      SetHistogramStyle(h1Spectrum, spectraColors[i]);

      spectraCanvases[pIdx]->cd();
      h1Spectrum->Draw(i == 0 ? "" : "SAME");

      outputFiles[pIdx]->cd();
      h1Spectrum->Write();

      constructMultTrend<false>(h1MultTrends1[pIdx], h1Spectrum, i);

      // /*
      TH1* h1Spectrum05 = constructSpectrum(h1PhiAssocNoPtPhi[pIdx][i], config.binning, spectraName, 0.5);
      h1Spectrum05->Scale(1.0 / totalTriggerSignalPerMult / 0.5);
      constructMultTrend<false>(h1MultTrends05[pIdx], h1Spectrum05, i);

      TH1* h1Spectrum01 = constructSpectrum(h1PhiAssocNoPtPhi[pIdx][i], config.binning, spectraName, 0.1);
      h1Spectrum01->Scale(1.0 / totalTriggerSignalPerMult / 0.1);
      constructMultTrend<false>(h1MultTrends01[pIdx], h1Spectrum01, i);
      // */
    }
  }

  filePhiDataOutput->Close();
  for (const auto& file : outputFiles) {
    file->Close();
  }

  TFile* fileOutputSpectra = new TFile("../DataFile/Output/PhiAssocSpectra.root", "RECREATE");
  fileOutputSpectra->cd();
  for (const auto& canvas : spectraCanvases) {
    canvas->Write();
  }
  fileOutputSpectra->Close();

  TH1* h1RatioMultTrend = static_cast<TH1*>(h1MultTrends1[0]->Clone("RatioMultTrend"));
  h1RatioMultTrend->SetDirectory(0);
  h1RatioMultTrend->Divide(h1MultTrends1[0], h1MultTrends1[1], 2.0, 1.0);

  // /*
  TH1* h1RatioMultTrend05 = static_cast<TH1*>(h1MultTrends05[0]->Clone("RatioMultTrend05"));
  h1RatioMultTrend05->SetDirectory(0);
  h1RatioMultTrend05->Divide(h1MultTrends05[0], h1MultTrends05[1], 2.0, 1.0);

  TH1* h1RatioMultTrend01 = static_cast<TH1*>(h1MultTrends01[0]->Clone("RatioMultTrend01"));
  h1RatioMultTrend01->SetDirectory(0);
  h1RatioMultTrend01->Divide(h1MultTrends01[0], h1MultTrends01[1], 2.0, 1.0);
  // */

  /*TCanvas *canvasK0SMultTrend = new TCanvas("canvasK0SMultTrend", "canvasK0SMultTrend", 800, 600);
  canvasK0SMultTrend->cd();
  SetHistogramStyle(h1MultTrends1[0], kRed);
  h1MultTrends1[0]->Draw();

  TCanvas *canvasPiMultTrend = new TCanvas("canvasPiMultTrend", "canvasPiMultTrend", 800, 600);
  canvasPiMultTrend->cd();
  SetHistogramStyle(h1MultTrends1[1], kRed);
  h1MultTrends1[1]->Draw();*/

  TCanvas* canvasRatioMultTrend = new TCanvas("canvasRatioMultTrend", "canvasRatioMultTrend", 800, 600);
  canvasRatioMultTrend->cd();
  SetHistogramStyle(h1RatioMultTrend, kRed);
  h1RatioMultTrend->Draw();
  // /*
  SetHistogramStyle(h1RatioMultTrend05, kBlue);
  h1RatioMultTrend05->Draw("SAME");
  SetHistogramStyle(h1RatioMultTrend01, kGreen + 2);
  h1RatioMultTrend01->Draw("SAME");
  // */
}

void AnalysisMC()
{
  std::string mcBasePath{"phi-strangeness-correlation/phiStrangenessCorrelation/"};

  std::vector<ParticleConfig<3>> particles = {
    {"Phi", {"phi/h3PhiMCGen", "phi/h4PhiMCGenAssocReco", "phi/h4PhiMCReco"}},
    {"K0S", {"k0s/h3K0SMCGen", "k0s/h4K0SMCGenAssocReco", "k0s/h4K0SMCReco"}},
    {"Pi", {"pi/h3PiMCGen", "pi/h4PiMCGenAssocReco", "pi/h4PiMCReco"}}};

  std::vector<LoadedMc> dataCollection;
  dataCollection.reserve(particles.size());

  TFile* fileMCInput = new TFile("../DataFile/MC/AnalysisResultsMC_136.root");

  auto loadMc = [&](const ParticleConfig<3>& config) -> LoadedMc {
    LoadedMc data;

    data.name = config.name;
    data.h3MCGen = static_cast<TH3F*>(fileMCInput->Get((mcBasePath + config.titles[0]).c_str()));
    data.h3MCGen->SetDirectory(0);
    data.h4MCGenAssocReco = static_cast<THnSparseF*>(fileMCInput->Get((mcBasePath + config.titles[1]).c_str()));
    data.h4MCReco = static_cast<THnSparseF*>(fileMCInput->Get((mcBasePath + config.titles[2]).c_str()));

    return data;
  };

  for (const auto& p : particles) {
    dataCollection.push_back(loadMc(p));
  }

  fileMCInput->Close();

  AxisToCut axisToCutZVtx{0, 1, nBinZVtx};
  AxisToCut axisToCutY{3, 1, nBinY};

  TFile* fileMCOutput = new TFile("../DataFile/Corrections/Corrections.root", "RECREATE");

  auto processData = [&](const LoadedMc& data) {
    if (!data.h3MCGen || !data.h4MCGenAssocReco || !data.h4MCReco) {
      std::cerr << "Missing histograms for " << data.name << ", skipping." << std::endl;
      return;
    }

    data.h3MCGen->SetName(Form("h3%sMCGen", data.name.c_str()));
    data.h3MCGen->GetZaxis()->SetRange(1, nBinY);
    data.h3MCGen->Sumw2();

    TH3* h3MCGenAssocReco = projectTHnSparse<TH3>(data.h4MCGenAssocReco, {axisToCutZVtx}, {1, 2, 3}, Form("h3%sMCGenAssocReco", data.name.c_str()));
    TH3* h3MCReco = projectTHnSparse<TH3>(data.h4MCReco, {axisToCutZVtx}, {1, 2, 3}, Form("h3%sMCReco", data.name.c_str()));

    TH3* h3Efficiency = static_cast<TH3*>(h3MCReco->Clone(Form("h3%s`Efficiency", data.name.c_str())));
    h3Efficiency->SetDirectory(0);
    h3Efficiency->Divide(h3MCReco, h3MCGenAssocReco, 1.0, 1.0, "B");

    TH3* h3SignalLoss = static_cast<TH3*>(h3MCGenAssocReco->Clone(Form("h3%s`SignalLoss", data.name.c_str())));
    h3SignalLoss->SetDirectory(0);
    h3SignalLoss->Divide(h3MCGenAssocReco, data.h3MCGen, 1.0, 1.0, "B");

    h3Efficiency->Multiply(h3SignalLoss);

    TFile* fileMCOutputPerPart = new TFile(Form("../DataFile/Corrections/h3EffMap%s.root", data.name.c_str()), "RECREATE");
    fileMCOutputPerPart->cd();
    h3Efficiency->Write();
    fileMCOutputPerPart->Close();

    TCanvas* canvasEfficiency = new TCanvas(Form("h3%s`Efficiency", data.name.c_str()), Form("h3%s`Efficiency", data.name.c_str()), 800, 600);
    TCanvas* canvasSignalLoss = new TCanvas(Form("h3%s`SignalLoss", data.name.c_str()), Form("h3%s`SignalLoss", data.name.c_str()), 800, 600);

    for (int i{0}; i < nBinMult; i++) {
      TH1* h1MCGen = data.h3MCGen->ProjectionY(Form("h1%sMCGen_multBin%d", data.name.c_str(), i), i + 1, i + 1, 1, nBinY);
      h1MCGen->SetDirectory(0);

      AxisToCut axisToCutMult{1, i + 1, i + 1};

      TH1* h1MCGenAssocReco = projectTHnSparse<TH1>(data.h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, Form("h1%sMCGenAssocReco_multBin%d", data.name.c_str(), i));
      TH1* h1MCReco = projectTHnSparse<TH1>(data.h4MCReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, Form("h1%sMCReco_multBin%d", data.name.c_str(), i));

      TH1* h1Efficiency = static_cast<TH1*>(h1MCReco->Clone(Form("h1%sEfficiency_multBin%d", data.name.c_str(), i)));
      h1Efficiency->SetDirectory(0);
      h1Efficiency->Divide(h1MCReco, h1MCGenAssocReco, 1.0, 1.0, "B");
      SetHistogramStyle(h1Efficiency, spectraColors[i]);
      canvasEfficiency->cd();
      h1Efficiency->Draw(i == 0 ? "" : "SAME");

      TH1* h1SignalLoss = static_cast<TH1*>(h1MCGenAssocReco->Clone(Form("h1%sSigLoss_multBin%d", data.name.c_str(), i)));
      h1SignalLoss->SetDirectory(0);
      h1SignalLoss->Divide(h1MCGenAssocReco, h1MCGen, 1.0, 1.0, "B");
      SetHistogramStyle(h1SignalLoss, spectraColors[i]);
      canvasSignalLoss->cd();
      h1SignalLoss->Draw(i == 0 ? "" : "SAME");

      fileMCOutput->cd();
      h1Efficiency->Write();
      h1SignalLoss->Write();
    }
  };

  for (const auto& data : dataCollection) {
    processData(data);
  }

  fileMCOutput->Close();
}

struct ParticleTask {
  std::string name;            // Es: "K0S", "Pi"
  TH3F* h3Source;              // Pointer to the 3D source histogram
  int nBinPt;                  // Numero bin Pt (nBinPtK0S o nBinPtPi)
  std::vector<double> binning; // Binning per lo spettro finale
  TFile* outputFile;           // Output file for this particle
  TCanvas* canvas;             // Canvas for plotting
};

void AnalysisPurity()
{
  // gROOT->SetBatch(kTRUE);

  TFile* fileInput = new TFile("../DataFile/Data/AnalysisResults_136_test.root");

  std::string k0sPath{"k0s-reduced-cand-producer/k0sReducedCandidates/h3K0sCandidatesMass"};
  std::string piTPCPath{"pion-track-producer/pionTracks/h3PionTPCnSigma"};
  std::string piTOFPath{"pion-track-producer/pionTracks/h3PionTOFnSigma"};

  TH3F* h3K0SData = static_cast<TH3F*>(fileInput->Get(k0sPath.c_str()));
  h3K0SData->SetDirectory(0);
  TH3F* h3PiTPCData = static_cast<TH3F*>(fileInput->Get(piTPCPath.c_str()));
  h3PiTPCData->SetDirectory(0);
  TH3F* h3PiTOFData = static_cast<TH3F*>(fileInput->Get(piTOFPath.c_str()));
  h3PiTOFData->SetDirectory(0);

  fileInput->Close();

  TFile* fileOutputK0S = new TFile("../DataFile/Purities/K0SPurity.root", "RECREATE");
  TFile* fileOutputPi = new TFile("../DataFile/Purities/PiPurity.root", "RECREATE");

  TCanvas* canvasPurityK0S = new TCanvas("canvasK0SPurity", "canvasK0SPurity", 800, 600);
  TCanvas* canvasPurityPiTPC = new TCanvas("canvasPiTPCPurity", "canvasPiTPCPurity", 800, 600);
  TCanvas* canvasPurityPiTOF = new TCanvas("canvasPiTOFPurity", "canvasPiTOFPurity", 800, 600);

  std::vector<ParticleTask> particleTasks = {
    {"k0s", h3K0SData, nBinPtK0S, binspTK0S, fileOutputK0S, canvasPurityK0S},
    {"pi_tpc", h3PiTPCData, nBinPtPi, binspTPi, fileOutputPi, canvasPurityPiTPC},
    {"pi_tof", h3PiTOFData, nBinPtPi, binspTPi, fileOutputPi, canvasPurityPiTOF}};

  FitConfigManager fitConfigManager("fitConfig.json");

  for (int i{0}; i < nBinMult; i++) {
    auto processParticle = [&](ParticleTask& task) {
      std::string hName = "h1" + task.name + "Purity_multBin" + std::to_string(i);
      TH1* h1PuritySpectrum = new TH1F(hName.c_str(), hName.c_str(), task.nBinPt, task.binning.data());
      h1PuritySpectrum->SetDirectory(0);

      for (int k{0}; k < task.nBinPt; k++) {
        std::string histName = "h1" + task.name + "_multBin" + std::to_string(i) + "_ptBin" + std::to_string(k);
        TH1* h1Data = static_cast<TH1D*>(task.h3Source->ProjectionZ(histName.c_str(), i + 1, i + 1, k + 1, k + 1));
        h1Data->SetDirectory(0);

        // Run Fitter using JSON-based configuration
        FitConfig cfg = fitConfigManager.GetConfig(task.name, i, k);
        FitAssocSignalAndBkg fitter(h1Data, cfg);

        fitter.DoFit();

        auto res = fitter.ExtractYieldsAndPurity(3.0, true);
        h1PuritySpectrum->SetBinContent(k + 1, res.purity.first);
        h1PuritySpectrum->SetBinError(k + 1, res.purity.second);

        // Save diagnostic plot
        std::string cName = "cFit_" + task.name + "_m" + std::to_string(i) + "_p" + std::to_string(k);
        fitter.SaveFitCanvas(task.outputFile, cName, 3.0);

        delete h1Data;
      }

      // Save the finished spectrum for this multiplicity
      SetHistogramStyle(h1PuritySpectrum, spectraColors[i]);
      task.canvas->cd();
      h1PuritySpectrum->DrawCopy(i == 0 ? "" : "SAME");
      delete h1PuritySpectrum;
    };

    for (auto& task : particleTasks) {
      processParticle(task);
    }
  }

  // Save the summary canvases to their respective files
  fileOutputK0S->cd();
  canvasPurityK0S->Write();

  fileOutputPi->cd();
  canvasPurityPiTPC->Write();
  canvasPurityPiTOF->Write();

  // Final Cleanup
  fileOutputK0S->Close();
  fileOutputPi->Close();

  /*std::array<TH1 *, nBinMult> h1K0SPurityMult;
  std::array<TH1 *, nBinMult> h1PiTPCPurityMult;
  std::array<TH1 *, nBinMult> h1PiTOFPurityMult;

  for (int i{0}; i < nBinMult; i++)
  {
      h1K0SPurityMult[i] = new TH1F(Form("h1K0SPurity_multBin%d", i), Form("h1K0SPurity_multBin%d", i), nBinPtK0S, binspTK0S.data());

      for (int k{0}; k < nBinPtK0S; k++)
      {
          std::string histName = "h1K0SPurity_multBin" + std::to_string(binsMult[i]) + "-" + std::to_string(binsMult[i + 1]) +
                                 "_ptBin" + std::to_string(binspTK0S[k]) + "-" + std::to_string(binspTK0S[k + 1]);
          TH1 *h1K0SData = static_cast<TH1D *>(h3K0SData->ProjectionZ(histName.c_str(), i + 1, i + 1, k + 1, k + 1));
          h1K0SData->SetDirectory(0);

          FitAssocSignalAndBkg<kK0S> fitK0SSignalAndBkg{h1K0SData, {i, k}, fileOutputK0S};
          fitK0SSignalAndBkg.constructPuritySpectrum(h1K0SPurityMult[i], k + 1);
      }

      SetHistogramStyle(h1K0SPurityMult[i], spectraColors[i]);
      canvasPurityK0S->cd();
      h1K0SPurityMult[i]->Draw(i == 0 ? "" : "SAME");

      h1PiTPCPurityMult[i] = new TH1F(Form("h1PiTPCPurity_multBin%d", i), Form("h1PiTPCPurity_multBin%d", i), nBinPtPi, binspTPi.data());
      h1PiTOFPurityMult[i] = new TH1F(Form("h1PiTOFPurity_multBin%d", i), Form("h1PiTOFPurity_multBin%d", i), nBinPtPi, binspTPi.data());

      for (int k{0}; k < nBinPtPi; k++)
      {
          std::string histNameTPC = "h1PiTPCPurity_multBin" + std::to_string(binsMult[i]) + "-" + std::to_string(binsMult[i + 1]) +
                                    "_ptBin" + std::to_string(binspTPi[k]) + "-" + std::to_string(binspTPi[k + 1]);
          TH1 *h1PiTPCData = static_cast<TH1D *>(h3PiTPCData->ProjectionZ(histNameTPC.c_str(), i + 1, i + 1, k + 1, k + 1));
          h1PiTPCData->SetDirectory(0);

          FitAssocSignalAndBkg<kPi> fitPiTPCSignalAndBkg{h1PiTPCData, {i, k}, fileOutputPi};
          fitPiTPCSignalAndBkg.constructPuritySpectrum(h1PiTPCPurityMult[i], k + 1);

          std::string histNameTOF = "h1PiTOFPurity_multBin" + std::to_string(binsMult[i]) + "-" + std::to_string(binsMult[i + 1]) +
                                    "_ptBin" + std::to_string(binspTPi[k]) + "-" + std::to_string(binspTPi[k + 1]);
          TH1 *h1PiTOFData = static_cast<TH1D *>(h3PiTOFData->ProjectionZ(histNameTOF.c_str(), i + 1, i + 1, k + 1, k + 1));
          h1PiTOFData->SetDirectory(0);

          FitAssocSignalAndBkg<kPi> fitPiTOFSignalAndBkg{h1PiTOFData, {i, k}, fileOutputPi};
          fitPiTOFSignalAndBkg.constructPuritySpectrum(h1PiTOFPurityMult[i], k + 1);
      }

      SetHistogramStyle(h1PiTPCPurityMult[i], spectraColors[i]);
      canvasPurityPiTPC->cd();
      h1PiTPCPurityMult[i]->Draw(i == 0 ? "" : "SAME");

      SetHistogramStyle(h1PiTOFPurityMult[i], spectraColors[i]);
      canvasPurityPiTOF->cd();
      h1PiTOFPurityMult[i]->Draw(i == 0 ? "" : "SAME");
  }

  fileOutputK0S->cd();
  canvasPurityK0S->Write();
  // fileOutputK0S->Close();

  fileOutputPi->cd();
  canvasPurityPiTPC->Write();
  canvasPurityPiTOF->Write();
  // fileOutputPi->Close();

  // delete canvasPurityK0S;
  delete canvasPurityPiTPC;
  delete canvasPurityPiTOF;*/
}

void PhiStrangeCorr_polished(int mode = 0)
{
  switch (mode) {
    case 0:
      AnalysisData(true, true);
      break;
    case 1:
      AnalysisMC();
      break;
    case 2:
      AnalysisPurity();
      break;
    default:
      throw std::runtime_error("Invalid mode selected!");
  }
}

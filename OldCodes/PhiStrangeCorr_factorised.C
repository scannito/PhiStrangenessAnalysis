#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TMath.h"
#include "TFitResult.h"
#include "TMatrixDSym.h"
#include "TStyle.h"

#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <cmath>

enum PartType
{
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

struct AxisToCut
{
    int axis;
    int binLow;
    int binUp;
};

template <size_t size>
struct ParticleConfig
{
    std::string name;                     // Es. "Phi", "K0S", "Pi"
    std::array<std::string, size> titles; // Es. {Gen, GenAssoc, Reco} o {Efficiency, SignalLoss}
};

struct AssocParticleConfig
{
    std::string name;            // Es: "K0S", "Pi"
    std::string dirName;         // Es: "phiK0S", "phiPi"
    int nBinPt;                  // Numero bin Pt (nBinPtK0S o nBinPtPi)
    int effIndex;                // Indice nella collezione efficienze (1 per K0S, 2 per Pi)
    std::vector<double> binning; // Binning per lo spettro finale
};

struct LoadedAssocData
{
    THnSparseF *h5DataSignal{nullptr};
    THnSparseF *h5DataSideband{nullptr};
    THnSparseF *h5DataMESignal{nullptr};
    THnSparseF *h5DataMESideband{nullptr};
};

template <size_t size>
struct LoadedCorrections
{
    std::string name;
    std::array<TH1F *, size> h1Corrections;
};

struct LoadedMc
{
    std::string name;
    TH3F *h3MCGen{nullptr};
    THnSparseF *h4MCGenAssocReco{nullptr};
    THnSparseF *h4MCReco{nullptr};
};

std::pair<double, double> operator/(const std::pair<double, double> &pair, double divide)
{
    return {pair.first / divide, pair.second / divide};
}

template <typename THType>
THType *projectTHnSparse(THnSparse *hnSparse,
                         const std::vector<AxisToCut> &axesToBeCut,
                         const std::vector<int> &axesToProject,
                         const std::string &histName)
{
    if (!hnSparse)
        return nullptr;

    for (const auto &axisToCut : axesToBeCut)
    {
        hnSparse->GetAxis(axisToCut.axis)->SetRange(axisToCut.binLow, axisToCut.binUp);
    }

    THType *hProjection = nullptr;

    if constexpr (std::is_base_of_v<TH3, THType>)
    {
        hProjection = hnSparse->Projection(axesToProject[0], axesToProject[1], axesToProject[2]);
    }
    else if constexpr (std::is_base_of_v<TH2, THType>)
    {
        hProjection = hnSparse->Projection(axesToProject[0], axesToProject[1]);
    }
    else if constexpr (std::is_base_of_v<TH1, THType>)
    {
        hProjection = hnSparse->Projection(axesToProject[0]);
    }

    hProjection->SetName(histName.c_str());
    hProjection->SetDirectory(0);
    hProjection->Sumw2();

    return hProjection;
}

std::pair<double, double> IntegralAndErrorPair(TH1 *h1, double x1, double x2)
{
    double integral{0.0};
    double error{0.0};
    double epsilon{0.00001};

    integral = h1->IntegralAndError(h1->GetXaxis()->FindBin(x1 + epsilon), h1->GetXaxis()->FindBin(x2 - epsilon), error);

    return std::make_pair(integral, error);
}

double Voigt(double *x, double *par)
{
    double mass = x[0];

    return par[0] * TMath::Voigt(mass - par[1], par[2], par[3]);
}

double BkgSourav(double *x, double *par)
{
    double mass = x[0];

    return par[0] + par[1] * mass + par[2] * std::sqrt(mass - 2 * kaonMass);
}

double BkgMattia(double *x, double *par)
{
    double mass = x[0];

    return par[0] * std::pow(mass - 2 * kaonMass, par[1]) * std::exp(par[2] * (mass - 2 * kaonMass) + par[3] * std::pow(mass - 2 * kaonMass, 2) + par[4] * std::pow(mass - 2 * kaonMass, 3));
}

double VoigtBkgSourav(double *x, double *par)
{
    return Voigt(x, &par[0]) + BkgSourav(x, &par[4]);
}

double VoigtBkgMattia(double *x, double *par)
{
    return Voigt(x, &par[0]) + BkgMattia(x, &par[4]);
}

template <bool wSidebandFit>
class FitSignalAndBkg
{
public:
    FitSignalAndBkg(TH1 *h,
                    TF1 *fitFunc,
                    int indexFirstBkgParam,
                    std::pair<double, double> signalRegion,
                    std::pair<double, double> sidebandRegion)
        : h1(h), fitFunction(fitFunc)
    {
        double binWidth = h1->GetXaxis()->GetBinWidth(1);

        TFitResultPtr fitResult = h1->Fit(fitFunction, "RSN");
        TMatrixDSym covMatrix = fitResult->GetCovarianceMatrix();

        h1->GetListOfFunctions()->Add(fitFunction->Clone());

        TF1 *signalFunction = new TF1("Voigt", Voigt, signalRegion.first, signalRegion.second, 4);
        signalFunction->SetLineColor(kBlue);
        TF1 *bkgFunction = new TF1("Bkg", BkgSourav, signalRegion.first, signalRegion.second, 3);
        bkgFunction->SetLineColor(kGreen + 2);

        TMatrixDSym covSignal(indexFirstBkgParam);
        TMatrixDSym covBkg(fitFunction->GetNpar() - indexFirstBkgParam);

        for (int i = 0; i < indexFirstBkgParam; i++)
        {
            signalFunction->SetParameter(i, fitFunction->GetParameter(i));
            for (int j = 0; j < indexFirstBkgParam; j++)
            {
                covSignal(i, j) = covMatrix(i, j);
            }
        }

        for (int i = indexFirstBkgParam; i < fitFunction->GetNpar(); i++)
        {
            bkgFunction->SetParameter(i - indexFirstBkgParam, fitFunction->GetParameter(i));
            for (int j = indexFirstBkgParam; j < fitFunction->GetNpar(); j++)
            {
                covBkg(i - indexFirstBkgParam, j - indexFirstBkgParam) = covMatrix(i, j);
            }
        }

        h1->GetListOfFunctions()->Add(signalFunction->Clone());
        h1->GetListOfFunctions()->Add(bkgFunction->Clone());

        signalIntegralAndError.first = signalFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
        signalIntegralAndError.second = signalFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covSignal.GetMatrixArray()) / binWidth;

        bkgIntegralAndErrorInSigRegion.first = bkgFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
        bkgIntegralAndErrorInSigRegion.second = bkgFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covBkg.GetMatrixArray()) / binWidth;

        if constexpr (wSidebandFit)
        {
            bkgIntegralAndErrorInSideRegion.first = bkgFunction->Integral(sidebandRegion.first, sidebandRegion.second) / binWidth;
            bkgIntegralAndErrorInSideRegion.second = bkgFunction->IntegralError(sidebandRegion.first, sidebandRegion.second, fitFunction->GetParameters(), covBkg.GetMatrixArray()) / binWidth;
        }
        else
        {
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
    TH1 *h1 = nullptr;
    TF1 *fitFunction = nullptr;

    std::pair<double, double> signalIntegralAndError{0.0, 0.0};
    std::pair<double, double> bkgIntegralAndErrorInSigRegion{0.0, 0.0};
    std::pair<double, double> bkgIntegralAndErrorInSideRegion{0.0, 0.0};
};

template <typename Container>
TH1 *constructSpectrum(const Container &hContainer,
                       const std::vector<double> &binsVec,
                       const std::string &histName,
                       double absLimToIntegrate)
{
    if (binsVec.size() - 1 != hContainer.size())
    {
        throw std::runtime_error("Size of histogram container must be equal to number of bins - 1");
    }

    TH1 *hSpectrum = new TH1D(histName.c_str(), histName.c_str(), binsVec.size() - 1, binsVec.data());

    for (size_t i{0}; i < hContainer.size(); i++)
    {
        auto [binContent, binError] = IntegralAndErrorPair(hContainer[i], -absLimToIntegrate, absLimToIntegrate);
        double normalizationFactor = (binsVec[i + 1] - binsVec[i]);

        hSpectrum->SetBinContent(i + 1, binContent / normalizationFactor);
        hSpectrum->SetBinError(i + 1, binError / normalizationFactor);
    }

    hSpectrum->SetDirectory(0);

    return hSpectrum;
}

template <bool wExtrapolation>
void constructMultTrend(TH1 *hMultTrend,
                        TH1 *hPtSpectrum,
                        int i)
{
    double content{0.0};
    for (int k{1}; k <= hPtSpectrum->GetNbinsX(); k++)
    {
        content += hPtSpectrum->GetBinContent(k) * hPtSpectrum->GetBinWidth(k);
    }
    if constexpr (wExtrapolation)
    {
        content += 0.0;
    }

    hMultTrend->SetBinContent(i + 1, content);
    hMultTrend->SetBinError(i + 1, 0.01);
}

void SetHistogramStyle(TH1 *h1, int color)
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

/// @brief ////////
/// @param applyME
void ProcessRawData(bool applyME = false)
{
    std::cout << "--- STEP 1: Processing Raw Data & Saving Intermediate Results ---" << std::endl;

    std::string basePath = "phi-strangeness-correlation/phiStrangenessCorrelation/";

    std::vector<AssocParticleConfig> assocParticles = {
        {"K0S", "phiK0S", nBinPtK0S, 1, binspTK0S}, // effIndex 1 perché 0 è Phi
        {"Pi", "phiPi", nBinPtPi, 2, binspTPi}};

    std::vector<LoadedAssocData> loadedDataCollection;
    loadedDataCollection.reserve(assocParticles.size());

    TFile *fileDataInput = new TFile("../DataFile/Data/AnalysisResults_136.root");
    TFile *fileDataMEInput = new TFile("../DataFile/DataME/AnalysisResultsME_136.root");

    TH3F *h3PhiData = static_cast<TH3F *>(fileDataInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phi/h3PhiData"));
    h3PhiData->SetDirectory(0);

    auto loadAssocData = [&](const AssocParticleConfig &config) -> LoadedAssocData
    {
        LoadedAssocData data;

        std::string base = basePath + config.dirName + "/h5Phi" + config.name;

        data.h5DataSignal = static_cast<THnSparseF *>(fileDataInput->Get((base + "DataSignal").c_str()));
        data.h5DataSideband = static_cast<THnSparseF *>(fileDataInput->Get((base + "DataSideband").c_str()));

        data.h5DataMESignal = static_cast<THnSparseF *>(fileDataMEInput->Get((base + "DataMESignal").c_str()));
        data.h5DataMESideband = static_cast<THnSparseF *>(fileDataMEInput->Get((base + "DataMESideband").c_str()));

        return data;
    };

    for (const auto &p : assocParticles)
    {
        loadedDataCollection.push_back(loadAssocData(p));
    }

    fileDataInput->Close();
    fileDataMEInput->Close();

    TFile *fileInter = new TFile("../DataFile/Output/IntermediateRaw.root", "RECREATE");

    TH2 *h2TriggerRaw = new TH2D("h2TriggerRaw", "Raw Trigger Counts;Mult Bin;PtPhi Bin", nBinMult, 0, nBinMult, nBinPtPhi, 0, nBinPtPhi);

    for (int i{0}; i < nBinMult; i++)
    {
        AxisToCut axisToCutMult{0, i + 1, i + 1};

        for (int j{0}; j < nBinPtPhi; j++)
        {
            AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

            std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
            TH1 *h1PhiData = static_cast<TH1D *>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1));
            h1PhiData->SetDirectory(0);

            TF1 *fitVoigtBkgSourav = new TF1("fitVoigtBkgSourav", VoigtBkgSourav, 0.995, 1.06, 7);
            // fitVoigtBkgSourav->SetParameter(0, 2);
            fitVoigtBkgSourav->SetParameter(1, 1.019);
            fitVoigtBkgSourav->SetParameter(2, 0.001);
            fitVoigtBkgSourav->FixParameter(3, 0.00426);
            fitVoigtBkgSourav->SetNpx(400);
            fitVoigtBkgSourav->SetLineColor(kRed);

            FitSignalAndBkg<false> fitSignalAndBkg{h1PhiData, fitVoigtBkgSourav, 4, phiMassSignalRange, phiMassSidebandRange};
            double triggerSignal = fitSignalAndBkg.getSignal();
            double triggerBkgRatio = fitSignalAndBkg.getBkgInSigRegion() / fitSignalAndBkg.getBkgInSideRegion();

            h2TriggerRaw->SetBinContent(i + 1, j + 1, triggerSignal);

            delete fitVoigtBkgSourav;
            delete h1PhiData;

            auto processRawYields = [&](const AssocParticleConfig &config, const LoadedAssocData &data)
            {
                // Setup directory nel file intermedio
                std::string dirPath = Form("%s/Mult_%d/PtPhi_%d", config.name.c_str(), i, j);
                if (!fileInter->GetDirectory(dirPath.c_str()))
                    fileInter->mkdir(dirPath.c_str());
                fileInter->cd(dirPath.c_str());

                for (int k{0}; k < config.nBinPt; k++)
                {
                    AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};

                    // Proiezioni
                    TH1 *h1Sig = projectTHnSparse<TH1>(data.h5DataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "tmpSig");
                    TH1 *h1Sb = projectTHnSparse<TH1>(data.h5DataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "tmpSb");

                    // Mixed Event Signal Correction
                    TH1 *h1MESig = projectTHnSparse<TH1>(data.h5DataMESignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "tmpMESig");
                    auto [normMESig, _] = IntegralAndErrorPair(h1MESig, -0.1, 0.1);
                    if (applyME && normMESig > 0)
                    {
                        h1MESig->Scale(1.0 / (normMESig / 2.0));
                        h1Sig->Divide(h1MESig);
                    }

                    // Mixed Event Sideband Correction
                    TH1 *h1MESb = projectTHnSparse<TH1>(data.h5DataMESideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "tmpMESb");
                    auto [normMESb, __] = IntegralAndErrorPair(h1MESb, -0.1, 0.1);
                    if (applyME && normMESb > 0)
                    {
                        h1MESb->Scale(1.0 / (normMESb / 2.0));
                        h1Sb->Divide(h1MESb);
                    }

                    // Sottrazione Sideband (Scalata solo per bkgRatio, NO Efficienza qui)
                    h1Sb->Scale(triggerBkgRatio);

                    TH1 *h1RawYield = (TH1 *)h1Sig->Clone(Form("h1Raw_%s_m%d_p%d_a%d", config.name.c_str(), i, j, k));
                    h1RawYield->Add(h1Sb, -1);

                    h1RawYield->Write();

                    delete h1Sig;
                    delete h1Sb;
                    delete h1MESig;
                    delete h1MESb;
                    delete h1RawYield;
                }
            };

            for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx)
            {
                processRawYields(assocParticles[pIdx], loadedDataCollection[pIdx]);
            }
        }
    }

    fileInter->cd();
    h2TriggerRaw->Write();
    fileInter->Close();
}

/// @brief
/// @param applyEfficiency
void RunStep2_Analysis(bool applyEfficiency)
{
    std::cout << "--- STEP 2: Building Spectra with Lambdas ---" << std::endl;

    TFile *fileInter = new TFile("../DataFile/Output/IntermediateRaw.root", "READ");
    if (!fileInter || fileInter->IsZombie())
        return;

    TH2D *h2TriggerRaw = (TH2D *)fileInter->Get("h2TriggerRaw");
    h2TriggerRaw->SetDirectory(0);

    // --- CARICAMENTO EFFICIENZE ---
    std::vector<LoadedCorrections<nBinMult>> effs(kNParticles);
    TFile *fileEffInput = nullptr;

    if (applyEfficiency)
    {
        fileEffInput = new TFile("../DataFile/Corrections/Efficiencies.root");
        // Lambda caricamento Eff
        auto loadEff = [&](PartType id, std::string name, std::string t1, std::string t2)
        {
            effs[id].id = id;
            effs[id].name = name;
            for (int i{0}; i < nBinMult; i++)
            {
                TH1F *h1 = (TH1F *)fileEffInput->Get((t1 + "_multBin" + std::to_string(i)).c_str());
                TH1F *h2 = (TH1F *)fileEffInput->Get((t2 + "_multBin" + std::to_string(i)).c_str());
                if (h1 && h2)
                {
                    effs[id].h1Corrections[i] = (TH1F *)h1->Clone();
                    effs[id].h1Corrections[i]->Multiply(h1, h2);
                    effs[id].h1Corrections[i]->SetDirectory(0);
                }
                else
                    effs[id].h1Corrections[i] = nullptr;
            }
        };
        loadEff(kPhi, "Phi", "h1PhiEfficiency", "h1PhiSignalLoss");
        loadEff(kK0S, "K0S", "h1K0SEfficiency", "h1K0SSignalLoss");
        loadEff(kPi, "Pi", "h1PiEfficiency", "h1PiSignalLoss");
        fileEffInput->Close();
        delete fileEffInput;
    }

    std::vector<AssocParticleConfig> assocParticles{
        {kK0S, "K0S", "phiK0S", nBinPtK0S, binspTK0S},
        {kPi, "Pi", "phiPi", nBinPtPi, binspTPi}};

    // Output Files
    std::vector<TFile *> outFiles;
    std::vector<TCanvas *> canvases;
    std::vector<TH1 *> h1MultTrends;
    for (auto &p : assocParticles)
    {
        outFiles.push_back(new TFile(("../DataFile/Output/Final" + p.name + ".root").c_str(), "RECREATE"));
        canvases.push_back(new TCanvas(p.name.c_str(), p.name.c_str()));
        h1MultTrends.push_back(new TH1F(("multTrend" + p.name).c_str(), "Trend", nBinMult, binsMult.data()));
    }

    // --- LOOP ANALISI ---
    for (int i = 0; i < nBinMult; ++i)
    {
        // Calcolo Trigger Corretto (Somma su j)
        double N_trig_corrected = 0.0;
        for (int j = 0; j < nBinPtPhi; ++j)
        {
            double raw = h2TriggerRaw->GetBinContent(i + 1, j + 1);
            double eff = (applyEfficiency && effs[kPhi].h1Corrections[i]) ? effs[kPhi].h1Corrections[i]->GetBinContent(j) : 1.0;
            if (eff > 0)
                N_trig_corrected += raw / eff;
        }

        // LAMBDA PROCESSAMENTO SPETTRI
        auto processSpectra = [&](const AssocParticleConfig &config, int pIdx)
        {
            std::vector<TH1 *> vecFinalSpectrum(config.nBinPt, nullptr);

            // Somma su tutti i bin PtPhi (j)
            for (int j = 0; j < nBinPtPhi; ++j)
            {
                double effPhi = (applyEfficiency && effs[kPhi].h1Corrections[i]) ? effs[kPhi].h1Corrections[i]->GetBinContent(j) : 1.0;
                if (effPhi <= 0)
                    continue;

                // CD nel file intermedio
                if (!fileInter->cd(Form("%s/Mult_%d/PtPhi_%d", config.name.c_str(), i, j)))
                    continue;

                // Loop su PtAssoc (k)
                for (int k = 0; k < config.nBinPt; ++k)
                {
                    TH1 *hRaw = (TH1 *)gDirectory->Get(Form("h1Raw_%s_m%d_p%d_a%d", config.name.c_str(), i, j, k));
                    if (!hRaw)
                        continue;

                    double effAssoc = (applyEfficiency && effs[config.id].h1Corrections[i]) ? effs[config.id].h1Corrections[i]->GetBinContent(k) : 1.0;
                    if (effAssoc <= 0)
                        continue;

                    // Applicazione Doppia Efficienza (Trigger * Assoc)
                    TH1 *hCorr = (TH1 *)hRaw->Clone();
                    hCorr->Scale(1.0 / (effPhi * effAssoc));

                    if (vecFinalSpectrum[k] == nullptr)
                    {
                        vecFinalSpectrum[k] = (TH1 *)hCorr->Clone();
                        vecFinalSpectrum[k]->SetDirectory(0);
                    }
                    else
                    {
                        vecFinalSpectrum[k]->Add(hCorr);
                    }
                    delete hCorr;
                }
            }

            // Costruzione e Disegno
            TH1 *hSpectra = constructSpectrum(vecFinalSpectrum, config.binning, Form("Spectra_%s_m%d", config.name.c_str(), i), 1.0);

            if (N_trig_corrected > 0)
                hSpectra->Scale(1.0 / N_trig_corrected);
            SetHistogramStyle(hSpectra, spectraColors[i]);

            canvases[pIdx]->cd();
            hSpectra->Draw(i == 0 ? "" : "SAME");
            outFiles[pIdx]->cd();
            hSpectra->Write();

            constructMultTrend<false>(h1MultTrends[pIdx], hSpectra);

            for (auto *h : vecFinalSpectrum)
                delete h;
        };

        // Esecuzione Lambda
        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx)
        {
            processSpectra(assocParticles[pIdx], pIdx);
        }
    }

    // Chiusura e Pulizia
    fileInter->Close();
    delete fileInter;
    delete h2TriggerRaw;

    TFile *fileOutputSpectra = new TFile("../DataFile/Output/PhiAssocSpectra.root", "RECREATE");
    for (auto c : canvases)
        c->Write();
    TH1 *h1Ratio = (TH1 *)h1MultTrends[0]->Clone("RatioMultTrend");
    h1Ratio->Divide(h1MultTrends[0], h1MultTrends[1]);
    h1Ratio->Write();
    fileOutputSpectra->Close();
    delete fileOutputSpectra;

    for (auto f : outFiles)
    {
        f->Close();
    }
}

/// @brief
void AnalysisMC()
{
    std::string mcBasePath{"phi-strangeness-correlation/phiStrangenessCorrelation/"};

    std::vector<ParticleConfig<3>> particles = {
        {"Phi", {"phi/h3PhiMCGen", "phi/h4PhiMCGenAssocReco", "phi/h4PhiMCReco"}},
        {"K0S", {"k0s/h3K0SMCGen", "k0s/h4K0SMCGenAssocReco", "k0s/h4K0SMCReco"}},
        {"Pi", {"pi/h3PiMCGen", "pi/h4PiMCGenAssocReco", "pi/h4PiMCReco"}}};

    std::vector<LoadedMc> dataCollection;
    dataCollection.reserve(particles.size());

    TFile *fileMCInput = new TFile("../DataFile/MC/AnalysisResultsMC_136.root");

    auto loadMc = [&](const ParticleConfig<3> &config) -> LoadedMc
    {
        LoadedMc data;

        data.name = config.name;
        data.h3MCGen = static_cast<TH3F *>(fileMCInput->Get((mcBasePath + config.titles[0]).c_str()));
        data.h3MCGen->SetDirectory(0);
        data.h4MCGenAssocReco = static_cast<THnSparseF *>(fileMCInput->Get((mcBasePath + config.titles[1]).c_str()));
        data.h4MCReco = static_cast<THnSparseF *>(fileMCInput->Get((mcBasePath + config.titles[2]).c_str()));

        return data;
    };

    for (const auto &p : particles)
    {
        dataCollection.push_back(loadMc(p));
    }

    fileMCInput->Close();

    AxisToCut axisToCutZVtx{0, 1, nBinZVtx};
    AxisToCut axisToCutY{3, 1, nBinY};

    TFile *fileMCOutput = new TFile("../DataFile/Corrections/Corrections.root", "RECREATE");

    auto processData = [&](const LoadedMc &data)
    {
        if (!data.h3MCGen || !data.h4MCGenAssocReco || !data.h4MCReco)
        {
            std::cerr << "Missing histograms for " << data.name << ", skipping." << std::endl;
            return;
        }

        data.h3MCGen->SetName(Form("h3%sMCGen", data.name.c_str()));
        data.h3MCGen->GetZaxis()->SetRange(1, nBinY);
        data.h3MCGen->Sumw2();

        TH3 *h3MCGenAssocReco = projectTHnSparse<TH3>(data.h4MCGenAssocReco, {axisToCutZVtx}, {1, 2, 3}, Form("h3%sMCGenAssocReco", data.name.c_str()));
        TH3 *h3MCReco = projectTHnSparse<TH3>(data.h4MCReco, {axisToCutZVtx}, {1, 2, 3}, Form("h3%sMCReco", data.name.c_str()));

        TH3 *h3Efficiency = static_cast<TH3 *>(h3MCReco->Clone(Form("h3%s`Efficiency", data.name.c_str())));
        h3Efficiency->SetDirectory(0);
        h3Efficiency->Divide(h3MCReco, h3MCGenAssocReco, 1.0, 1.0, "B");

        TH3 *h3SignalLoss = static_cast<TH3 *>(h3MCGenAssocReco->Clone(Form("h3%s`SignalLoss", data.name.c_str())));
        h3SignalLoss->SetDirectory(0);
        h3SignalLoss->Divide(h3MCGenAssocReco, data.h3MCGen, 1.0, 1.0, "B");

        h3Efficiency->Multiply(h3SignalLoss);

        TFile *fileMCOutputPerPart = new TFile(Form("../DataFile/Corrections/h3EffMap%s.root", data.name.c_str()), "RECREATE");
        fileMCOutputPerPart->cd();
        h3Efficiency->Write();
        fileMCOutputPerPart->Close();

        TCanvas *canvasEfficiency = new TCanvas(Form("h3%s`Efficiency", data.name.c_str()), Form("h3%s`Efficiency", data.name.c_str()), 800, 600);
        TCanvas *canvasSignalLoss = new TCanvas(Form("h3%s`SignalLoss", data.name.c_str()), Form("h3%s`SignalLoss", data.name.c_str()), 800, 600);

        for (int i{0}; i < nBinMult; i++)
        {
            TH1 *h1MCGen = data.h3MCGen->ProjectionY(Form("h1%sMCGen_multBin%d", data.name.c_str(), i), i + 1, i + 1, 1, nBinY);
            h1MCGen->SetDirectory(0);

            AxisToCut axisToCutMult{1, i + 1, i + 1};

            TH1 *h1MCGenAssocReco = projectTHnSparse<TH1>(data.h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, Form("h1%sMCGenAssocReco_multBin%d", data.name.c_str(), i));
            TH1 *h1MCReco = projectTHnSparse<TH1>(data.h4MCReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, Form("h1%sMCReco_multBin%d", data.name.c_str(), i));

            TH1 *h1Efficiency = static_cast<TH1 *>(h1MCReco->Clone(Form("h1%sEfficiency_multBin%d", data.name.c_str(), i)));
            h1Efficiency->SetDirectory(0);
            h1Efficiency->Divide(h1MCReco, h1MCGenAssocReco, 1.0, 1.0, "B");
            SetHistogramStyle(h1Efficiency, spectraColors[i]);
            canvasEfficiency->cd();
            h1Efficiency->Draw(i == 0 ? "" : "SAME");

            TH1 *h1SignalLoss = static_cast<TH1 *>(h1MCGenAssocReco->Clone(Form("h1%sSigLoss_multBin%d", data.name.c_str(), i)));
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

    for (const auto &data : dataCollection)
    {
        processData(data);
    }

    fileMCOutput->Close();
}

void PhiStrangeCorr_factorised(int mode = 0)
{
    switch (mode)
    {
    case 0:
        ProcessRawData(true);
        break;
    case 1:
        AnalysisMC();
        break;
    default:
        throw std::runtime_error("Invalid mode selected!");
    }
}
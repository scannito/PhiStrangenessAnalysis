#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "THnSparse.h"

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

template <size_t N>
struct ParticleConfig
{
    std::string name;                  // es. "Phi", "K0S", "Pi"
    std::array<std::string, N> titles; // es. {Gen, GenAssoc, Reco} o {Efficiency, SignalLoss}
};

// Strutture per tenere in memoria gli istogrammi dopo aver chiuso il file
template <size_t N>
struct LoadedCorrections
{
    std::string name;
    std::array<TH1F *, N> h1Corrections;
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

std::pair<double, double> IntegralAndErrorPair(TH1 *h1, double x1, double x2)
{
    double integral{0.0};
    double error{0.0};
    double epsilon{0.00001};

    integral = h1->IntegralAndError(h1->GetXaxis()->FindBin(x1 + epsilon), h1->GetXaxis()->FindBin(x2 - epsilon), error);

    return std::make_pair(integral, error);
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

        /*
        TF1 *signalFunction = static_cast<TF1 *>(fitFunction->Clone("signalFunction"));
        TF1 *bkgFunction = static_cast<TF1 *>(fitFunction->Clone("bkgFunction"));

        for (int i = 0; i < fitFunction->GetNpar(); i++)
        {
            if (i < indexFirstBkgParam)
            {
                bkgFunction->FixParameter(i, 0.0);
            }
            else
            {
                signalFunction->FixParameter(i, 0.0);
            }
        }


        signalIntegralAndError.first = signalFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
        signalIntegralAndError.second = signalFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covMatrix.GetMatrixArray()) / binWidth;

        bkgIntegralAndErrorInSigRegion.first = bkgFunction->Integral(signalRegion.first, signalRegion.second) / binWidth;
        bkgIntegralAndErrorInSigRegion.second = bkgFunction->IntegralError(signalRegion.first, signalRegion.second, fitFunction->GetParameters(), covMatrix.GetMatrixArray()) / binWidth;

        if constexpr (wSidebandFit)
        {
            bkgIntegralAndErrorInSideRegion.first = bkgFunction->Integral(sidebandRegion.first, sidebandRegion.second) / binWidth;
            bkgIntegralAndErrorInSideRegion.second = bkgFunction->IntegralError(sidebandRegion.first, sidebandRegion.second, fitFunction->GetParameters(), covMatrix.GetMatrixArray()) / binWidth;
        }
        else
        {
            bkgIntegralAndErrorInSideRegion = IntegralAndErrorPair(h1, sidebandRegion.first, sidebandRegion.second);
        }
        */

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

/*
double extractTriggerSignal(TF1 *fitFunction, double valueLow, double valueUp)
{
    return 1.0;
}

double extractTriggerBkgInSignalReg(TF1 *fitFunction, double valueLow, double valueUp)
{
    return 1.0;
}

template <bool wFit>
double extractTriggerBkgInSidebandReg(TH1 *h1, TF1 *fitFunction, double valueLow, double valueUp)
{
    if constexpr (wFit)
    {
        return 1.0;
    }
    else
    {
        return 1.0;
    }
}
*/

/*class CorrectedHisto
{
public:
    CorrectedHisto(TH1 *h1Efficiency, TH1 *h1SignalLoss)
        : h1Efficiency(h1Efficiency), h1SignalLoss(h1SignalLoss)
    {
    }

private:
    const TH1 *h1Efficiency;
    const TH1 *h1SignalLoss;
};

class FetchCorrection
{
public:
    FetchCorrection(TH1 *h1Efficiency, TH1 *h1SignalLoss)
        : h1Efficiency(h1Efficiency), h1SignalLoss(h1SignalLoss)
    {
    }

    double getCorrection(TH1 *h1Correction, int ptIndex)
    {
    }

private:
    TH1 *h1Correction;
};*/

template <size_t size>
TH1 *constructSpectrum(const std::array<TH1 *, size> &hArray,
                       const std::vector<double> &binsVec,
                       /*const std::pair<double, double> &furtherNormRange,*/
                       const std::string &histName,
                       int absLimToIntegrate)
{
    if (binsVec.size() - 1 != size)
    {
        throw std::runtime_error("Size of histogram array must be equal to number of bins - 1");
    }

    TH1 *hSpectrum = new TH1D(histName.c_str(), histName.c_str(), binsVec.size() - 1, binsVec.data());

    for (size_t i{0}; i < size; i++)
    {
        auto [binContent, binError] = IntegralAndErrorPair(hArray[i], -absLimToIntegrate, absLimToIntegrate);
        double normalizationFactor = (binsVec[i + 1] - binsVec[i]) /** ((furtherNormRange.second - furtherNormRange.first) / 100.0)*/;

        hSpectrum->SetBinContent(i + 1, binContent / normalizationFactor);
        hSpectrum->SetBinError(i + 1, binError / normalizationFactor);
    }

    hSpectrum->SetDirectory(0);

    return hSpectrum;
}

TH1 *constructSpectrum(const std::vector<TH1 *> &hArray,
                       const std::vector<double> &binsVec,
                       /*const std::pair<double, double> &furtherNormRange,*/
                       const std::string &histName,
                       int absLimToIntegrate)
{
    if (binsVec.size() - 1 != hArray.size())
    {
        throw std::runtime_error("Size of histogram array must be equal to number of bins - 1");
    }

    TH1 *hSpectrum = new TH1D(histName.c_str(), histName.c_str(), binsVec.size() - 1, binsVec.data());

    for (size_t i{0}; i < hArray.size(); i++)
    {
        auto [binContent, binError] = IntegralAndErrorPair(hArray[i], -absLimToIntegrate, absLimToIntegrate);
        double normalizationFactor = (binsVec[i + 1] - binsVec[i]) /** ((furtherNormRange.second - furtherNormRange.first) / 100.0)*/;

        hSpectrum->SetBinContent(i + 1, binContent / normalizationFactor);
        hSpectrum->SetBinError(i + 1, binError / normalizationFactor);
    }

    hSpectrum->SetDirectory(0);

    return hSpectrum;
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

void AnalysisData(bool applyME = false, bool applyEfficiency = false)
{
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

        std::string pathData = basePath + config.dirName + "/";
        std::string nameBase = "h5Phi" + config.name;

        data.h5DataSignal = static_cast<THnSparseF *>(fileDataInput->Get((pathData + nameBase + "DataSignal").c_str()));
        data.h5DataSideband = static_cast<THnSparseF *>(fileDataInput->Get((pathData + nameBase + "DataSideband").c_str()));

        data.h5DataMESignal = static_cast<THnSparseF *>(fileDataMEInput->Get((pathData + nameBase + "DataMESignal").c_str()));
        data.h5DataMESideband = static_cast<THnSparseF *>(fileDataMEInput->Get((pathData + nameBase + "DataMESideband").c_str()));

        return data;
    };

    for (const auto &p : assocParticles)
    {
        loadedDataCollection.push_back(loadAssocData(p));
    }

    fileDataInput->Close();
    fileDataMEInput->Close();

    std::vector<ParticleConfig<2>> particles = {
        {"Phi", {"h1PhiEfficiency", "h1PhiSignalLoss"}},
        {"K0S", {"h1K0SEfficiency", "h1K0SSignalLoss"}},
        {"Pi", {"h1PiEfficiency", "h1PiSignalLoss"}}};

    std::vector<LoadedCorrections<nBinMult>> correctionCollection;

    if (applyEfficiency)
    {
        TFile *fileEffInput = new TFile("../DataFile/Corrections/Efficiencies.root");

        correctionCollection.reserve(particles.size());

        auto loadCorrections = [&](const ParticleConfig<2> &config) -> LoadedCorrections<nBinMult>
        {
            LoadedCorrections<nBinMult> corrections;

            corrections.name = config.name;

            for (int i{0}; i < nBinMult; i++)
            {
                TH1F *h1Efficiency = static_cast<TH1F *>(fileEffInput->Get((config.titles[0] + "_multBin" + std::to_string(i)).c_str()));
                TH1F *h1SignalLoss = static_cast<TH1F *>(fileEffInput->Get((config.titles[1] + "_multBin" + std::to_string(i)).c_str()));
                corrections.h1Corrections[i] = static_cast<TH1F *>(h1Efficiency->Clone());
                corrections.h1Corrections[i]->Multiply(h1Efficiency, h1SignalLoss);
                corrections.h1Corrections[i]->SetDirectory(0);
            }

            return corrections;
        };

        for (const auto &p : particles)
        {
            correctionCollection.push_back(loadCorrections(p));
        }

        fileEffInput->Close();
    }

    std::vector<std::array<std::vector<TH1 *>, nBinMult>> h1PhiAssocNoPtPhi(assocParticles.size());
    for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx)
    {
        for (int i = 0; i < nBinMult; ++i)
        {
            h1PhiAssocNoPtPhi[pIdx][i].resize(assocParticles[pIdx].nBinPt, nullptr);
        }
    }

    TFile *filePhiDataOutput = new TFile("../DataFile/Output/PhiDataHistograms.root", "RECREATE");
    std::vector<TFile *> outputFiles;

    std::vector<TCanvas *> spectraCanvases;

    for (const auto &p : assocParticles)
    {
        outputFiles.push_back(new TFile(("../DataFile/Output/Phi" + p.name + "DataHistograms.root").c_str(), "RECREATE"));
        spectraCanvases.push_back(new TCanvas(("canvasSpectra" + p.name).c_str(), ("canvasSpectra" + p.name).c_str(), 800, 600));
    }

    /*std::array<std::array<TH1 *, nBinPtK0S>, nBinMult> h1PhiK0SDataNoPtPhi{};
        std::array<std::array<TH1 *, nBinPtPi>, nBinMult> h1PhiPiDataNoPtPhi{};

        TCanvas *canvasSpectraK0S = new TCanvas("canvasSpectraK0S", "canvasSpectraK0S", 800, 600);
    TCanvas *canvasSpectraPi = new TCanvas("canvasSpectraPi", "canvasSpectraPi", 800, 600);*/

    for (int i{0}; i < nBinMult; i++)
    {
        AxisToCut axisToCutMult{0, i + 1, i + 1};

        double totalTriggerSignalPerMult{0.0};

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

            /*TF1 *fitVoigtBkgMattia = new TF1("fitVoigtBkgMattia", VoigtBkgMattia, 0.995, 1.2, 9);
            fitVoigtBkgMattia->SetParameter(0, 10);
            fitVoigtBkgMattia->SetParameter(1, 1.019);
            fitVoigtBkgMattia->SetParameter(2, 0.001);
            fitVoigtBkgMattia->FixParameter(3, 0.00426);
            fitVoigtBkgMattia->SetNpx(400);
            fitVoigtBkgMattia->SetLineColor(kBlue);*/

            FitSignalAndBkg<false> fitSignalAndBkg{h1PhiData, fitVoigtBkgSourav, 4, phiMassSignalRange, phiMassSidebandRange};
            double triggerSignal = fitSignalAndBkg.getSignal();
            double triggerBkgRatio = fitSignalAndBkg.getBkgInSigRegion() / fitSignalAndBkg.getBkgInSideRegion();

            double phiEff = applyEfficiency ? correctionCollection[0].h1Corrections[i]->GetBinContent(j) : 1.0;
            totalTriggerSignalPerMult += triggerSignal / phiEff;

            // double triggerSignal = extractTriggerSignal(phiMassSignalRange.first, phiMassSignalRange.second);
            // double triggerBkgRatio = extractTriggerBkgInSidebandReg<false>(phiMassSidebandRange.first, phiMassSidebandRange.second) / extractTriggerBkgInSignalReg(phiMassSignalRange.first, phiMassSignalRange.second);

            filePhiDataOutput->cd();
            h1PhiData->Write();

            delete fitVoigtBkgSourav;

            auto processAssocParticle = [&](const AssocParticleConfig &config, const LoadedAssocData &data, int pIndex)
            {
                for (int k{0}; k < config.nBinPt; k++)
                {
                    AxisToCut axisToCutPtAssoc{2, k + 1, k + 1};

                    std::string suffix = "_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_pt" + config.name + "Bin" + std::to_string(k);

                    TH1 *h1Signal = projectTHnSparse<TH1>(data.h5DataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataSignal" + suffix);
                    TH1 *h1Sideband = projectTHnSparse<TH1>(data.h5DataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataSideband" + suffix);
                    TH1 *h1MESignal = projectTHnSparse<TH1>(data.h5DataMESignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataMESignal" + suffix);
                    TH1 *h1MESideband = projectTHnSparse<TH1>(data.h5DataMESideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtAssoc}, {3}, "h1Phi" + config.name + "DataMESideband" + suffix);

                    outputFiles[pIndex]->cd();
                    h1Signal->Write();
                    h1Sideband->Write();
                    h1MESignal->Write();
                    h1MESideband->Write();

                    double assocEff = applyEfficiency ? correctionCollection[config.effIndex].h1Corrections[i]->GetBinContent(k) : 1.0;
                    double totalEff = phiEff * assocEff;

                    // Correzione Signal
                    h1Signal->Scale(1.0 / totalEff);
                    auto [normMESignal, errnormMESignal] = IntegralAndErrorPair(h1MESignal, -0.1, 0.1) / 2;
                    if (normMESignal > 0)
                    {
                        h1MESignal->Scale(1.0 / normMESignal);
                    }
                    if (applyME)
                    {
                        h1Signal->Divide(h1MESignal);
                    }

                    // Correzione Sideband
                    auto [normMESideband, errnormMESideband] = IntegralAndErrorPair(h1MESideband, -0.1, 0.1) / 2;
                    if (normMESideband > 0)
                    {
                        h1MESideband->Scale(1.0 / normMESideband);
                    }
                    if (applyME)
                    {
                        h1Sideband->Divide(h1MESideband);
                    }
                    h1Sideband->Scale(triggerBkgRatio / totalEff);
                    h1Sideband->Write((std::string(h1Sideband->GetName()) + "_Scaled").c_str());

                    h1Signal->Add(h1Sideband, -1);

                    TH1 *&h1Accumulator = h1PhiAssocNoPtPhi[pIndex][i][k];

                    if (j == 0)
                    {
                        std::string accumName = "h1Phi" + config.name + "DataSignal_multBin" + std::to_string(i) + "_pt" + config.name + "Bin" + std::to_string(k);
                        h1Accumulator = static_cast<TH1 *>(h1Signal->Clone(accumName.c_str()));
                        h1Accumulator->SetDirectory(0);
                    }
                    else
                    {
                        h1Accumulator->Add(h1Signal);
                    }
                }
            };

            for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx)
            {
                processAssocParticle(assocParticles[pIdx], loadedDataCollection[pIdx], pIdx);
            }

            /*
            for (int k{0}; k < nBinPtK0S; k++)
            {
                AxisToCut axisToCutPtK0S{2, k + 1, k + 1};

                std::string h1PhiK0SDataSignalName = "h1PhiK0SDataSignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);
                std::string h1PhiK0SDataSidebandName = "h1PhiK0SDataSideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);

                TH1 *h1PhiK0SDataSignal = projectTHnSparse<TH1>(h5PhiK0SDataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3}, h1PhiK0SDataSignalName);
                TH1 *h1PhiK0SDataSideband = projectTHnSparse<TH1>(h5PhiK0SDataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3}, h1PhiK0SDataSidebandName);

                std::string h1PhiK0SDataMESignalName = "h1PhiK0SDataMESignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);
                std::string h1PhiK0SDataMESidebandName = "h1PhiK0SDataMESideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);

                TH1 *h1PhiK0SDataMESignal = projectTHnSparse<TH1>(h5PhiK0SDataMESignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3}, h1PhiK0SDataMESignalName);
                TH1 *h1PhiK0SDataMESideband = projectTHnSparse<TH1>(h5PhiK0SDataMESideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3}, h1PhiK0SDataMESidebandName);

                filePhiK0SDataOutput->cd();
                h1PhiK0SDataSignal->Write();
                h1PhiK0SDataSideband->Write();
                h1PhiK0SDataMESignal->Write();
                h1PhiK0SDataMESideband->Write();

                h1PhiK0SDataSignal->Scale(1.0 / (applyEfficiency ? (correctionCollection[0].h1Corrections[i]->GetBinContent(j) * correctionCollection[1].h1Corrections[i]->GetBinContent(k)) : 1.0));

                auto [normMESignal, errnormMESignal] = IntegralAndErrorPair(h1PhiK0SDataMESignal, -0.1, 0.1) / 2;
                h1PhiK0SDataMESignal->Scale(1.0 / normMESignal);

                h1PhiK0SDataSignal->Divide(h1PhiK0SDataMESignal);

                auto [normMESideband, errnormMESideband] = IntegralAndErrorPair(h1PhiK0SDataMESideband, -0.1, 0.1) / 2;
                h1PhiK0SDataMESideband->Scale(1.0 / normMESideband);

                h1PhiK0SDataSideband->Divide(h1PhiK0SDataMESideband);
                h1PhiK0SDataSideband->Scale(triggerBkgRatio / (applyEfficiency ? (correctionCollection[0].h1Corrections[i]->GetBinContent(j) * correctionCollection[1].h1Corrections[i]->GetBinContent(k)) : 1.0));

                h1PhiK0SDataSidebandName += "_Scaled";
                h1PhiK0SDataSideband->Write(h1PhiK0SDataSidebandName.c_str());

                if (!h1PhiK0SDataNoPtPhi[i][k])
                {
                    std::string h1PhiK0SDataNoPtPhiName = "h1PhiK0SDataSignal_multBin" + std::to_string(i) + "_ptK0SBin" + std::to_string(k);
                    h1PhiK0SDataNoPtPhi[i][k] = static_cast<TH1 *>(h1PhiK0SDataSignal->Clone(h1PhiK0SDataNoPtPhiName.c_str()));
                    h1PhiK0SDataNoPtPhi[i][k]->SetDirectory(0);
                    h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSideband, -1);
                }
                else
                {
                    h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSignal);
                    h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSideband, -1);
                }
            }

            for (int k{0}; k < nBinPtPi; k++)
            {
                AxisToCut axisToCutPtPion{2, k + 1, k + 1};

                std::string h1PhiPiDataSignalName = "h1PhiPiDataSignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptPionBin" + std::to_string(k);
                std::string h1PhiPiDataSidebandName = "h1PhiPiDataSideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptPionBin" + std::to_string(k);

                TH1 *h1PhiPiDataSignal = projectTHnSparse<TH1>(h5PhiPiDataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtPion}, {3}, h1PhiPiDataSignalName);
                TH1 *h1PhiPiDataSideband = projectTHnSparse<TH1>(h5PhiPiDataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtPion}, {3}, h1PhiPiDataSidebandName);

                filePhiPiDataOutput->cd();
                h1PhiPiDataSignal->Write();
                h1PhiPiDataSideband->Write();
                h1PhiPiDataMESignal->Write();
                h1PhiPiDataMESideband->Write();

                h1PhiPiDataSignal->Scale(1.0 / (applyEfficiency ? (correctionCollection[0].h1Corrections[i]->GetBinContent(j) * correctionCollection[2].h1Corrections[i]->GetBinContent(k)) : 1.0));

                auto [normMESignal, errnormMESignal] = IntegralAndErrorPair(h1PhiPiDataMESignal, -0.1, 0.1) / 2;
                h1PhiPiDataMESignal->Scale(1.0 / normMESignal);

                h1PhiPiDataSignal->Divide(h1PhiPiDataMESignal);

                auto [normMESideband, errnormMESideband] = IntegralAndErrorPair(h1PhiPiDataMESideband, -0.1, 0.1) / 2;
                h1PhiPiDataMESideband->Scale(1.0 / normMESideband);

                h1PhiPiDataSideband->Divide(h1PhiPiDataMESideband);
                h1PhiPiDataSideband->Scale(triggerBkgRatio / (applyEfficiency ? (correctionCollection[0].h1Corrections[i]->GetBinContent(j) * correctionCollection[2].h1Corrections[i]->GetBinContent(k)) : 1.0));

                h1PhiPiDataSidebandName += "_Scaled";
                h1PhiPiDataSideband[i][j][k]->Write(h1PhiPiDataSidebandName.c_str());

                if (!h1PhiPiDataNoPtPhi[i][k])
                {
                    std::string h1PhiPiDataNoPtPhiName = "h1PhiPiDataSignal_multBin" + std::to_string(i) + "_ptPionBin" + std::to_string(k);
                    h1PhiPiDataNoPtPhi[i][k] = static_cast<TH1 *>(h1PhiPiDataSignal->Clone(h1PhiPiDataNoPtPhiName.c_str()));
                    h1PhiPiDataNoPtPhi[i][k]->SetDirectory(0);
                    h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSideband, -1);
                }
                else
                {
                    h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSignal);
                    h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSideband, -1);
                }
            }*/
        }

        for (size_t pIdx = 0; pIdx < assocParticles.size(); ++pIdx)
        {
            auto &config = assocParticles[pIdx];

            std::vector<TH1 *> &histogramsForSpectrum = h1PhiAssocNoPtPhi[pIdx][i];

            std::string spectraName = "h1SpectraPhi" + config.name + "_multBin" + std::to_string(i);
            TH1 *h1Spectra = constructSpectrum(histogramsForSpectrum, config.binning, spectraName, 1.0);
            h1Spectra->Scale(1.0 / totalTriggerSignalPerMult);
            SetHistogramStyle(h1Spectra, spectraColors[i]);

            spectraCanvases[pIdx]->cd();
            h1Spectra->Draw(i == 0 ? "" : "SAME");

            outputFiles[pIdx]->cd();
            h1Spectra->Write();
        }

        /*
        std::string h1SpectraPhiK0SName = "h1SpectraPhiK0S_multBin" + std::to_string(i);
        TH1 *h1SpectraPhiK0S = constructSpectrum(h1PhiK0SDataNoPtPhi[i], binspTK0S, h1SpectraPhiK0SName, 1.0);
        h1SpectraPhiK0S->Scale(1.0 / totalTriggerSignalPerMult);
        SetHistogramStyle(h1SpectraPhiK0S, spectraColors[i]);
        canvasSpectraK0S->cd();
        h1SpectraPhiK0S->Draw(i == 0 ? "" : "SAME");
        filePhiK0SDataOutput->cd();
        h1SpectraPhiK0S->Write();

        std::string h1SpectraPhiPiName = "h1SpectraPhiPi_multBin" + std::to_string(i);
        TH1 *h1SpectraPhiPi = constructSpectrum(h1PhiPiDataNoPtPhi[i], binspTPi, h1SpectraPhiPiName, 1.0);
        h1SpectraPhiPi->Scale(1.0 / totalTriggerSignalPerMult);
        SetHistogramStyle(h1SpectraPhiPi, spectraColors[i]);
        canvasSpectraPi->cd();
        h1SpectraPhiPi->Draw(i == 0 ? "" : "SAME");
        filePhiPiDataOutput->cd();
        h1SpectraPhiPi->Write();
        */
    }

    filePhiDataOutput->Close();
    for (const auto& file : outputFiles)
    {
        file->Close();
    }

    TFile *fileOutputSpectra = new TFile("../DataFile/Output/PhiAssocSpectra.root", "RECREATE");
    fileOutputSpectra->cd();
    for (const auto& canvas : spectraCanvases)
    {
        canvas->Write();
    }
    fileOutputSpectra->Close();

    /*
    TFile *fileDataInput = new TFile("../DataFile/Data/AnalysisResults_136.root");

    TH3F *h3PhiData = static_cast<TH3F *>(fileDataInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phi/h3PhiData"));
    h3PhiData->SetDirectory(0);

    THnSparseF *h5PhiK0SDataSignal = static_cast<THnSparseF *>(fileDataInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiK0S/h5PhiK0SDataSignal"));
    THnSparseF *h5PhiK0SDataSideband = static_cast<THnSparseF *>(fileDataInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiK0S/h5PhiK0SDataSideband"));

    THnSparseF *h5PhiPiDataSignal = static_cast<THnSparseF *>(fileDataInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiPi/h5PhiPiDataSignal"));
    THnSparseF *h5PhiPiDataSideband = static_cast<THnSparseF *>(fileDataInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiPi/h5PhiPiDataSideband"));

    fileDataInput->Close();

    TFile *fileDataMEInput = new TFile("../DataFile/Data/AnalysisResultsME_136.root");

    THnSparseF *h5PhiK0SDataMESignal = static_cast<THnSparseF *>(fileDataMEInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiK0S/h5PhiK0SDataMESignal"));
    THnSparseF *h5PhiK0SDataMESideband = static_cast<THnSparseF *>(fileDataMEInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiK0S/h5PhiK0SDataMESideband"));

    THnSparseF *h5PhiPiDataMESignal = static_cast<THnSparseF *>(fileDataMEInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiPi/h5PhiPiDataMESignal"));
    THnSparseF *h5PhiPiMEDataSideband = static_cast<THnSparseF *>(fileDataMEInput->Get("phi-strangeness-correlation/phiStrangenessCorrelation/phiPi/h5PhiPiDataMESideband"));

    fileDataMEInput->Close();

    TFile *filePhiK0SDataOutput = new TFile("../DataFile/Output/PhiK0SDataHistograms.root", "RECREATE");
    TFile *filePhiPiDataOutput = new TFile("../DataFile/Output/PhiPiDataHistograms.root", "RECREATE");

        std::array<std::array<TH1D *, nBinPtPhi>, nBinMult> h1PhiData{};

        std::array<std::array<std::array<TH2 *, nBinPtK0S>, nBinPtPhi>, nBinMult> h2PhiK0SDataSignal{};
        std::array<std::array<std::array<TH2 *, nBinPtK0S>, nBinPtPhi>, nBinMult> h2PhiK0SDataSideband{};
        std::array<std::array<std::array<TH2 *, nBinPtPi>, nBinPtPhi>, nBinMult> h2PhiPiDataSignal{};
        std::array<std::array<std::array<TH2 *, nBinPtPi>, nBinPtPhi>, nBinMult> h2PhiPiDataSideband{};

        std::array<std::array<std::array<TH1 *, nBinPtK0S>, nBinPtPhi>, nBinMult> h1PhiK0SDataSignal{};
        std::array<std::array<std::array<TH1 *, nBinPtK0S>, nBinPtPhi>, nBinMult> h1PhiK0SDataSideband{};
        std::array<std::array<std::array<TH1 *, nBinPtPi>, nBinPtPhi>, nBinMult> h1PhiPiDataSignal{};
        std::array<std::array<std::array<TH1 *, nBinPtPi>, nBinPtPhi>, nBinMult> h1PhiPiDataSideband{};

        std::array<std::array<TH1 *, nBinPtK0S>, nBinMult> h1PhiK0SDataNoPtPhi{};
        std::array<std::array<TH1 *, nBinPtPi>, nBinMult> h1PhiPiDataNoPtPhi{};

        std::array<TH1 *, nBinMult> h1SpectraPhiK0S{};
        std::array<TH1 *, nBinMult> h1SpectraPhiPi{};

        for (int i{0}; i < nBinMult; i++)
        {
            AxisToCut axisToCutMult{0, i + 1, i + 1};

            double totalTriggerSignalPerMult{0.0};

            for (int j{0}; j < nBinPtPhi; j++)
            {
                AxisToCut axisToCutPtPhi{1, j + 1, j + 1};

                std::string phiHistName = "h1PhiData_multBin" + std::to_string(i) + "_ptBin" + std::to_string(j);
                h1PhiData[i][j] = static_cast<TH1D *>(h3PhiData->ProjectionZ(phiHistName.c_str(), i + 1, i + 1, j + 1, j + 1));
                h1PhiData[i][j]->SetDirectory(0);

                TF1 *fitVoigtBkgSourav = new TF1("fitVoigtBkgSourav", VoigtBkgSourav, 0.995, 1.06, 7);
                // fitVoigtBkgSourav->SetParameter(0, 2);
                fitVoigtBkgSourav->SetParameter(1, 1.019);
                fitVoigtBkgSourav->SetParameter(2, 0.001);
                fitVoigtBkgSourav->FixParameter(3, 0.00426);
                fitVoigtBkgSourav->SetNpx(400);
                fitVoigtBkgSourav->SetLineColor(kRed);

                TF1 *fitVoigtBkgMattia = new TF1("fitVoigtBkgMattia", VoigtBkgMattia, 0.995, 1.2, 9);
                fitVoigtBkgMattia->SetParameter(0, 10);
                fitVoigtBkgMattia->SetParameter(1, 1.019);
                fitVoigtBkgMattia->SetParameter(2, 0.001);
                fitVoigtBkgMattia->FixParameter(3, 0.00426);
                fitVoigtBkgMattia->SetNpx(400);
                fitVoigtBkgMattia->SetLineColor(kBlue);

                FitSignalAndBkg<false> fitSignalAndBkg{h1PhiData[i][j], fitVoigtBkgSourav, 4, phiMassSignalRange, phiMassSidebandRange};
                double triggerSignal = fitSignalAndBkg.getSignal();
                double triggerBkgRatio = fitSignalAndBkg.getBkgInSigRegion() / fitSignalAndBkg.getBkgInSideRegion();

                totalTriggerSignalPerMult += triggerSignal;

                // double triggerSignal = extractTriggerSignal(phiMassSignalRange.first, phiMassSignalRange.second);
                // double triggerBkgRatio = extractTriggerBkgInSidebandReg<false>(phiMassSidebandRange.first, phiMassSidebandRange.second) / extractTriggerBkgInSignalReg(phiMassSignalRange.first, phiMassSignalRange.second);

                filePhiDataOutput->cd();
                h1PhiData[i][j]->Write();

                for (int k{0}; k < nBinPtK0S; k++)
                {
                    AxisToCut axisToCutPtK0S{2, k + 1, k + 1};

                    std::string h2PhiK0SDataSignalName = "2D/h2PhiK0SDataSignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);
                    std::string h2PhiK0SDataSidebandName = "2D/h2PhiK0SDataSideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);

                    std::string h1PhiK0SDataSignalName = "1D/h1PhiK0SDataSignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);
                    std::string h1PhiK0SDataSidebandName = "1D/h1PhiK0SDataSideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptK0SBin" + std::to_string(k);

                    h2PhiK0SDataSignal[i][j][k] = projectTHnSparse<TH2>(h5PhiK0SDataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3, 4}, h2PhiK0SDataSignalName);
                    h2PhiK0SDataSideband[i][j][k] = projectTHnSparse<TH2>(h5PhiK0SDataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3, 4}, h2PhiK0SDataSidebandName);

                    h1PhiK0SDataSignal[i][j][k] = projectTHnSparse<TH1>(h5PhiK0SDataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3}, h1PhiK0SDataSignalName);
                    h1PhiK0SDataSideband[i][j][k] = projectTHnSparse<TH1>(h5PhiK0SDataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtK0S}, {3}, h1PhiK0SDataSidebandName);

                    filePhiK0SDataOutput->cd();
                    // h2PhiK0SDataSignal[i][j][k]->Write();
                    // h2PhiK0SDataSideband[i][j][k]->Write();
                    h1PhiK0SDataSignal[i][j][k]->Write();
                    h1PhiK0SDataSideband[i][j][k]->Write();

                    h2PhiK0SDataSignal[i][j][k]->Sumw2();
                    h2PhiK0SDataSideband[i][j][k]->Sumw2();

                    // h1PhiK0SDataSignal[i][j][k]->Scale(1.0 / triggerSignal);
                    h1PhiK0SDataSideband[i][j][k]->Scale(triggerBkgRatio);

                    h1PhiK0SDataSidebandName += "_Scaled";
                    h1PhiK0SDataSideband[i][j][k]->Write(h1PhiK0SDataSidebandName.c_str());

                    if (!h1PhiK0SDataNoPtPhi[i][k])
                    {
                        std::string h1PhiK0SDataNoPtPhiName = "1D/h1PhiK0SDataSignal_multBin" + std::to_string(i) + "_ptK0SBin" + std::to_string(k);
                        h1PhiK0SDataNoPtPhi[i][k] = static_cast<TH1 *>(h1PhiK0SDataSignal[i][j][k]->Clone(h1PhiK0SDataNoPtPhiName.c_str()));
                        h1PhiK0SDataNoPtPhi[i][k]->SetDirectory(0);
                        // h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSideband[i][j][k], -triggerBkgRatio);
                        h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSideband[i][j][k], -1);
                    }
                    else
                    {
                        h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSignal[i][j][k]);
                        // h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSideband[i][j][k], -triggerBkgRatio);
                        h1PhiK0SDataNoPtPhi[i][k]->Add(h1PhiK0SDataSideband[i][j][k], -1);
                    }
                }

                for (int k{0}; k < nBinPtPi; k++)
                {
                    AxisToCut axisToCutPtPion{2, k + 1, k + 1};

                    std::string h2PhiPiDataSignalName = "2D/h2PhiPiDataSignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptPionBin" + std::to_string(k);
                    std::string h2PhiPiDataSidebandName = "2D/h2PhiPiDataSideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptPionBin" + std::to_string(k);

                    std::string h1PhiPiDataSignalName = "1D/h1PhiPiDataSignal_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptPionBin" + std::to_string(k);
                    std::string h1PhiPiDataSidebandName = "1D/h1PhiPiDataSideband_multBin" + std::to_string(i) + "_ptPhiBin" + std::to_string(j) + "_ptPionBin" + std::to_string(k);

                    h2PhiPiDataSignal[i][j][k] = projectTHnSparse<TH2>(h5PhiPiDataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtPion}, {3, 4}, h2PhiPiDataSignalName);
                    h2PhiPiDataSideband[i][j][k] = projectTHnSparse<TH2>(h5PhiPiDataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtPion}, {3, 4}, h2PhiPiDataSidebandName);

                    h1PhiPiDataSignal[i][j][k] = projectTHnSparse<TH1>(h5PhiPiDataSignal, {axisToCutMult, axisToCutPtPhi, axisToCutPtPion}, {3}, h1PhiPiDataSignalName);
                    h1PhiPiDataSideband[i][j][k] = projectTHnSparse<TH1>(h5PhiPiDataSideband, {axisToCutMult, axisToCutPtPhi, axisToCutPtPion}, {3}, h1PhiPiDataSidebandName);

                    filePhiPiDataOutput->cd();
                    // h2PhiPiDataSignal[i][j][k]->Write();
                    // h2PhiPiDataSideband[i][j][k]->Write();
                    h1PhiPiDataSignal[i][j][k]->Write();
                    h1PhiPiDataSideband[i][j][k]->Write();

                    h2PhiPiDataSignal[i][j][k]->Sumw2();
                    h2PhiPiDataSideband[i][j][k]->Sumw2();

                    // h1PhiPiDataSignal[i][j][k]->Scale(1.0 / triggerSignal);
                    h1PhiPiDataSideband[i][j][k]->Scale(triggerBkgRatio);

                    h1PhiPiDataSidebandName += "_Scaled";
                    h1PhiPiDataSideband[i][j][k]->Write(h1PhiPiDataSidebandName.c_str());

                    if (!h1PhiPiDataNoPtPhi[i][k])
                    {
                        std::string h1PhiPiDataNoPtPhiName = "1D/h1PhiPiDataSignal_multBin" + std::to_string(i) + "_ptPionBin" + std::to_string(k);
                        h1PhiPiDataNoPtPhi[i][k] = static_cast<TH1 *>(h1PhiPiDataSignal[i][j][k]->Clone(h1PhiPiDataNoPtPhiName.c_str()));
                        h1PhiPiDataNoPtPhi[i][k]->SetDirectory(0);
                        // h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSideband[i][j][k], -triggerBkgRatio);
                        h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSideband[i][j][k], -1);
                    }
                    else
                    {
                        h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSignal[i][j][k]);
                        // h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSideband[i][j][k], -triggerBkgRatio);
                        h1PhiPiDataNoPtPhi[i][k]->Add(h1PhiPiDataSideband[i][j][k], -1);
                    }
                }

                // delete fitVoigtBkgSourav;
                // delete fitVoigtBkgMattia;
            }

            std::string h1SpectraPhiK0SName = "h1SpectraPhiK0S_multBin" + std::to_string(i);
            h1SpectraPhiK0S[i] = constructSpectrum(h1PhiK0SDataNoPtPhi[i], binspTK0S, h1SpectraPhiK0SName, 1.0);
            h1SpectraPhiK0S[i]->Scale(1.0 / totalTriggerSignalPerMult);
            SetHistogramStyle(h1SpectraPhiK0S[i], spectraColors[i]);
            canvasSpectraK0S->cd();
            h1SpectraPhiK0S[i]->Draw(i == 0 ? "" : "SAME");
            filePhiK0SDataOutput->cd();
            h1SpectraPhiK0S[i]->Write();

            std::string h1SpectraPhiPiName = "h1SpectraPhiPi_multBin" + std::to_string(i);
            h1SpectraPhiPi[i] = constructSpectrum(h1PhiPiDataNoPtPhi[i], binspTPi, h1SpectraPhiPiName, 1.0);
            h1SpectraPhiPi[i]->Scale(1.0 / totalTriggerSignalPerMult);
            SetHistogramStyle(h1SpectraPhiPi[i], spectraColors[i]);
            canvasSpectraPi->cd();
            h1SpectraPhiPi[i]->Draw(i == 0 ? "" : "SAME");
            filePhiPiDataOutput->cd();
            h1SpectraPhiPi[i]->Write();
        }
            

    filePhiDataOutput->Close();
    filePhiK0SDataOutput->Close();
    filePhiPiDataOutput->Close();

    TFile *fileOutputSpectra = new TFile("../DataFile/Output/PhiAssocSpectra.root", "RECREATE");
    fileOutputSpectra->cd();
    canvasSpectraK0S->Write();
    canvasSpectraPi->Write();
    fileOutputSpectra->Close();*/
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
        h3MCGenAssocReco->Sumw2();

        TH3 *h3MCReco = projectTHnSparse<TH3>(data.h4MCReco, {axisToCutZVtx}, {1, 2, 3}, Form("h3%sMCReco", data.name.c_str()));
        h3MCReco->Sumw2();

        TH3 *h3Efficiency = static_cast<TH3 *>(h3MCReco->Clone(Form("h3%s`Efficiency", data.name.c_str())));
        h3Efficiency->SetDirectory(0);
        h3Efficiency->Divide(h3MCReco, h3MCGenAssocReco, 1.0, 1.0, "B");

        TH3 *h3SignalLoss = static_cast<TH3 *>(h3MCGenAssocReco->Clone(Form("h3%s`SignalLoss", data.name.c_str())));
        h3SignalLoss->SetDirectory(0);
        h3SignalLoss->Divide(h3MCGenAssocReco, data.h3MCGen, 1.0, 1.0, "B");

        TCanvas *canvasEfficiency = new TCanvas(Form("h3%s`Efficiency", data.name.c_str()), Form("h3%s`Efficiency", data.name.c_str()), 800, 600);
        TCanvas *canvasSignalLoss = new TCanvas(Form("h3%s`SignalLoss", data.name.c_str()), Form("h3%s`SignalLoss", data.name.c_str()), 800, 600);

        /*TFile *fileMCOutput = new TFile("../DataFile/Output/PhiMCHistograms.root", "RECREATE");
        fileMCOutput->cd();
        h3Efficiency->Write();
        h3SignalLoss->Write();
        fileMCOutput->Close();*/

        for (int i{0}; i < nBinMult; i++)
        {
            TH1 *h1MCGen = data.h3MCGen->ProjectionY(Form("h1%sMCGen_multBin%d", data.name.c_str(), i), i + 1, i + 1, 1, nBinY);
            h1MCGen->SetDirectory(0);

            AxisToCut axisToCutMult{1, i + 1, i + 1};

            TH1 *h1MCGenAssocReco = projectTHnSparse<TH1>(data.h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, Form("h1%sMCGenAssocReco_multBin%d", data.name.c_str(), i));
            h1MCGenAssocReco->Sumw2();

            TH1 *h1MCReco = projectTHnSparse<TH1>(data.h4MCReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, Form("h1%sMCReco_multBin%d", data.name.c_str(), i));
            h1MCReco->Sumw2();

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
        }
    };

    for (const auto &data : dataCollection)
    {
        processData(data);
    }

    /*
    std::array<std::string, 3> mcTitlesPhi{"phi/h3PhiMCGen", "phi/h4PhiMCGenAssocReco", "phi/h4PhiMCReco"};
    std::array<std::string, 3> mcTitlesK0S{"k0s/h3K0SMCGen", "k0s/h4K0SMCGenAssocReco", "k0s/h4K0SMCReco"};
    std::array<std::string, 3> mcTitlesPi{"pi/h3PiMCGen", "pi/h4PiMCGenAssocReco", "pi/h4PiMCReco"};

    TH3F *h3MCGen = static_cast<TH3F *>(fileMCInput->Get((mcBasePath + mcTitlesPhi[0]).c_str()));
    h3MCGen->SetDirectory(0);

    THnSparseF *h4MCGenAssocReco = static_cast<THnSparseF *>(fileMCInput->Get((mcBasePath + mcTitlesPhi[1]).c_str()));
    THnSparseF *h4MCReco = static_cast<THnSparseF *>(fileMCInput->Get((mcBasePath + mcTitlesPhi[2]).c_str()));

    fileMCInput->Close();

    AxisToCut axisToCutZVtx{0, 1, nBinZVtx};
    AxisToCut axisToCutY{3, 1, nBinY};

    h3MCGen->GetZaxis()->SetRange(1, nBinY);
    TH3 *h3MCGenAssocReco = projectTHnSparse<TH3>(h4MCGenAssocReco, {axisToCutZVtx}, {1, 2, 3}, "h3PhiMCGenAssocReco");
    TH3 *h3MCReco = projectTHnSparse<TH3>(h4MCReco, {axisToCutZVtx}, {1, 2, 3}, "h3PhiMCReco");

    h3MCGen->Sumw2();
    h3MCGenAssocReco->Sumw2();
    h3MCReco->Sumw2();

    TH3 *h3Efficiency = static_cast<TH3 *>(h3MCReco->Clone("h3PhiEfficiency"));
    h3Efficiency->SetDirectory(0);
    h3Efficiency->Divide(h3MCReco, h3MCGenAssocReco, 1.0, 1.0, "B");

    TH3 *h3SignalLoss = static_cast<TH3 *>(h3MCGenAssocReco->Clone("h3PhiSignalLoss"));
    h3SignalLoss->SetDirectory(0);
    h3SignalLoss->Divide(h3MCGenAssocReco, h3MCGen, 1.0, 1.0, "B");

    std::array<TH1 *, nBinMult> h1MCReco{};
    std::array<TH1 *, nBinMult> h1MCGenAssocReco{};
    std::array<TH1 *, nBinMult> h1MCGen{};

    std::array<TH1 *, nBinMult> h1Efficiency{};
    std::array<TH1 *, nBinMult> h1SignalLoss{};

    TCanvas *canvasEfficiency = new TCanvas("canvasEfficiency", "canvasEfficiency", 800, 600);
    TCanvas *canvasSignalLoss = new TCanvas("canvasSignalLoss", "canvasSignalLoss", 800, 600);

    for (int i{0}; i < nBinMult; i++)
    {
        std::string h1MCGenName = "h1PhiMCGen_multBin" + std::to_string(i);
        h1MCGen[i] = h3MCGen->ProjectionY(h1MCGenName.c_str(), i + 1, i + 1, 1, nBinY);
        h1MCGen[i]->SetDirectory(0);

        AxisToCut axisToCutMult{1, i + 1, i + 1};

        std::string h1MCGenAssocRecoName = "h1PhiMCGenAssocReco_multBin" + std::to_string(i);
        h1MCGenAssocReco[i] = projectTHnSparse<TH1>(h4MCGenAssocReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, h1MCGenAssocRecoName.c_str());

        std::string h1MCRecoName = "h1PhiMCReco_multBin" + std::to_string(i);
        h1MCReco[i] = projectTHnSparse<TH1>(h4MCReco, {axisToCutZVtx, axisToCutMult, axisToCutY}, {2}, h1MCRecoName.c_str());

        h1MCGen[i]->Sumw2();
        h1MCGenAssocReco[i]->Sumw2();
        h1MCReco[i]->Sumw2();

        h1Efficiency[i] = static_cast<TH1 *>(h1MCReco[i]->Clone());
        h1Efficiency[i]->SetDirectory(0);
        h1Efficiency[i]->Divide(h1MCReco[i], h1MCGenAssocReco[i], 1.0, 1.0, "B");
        SetHistogramStyle(h1Efficiency[i], spectraColors[i]);
        canvasEfficiency->cd();
        h1Efficiency[i]->Draw(i == 0 ? "" : "SAME");

        h1SignalLoss[i] = static_cast<TH1 *>(h1MCGenAssocReco[i]->Clone());
        h1SignalLoss[i]->SetDirectory(0);
        h1SignalLoss[i]->Divide(h1MCGenAssocReco[i], h1MCGen[i], 1.0, 1.0, "B");
        SetHistogramStyle(h1SignalLoss[i], spectraColors[i]);
        canvasSignalLoss->cd();
        h1SignalLoss[i]->Draw(i == 0 ? "" : "SAME");
    }
    */
}

void PhiStrangeCorr(int mode = 0)
{
    switch (mode)
    {
    case 0:
        AnalysisData();
        break;
    case 1:
        AnalysisMC();
        break;
    default:
        throw std::runtime_error("Invalid mode selected!");
    }
}
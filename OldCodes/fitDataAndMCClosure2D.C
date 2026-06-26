#include <utility>

#include "Riostream.h"
#include "TFile.h"
#include "TLegend.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include "THnSparse.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TStyle.h"
#include "TF1.h"
#include "TF2.h"
#include "TFitResult.h"
#include "TGraphAsymmErrors.h"
#include "TMultiGraph.h"

#include "RooRealVar.h"
#include "RooDataHist.h"
#include "RooHistPdf.h"
#include "RooPlot.h"
#include "RooFitResult.h"
#include "RooProduct.h"
#include "RooGaussian.h"
#include "RooPolynomial.h"
#include "RooAddPdf.h"
#include "RooDataSet.h"
#include "RooArgList.h"
#include "RooProdPdf.h"
#include "RooCBShape.h"
#include "RooCrystalBall.h"
#include "RooVoigtian.h"
#include "RooGenericPdf.h"

using namespace std;
using namespace RooFit;

Double_t VoigtPoly(Double_t* x, Double_t* par)
{
    return par[0] * TMath::Voigt(x[0] - par[2], par[3], par[1], 4) + (par[4] + par[5] * x[0] + par[6] * TMath::Sqrt(x[0] - 0.987));
}

Double_t Voigt(Double_t* x, Double_t* par)
{
    return par[0] * TMath::Voigt(x[0] - par[2], par[3], par[1], 4);
}

Double_t Voigt1(Double_t* x, Double_t* par)
{
    return TMath::Voigt(x[0] - par[1], par[2], par[0], 4);
}

Double_t PolySqrt(Double_t* x, Double_t* par)
{
    return par[0] + par[1] * x[0] + par[2] * TMath::Sqrt(x[0] - 0.987); 
}

Double_t DoubleSidedCrystalBallPoly(Double_t *x, Double_t *par)
{ 
    Double_t alpha_l = par[0]; 
    Double_t alpha_h = par[1]; 
    Double_t n_l = par[2]; 
    Double_t n_h = par[3]; 
    Double_t mean = par[4]; 
    Double_t sigma = par[5];
    Double_t N = par[6];

    Double_t t = (x[0] - mean) / sigma;
    Double_t fact1TLowerAlphaL = alpha_l / n_l;
    Double_t fact2TLowerAlphaL = (n_l / alpha_l) - alpha_l - t;
    Double_t fact1THigherAlphaH = alpha_h / n_h;
    Double_t fact2THigherAlphaH = (n_h / alpha_h) - alpha_h + t;
    Double_t result;
    
    if (-alpha_l <= t && t <= alpha_h) {
        result = exp(-0.5 * TMath::Power(t, 2));
    } else if (t < -alpha_l) {
        result = exp(-0.5 * TMath::Power(alpha_l, 2)) * TMath::Power(fact1TLowerAlphaL * fact2TLowerAlphaL, -n_l);
    } else if (t > alpha_h) {
        result = exp(-0.5 * TMath::Power(alpha_h, 2)) * TMath::Power(fact1THigherAlphaH * fact2THigherAlphaH, -n_h);
    }

    return N * result + (par[7] + par[8] * x[0]);
}

Double_t DoubleSidedCrystalBall(Double_t *x, Double_t *par)
{ 
    Double_t alpha_l = par[0]; 
    Double_t alpha_h = par[1]; 
    Double_t n_l = par[2]; 
    Double_t n_h = par[3]; 
    Double_t mean = par[4]; 
    Double_t sigma = par[5];
    Double_t N = par[6];

    Double_t t = (x[0] - mean) / sigma;
    Double_t fact1TLowerAlphaL = alpha_l / n_l;
    Double_t fact2TLowerAlphaL = (n_l / alpha_l) - alpha_l - t;
    Double_t fact1THigherAlphaH = alpha_h / n_h;
    Double_t fact2THigherAlphaH = (n_h / alpha_h) - alpha_h + t;
    Double_t result;
    
    if (-alpha_l <= t && t <= alpha_h) {
        result = exp(-0.5 * TMath::Power(t, 2));
    } else if (t < -alpha_l) {
        result = exp(-0.5 * TMath::Power(alpha_l, 2)) * TMath::Power(fact1TLowerAlphaL * fact2TLowerAlphaL, -n_l);
    } else if (t > alpha_h) {
        result = exp(-0.5 * TMath::Power(alpha_h, 2)) * TMath::Power(fact1THigherAlphaH * fact2THigherAlphaH, -n_h);
    }

    return N * result;
}

Double_t DoubleSidedCrystalBall1(Double_t *x, Double_t *par)
{ 
    Double_t alpha_l = par[0]; 
    Double_t alpha_h = par[1]; 
    Double_t n_l = par[2]; 
    Double_t n_h = par[3]; 
    Double_t mean = par[4]; 
    Double_t sigma = par[5];

    Double_t t = (x[0] - mean) / sigma;
    Double_t fact1TLowerAlphaL = alpha_l / n_l;
    Double_t fact2TLowerAlphaL = (n_l / alpha_l) - alpha_l - t;
    Double_t fact1THigherAlphaH = alpha_h / n_h;
    Double_t fact2THigherAlphaH = (n_h / alpha_h) - alpha_h + t;
    Double_t result;
    
    if (-alpha_l <= t && t <= alpha_h) {
        result = exp(-0.5 * TMath::Power(t, 2));
    } else if (t < -alpha_l) {
        result = exp(-0.5 * TMath::Power(alpha_l, 2)) * TMath::Power(fact1TLowerAlphaL * fact2TLowerAlphaL, -n_l);
    } else if (t > alpha_h) {
        result = exp(-0.5 * TMath::Power(alpha_h, 2)) * TMath::Power(fact1THigherAlphaH * fact2THigherAlphaH, -n_h);
    }

    return result;
}

Double_t Poly1(Double_t* x, Double_t* par)
{
    return par[0] + par[1] * x[0];
}

Double_t PhiInvMassK0SNSigmadEdx(Double_t* x, Double_t* par) 
{
    Double_t K0Ssig = DoubleSidedCrystalBall1(&x[0], &par[0]);
    Double_t K0Sbkg = Poly1(&x[0], &par[6]);

    Double_t Phisig = Voigt1(&x[1], &par[8]);
    Double_t Phibkg = PolySqrt(&x[1], &par[11]);

    return par[14] * Phisig * K0Ssig + par[15] * Phisig * K0Sbkg + par[16] * Phibkg * K0Ssig + par[17] * Phibkg * K0Sbkg;
}

Double_t PhiInvMassK0SNSigmadEdxSig(Double_t* x, Double_t* par) 
{
    Double_t K0Ssig = DoubleSidedCrystalBall1(&x[0], &par[0]);
    Double_t Phisig = Voigt1(&x[1], &par[6]);

    return par[9] * Phisig * K0Ssig;
}

TH2F* Project2D(THnSparseF* hn, Int_t axistocut, Int_t binlow, Int_t binup, Int_t axistoproj1, Int_t axistoproj2, Option_t* option = "", string hname = "") 
{ 
    if (!hn) return 0;
    hn->GetAxis(axistocut)->SetRange(binlow, binup);
    TH2F* h2 = (TH2F*)hn->Projection(axistoproj1, axistoproj2, option);
    h2->SetName(hname.c_str());
    h2->SetDirectory(0);
    return h2;
}

TH2F* Project2D(THnSparseF* hn, Int_t axistocut1, Int_t binlow1, Int_t binup1, Int_t axistocut2, Int_t binlow2, Int_t binup2, Int_t axistoproj1, Int_t axistoproj2, Option_t* option = "", string hname = "") 
{ 
    if (!hn) return 0;
    hn->GetAxis(axistocut1)->SetRange(binlow1, binup1);
    hn->GetAxis(axistocut2)->SetRange(binlow2, binup2);
    TH2F* h2 = (TH2F*)hn->Projection(axistoproj1, axistoproj2, option);
    h2->SetName(hname.c_str());
    h2->SetDirectory(0);
    return h2;
}
TH1F* Project1D(THnSparseF* hn, Int_t axistocut1, Int_t binlow1, Int_t binup1, Int_t axistocut2, Int_t binlow2, Int_t binup2, Int_t axistocut3, Int_t binlow3, Int_t binup3, Int_t axistoproj, Option_t* option = "", string hname = "") 
{ 
    if (!hn) return 0;
    hn->GetAxis(axistocut1)->SetRange(binlow1, binup1);
    hn->GetAxis(axistocut2)->SetRange(binlow2, binup2);
    hn->GetAxis(axistocut3)->SetRange(binlow3, binup3);
    TH1F* h1 = (TH1F*)hn->Projection(axistoproj, option);
    h1->SetName(hname.c_str());
    h1->SetDirectory(0);
    return h1;
}

const vector<Int_t> Colors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856, 601, kViolet, kPink + 9, kPink + 1, 1};
const vector<Int_t> FullMarkers  = {20, 21, 33, 34, 29, 41, 47, 43};
const vector<Int_t> EmptyMarkers = {53, 56, 57, 58, 64, 67, 54, 65};
const vector<Int_t> Markers = {20, 21, 33, 34, 29, 41, 47, 43, 53, 56, 57, 58, 64, 67, 54, 65};

constexpr Int_t nbin_deltay = 3, nbin_mult = 10, nbin_pTK0S = 7, nbin_pTPi = 9, nbin_pTPi2 = 9, nbin_massPhi = 13;

constexpr Double_t deltay_axis[nbin_deltay] = {1.0, 0.5, 0.1};
constexpr Double_t mult_axis[nbin_mult+1] = {0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
constexpr Double_t pTK0S_axis[nbin_pTK0S+1] = {0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0};
constexpr Double_t pTPi_axis[nbin_pTPi+1] = {0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};

constexpr Double_t mult[nbin_mult] = {20.3, 17.1, 14.6, 12.9, 11.6, 10.1, 8.5, 7.2, 5.7, 3.8};
constexpr Double_t errmult[nbin_mult] = {0.5, 0.4, 0.3, 0.3, 0.3, 0.2, 0.2, 0.2, 0.1, 0.1};

constexpr Double_t topPadHeight = 0.7; 
constexpr Double_t bottomPadHeight = 1.0 - topPadHeight;
constexpr Double_t scaleFactor = bottomPadHeight / topPadHeight;

const array<string, nbin_deltay> yCut = {"Inclusive", "|#Delta#it{y}| < 0.5", "|#Delta#it{y}| < 0.1"};

constexpr Double_t lowmPhiPur = 1.0095, upmPhiPur = 1.029;

pair<Double_t, Double_t> GetPhiPurityAndError(TH1F* h1PhiInvMass, string nameCanvas, Int_t isDataOrReco, Int_t isK0SOrPi, vector<Int_t> indices, bool printCanvas = false)
{
    h1PhiInvMass->SetTitle("; #it{M}(K^{+}K^{#minus}) (GeV/#it{c}^{2}); Counts");
    Double_t binsize = h1PhiInvMass->GetXaxis()->GetBinWidth(1);

    TF1* fitVoigtPolyPur = new TF1("fitVoigtPolyPur", VoigtPoly, 0.987, 1.06, 7);
    if (isDataOrReco == 2) {
        if (isK0SOrPi == 1) {
            fitVoigtPolyPur->SetParameter(0, 10);
            if (indices.size() == 2 && (indices[0] == 0 && (indices[1] == 0 || indices[1] == 5))) fitVoigtPolyPur->SetParameter(0, 100);
            if (indices.size() == 2 && (indices[0] == 2 && indices[1] == 6)) fitVoigtPolyPur->SetParameter(0, 50);
        }
        else if (isK0SOrPi == 2) fitVoigtPolyPur->SetParameter(0, 1.75);
    }
    fitVoigtPolyPur->FixParameter(1, 0.00426);
    fitVoigtPolyPur->SetParameter(2, 1.019);
    fitVoigtPolyPur->SetParameter(3, 0.001);
    fitVoigtPolyPur->SetNpx(400);
    fitVoigtPolyPur->SetLineColor(kRed);

    TFitResultPtr fitResultVoigtPolyPur = h1PhiInvMass->Fit("fitVoigtPolyPur", "RS0");
    const Double_t* paramsVoigtPolyPur = fitResultVoigtPolyPur->GetParams();

    TF1* fitVoigtPolybisPur = new TF1("fitVoigtPolybisPur", VoigtPoly, 0.987, 1.06, 7);
    fitVoigtPolybisPur->SetParameter(0, paramsVoigtPolyPur[0]);
    fitVoigtPolybisPur->SetParameter(1, paramsVoigtPolyPur[1]);
    fitVoigtPolybisPur->SetParameter(2, paramsVoigtPolyPur[2]);
    fitVoigtPolybisPur->SetParameter(3, paramsVoigtPolyPur[3]);
    fitVoigtPolybisPur->FixParameter(4, 0);
    fitVoigtPolybisPur->FixParameter(5, 0);
    fitVoigtPolybisPur->FixParameter(6, 0);

    TF1* fitVoigtPur = new TF1("fitVoigtPur", Voigt, 0.987, 1.05, 4);
    fitVoigtPur->SetParameter(0, paramsVoigtPolyPur[0]);
    fitVoigtPur->SetParameter(1, paramsVoigtPolyPur[1]);
    fitVoigtPur->SetParameter(2, paramsVoigtPolyPur[2]);
    fitVoigtPur->SetParameter(3, paramsVoigtPolyPur[3]);
    fitVoigtPur->SetNpx(400);
    fitVoigtPur->SetLineColor(kBlue);

    if (printCanvas) {
        TCanvas* fitPhiPur = new TCanvas(nameCanvas.c_str(), nameCanvas.c_str(), 800, 800);
        fitPhiPur->cd();
        gStyle->SetOptStat(0);
        h1PhiInvMass->Draw();
        fitVoigtPolyPur->Draw("same");
        fitVoigtPur->Draw("same");
        
        TLegend* legPhi1Pur = new TLegend(0.55, 0.4, 0.75, 0.43);
        legPhi1Pur->SetHeader("#bf{This work}");
        legPhi1Pur->SetTextSize(0.05);
        legPhi1Pur->SetLineWidth(0);
        legPhi1Pur->Draw("same");

        TLegend* legPhi2Pur = new TLegend(0.55, 0.2, 0.75, 0.4);
        legPhi2Pur->SetHeader("#phi #rightarrow K^{+}K^{#minus}");
        legPhi2Pur->AddEntry(fitVoigtPolyPur, "Voigt + bkg", "l");
        legPhi2Pur->AddEntry(fitVoigtPur, "Voigt", "l");
        legPhi2Pur->SetTextSize(0.05);
        legPhi2Pur->SetLineWidth(0);
        legPhi2Pur->Draw("same");

        fitPhiPur->cd();
        
        TLine* line1 = new TLine(lowmPhiPur, h1PhiInvMass->GetMinimum(), lowmPhiPur, h1PhiInvMass->GetMaximum());
        TLine* line2 = new TLine(upmPhiPur, h1PhiInvMass->GetMinimum(), upmPhiPur, h1PhiInvMass->GetMaximum());

        line1->SetLineColor(kBlack);
        line1->SetLineStyle(kDashed);
        line1->SetLineWidth(2);
        line1->Draw("same");

        line2->SetLineColor(kBlack);
        line2->SetLineStyle(kDashed);
        line2->SetLineWidth(2);
        line2->Draw("same");
    }

    Double_t integralVoigtPolyPur = fitVoigtPolyPur->Integral(lowmPhiPur, upmPhiPur) / binsize;
    Double_t integralVoigt1Pur = fitVoigtPur->Integral(lowmPhiPur, upmPhiPur) / binsize;

    TMatrixDSym covMatrixVoigtPolyPur = fitResultVoigtPolyPur->GetCovarianceMatrix();
    Double_t errintegralVoigtPolyPur = fitVoigtPolyPur->IntegralError(lowmPhiPur, upmPhiPur, paramsVoigtPolyPur, covMatrixVoigtPolyPur.GetMatrixArray()) / binsize;
    Double_t errintegralVoigt1Pur = fitVoigtPolybisPur->IntegralError(lowmPhiPur, upmPhiPur, paramsVoigtPolyPur, covMatrixVoigtPolyPur.GetMatrixArray()) / binsize;

    Double_t purityVoigt = integralVoigt1Pur / integralVoigtPolyPur;
    Double_t errpurityVoigt = purityVoigt * TMath::Sqrt(TMath::Power(errintegralVoigt1Pur / integralVoigt1Pur, 2) + TMath::Power(errintegralVoigtPolyPur / integralVoigtPolyPur, 2));

    return make_pair(purityVoigt, errpurityVoigt);
}

pair<Double_t, Double_t> FitPhiK0S(TH2F* h2PhiK0SInvMass, vector<Int_t> indices, TFile* file,
                                   const vector<Double_t>& params = {1., 1., 5., 5., 0.49, 0.003, -1.}, 
                                   const vector<Double_t>& lowLimits = {1., 1., 1., 1., 0.48, 0.001, -1.7},
                                   const vector<Double_t>& upLimits = {2., 2., 10., 10., 0.5, 0.01, 10})
{
    // Definisci le variabili x e y
    RooRealVar x("x", "x", h2PhiK0SInvMass->GetXaxis()->GetXmin(), h2PhiK0SInvMass->GetXaxis()->GetXmax());
    RooRealVar y("y", "y", h2PhiK0SInvMass->GetYaxis()->GetXmin(), h2PhiK0SInvMass->GetYaxis()->GetXmax());

    // Converte l'istogramma 2D in un RooDataHist
    RooDataHist data("data", "data", RooArgList(x, y), h2PhiK0SInvMass);

    // Definisci i parametri per la Double Sided Crystal Ball e pol1 per l'asse x
    RooRealVar alpha1CB("alpha1CB", "alpha1CB", params.at(0), lowLimits.at(0), upLimits.at(0));
    RooRealVar alpha2CB("alpha2CB", "alpha2CB", params.at(1), lowLimits.at(1), upLimits.at(1));
    RooRealVar n1CB("n1CB", "n1CB", params.at(2), lowLimits.at(2), upLimits.at(2));
    RooRealVar n2CB("n2CB", "n2CB", params.at(3), lowLimits.at(3), upLimits.at(3));
    RooRealVar meanCB("meanCB", "meanCB", params.at(4), lowLimits.at(4), upLimits.at(4));
    RooRealVar sigmaCB("sigmaCB", "sigmaCB", params.at(5), lowLimits.at(5), upLimits.at(5));
    RooCrystalBall dsCrystalBall("dsCrystalBall", "DoubleSidedCrystalBall1", x, meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB);

    RooRealVar a1("a1", "a1", params.at(6), lowLimits.at(6), upLimits.at(6));
    RooPolynomial bkgDSCB("bkgDSCB", "bkgDSCB", x, RooArgList(a1));

    RooRealVar meanV("meanV", "meanV", 1.02, 0.987, 1.2);
    RooRealVar width("width", "width", 0.00426);
    width.setConstant(true);
    RooRealVar sigmaV("sigmaV", "sigmaV", 0.001, 0.0001, 0.01);
    RooVoigtian voigt("voigt", "Voigtian", y, meanV, width, sigmaV);

    RooRealVar b0("b0", "b0", 5, 0, 10);
    RooRealVar b1("b1", "b1", 7, 0, 50);
    RooRealVar b2("b2", "b2", 2, 0, 50);
    RooGenericPdf bkgVoigt("bkgVoigt", "bkgVoigt", "b0 + b1*y + b2*sqrt(y-0.987)", RooArgList(y, b0, b1, b2));
    
    RooProdPdf sigsig("sigsig", "sigsig", RooArgList(dsCrystalBall, voigt));
    RooProdPdf sigbkg("sigbkg", "sigbkg", RooArgList(dsCrystalBall, bkgVoigt));
    RooProdPdf bkgsig("bkgsig", "bkgsig", RooArgList(bkgDSCB, voigt));
    RooProdPdf bkgbkg("bkgbkg", "bkgbkg", RooArgList(bkgDSCB, bkgVoigt));

    RooRealVar nsigsig("nsigsig", "nsigsig", 1000, 0, 1000000);
    RooRealVar nsigbkg("nsigbkg", "nsigbkg", 50000, 0, 2000000);
    RooRealVar nbkgsig("nbkgsig", "nbkgsig", 5000, 0, 250000);
    RooRealVar nbkgbkg("nbkgbkg", "nbkgbkg", 10000, 0, 500000);

    RooAddPdf model("model", "model", RooArgList(sigsig, sigbkg, bkgsig, bkgbkg), RooArgList(nsigsig, nsigbkg, nbkgsig, nbkgbkg));

    /*RooAddPdf* model;
    if (indices.size() == 3 && indices[0] == 2 && (indices[1] == 5 || indices[1] == 6) && indices[2 == 6]) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig));
    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, bkgDSCB), RooArgList(nsig, nbkg));*/

    // Fitta il modello ai dati
    RooFitResult* result = model.fitTo(data, Optimize(1), Extended(1), Save(1));

    meanCB.Print();
    sigmaCB.Print();
    alpha1CB.Print();
    n1CB.Print();
    alpha2CB.Print();
    n2CB.Print();
    a1.Print();

    meanV.Print();
    width.Print();
    sigmaV.Print();
    b0.Print();
    b1.Print();
    b2.Print();

    nsigsig.Print();
    nsigbkg.Print();
    nbkgsig.Print();
    nbkgbkg.Print();

    Int_t lowedge = h2PhiK0SInvMass->GetXaxis()->FindFixBin(meanCB.getVal() - 3 * sigmaCB.getVal());
    Int_t upedge = h2PhiK0SInvMass->GetXaxis()->FindFixBin(meanCB.getVal() + 3 * sigmaCB.getVal());

    Double_t lowfitK0S = h2PhiK0SInvMass->GetXaxis()->GetBinLowEdge(lowedge);
    Double_t upfitK0S = h2PhiK0SInvMass->GetXaxis()->GetBinLowEdge(upedge +1);

    TCanvas* cPhiK0SInvMass;
    if (indices.size() == 2) cPhiK0SInvMass = new TCanvas(Form("cPhiK0SInvMass_%d_%d", indices[0], indices[1]), Form("cPhiK0SInvMass_%d_%d", indices[0], indices[1]), 800, 800);
    else if (indices.size() == 3) cPhiK0SInvMass = new TCanvas(Form("cPhiK0SInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), Form("cPhiK0SInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), 800, 800);
    cPhiK0SInvMass->cd();
    gPad->SetMargin(0.16,0.03,0.13,0.06);
    gStyle->SetOptStat(0);

    /*RooPlot* frame = x.frame();
    if (indices.size() == 2) {
        frame->SetName(Form("frame_%d_%d", indices[0], indices[1]));
        frame->SetTitle(Form("Mult int, %f - %f (GeV/#it{c}); #it{M}(#pi^{+} + #pi^{#minus}) (GeV/#it{c}^{2}); Counts", pTK0S_axis[indices[1]], pTK0S_axis[indices[1]+1]));
    }
    else if (indices.size() == 3) {
        frame->SetName(Form("frame_%d_%d_%d", indices[0], indices[1], indices[2]));
        frame->SetTitle(Form("%f - %f %%, %f - %f (GeV/#it{c}); #it{M}(#pi^{+} + #pi^{#minus}) (GeV/#it{c}^{2}); Counts", mult_axis[indices[1]], mult_axis[indices[1]+1], pTK0S_axis[indices[2]], pTK0S_axis[indices[2]+1]));
    }
    data.plotOn(frame);
    model.plotOn(frame);
    //model.plotOn(frame, Components(sigsig), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    //model.plotOn(frame, Components(sigbkg), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    //model.plotOn(frame, Components(bkgsig), LineColor(kBlue), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    //model.plotOn(frame, Components(bkgbkg), LineColor(kMagenta), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();*/

    h2PhiK0SInvMass->Draw("LEGO2");
    h2PhiK0SInvMass->SetTitle("; #it{M}(#pi^{+}#pi^{#minus}) (GeV/#it{c}^{2}); #it{M}(K^{+}K^{#minus}) (GeV/#it{c}^{2}); Counts");
    h2PhiK0SInvMass->GetXaxis()->SetTitleOffset(1.7);
    h2PhiK0SInvMass->GetXaxis()->SetLabelSize(0.03);
    h2PhiK0SInvMass->GetYaxis()->SetTitleOffset(2.3);
    h2PhiK0SInvMass->GetYaxis()->SetLabelSize(0.03);
    h2PhiK0SInvMass->GetZaxis()->SetTitleOffset(1.7);
    h2PhiK0SInvMass->GetZaxis()->SetLabelSize(0.03);

    TH2F* hist2D_fit = (TH2F*)model.createHistogram("hist2D_fit", x, Binning(h2PhiK0SInvMass->GetNbinsX()), YVar(y, Binning(h2PhiK0SInvMass->GetNbinsY())));
    hist2D_fit->SetLineColor(kRed);
    hist2D_fit->GetZaxis()->SetTitleOffset(1.5);
    hist2D_fit->Draw("SURF SAME");

    file->cd();
    cPhiK0SInvMass->Write();
    delete cPhiK0SInvMass;

    // Calcola l'integrale della funzione prodotto nel range specificato
    x.setRange("signal", lowfitK0S, upfitK0S);
    RooAbsReal* integralsigsig = sigsig.createIntegral(RooArgSet(x, y), NormSet(x, y), Range("signal"));

    RooProduct sigyield("sigyield", "sigyield", RooArgList(nsigsig, *integralsigsig));
    Double_t PhiK0SYieldpTdiff = sigyield.getVal();
    Double_t errPhiK0SYieldpTdiff = sigyield.getPropagatedError(*result, RooArgSet(x, y));

    return make_pair(PhiK0SYieldpTdiff, errPhiK0SYieldpTdiff);
}

pair<Double_t, Double_t> FitPhiPi(TH2F* h2PhiPiInvMass, vector<Int_t> indices, Int_t isTPCOrTOF, Int_t isDataOrMcReco, TFile* file,
                                  const vector<Double_t>& params = {1., 1., 10., 10., 0., 1., 7., 1.}, 
                                  const vector<Double_t>& lowLimits = {1., 1., 1., 1., -1., 0.001, 3., 0.001}, 
                                  const vector<Double_t>& upLimits = {5., 5., 10., 10., 1.5, 2.5, 10., 5.})
{
    // Definisci le variabili x e y
    RooRealVar x("x", "x", h2PhiPiInvMass->GetXaxis()->GetXmin(), h2PhiPiInvMass->GetXaxis()->GetXmax());
    RooRealVar y("y", "y", h2PhiPiInvMass->GetYaxis()->GetXmin(), h2PhiPiInvMass->GetYaxis()->GetXmax());

    // Converte l'istogramma 2D in un RooDataHist
    RooDataHist data("data", "data", RooArgList(x, y), h2PhiPiInvMass);

    // Definisci i parametri per la Double Sided Crystal Ball e pol1 per l'asse x
    RooRealVar alpha1CB("alpha1CB", "alpha1CB", params.at(0), lowLimits.at(0), upLimits.at(0));
    RooRealVar alpha2CB("alpha2CB", "alpha2CB", params.at(1), lowLimits.at(1), upLimits.at(1));
    RooRealVar n1CB("n1CB", "n1CB", params.at(2), lowLimits.at(2), upLimits.at(2));
    RooRealVar n2CB("n2CB", "n2CB", params.at(3), lowLimits.at(3), upLimits.at(3));
    RooRealVar meanCB("meanCB", "meanCB", params.at(4), lowLimits.at(4), upLimits.at(4));
    RooRealVar sigmaCB("sigmaCB", "sigmaCB", params.at(5), lowLimits.at(5), upLimits.at(5));
    RooCrystalBall dsCrystalBall("dsCrystalBall", "DoubleSidedCrystalBall1", x, meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB);

    RooRealVar meanG1("meanG1", "meanG1", -7., -8., -6.);
    RooRealVar sigmaG1("sigmaG1", "sigmaG1", 0.2, 0.1, 1.);
    RooGaussian gauss1("gauss1", "gauss1", x, meanG1, sigmaG1);

    RooRealVar meanG2("meanG2", "meanG2", params.at(6), lowLimits.at(6), upLimits.at(6));
    RooRealVar sigmaG2("sigmaG2", "sigmaG2", params.at(7), lowLimits.at(7), upLimits.at(7));
    RooGaussian gauss2("gauss2", "gauss2", x, meanG2, sigmaG2);

    RooRealVar meanG3("meanG3", "meanG3", 2., 0., 7.);
    RooRealVar sigmaG3("sigmaG3", "sigmaG3", 1., 0.001, 5.);
    RooGaussian gauss3("gauss3", "gauss3", x, meanG3, sigmaG3);

    RooRealVar meanG4("meanG4", "meanG4", 4., 2., 6.);
    RooRealVar sigmaG4("sigmaG4", "sigmaG4", 0.2, 0.1, 2.);
    RooGaussian gauss4("gauss4", "gauss4", x, meanG4, sigmaG4);

    RooRealVar meanV("meanV", "meanV", 1.02, 0.987, 1.2);
    RooRealVar width("width", "width", 0.00426);
    width.setConstant(true);
    RooRealVar sigmaV("sigmaV", "sigmaV", 0.001, 0.0001, 0.01);
    RooVoigtian voigt("voigt", "Voigtian", y, meanV, width, sigmaV);

    RooRealVar b0("b0", "b0", 5, 0, 10);
    RooRealVar b1("b1", "b1", 7, 0, 50);
    RooRealVar b2("b2", "b2", 2, 0, 50);
    RooGenericPdf bkgVoigt("bkgVoigt", "bkgVoigt", "b0 + b1*y + b2*sqrt(y-0.987)", RooArgList(y, b0, b1, b2));

    RooProdPdf sigsig("sigsig", "sigsig", RooArgList(dsCrystalBall, voigt));
    RooProdPdf sigbkg("sigbkg", "sigbkg", RooArgList(dsCrystalBall, bkgVoigt));
    RooProdPdf missig1sig("missig1sig", "missig1sig", RooArgList(gauss1, voigt));
    RooProdPdf missig1bkg("missig1bkg", "missig1bkg", RooArgList(gauss1, bkgVoigt));
    RooProdPdf missig2sig("missig2sig", "missig2sig", RooArgList(gauss2, voigt));
    RooProdPdf missig2bkg("missig2bkg", "missig2bkg", RooArgList(gauss2, bkgVoigt));
    RooProdPdf missig3sig("missig3sig", "missig3sig", RooArgList(gauss3, voigt));
    RooProdPdf missig3bkg("missig3bkg", "missig3bkg", RooArgList(gauss3, bkgVoigt));
    RooProdPdf missig4sig("missig4sig", "missig4sig", RooArgList(gauss4, voigt));
    RooProdPdf missig4bkg("missig4bkg", "missig4bkg", RooArgList(gauss4, bkgVoigt));

    RooRealVar nsigsig("nsigsig", "nsigsig", 100000, 0, 90000000);
    RooRealVar nsigbkg("nsigbkg", "nsigbkg", 50000, 0, 50000000);
    RooRealVar nmissig1sig("nmissig1sig", "nmissig1sig", 100000, 0, 90000000);
    RooRealVar nmissig1bkg("nmissig1bkg", "nmissig1bkg", 50000, 0, 50000000);
    RooRealVar nmissig2sig("nmissig2sig", "nmissig2sig", 10000, 0, 900000);
    RooRealVar nmissig2bkg("nmissig2bkg", "nmissig2bkg", 5000, 0, 500000);
    RooRealVar nmissig3sig("nmissig3sig", "nmissig3sig", 10000, 0, 900000);
    RooRealVar nmissig3bkg("nmissig3bkg", "nmissig3bkg", 5000, 0, 500000);
    RooRealVar nmissig4sig("nmissig4sig", "nmissig4sig", 10000, 0, 900000);
    RooRealVar nmissig4bkg("nmissig4bkg", "nmissig4bkg", 5000, 0, 500000);

    //RooAddPdf model("model", "model", RooArgList(sig1, sig2), RooArgList(nsig1, nsig2));
    RooAddPdf* model;//("model", "model", RooArgList(dsCrystalBall, gauss), RooArgList(nsig1, nsig2));
    if (isDataOrMcReco == 1) {
        if (isTPCOrTOF == 0) {
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig3sig, sigbkg, missig3bkg), RooArgList(nsigsig, nmissig3sig, nsigbkg, nmissig3bkg));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig3sig, sigbkg, missig3bkg), RooArgList(nsigsig, nmissig3sig, nsigbkg, nmissig3bkg));
            }
        } else if (isTPCOrTOF == 1) {
            if (indices.size() == 2){
                if (indices[1] < 6) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig1sig, sigbkg, missig1bkg), RooArgList(nsigsig, nmissig1sig, nsigbkg, nmissig1bkg));
                    else model = new RooAddPdf("model", "model", RooArgList(sigsig, sigbkg), RooArgList(nsigsig, nsigbkg));
                }
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig1sig, sigbkg, missig1bkg), RooArgList(nsigsig, nmissig1sig, nsigbkg, nmissig1bkg));
                    else model = new RooAddPdf("model", "model", RooArgList(sigsig, sigbkg), RooArgList(nsigsig, nsigbkg));
                }
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                if (indices[0] == 2 && indices[1] == 9 && (indices[2] == 4 || indices[2] == 5)) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
            }
        }
    } else if (isDataOrMcReco == 2) {
        if (isTPCOrTOF == 0) {
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig3sig, sigbkg, missig3bkg), RooArgList(nsigsig, nmissig3sig, nsigbkg, nmissig3bkg));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig3sig, sigbkg, missig3bkg), RooArgList(nsigsig, nmissig3sig, nsigbkg, nmissig3bkg));
            }
        } else if (isTPCOrTOF == 1) {
            if (indices.size() == 2){
                if (indices[1] < 3) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig1sig, sigbkg, missig1bkg), RooArgList(nsigsig, nmissig1sig, nsigbkg, nmissig1bkg));
                    else model = new RooAddPdf("model", "model", RooArgList(sigsig, sigbkg), RooArgList(nsigsig, nsigbkg));
                }
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                if (indices[0] == 2 && indices[1] == 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, missig4sig, sigbkg, missig2bkg, missig4bkg), RooArgList(nsigsig, nmissig2sig, nmissig4sig, nsigbkg, nmissig2bkg, nmissig4bkg));
            } else if (indices.size() == 3) {
                if (indices[2] < 3) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig1sig, sigbkg, missig1bkg), RooArgList(nsigsig, nmissig1sig, nsigbkg, nmissig1bkg));
                    else model = new RooAddPdf("model", "model", RooArgList(sigsig, sigbkg), RooArgList(nsigsig, nsigbkg));
                }
                else model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, sigbkg, missig2bkg), RooArgList(nsigsig, nmissig2sig, nsigbkg, nmissig2bkg));
                if (indices[0] == 0 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, missig4sig, sigbkg, missig2bkg, missig4bkg), RooArgList(nsigsig, nmissig2sig, nmissig4sig, nsigbkg, nmissig2bkg, nmissig4bkg));
                if (indices[0] == 1 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, missig4sig, sigbkg, missig2bkg, missig4bkg), RooArgList(nsigsig, nmissig2sig, nmissig4sig, nsigbkg, nmissig2bkg, nmissig4bkg));
                if (indices[0] == 2 && (indices[1] == 5 || indices[1] == 6 ||
                    indices[1] == 7 || indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(sigsig, missig2sig, missig4sig, sigbkg, missig2bkg, missig4bkg), RooArgList(nsigsig, nmissig2sig, nmissig4sig, nsigbkg, nmissig2bkg, nmissig4bkg));
            }
        }
    }

    // Fitta il modello ai dati
    RooFitResult* result = model->fitTo(data, Optimize(1), Extended(1), Save(1));

    meanCB.Print();
    sigmaCB.Print();
    alpha1CB.Print();
    n1CB.Print();
    alpha2CB.Print();
    n2CB.Print();

    Int_t lowedge = h2PhiPiInvMass->GetXaxis()->FindFixBin(meanCB.getVal() - 3 * sigmaCB.getVal());
    Int_t upedge = h2PhiPiInvMass->GetXaxis()->FindFixBin(meanCB.getVal() + 3 * sigmaCB.getVal());

    Double_t lowfitPi = h2PhiPiInvMass->GetXaxis()->GetBinLowEdge(lowedge);
    Double_t upfitPi = h2PhiPiInvMass->GetXaxis()->GetBinLowEdge(upedge +1);

    TCanvas* cPhiPiInvMass;
    if (indices.size() == 2) cPhiPiInvMass = new TCanvas(Form("cPhiPiInvMass_%d_%d", indices[0], indices[1]), Form("cPhiPiInvMass_%d_%d", indices[0], indices[1]), 800, 800);
    else if (indices.size() == 3) cPhiPiInvMass = new TCanvas(Form("cPhiPiInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), Form("cPhiPiInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), 800, 800);
    cPhiPiInvMass->cd();
    gPad->SetMargin(0.16,0.03,0.13,0.06);
    gStyle->SetOptStat(0);

    /*RooPlot* frame = x.frame();
    frame->SetTitle("; #it{M}(#pi^{#pm} + #pi^{#mp}) (GeV/#it{c}^{2}); Counts");
    if (indices.size() == 2) {
        frame->SetName(Form("frame_%d_%d", indices[0], indices[1]));
        frame->SetTitle(Form("Mult int, %f - %f (GeV/#it{c}); n#sigma(#pi^{#pm}); Counts", pTPi_axis[indices[1]], pTPi_axis[indices[1]+1]));
    }
    else if (indices.size() == 3) {
        frame->SetName(Form("frame_%d_%d_%d", indices[0], indices[1], indices[2]));
        frame->SetTitle(Form("%f - %f %%, %f - %f (GeV/#it{c}); n#sigma(#pi^{#pm}); Counts", mult_axis[indices[1]], mult_axis[indices[1]+1], pTPi_axis[indices[2]], pTPi_axis[indices[2]+1]));
    }
    data.plotOn(frame);
    model->plotOn(frame);
    model->plotOn(frame, Components(dsCrystalBall), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss1), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss2), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss3), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss4), LineColor(kOrange), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();*/      

    h2PhiPiInvMass->Draw("LEGO2");
    h2PhiPiInvMass->SetTitle("; n#sigma(#pi^{#pm}); #it{M}(K^{+}K^{#minus}) (GeV/#it{c}^{2}); Counts");
    h2PhiPiInvMass->GetXaxis()->SetTitleOffset(1.7);
    h2PhiPiInvMass->GetXaxis()->SetLabelSize(0.03);
    h2PhiPiInvMass->GetYaxis()->SetTitleOffset(2.3);
    h2PhiPiInvMass->GetYaxis()->SetLabelSize(0.03);
    h2PhiPiInvMass->GetZaxis()->SetTitleOffset(1.7);
    h2PhiPiInvMass->GetZaxis()->SetLabelSize(0.03);

    TH2F* hist2D_fit = (TH2F*)model->createHistogram("hist2D_fit", x, Binning(h2PhiPiInvMass->GetNbinsX()), YVar(y, Binning(h2PhiPiInvMass->GetNbinsY())));
    hist2D_fit->SetLineColor(kRed);
    hist2D_fit->GetZaxis()->SetTitleOffset(1.5);
    hist2D_fit->Draw("SURF SAME");

    file->cd();
    cPhiPiInvMass->Write();
    delete cPhiPiInvMass;

    // Calcola l'integrale della funzione prodotto nel range specificato
    x.setRange("signal", lowfitPi, upfitPi);
    RooAbsReal* integralsigsig = sigsig.createIntegral(RooArgSet(x, y), NormSet(x, y), Range("signal"));

    RooProduct sigyield("sigyield", "sigyield", RooArgList(nsigsig, *integralsigsig));
    Double_t PhiPiYieldpTdiff = sigyield.getVal();
    Double_t errPhiPiYieldpTdiff = sigyield.getPropagatedError(*result, RooArgSet(x, y));

    return make_pair(PhiPiYieldpTdiff, errPhiPiYieldpTdiff);
}

void PlotFeatures(TGraphAsymmErrors* graph, Style_t markstyle, Color_t markcolor, Size_t marksize, Style_t linestyle, Color_t linecolor, Width_t linewidth, Style_t fillstyle, Color_t fillcolor, Float_t alpha, TMultiGraph* mg) 
{													
    graph->SetMarkerStyle(markstyle);
    graph->SetMarkerColor(markcolor);
    graph->SetMarkerSize(marksize);
    graph->SetLineStyle(linestyle);
    graph->SetLineColor(linecolor);
    graph->SetLineWidth(linewidth);
    graph->SetFillStyle(fillstyle);
    graph->SetFillColorAlpha(fillcolor,alpha);
    mg->Add(graph);
}

array<TCanvas*, nbin_deltay> PlotHistograms(TH1D* h1Yield[nbin_deltay][nbin_mult], TH1D* h1YieldMB[nbin_deltay], string outPath, string name) 
{
    array<TCanvas*, nbin_deltay> cYield;
    TPad* topPad[nbin_deltay]; 
    TPad* bottomPad[nbin_deltay];

    TH1D* h1YieldRatio[nbin_deltay][nbin_mult];

    TLegend* leg1 [nbin_deltay];
    TLegend* leg2 [nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        string cName = "c" + name;
        cYield[i] = new TCanvas(Form(cName.c_str(), i), Form(cName.c_str(), i), 800, 800);
        cYield[i]->cd();
        //gPad->SetMargin(0.16,0.01,0.13,0.06);
        gStyle->SetOptStat(0);

        topPad[i] = new TPad("topPad", "Top Pad", 0, bottomPadHeight, 1, 1);
        topPad[i]->SetBottomMargin(0);
        topPad[i]->SetLogy();
        topPad[i]->Draw();

        bottomPad[i] = new TPad("bottomPad", "Bottom Pad", 0, 0, 1, bottomPadHeight);
        bottomPad[i]->SetTopMargin(0);
        bottomPad[i]->SetBottomMargin(0.3);
        bottomPad[i]->SetLogy();
        bottomPad[i]->Draw();

        topPad[i]->cd();
        
        leg1[i] = new TLegend(0.5, 0.82, 0.8, 0.85);
        leg1[i]->SetHeader("#bf{This work}");
        leg1[i]->SetTextSize(0.05);
        leg1[i]->SetLineWidth(0);

        leg2[i] = new TLegend(0.5, 0.62, 0.8, 0.82);
        leg2[i]->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, %s", yCut[i].c_str()));
        leg2[i]->SetTextSize(0.035);
        leg2[i]->SetLineWidth(0);
        leg2[i]->SetNColumns(2);
        
        for (int j = 0; j < nbin_mult; j++) {
            h1Yield[i][j]->SetMarkerStyle(20);
            h1Yield[i][j]->SetMarkerColor(Colors[j]);
            h1Yield[i][j]->SetMarkerSize(1.5);
            h1Yield[i][j]->SetLineColor(Colors[j]);
            h1Yield[i][j]->SetLineWidth(2);
            h1Yield[i][j]->SetFillStyle(3001);
            h1Yield[i][j]->SetFillColor(Colors[j]);
            h1Yield[i][j]->GetXaxis()->SetLabelOffset(0.5);
            h1Yield[i][j]->GetYaxis()->SetTitleSize(0.045);
            h1Yield[i][j]->GetYaxis()->SetTitleOffset(1.0);
            h1Yield[i][j]->GetYaxis()->SetLabelSize(0.045);
            h1Yield[i][j]->GetYaxis()->SetRangeUser(1e-3, 1e1);
            h1Yield[i][j]->GetYaxis()->SetRangeUser(0.4e-5, 1.3e-1);

            if (j == 0) h1Yield[i][j]->Draw();
            else h1Yield[i][j]->Draw("same");

            leg2[i]->AddEntry(h1Yield[i][j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
        }

        leg1[i]->Draw("same");
        leg2[i]->Draw("same");

        bottomPad[i]->cd();

        for (int j = 0; j < nbin_mult; j++) {
            h1YieldRatio[i][j] = (TH1D*)h1Yield[i][j]->Clone(Form("h1YieldRatio%i_%i", i, j));
            h1YieldRatio[i][j]->Divide(h1YieldMB[i]);
            h1YieldRatio[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
            h1YieldRatio[i][j]->SetMarkerStyle(20);
            h1YieldRatio[i][j]->SetMarkerColor(Colors[j]);
            h1YieldRatio[i][j]->SetMarkerSize(1.5);
            h1YieldRatio[i][j]->SetLineColor(Colors[j]);
            h1YieldRatio[i][j]->SetLineWidth(2);
            h1YieldRatio[i][j]->SetFillStyle(3001);
            h1YieldRatio[i][j]->SetFillColor(Colors[j]);
            h1YieldRatio[i][j]->GetXaxis()->SetLabelOffset(0.03);
            h1YieldRatio[i][j]->GetXaxis()->SetNdivisions(515);
            h1YieldRatio[i][j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
            h1YieldRatio[i][j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
            h1YieldRatio[i][j]->GetXaxis()->SetTitleOffset(1.2);
            h1YieldRatio[i][j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
            h1YieldRatio[i][j]->GetYaxis()->SetTitleOffset(0.45);
            h1YieldRatio[i][j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
            h1YieldRatio[i][j]->GetYaxis()->SetRangeUser(0.5e-1, 1.5e1);

            if (j == 0) h1YieldRatio[i][j]->Draw();
            else h1YieldRatio[i][j]->Draw("same");
        }

        string outName = outPath + name + ".root";
        cYield[i]->SaveAs(Form(outName.c_str(), i));
        outName = outPath + name + ".pdf";
        cYield[i]->SaveAs(Form(outName.c_str(), i));
    }

    return cYield;
}

void fitDataAndMCClosure2D(int mode) 
{
    //**************************************************************************************************************
    // Histograms retrieving and projections depending on the mode: data (old or new) or MC closure test
    //**************************************************************************************************************

    //TFile* file1 = TFile::Open("AnalysisResultsPurMedium.root");
    //TFile* file1 = TFile::Open("AnalysisResultsPurLong.root");
    //TFile* file1 = TFile::Open("AnalysisResultsMediumNoTLV.root");
    //TDirectoryFile* lfphik0shortanalysis1 = (TDirectoryFile*)file1->Get("lf-phik0shortanalysis_id19979");

    //TFile* file1 = TFile::Open("AnalysisResultsPurMediumMod.root");
    //TDirectoryFile* lfphik0shortanalysis1 = (TDirectoryFile*)file1->Get("lf-phik0shortanalysis");

    TFile* file1;
    TDirectoryFile* phik0shortanalysis1;
    TDirectoryFile* PhipurHist;
    TDirectoryFile* eventHist;

    string purPhiHistName;
    array<string, nbin_deltay> purPhiK0SInvMassHistName;
    array<string, nbin_deltay> purPhiPiInvMassHistName;
    string eventHistName;
    string binEventHistName;

    TFile* file2;
    TDirectoryFile* phik0shortanalysis2;
    TDirectoryFile* PhiK0SHist;

    array<string, nbin_deltay> PhiK0SInvMassHistName;

    TDirectoryFile* phik0shortanalysis3;
    TDirectoryFile* PhiPiHist;

    array<string, nbin_deltay> PhiPiInvMassHistName;

    string outPath;
    string outFileName;

    if (mode == 0) {
        file1 = TFile::Open("AnalysisResultsMediumNoTLV.root");
        //file1 = TFile::Open("AnalysisResultsLongNoTLV.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("lf-phik0shortanalysis_id19979");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("PhipurHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("eventHist");

        purPhiHistName = "h2PhipurInvMass";
        purPhiK0SInvMassHistName = {"h3PhipurK0SInvMassInc", "h3PhipurK0SInvMassFCut", "h3PhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3PhipurPiInvMassInc", "h3PhipurPiInvMassFCut", "h3PhipurPiInvMassSCut"};
        eventHistName = "hEventSelection";
        binEventHistName = "With at least a #phi cand";

        //file2 = TFile::Open("AnalysisResultsMediumNoTLV.root");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("lf-phik0shortanalysis_id19977");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("PhiK0SHist");

        PhiK0SInvMassHistName = {"h4PhiK0SSEInc", "h4PhiK0SSEFCut", "h4PhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file1->Get("lf-phik0shortanalysis_id19978");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("PhiPionHist");

        PhiPiInvMassHistName = {"h5PhiPiSEInc", "h5PhiPiSEFCut", "h5PhiPiSESCut"};

        outPath = "ResultsDataOld/";
        outFileName = "ResultsDataOld2D.root";

        /*file1 = TFile::Open("AnalysisResultsMedium.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("lf-phik0shortanalysis_id19979");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("PhipurHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("eventHist");

        purPhiHistName = "h2PhipurInvMass";
        purPhiK0SInvMassHistName = {"h3PhipurK0SInvMassInclusive", "h3PhipurK0SInvMassFirstCut", "h3PhipurK0SInvMassSecondCut"};
        purPhiPiInvMassHistName = {"h3PhipurPiInvMassInclusive", "h3PhipurPiInvMassFirstCut", "h3PhipurPiInvMassSecondCut"};
        eventHistName = "hEventSelection";
        binEventHistName = "With at least a #phi cand";

        file2 = TFile::Open("AnalysisResultsMedium.root");
        phik0shortanalysis2 = (TDirectoryFile*)file2->Get("lf-phik0shortanalysis_id19977");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("PhiK0SHist");

        PhiK0SInvMassHistName = {"h4PhiK0SSEInc", "h4PhiK0SSEFCut", "h4PhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file2->Get("lf-phik0shortanalysis_id19978");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("PhiPionHist");

        PhiPiInvMassHistName = {"h5PhiPiSEInc", "h5PhiPiSEFCut", "h5PhiPiSESCut"};

        outPath = "ResultsDataOld/";
        outFileName = "ResultsDataOld.root";*/
    } else if (mode == 1) {
        //file1 = TFile::Open("AnalysisResultsSmallNewSel.root");
        file1 = TFile::Open("AnalysisResultsMediumNewSel.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id19979");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("dataPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("dataEventHist");

        purPhiHistName = "h2PhipurInvMass";
        purPhiK0SInvMassHistName = {"h3PhipurK0SInvMassInc", "h3PhipurK0SInvMassFCut", "h3PhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3PhipurPiInvMassInc", "h3PhipurPiInvMassFCut", "h3PhipurPiInvMassSCut"};
        eventHistName = "hEventSelection";
        binEventHistName = "With at least a #phi cand";

        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id19977");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("dataPhiK0SHist");

        PhiK0SInvMassHistName = {"h4PhiK0SSEInc", "h4PhiK0SSEFCut", "h4PhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id19978");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("dataPhiPionHist");

        PhiPiInvMassHistName = {"h5PhiPiSEInc", "h5PhiPiSEFCut", "h5PhiPiSESCut"};

        outPath = "ResultsDataNew/";
        outFileName = "ResultsDataNew2D.root";
    } else if (mode == 2) {
        //file1 = TFile::Open("AnalysisResultsMCClosure.root");
        //file1 = TFile::Open("AnalysisResultsMCClosureNewSel.root");
        file1 = TFile::Open("AnalysisResultsMCLHC24f3b.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20119");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("closureMCPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("mcEventHist");

        purPhiHistName = "h2MCPhipurInvMass";
        purPhiK0SInvMassHistName = {"h3MCPhipurK0SInvMassInc", "h3MCPhipurK0SInvMassFCut", "h3MCPhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3MCPhipurPiInvMassInc", "h3MCPhipurPiInvMassFCut", "h3MCPhipurPiInvMassSCut"};
        eventHistName = "hRecMCEventSelection";
        binEventHistName = "With at least a #phi";

        //file2 = TFile::Open("AnalysisResultsMCClosure.root");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20120");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("closureMCPhiK0SHist");

        PhiK0SInvMassHistName = {"h4ClosureMCPhiK0SSEInc", "h4ClosureMCPhiK0SSEFCut", "h4ClosureMCPhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20121");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("closureMCPhiPionHist");

        PhiPiInvMassHistName = {"h5ClosureMCPhiPiSEInc", "h5ClosureMCPhiPiSEFCut", "h5ClosureMCPhiPiSESCut"};

        outPath = "ResultsMCClosure/";
        outFileName = "ResultsMCClosure2D.root";
    } else if (mode == 3) {
        file1 = TFile::Open("AnalysisResultsNewMCClosure.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20119");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("closureMCPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("mcEventHist");

        purPhiHistName = "h2MCPhipurInvMass";
        purPhiK0SInvMassHistName = {"h3MCPhipurK0SInvMassInc", "h3MCPhipurK0SInvMassFCut", "h3MCPhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3MCPhipurPiInvMassInc", "h3MCPhipurPiInvMassFCut", "h3MCPhipurPiInvMassSCut"};
        eventHistName = "hRecMCEventSelection";
        binEventHistName = "With at least a #phi";

        //file2 = TFile::Open("AnalysisResultsMCClosure.root");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id24153");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("closureMCPhiK0SHist");

        PhiK0SInvMassHistName = {"h4ClosureMCPhiK0SSEInc", "h4ClosureMCPhiK0SSEFCut", "h4ClosureMCPhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id24154");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("closureMCPhiPionHist");

        PhiPiInvMassHistName = {"h5ClosureMCPhiPiSEInc", "h5ClosureMCPhiPiSEFCut", "h5ClosureMCPhiPiSESCut"};

        outPath = "ResultsNewMCClosure/";
        outFileName = "ResultsNewMCClosure2D.root";
    } else if (mode == 4) {
        file1 = TFile::Open("AnalysisResultsNewMCClosure.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20119");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("closureMCPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("mcEventHist");

        purPhiHistName = "h2MCPhipurInvMass";
        purPhiK0SInvMassHistName = {"h3MCPhipurK0SInvMassInc", "h3MCPhipurK0SInvMassFCut", "h3MCPhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3MCPhipurPiInvMassInc", "h3MCPhipurPiInvMassFCut", "h3MCPhipurPiInvMassSCut"};
        eventHistName = "hRecMCEventSelection";
        binEventHistName = "With at least a #phi";

        //file2 = TFile::Open("AnalysisResultsMCClosure.root");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20120");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("closureMCPhiK0SHist");

        PhiK0SInvMassHistName = {"h4ClosureMCPhiK0SSEInc", "h4ClosureMCPhiK0SSEFCut", "h4ClosureMCPhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20121");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("closureMCPhiPionHist");

        PhiPiInvMassHistName = {"h5ClosureMCPhiPiSEInc", "h5ClosureMCPhiPiSEFCut", "h5ClosureMCPhiPiSESCut"};

        outPath = "ResultsNewMCClosure/";
        outFileName = "ResultsNewMCClosure22D.root";
    } else return;

    //**************************************************************************************************************

    TH1F* hEventSelection = (TH1F*)eventHist->Get(eventHistName.c_str());
    hEventSelection->SetDirectory(0);
    Int_t binNumber = hEventSelection->GetXaxis()->FindBin(binEventHistName.c_str());
    Double_t nEventsPhi = hEventSelection->GetBinContent(binNumber);

    //**************************************************************************************************************

    TH2F* h2PhipurInvMass = (TH2F*)PhipurHist->Get(purPhiHistName.c_str());
    h2PhipurInvMass->SetDirectory(0);

    TH1F* h1PhipurInvMass[nbin_mult];
    for (int j = 0; j < nbin_mult; j++) {
        h1PhipurInvMass[j] = (TH1F*)h2PhipurInvMass->ProjectionY(Form("Phipur%i",j), j+1, j+1);
    }

    TH1F* h1PhipurInvMassMB = (TH1F*)h2PhipurInvMass->ProjectionY("PhipurMB", 1, nbin_mult);

    //**************************************************************************************************************

    TH3F* h3PhipurK0SInvMass[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h3PhipurK0SInvMass[i] = (TH3F*)PhipurHist->Get(purPhiK0SInvMassHistName[i].c_str());
        h3PhipurK0SInvMass[i]->SetDirectory(0);
    }

    TH1F* h1PhipurK0SInvMass[nbin_deltay][nbin_mult];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhipurK0SInvMass[i][j] = (TH1F*)h3PhipurK0SInvMass[i]->ProjectionZ(Form("PhipurK0S%i%i", i, j), j+1, j+1, 1, nbin_pTK0S);
        }
    }

    TH1F* h1PhipurK0SInvMassMB[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h1PhipurK0SInvMassMB[i] = (TH1F*)h3PhipurK0SInvMass[i]->ProjectionZ(Form("PhipurK0SMB%i", i), 1, nbin_mult, 1, nbin_pTK0S);
    }

    //**************************************************************************************************************

    TH3F* h3PhipurPiInvMass[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h3PhipurPiInvMass[i] = (TH3F*)PhipurHist->Get(purPhiPiInvMassHistName[i].c_str());
        h3PhipurPiInvMass[i]->SetDirectory(0);
    }

    TH1F* h1PhipurPiInvMass[nbin_deltay][nbin_mult];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhipurPiInvMass[i][j] = (TH1F*)h3PhipurPiInvMass[i]->ProjectionZ(Form("PhipurPi%i%i", i, j), j+1, j+1, 1, nbin_pTPi);
        }
    }

    TH1F* h1PhipurPiInvMassMB[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h1PhipurPiInvMassMB[i] = (TH1F*)h3PhipurPiInvMass[i]->ProjectionZ(Form("PhipurPiMB%i", i), 1, nbin_mult, 1, nbin_pTPi);
    }

    //**************************************************************************************************************

    THnSparseF* h4PhiK0SInvMass[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h4PhiK0SInvMass[i] = (THnSparseF*)PhiK0SHist->Get(PhiK0SInvMassHistName[i].c_str());
    }

    TH2F* h2PhiK0SInvMass[nbin_deltay][nbin_mult][nbin_pTK0S];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTK0S; k++) {
                h2PhiK0SInvMass[i][j][k] = Project2D(h4PhiK0SInvMass[i], 0, j+1, j+1, 1, k+1, k+1, 3, 2, "", Form("h2PhiK0SInvMass%i_%i_%i", i, j, k));
            }
        }
    }

    TH2F* h2PhiK0SInvMassMB[nbin_deltay][nbin_pTK0S];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int k = 0; k < nbin_pTK0S; k++) {
            h2PhiK0SInvMassMB[i][k] = Project2D(h4PhiK0SInvMass[i], 0, 1, nbin_mult, 1, k+1, k+1, 3, 2, "", Form("h2PhiK0SInvMassMB%i_%i", i, k));
        }
    }   

    //**************************************************************************************************************

    THnSparseF* h5PhiInvMassPiNSigma[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h5PhiInvMassPiNSigma[i] = (THnSparseF*)PhiPiHist->Get(PhiPiInvMassHistName[i].c_str());
    }

    TH2F* h2PhiInvMassPiNSigmaTPC[nbin_deltay][nbin_mult][nbin_pTPi];
    TH2F* h2PhiInvMassPiNSigmaTOF[nbin_deltay][nbin_mult][nbin_pTPi];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTPi; k++) {
                h2PhiInvMassPiNSigmaTPC[i][j][k] = Project2D(h5PhiInvMassPiNSigma[i], 0, j+1, j+1, 1, k+1, k+1, 4, 2, "", Form("h2PhiInvMassPiNSigmaTPC%i_%i_%i", i, j, k));
                h2PhiInvMassPiNSigmaTOF[i][j][k] = Project2D(h5PhiInvMassPiNSigma[i], 0, j+1, j+1, 1, k+1, k+1, 4, 3, "", Form("h2PhiInvMassPiNSigmaTOF%i_%i_%i", i, j, k));
            }
        }
    }

    TH2F* h2PhiInvMassPiNSigmaTPCMB[nbin_deltay][nbin_pTPi];
    TH2F* h2PhiInvMassPiNSigmaTOFMB[nbin_deltay][nbin_pTPi];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int k = 0; k < nbin_pTPi; k++) {
            h2PhiInvMassPiNSigmaTPCMB[i][k] = Project2D(h5PhiInvMassPiNSigma[i], 0, 1, nbin_mult, 1, k+1, k+1, 4, 2, "", Form("h2PhiInvMassPiNSigmaTPCMB%i_%i", i, k));
            h2PhiInvMassPiNSigmaTOFMB[i][k] = Project2D(h5PhiInvMassPiNSigma[i], 0, 1, nbin_mult, 1, k+1, k+1, 4, 3, "", Form("h2PhiInvMassPiNSigmaTOFMB%i_%i", i, k));
        }
    }

    //**************************************************************************************************************

    PhipurHist->Close();
    eventHist->Close();
    PhiK0SHist->Close();
    PhiPiHist->Close();
    phik0shortanalysis1->Close();
    phik0shortanalysis2->Close();
    phik0shortanalysis3->Close();
    
    //****************************************************************************************************
    // Multiplicity-dependent purities
    //****************************************************************************************************

    Double_t purityVoigt[nbin_mult] = {0}, errpurityVoigt[nbin_mult] = {0};
    Double_t purityVoigtK0S[nbin_deltay][nbin_mult] = {0}, errpurityVoigtK0S[nbin_deltay][nbin_mult] = {0};
    Double_t purityVoigtPi[nbin_deltay][nbin_mult] = {0}, errpurityVoigtPi[nbin_deltay][nbin_mult] = {0};

    for (int j = 0; j < nbin_mult; j++) {
        tie(purityVoigt[j], errpurityVoigt[j]) = GetPhiPurityAndError(h1PhipurInvMass[j], Form("Phipur%i", j), mode-1, 0, {j});
        for (int i = 0; i < nbin_deltay; i++) {
            tie(purityVoigtK0S[i][j], errpurityVoigtK0S[i][j]) = GetPhiPurityAndError(h1PhipurK0SInvMass[i][j], Form("PhipurK0S%i%i", i, j), mode-1, 1, {i, j});
            tie(purityVoigtPi[i][j], errpurityVoigtPi[i][j]) = GetPhiPurityAndError(h1PhipurPiInvMass[i][j], Form("PhipurPi%i%i", i, j), mode-1, 2, {i, j});
        }
    }

    //********************************************************************************************

    Double_t purityPhi[nbin_mult] = {0}, errpurityPhi[nbin_mult] = {0};
    Double_t purityPhiK0S[nbin_deltay][nbin_mult] = {0}, errpurityPhiK0S[nbin_deltay][nbin_mult] = {0};
    Double_t purityPhiPi[nbin_deltay][nbin_mult] = {0}, errpurityPhiPi[nbin_deltay][nbin_mult] = {0};

    for (int j = 0; j < nbin_mult; j++) {
        purityPhi[j] = purityVoigt[j];
        errpurityPhi[j] = errpurityVoigt[j];
        
        for (int i = 0; i < nbin_deltay; i++) {
            purityPhiK0S[i][j] = purityVoigtK0S[i][j];
            errpurityPhiK0S[i][j] = errpurityVoigtK0S[i][j];

            purityPhiPi[i][j] = purityVoigtPi[i][j];
            errpurityPhiPi[i][j] = errpurityVoigtPi[i][j];
        }
    }

    TMultiGraph* mgPhipurK0S = new TMultiGraph();
    TMultiGraph* mgPhipurPi = new TMultiGraph();

    TGraphAsymmErrors* PurityPhi = new TGraphAsymmErrors(nbin_mult, mult, purityPhi, errmult, errmult, errpurityPhi, errpurityPhi);
    PlotFeatures(PurityPhi, Markers[0], kViolet, 1, 1, kViolet, 2, 3001, kViolet, 0.4, mgPhipurK0S);
    PlotFeatures(PurityPhi, Markers[0], kViolet, 1, 1, kViolet, 2, 3001, kViolet, 0.4, mgPhipurPi);

    TGraphAsymmErrors* PurityPhiK0S[nbin_deltay];
    TGraphAsymmErrors* PurityPhiPi[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        PurityPhiK0S[i] = new TGraphAsymmErrors(nbin_mult, mult, purityPhiK0S[i], errmult, errmult, errpurityPhiK0S[i], errpurityPhiK0S[i]);
        PurityPhiPi[i] = new TGraphAsymmErrors(nbin_mult, mult, purityPhiPi[i], errmult, errmult, errpurityPhiPi[i], errpurityPhiPi[i]);
        if (i == 0) {
            PlotFeatures(PurityPhiK0S[i], Markers[0], kBlack, 1, 1, kBlack, 2, 3001, kBlack, 0.4, mgPhipurK0S);
            PlotFeatures(PurityPhiPi[i], Markers[0], kBlack, 1, 1, kBlack, 2, 3001, kBlack, 0.4, mgPhipurPi);
        } else if (i == 1) {
            PlotFeatures(PurityPhiK0S[i], Markers[0], kGreen+3, 1, 1, kGreen+3, 2, 3001, kGreen+3, 0.4, mgPhipurK0S);
            PlotFeatures(PurityPhiPi[i], Markers[0], kGreen+3, 1, 1, kGreen+3, 2, 3001, kGreen+3, 0.4, mgPhipurPi);
        } else if (i == 2) {
            PlotFeatures(PurityPhiK0S[i], Markers[0], kRed+1, 1, 1, kRed+1, 2, 3001, kRed+1, 0.4, mgPhipurK0S);
            PlotFeatures(PurityPhiPi[i], Markers[0], kRed+1, 1, 1, kRed+1, 2, 3001, kRed+1, 0.4, mgPhipurPi);
        }
    }

    //********************************************************************************************

    TCanvas* cPurityPhiK0S = new TCanvas("cPurityPhiK0S", "cPurityPhiK0S", 800, 800);
    cPurityPhiK0S->cd();
    gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);
    mgPhipurK0S->Draw("AP");
    mgPhipurK0S->SetTitle("; #LTd#it{N}_{ch}/d#eta#GT_{|#eta|<0.5} ; S/(S+B)");
    mgPhipurK0S->GetYaxis()->SetTitleOffset(1.4);
    mgPhipurK0S->GetYaxis()->SetTitleSize(0.045);
    mgPhipurK0S->GetYaxis()->SetLabelSize(0.045);
    mgPhipurK0S->GetXaxis()->SetTitleOffset(1.2);
    mgPhipurK0S->GetXaxis()->SetTitleSize(0.045);
    mgPhipurK0S->GetXaxis()->SetLabelSize(0.045);
    //PurityPhiK0S[2]->Draw("AP");

    TLegend* legPurityPhiK0S1 = new TLegend(0.45, 0.85, 0.8, 0.88);
    legPurityPhiK0S1->SetHeader("#bf{This work}");
    legPurityPhiK0S1->SetTextSize(0.05);
    legPurityPhiK0S1->SetLineWidth(0);
    legPurityPhiK0S1->Draw("same");

    TLegend* legPurityPhiK0S2 = new TLegend(0.45, 0.65, 0.8, 0.85);
    legPurityPhiK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legPurityPhiK0S2->AddEntry(PurityPhi, "MB", "p");
    legPurityPhiK0S2->AddEntry(PurityPhiK0S[0], "|#Delta#it{y}| < 1.0 (inclusive)", "p");
    legPurityPhiK0S2->AddEntry(PurityPhiK0S[1], "|#Delta#it{y}| < 0.5", "p");
    legPurityPhiK0S2->AddEntry(PurityPhiK0S[2], "|#Delta#it{y}| < 0.1", "p");
    legPurityPhiK0S2->SetTextSize(0.045);
    legPurityPhiK0S2->SetLineWidth(0);
    legPurityPhiK0S2->Draw("same");

    string outNamePurityPhiK0S = outPath + "purPhiK0S.root";
    cPurityPhiK0S->SaveAs(outNamePurityPhiK0S.c_str());
    outNamePurityPhiK0S = outPath + "purPhiK0S.pdf";
    cPurityPhiK0S->SaveAs(outNamePurityPhiK0S.c_str());

    //********************************************************************************************

    TCanvas* cPurityPhiPi = new TCanvas("cPurityPhiPi", "cPurityPhiPi", 800, 800);
    cPurityPhiPi->cd();
    gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);
    mgPhipurPi->SetTitle("; #LTd#it{N}_{ch}/d#eta#GT_{|#eta|<0.5} ; S/(S+B)");
    mgPhipurPi->Draw("AP");
    mgPhipurPi->GetYaxis()->SetTitleOffset(1.4);
    mgPhipurPi->GetYaxis()->SetTitleSize(0.045);
    mgPhipurPi->GetYaxis()->SetLabelSize(0.045);
    mgPhipurPi->GetXaxis()->SetTitleOffset(1.2);
    mgPhipurPi->GetXaxis()->SetTitleSize(0.045);
    mgPhipurPi->GetXaxis()->SetLabelSize(0.045);

    TLegend* legPurityPhiPi1 = new TLegend(0.45, 0.85, 0.8, 0.88);
    legPurityPhiPi1->SetHeader("#bf{This work}");
    legPurityPhiPi1->SetTextSize(0.05);
    legPurityPhiPi1->SetLineWidth(0);
    legPurityPhiPi1->Draw("same");

    TLegend* legPurityPhiPi2 = new TLegend(0.45, 0.65, 0.8, 0.85);
    legPurityPhiPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legPurityPhiPi2->AddEntry(PurityPhi, "MB", "p");
    legPurityPhiPi2->AddEntry(PurityPhiPi[0], "|#Delta#it{y}| < 1.0 (inclusive)", "p");
    legPurityPhiPi2->AddEntry(PurityPhiPi[1], "|#Delta#it{y}| < 0.5", "p");
    legPurityPhiPi2->AddEntry(PurityPhiPi[2], "|#Delta#it{y}| < 0.1", "p");
    legPurityPhiPi2->SetTextSize(0.045);
    legPurityPhiPi2->SetLineWidth(0);
    legPurityPhiPi2->Draw("same");

    string outNamePurityPhiPi = outPath + "purPhiPi.root";
    cPurityPhiPi->SaveAs(outNamePurityPhiPi.c_str());
    outNamePurityPhiPi = outPath + "purPhiPi.pdf";
    cPurityPhiPi->SaveAs(outNamePurityPhiPi.c_str());
    
    //********************************************************************************************
    // Multiplicity-integrated purities
    //********************************************************************************************

    Double_t purityVoigtMB{0}, errpurityVoigtMB{0};
    Double_t purityVoigtK0SMB[nbin_deltay] = {0}, errpurityVoigtK0SMB[nbin_deltay] = {0};
    Double_t purityVoigtPiMB[nbin_deltay] = {0}, errpurityVoigtPiMB[nbin_deltay] = {0};

    tie(purityVoigtMB, errpurityVoigtMB) = GetPhiPurityAndError(h1PhipurInvMassMB, "PhipurMB", mode-1, 0, {});
    for (int i = 0; i < nbin_deltay; i++) {
        tie(purityVoigtK0SMB[i], errpurityVoigtK0SMB[i]) = GetPhiPurityAndError(h1PhipurK0SInvMassMB[i], Form("PhipurK0SMB%i", i), mode-1, 1, {i});
        tie(purityVoigtPiMB[i], errpurityVoigtPiMB[i]) = GetPhiPurityAndError(h1PhipurPiInvMassMB[i], Form("PhipurPiMB%i", i), mode-1, 2, {i});
    }

    //********************************************************************************************

    Double_t purityPhiMB{0}, errpurityPhiMB{0};
    Double_t purityPhiK0SMB[nbin_deltay] = {0}, errpurityPhiK0SMB[nbin_deltay] = {0};
    Double_t purityPhiPiMB[nbin_deltay] = {0}, errpurityPhiPiMB[nbin_deltay] = {0};

    purityPhiMB = purityVoigtMB;
    errpurityPhiMB = errpurityVoigtMB;
    
    for (int i = 0; i < nbin_deltay; i++) {
        purityPhiK0SMB[i] = purityVoigtK0SMB[i];
        errpurityPhiK0SMB[i] = errpurityVoigtK0SMB[i];

        purityPhiPiMB[i] = purityVoigtPiMB[i];
        errpurityPhiPiMB[i] = errpurityVoigtPiMB[i];
    }

    //return;

    //********************************************************************************************
    // Signal extraction
    //********************************************************************************************

    Double_t PhiK0SYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTK0S] = {0}, errPhiK0SYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTK0S] = {0};
    TH1D* h1PhiK0SYield[nbin_deltay][nbin_mult];

    Double_t PhiK0SYieldpTdiffMB[nbin_deltay][nbin_pTK0S] = {0}, errPhiK0SYieldpTdiffMB[nbin_deltay][nbin_pTK0S] = {0};
    TH1D* h1PhiK0SYieldMB[nbin_deltay];
    
    Double_t PhiPiTPCYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTPi] = {0}, errPhiPiTPCYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTPi] = {0};
    TH1D* h1PhiPiTPCYield[nbin_deltay][nbin_mult];

    Double_t PhiPiTOFYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTPi] = {0}, errPhiPiTOFYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTPi] = {0};
    TH1D* h1PhiPiTOFYield[nbin_deltay][nbin_mult];

    Double_t PhiPiYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTPi] = {0}, errPhiPiYieldpTdiff[nbin_deltay][nbin_mult][nbin_pTPi] = {0};
    TH1D* h1PhiPiYield[nbin_deltay][nbin_mult];

    Double_t PhiPiTPCYieldpTdiffMB[nbin_deltay][nbin_pTPi] = {0}, errPhiPiTPCYieldpTdiffMB[nbin_deltay][nbin_pTPi] = {0};
    TH1D* h1PhiPiTPCYieldMB[nbin_deltay];

    Double_t PhiPiTOFYieldpTdiffMB[nbin_deltay][nbin_pTPi] = {0}, errPhiPiTOFYieldpTdiffMB[nbin_deltay][nbin_pTPi] = {0};
    TH1D* h1PhiPiTOFYieldMB[nbin_deltay];

    Double_t PhiPiYieldpTdiffMB[nbin_deltay][nbin_pTPi] = {0}, errPhiPiYieldpTdiffMB[nbin_deltay][nbin_pTPi] = {0};
    TH1D* h1PhiPiYieldMB[nbin_deltay];

    //********************************************************************************************

    string outNameCanvasK0S = outPath + "fitCanvasK0S2D.root";
    TFile* fileCanvasK0S = new TFile(outNameCanvasK0S.c_str(), "RECREATE");

    string outNameCanvasPiTPC = outPath + "fitCanvasPiTPC2D.root";
    TFile* fileCanvasPiTPC = new TFile(outNameCanvasPiTPC.c_str(), "RECREATE");

    string outNameCanvasPiTOF = outPath + "fitCanvasPiTOF2D.root";
    TFile* fileCanvasPiTOF = new TFile(outNameCanvasPiTOF.c_str(), "RECREATE");

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhiK0SYield[i][j] = new TH1D(Form("h1PhiK0SYield%i_%i", i, j), Form("h1PhiK0SYield%i_%i", i, j), nbin_pTK0S, pTK0S_axis);
            h1PhiK0SYield[i][j]->SetTitle("; ; 1/N_{ev,#phi} d^{2}N_{K^{0}_{S}}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            h1PhiPiTPCYield[i][j] = new TH1D(Form("h1PhiPiTPCYield%i_%i", i, j), Form("h1PhiPiTPCYield%i_%i", i, j), nbin_pTPi, pTPi_axis);
            h1PhiPiTPCYield[i][j]->SetTitle("; ; 1/N_{ev,#phi} d^{2}N_{(#pi^{+}+#pi^{#minus})}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            h1PhiPiTOFYield[i][j] = new TH1D(Form("h1PhiPiTOFYield%i_%i", i, j), Form("h1PhiPiTOFYield%i_%i", i, j), nbin_pTPi, pTPi_axis);
            h1PhiPiTOFYield[i][j]->SetTitle("; ; 1/N_{ev,#phi} d^{2}N_{(#pi^{+}+#pi^{#minus})}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            h1PhiPiYield[i][j] = new TH1D(Form("h1PhiPiYield%i_%i", i, j), Form("h1PhiPiYield%i_%i", i, j), nbin_pTPi, pTPi_axis);
            h1PhiPiYield[i][j]->SetTitle("; ; 1/N_{ev,#phi} d^{2}N_{(#pi^{+}+#pi^{#minus})}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            for (int k = 0; k < nbin_pTK0S; k++) {
                //if (i != 0 || j != 0 || k != 0) continue;
                //if (i != 2 || k != 3) continue;
                //if (i != 0) continue;

                tie(PhiK0SYieldpTdiff[i][j][k], errPhiK0SYieldpTdiff[i][j][k]) = FitPhiK0S(h2PhiK0SInvMass[i][j][k], {i, j, k}, fileCanvasK0S);
                PhiK0SYieldpTdiff[i][j][k] = PhiK0SYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTK0S_axis[k+1] - pTK0S_axis[k]) /*/ purityPhiK0S[i][j]*/ / (nEventsPhi * purityPhi[j]);
                errPhiK0SYieldpTdiff[i][j][k] = errPhiK0SYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTK0S_axis[k+1] - pTK0S_axis[k]) /*/ purityPhiK0S[i][j]*/ / (nEventsPhi * purityPhi[j]);
                if (mode != 3) {
                    PhiK0SYieldpTdiff[i][j][k] = PhiK0SYieldpTdiff[i][j][k] / purityPhiK0S[i][j];
                    errPhiK0SYieldpTdiff[i][j][k] = errPhiK0SYieldpTdiff[i][j][k] / purityPhiK0S[i][j];
                }

                h1PhiK0SYield[i][j]->SetBinContent(k+1, PhiK0SYieldpTdiff[i][j][k]);
                h1PhiK0SYield[i][j]->SetBinError(k+1, errPhiK0SYieldpTdiff[i][j][k]);
            }

            for (int k = 0; k < nbin_pTPi; k++) {
                //if (i != 2 || j != 6 || k != 6) continue;
                //if (i != 0 || j != 0) continue;
                //if (i != 0) continue;

                tie(PhiPiTPCYieldpTdiff[i][j][k], errPhiPiTPCYieldpTdiff[i][j][k]) = FitPhiPi(h2PhiInvMassPiNSigmaTPC[i][j][k], {i, j, k}, 0, mode-2, fileCanvasPiTPC);
                PhiPiTPCYieldpTdiff[i][j][k] = PhiPiTPCYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPi[i][j]*/ / (nEventsPhi * purityPhi[j]);
                errPhiPiTPCYieldpTdiff[i][j][k] = errPhiPiTPCYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPi[i][j]*/ / (nEventsPhi * purityPhi[j]);
                //if (errPhiPiTPCYieldpTdiff[i][j][k] != errPhiPiTPCYieldpTdiff[i][j][k]) errPhiPiTPCYieldpTdiff[i][j][k] = 0.0;
                if (mode != 3) {
                    PhiPiTPCYieldpTdiff[i][j][k] = PhiPiTPCYieldpTdiff[i][j][k] / purityPhiPi[i][j];
                    errPhiPiTPCYieldpTdiff[i][j][k] = errPhiPiTPCYieldpTdiff[i][j][k] / purityPhiPi[i][j];
                }

                h1PhiPiTPCYield[i][j]->SetBinContent(k+1, PhiPiTPCYieldpTdiff[i][j][k]);
                h1PhiPiTPCYield[i][j]->SetBinError(k+1, errPhiPiTPCYieldpTdiff[i][j][k]);
                
                tie(PhiPiTOFYieldpTdiff[i][j][k], errPhiPiTOFYieldpTdiff[i][j][k]) = FitPhiPi(h2PhiInvMassPiNSigmaTOF[i][j][k], {i, j, k}, 1, mode-2, fileCanvasPiTOF);
                PhiPiTOFYieldpTdiff[i][j][k] = PhiPiTOFYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPi[i][j]*/ / (nEventsPhi * purityPhi[j]);
                errPhiPiTOFYieldpTdiff[i][j][k] = errPhiPiTOFYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPi[i][j]*/ / (nEventsPhi * purityPhi[j]);
                //if (errPhiPiTOFYieldpTdiff[i][j][k] != errPhiPiTOFYieldpTdiff[i][j][k]) errPhiPiTOFYieldpTdiff[i][j][k] = 0.0;
                if (mode != 3) {
                    PhiPiTOFYieldpTdiff[i][j][k] = PhiPiTOFYieldpTdiff[i][j][k] / purityPhiPi[i][j];
                    errPhiPiTOFYieldpTdiff[i][j][k] = errPhiPiTOFYieldpTdiff[i][j][k] / purityPhiPi[i][j];
                }

                h1PhiPiTOFYield[i][j]->SetBinContent(k+1, PhiPiTOFYieldpTdiff[i][j][k]);
                h1PhiPiTOFYield[i][j]->SetBinError(k+1, errPhiPiTOFYieldpTdiff[i][j][k]);

                PhiPiYieldpTdiff[i][j][k] = (PhiPiTPCYieldpTdiff[i][j][k] + PhiPiTOFYieldpTdiff[i][j][k]) / 2.0;
                errPhiPiYieldpTdiff[i][j][k] = TMath::Sqrt(TMath::Power(errPhiPiTPCYieldpTdiff[i][j][k], 2) + TMath::Power(errPhiPiTOFYieldpTdiff[i][j][k], 2)) / 2.0;

                h1PhiPiYield[i][j]->SetBinContent(k+1, PhiPiYieldpTdiff[i][j][k]);
                h1PhiPiYield[i][j]->SetBinError(k+1, errPhiPiYieldpTdiff[i][j][k]);
            }
        }
    }

    //********************************************************************************************

    for (int i = 0; i < nbin_deltay; i++) {
        h1PhiK0SYieldMB[i] = new TH1D(Form("h1PhiK0SYieldMB%i", i), Form("h1PhiK0SYieldMB%i", i), nbin_pTK0S, pTK0S_axis);

        h1PhiPiTPCYieldMB[i] = new TH1D(Form("h1PhiPiTPCYieldMB%i", i), Form("h1PhiPiTPCYieldMB%i", i), nbin_pTPi, pTPi_axis);
        h1PhiPiTOFYieldMB[i] = new TH1D(Form("h1PhiPiTOFYieldMB%i", i), Form("h1PhiPiTOFYieldMB%i", i), nbin_pTPi, pTPi_axis);
        h1PhiPiYieldMB[i] = new TH1D(Form("h1PhiPiYieldMB%i", i), Form("h1PhiPiYieldMB%i", i), nbin_pTPi, pTPi_axis);

        for (int k = 0; k < nbin_pTK0S; k++) {

            //if (i != 0 || k != 1) continue;
            //if (i != 2) continue;
            //continue;

            tie(PhiK0SYieldpTdiffMB[i][k], errPhiK0SYieldpTdiffMB[i][k]) = FitPhiK0S(h2PhiK0SInvMassMB[i][k], {i, k}, fileCanvasK0S);
            PhiK0SYieldpTdiffMB[i][k] = PhiK0SYieldpTdiffMB[i][k] / deltay_axis[i] / (pTK0S_axis[k+1] - pTK0S_axis[k]) /*/ purityPhiK0SMB[i]*/ / (nEventsPhi * purityPhiMB);
            errPhiK0SYieldpTdiffMB[i][k] = errPhiK0SYieldpTdiffMB[i][k] / deltay_axis[i] / (pTK0S_axis[k+1] - pTK0S_axis[k]) /*/ purityPhiK0SMB[i]*/ / (nEventsPhi * purityPhiMB);
            if (mode != 3) {
                PhiK0SYieldpTdiffMB[i][k] = PhiK0SYieldpTdiffMB[i][k] / purityPhiK0SMB[i];
                errPhiK0SYieldpTdiffMB[i][k] = errPhiK0SYieldpTdiffMB[i][k] / purityPhiK0SMB[i];
            }

            h1PhiK0SYieldMB[i]->SetBinContent(k+1, PhiK0SYieldpTdiffMB[i][k]);
            h1PhiK0SYieldMB[i]->SetBinError(k+1, errPhiK0SYieldpTdiffMB[i][k]);
        }

        for (int k = 0; k < nbin_pTPi; k++) {
            //if (i != 0 || k != 0) continue;
            //if (i != 0) continue;
            //continue;

            tie(PhiPiTPCYieldpTdiffMB[i][k], errPhiPiTPCYieldpTdiffMB[i][k]) = FitPhiPi(h2PhiInvMassPiNSigmaTPCMB[i][k], {i, k}, 0, mode-2, fileCanvasPiTPC);
            PhiPiTPCYieldpTdiffMB[i][k] = PhiPiTPCYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPiMB[i]*/ / (nEventsPhi * purityPhiMB);
            errPhiPiTPCYieldpTdiffMB[i][k] = errPhiPiTPCYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPiMB[i]*/ / (nEventsPhi * purityPhiMB);
            //if (errPhiPiTPCYieldpTdiffMB[i][k] != errPhiPiTPCYieldpTdiffMB[i][k]) errPhiPiTPCYieldpTdiffMB[i][k] = 0.0;
            if (mode != 3) {
                PhiPiTPCYieldpTdiffMB[i][k] = PhiPiTPCYieldpTdiffMB[i][k] / purityPhiPiMB[i];
                errPhiPiTPCYieldpTdiffMB[i][k] = errPhiPiTPCYieldpTdiffMB[i][k] / purityPhiPiMB[i];
            }

            h1PhiPiTPCYieldMB[i]->SetBinContent(k+1, PhiPiTPCYieldpTdiffMB[i][k]);
            h1PhiPiTPCYieldMB[i]->SetBinError(k+1, errPhiPiTPCYieldpTdiffMB[i][k]);

            tie(PhiPiTOFYieldpTdiffMB[i][k], errPhiPiTOFYieldpTdiffMB[i][k]) = FitPhiPi(h2PhiInvMassPiNSigmaTOFMB[i][k], {i, k}, 1, mode-2, fileCanvasPiTOF);
            PhiPiTOFYieldpTdiffMB[i][k] = PhiPiTOFYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPiMB[i]*/ / (nEventsPhi * purityPhiMB);
            errPhiPiTOFYieldpTdiffMB[i][k] = errPhiPiTOFYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) /*/ purityPhiPiMB[i]*/ / (nEventsPhi * purityPhiMB);
            //if (errPhiPiTOFYieldpTdiffMB[i][k] != errPhiPiTOFYieldpTdiffMB[i][k]) errPhiPiTOFYieldpTdiffMB[i][k] = 0.0;
            if (mode != 3) {
                PhiPiTOFYieldpTdiffMB[i][k] = PhiPiTOFYieldpTdiffMB[i][k] / purityPhiPiMB[i];
                errPhiPiTOFYieldpTdiffMB[i][k] = errPhiPiTOFYieldpTdiffMB[i][k] / purityPhiPiMB[i];
            }

            h1PhiPiTOFYieldMB[i]->SetBinContent(k+1, PhiPiTOFYieldpTdiffMB[i][k]);
            h1PhiPiTOFYieldMB[i]->SetBinError(k+1, errPhiPiTOFYieldpTdiffMB[i][k]);

            PhiPiYieldpTdiffMB[i][k] = (PhiPiTPCYieldpTdiffMB[i][k] + PhiPiTOFYieldpTdiffMB[i][k]) / 2.0;
            errPhiPiYieldpTdiffMB[i][k] = TMath::Sqrt(TMath::Power(errPhiPiTPCYieldpTdiffMB[i][k], 2) + TMath::Power(errPhiPiTOFYieldpTdiffMB[i][k], 2)) / 2.0;

            h1PhiPiYieldMB[i]->SetBinContent(k+1, PhiPiYieldpTdiffMB[i][k]);
            h1PhiPiYieldMB[i]->SetBinError(k+1, errPhiPiYieldpTdiffMB[i][k]);
        }
    }

    //fileCanvasK0S->Close();
    //fileCanvasPiTPC->Close();
    //fileCanvasPiTOF->Close();

    //********************************************************************************************

    array<TCanvas*, nbin_deltay> cPhiK0SYield = PlotHistograms(h1PhiK0SYield, h1PhiK0SYieldMB, outPath, "rawSpectrumK0SDY%i2D");
    array<TCanvas*, nbin_deltay> cPhiPiTPCYield = PlotHistograms(h1PhiPiTPCYield, h1PhiPiTPCYieldMB, outPath, "rawSpectrumPiTPCDY%i2D");
    array<TCanvas*, nbin_deltay> cPhiPiTOFYield = PlotHistograms(h1PhiPiTOFYield, h1PhiPiTOFYieldMB, outPath, "rawSpectrumPiTOFDY%i2D");

    //********************************************************************************************

    Double_t PhiK0SYieldpTint[nbin_deltay][nbin_mult] = {0}, errPhiK0SYieldpTint[nbin_deltay][nbin_mult] = {0};

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTK0S; k++) {
                PhiK0SYieldpTint[i][j] += PhiK0SYieldpTdiff[i][j][k] * (pTK0S_axis[k+1] - pTK0S_axis[k]);
                errPhiK0SYieldpTint[i][j] += TMath::Power(errPhiK0SYieldpTdiff[i][j][k] * (pTK0S_axis[k+1] - pTK0S_axis[k]), 2);
            }
            errPhiK0SYieldpTint[i][j] = TMath::Sqrt(errPhiK0SYieldpTint[i][j]);
        }
    }

    TMultiGraph* mgK0S = new TMultiGraph();
    TGraphAsymmErrors* K0S[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        K0S[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiK0SYieldpTint[i], errmult, errmult, errPhiK0SYieldpTint[i], errPhiK0SYieldpTint[i]);
        if (i == 0) {
            PlotFeatures(K0S[i], 33, kBlack, 2, 1, kBlack, 2, 3001, kBlack, 0.4, mgK0S);
        } else if (i == 1) {
            PlotFeatures(K0S[i], 33, kGreen+3, 2, 1, kGreen+3, 2, 3001, kGreen+3, 0.4, mgK0S);
        } else if (i == 2) {
            PlotFeatures(K0S[i], 33, kRed+1, 2, 1, kRed+1, 2, 3001, kRed+1, 0.4, mgK0S);
        }
    }

    TCanvas* cK0S = new TCanvas("cK0S", "cK0S", 1000, 800);
    cK0S->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    //gPad->SetMargin(0.16,0.01,0.13,0.06)
    mgK0S->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; 1/N_{ev,#phi} dN_{K^{0}_{S}}/d#it{y}");
    mgK0S->Draw("AP");

    TLegend* legK0S1 = new TLegend(0.15, 0.82, 0.35, 0.85);
    legK0S1->SetHeader("#bf{This work}");
    legK0S1->SetTextSize(0.05);
    legK0S1->SetLineWidth(0);
    legK0S1->Draw("same");

    TLegend* legK0S2 = new TLegend(0.15, 0.62, 0.35, 0.82);
    legK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legK0S2->AddEntry(K0S[0], "Inclusive", "p");
    legK0S2->AddEntry(K0S[1], "|#it{#Deltay}| < 0.5", "p");
    legK0S2->AddEntry(K0S[2], "|#it{#Deltay}| < 0.1", "p");
    legK0S2->SetTextSize(0.05);
    legK0S2->SetLineWidth(0);
    legK0S2->Draw("same");

    /*string outNameRawK0SYield = outPath + "rawK0SYield.root";
    cK0S->SaveAs(outNameRawK0SYield.c_str());
    outNameRawK0SYield = outPath + "rawK0SYield.pdf";
    cK0S->SaveAs(outNameRawK0SYield.c_str());*/

    //********************************************************************************************

    Double_t PhiPiYieldpTint[nbin_deltay][nbin_mult] = {0}, errPhiPiYieldpTint[nbin_deltay][nbin_mult] = {0};

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTPi; k++) {
                PhiPiYieldpTint[i][j] += PhiPiYieldpTdiff[i][j][k] * (pTPi_axis[k+1] - pTPi_axis[k]);
                errPhiPiYieldpTint[i][j] += TMath::Power(errPhiPiYieldpTdiff[i][j][k] * (pTPi_axis[k+1] - pTPi_axis[k]), 2);
            }
            errPhiPiYieldpTint[i][j] = TMath::Sqrt(errPhiPiYieldpTint[i][j]);
        }
    }

    TMultiGraph* mgPi = new TMultiGraph();
    TGraphAsymmErrors* Pi[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        Pi[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiPiYieldpTint[i], errmult, errmult, errPhiPiYieldpTint[i], errPhiPiYieldpTint[i]);
        if (i == 0) {
            PlotFeatures(Pi[i], 33, kBlack, 2, 1, kBlack, 2, 3001, kBlack, 0.4, mgPi);
        } else if (i == 1) {
            PlotFeatures(Pi[i], 33, kGreen+3, 2, 1, kGreen+3, 2, 3001, kGreen+3, 0.4, mgPi);
        } else if (i == 2) {
            PlotFeatures(Pi[i], 33, kRed+1, 2, 1, kRed+1, 2, 3001, kRed+1, 0.4, mgPi);
        }
    }

    TCanvas* cPi = new TCanvas("cPi", "cPi", 1000, 800);
    cPi->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    mgPi->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; #frac{1}{N_{ev,#phi}} (#pi^{+}+#pi^{#minus})");
    mgPi->Draw("AP");

    TLegend* legPi1 = new TLegend(0.15, 0.82, 0.35, 0.85);
    legPi1->SetHeader("#bf{This work}");
    legPi1->SetTextSize(0.05);
    legPi1->SetLineWidth(0);
    legPi1->Draw("same");

    TLegend* legPi2 = new TLegend(0.15, 0.62, 0.35, 0.82);
    legPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legPi2->AddEntry(Pi[0], "Inclusive", "p");
    legPi2->AddEntry(Pi[1], "|#it{#Deltay}| < 0.5", "p");
    legPi2->AddEntry(Pi[2], "|#it{#Deltay}| < 0.1", "p");
    legPi2->SetTextSize(0.05);
    legPi2->SetLineWidth(0);
    legPi2->Draw("same");

    /*string outNameRawPiYield = outPath + "rawPiYield.root";
    cPi->SaveAs(outNameRawPiYield.c_str());
    outNameRawPiYield = outPath + "rawPiYield.pdf";
    cPi->SaveAs(outNameRawPiYield.c_str());*/

    //********************************************************************************************

    TFile* outFile = new TFile(outFileName.c_str(), "RECREATE");
    outFile->cd();

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhiK0SYield[i][j]->Write();
            h1PhiPiTPCYield[i][j]->Write();
            h1PhiPiTOFYield[i][j]->Write();
        }
        h1PhiK0SYieldMB[i]->Write();
        h1PhiPiTPCYieldMB[i]->Write();
        h1PhiPiTOFYieldMB[i]->Write();
    }

    outFile->Close();

    return;
}

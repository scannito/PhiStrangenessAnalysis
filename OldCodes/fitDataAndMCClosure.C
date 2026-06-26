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
#include "RooExponential.h"
#include "RooPolynomial.h"
#include "RooFormulaVar.h"
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
const vector<Int_t> ColorsFinal = {kBlack, kBlue, kGreen+3, 797, kRed+1};
const vector<Int_t> FullMarkers  = {20, 21, 33, 34, 29, 41, 47, 43};
const vector<Int_t> EmptyMarkers = {53, 56, 57, 58, 64, 67, 54, 65};
const vector<Int_t> Markers = {20, 21, 33, 34, 29, 41, 47, 43, 53, 56, 57, 58, 64, 67, 54, 65};

constexpr Int_t nbin_deltay = 5, nbin_deltay_red = 3, nbin_mult = 10, nbin_pTK0S = 9, nbin_pTPi = /*9*/ 11, nbin_massPhi = 13, nbin_pTPhi = 7;

//constexpr Double_t deltay_axis[nbin_deltay] = {1.0, 0.5, 0.1};
//constexpr Double_t deltay_axis[nbin_deltay] = {1.0, 0.8, 0.3};
constexpr Double_t deltay_axis[nbin_deltay] = {1.0, 0.8, 0.5, 0.3, 0.1};
constexpr Double_t mult_axis[nbin_mult+1] = {0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
constexpr Double_t pTK0S_axis[nbin_pTK0S+1] = {0.1, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0};
//constexpr Double_t pTPi_axis[nbin_pTPi+1] = {0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};
constexpr Double_t pTPi_axis[nbin_pTPi+1] = {0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};

constexpr Double_t mult[nbin_mult] = {20.3, 17.1, 14.6, 12.9, 11.6, 10.1, 8.5, 7.2, 5.7, 3.8};
constexpr Double_t errmult[nbin_mult] = {0.5, 0.4, 0.3, 0.3, 0.3, 0.2, 0.2, 0.2, 0.1, 0.1};

constexpr Double_t topPadHeight = 0.7; 
constexpr Double_t bottomPadHeight = 1.0 - topPadHeight;
constexpr Double_t scaleFactor = bottomPadHeight / topPadHeight;

const array<string, nbin_deltay> yCut = {"Inclusive", "|#Delta#it{y}| < 0.5", "|#Delta#it{y}| < 0.1"};

constexpr Double_t lowmPhiPur = 1.0095, upmPhiPur = 1.029;

pair<Double_t, Double_t> GetPhiPurityAndError(TH1F* h1PhiInvMass, bool printCanvas = false)
{
    h1PhiInvMass->SetTitle("; #it{M}(K^{+}K^{#minus}) (GeV/#it{c}^{2}); Counts");
    Double_t binsize = h1PhiInvMass->GetXaxis()->GetBinWidth(1);

    TF1* fitVoigtPolyPur = new TF1("fitVoigtPolyPur", VoigtPoly, 0.987, 1.06, 7);
    fitVoigtPolyPur->SetParameter(0, 2);
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
        TCanvas* fitPhiPur = new TCanvas("fitPhiPur", "fitPhiPur", 800, 800);
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

pair<Double_t, Double_t> FitPhiK0S(TH1F* h1PhiK0SInvMass, vector<Int_t> indices, TFile* file,
                                   const vector<Double_t>& params = {1., 1., 5., 5., 0.49, 0.003, -1.}, 
                                   const vector<Double_t>& lowLimits = {1., 1., 1., 1., 0.48, 0.001, -1.7},
                                   const vector<Double_t>& upLimits = {2., 2., 10., 10., 0.5, 0.01, 10})
{
    // Definisci le variabili x e y
    RooRealVar x("x", "x", h1PhiK0SInvMass->GetXaxis()->GetXmin(), h1PhiK0SInvMass->GetXaxis()->GetXmax());

    // Converte l'istogramma 2D in un RooDataHist
    RooDataHist data("data", "data", RooArgList(x), h1PhiK0SInvMass);

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

    //RooProdPdf sig("sig", "sig", RooArgList(dsCrystalBall));
    //RooProdPdf bkg("bkg", "bkg", RooArgList(bkgDSCB));

    RooRealVar nsig("nsig", "nsig", 1000, 0, 1000000);
    RooRealVar nbkg("nbkg", "nbkg", 5000, 0, 250000);

    //RooAddPdf model("model", "model", RooArgList(sig, bkg), RooArgList(nsig, nbkg));
    RooAddPdf model("model", "model", RooArgList(dsCrystalBall, bkgDSCB), RooArgList(nsig, nbkg));

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

    nsig.Print();
    nbkg.Print();

    /*RooArgSet* copy = RooArgSet(model).snapshot(true);
    auto& copiedmodel = static_cast<RooAbsPdf&>((*copy)["model"]);
    RooArgSet* obs = copiedmodel.getObservables(data);
    RooArgSet* pars = copiedmodel.getParameters(*obs);

    TF1* fitfunc = copiedmodel.asTF(*obs, *pars, *obs);*/

    Int_t lowedge = h1PhiK0SInvMass->GetXaxis()->FindFixBin(meanCB.getVal() - 6. * sigmaCB.getVal());
    Int_t upedge = h1PhiK0SInvMass->GetXaxis()->FindFixBin(meanCB.getVal() + 6. * sigmaCB.getVal());

    Double_t lowfitPhiK0S = h1PhiK0SInvMass->GetXaxis()->GetBinLowEdge(lowedge);
    Double_t upfitPhiK0S = h1PhiK0SInvMass->GetXaxis()->GetBinLowEdge(upedge +1);

    TCanvas* cPhiK0SInvMass;
    if (indices.size() == 2) cPhiK0SInvMass = new TCanvas(Form("cPhiK0SInvMass_%d_%d", indices[0], indices[1]), Form("cPhiK0SInvMass_%d_%d", indices[0], indices[1]), 800, 800);
    else if (indices.size() == 3) cPhiK0SInvMass = new TCanvas(Form("cPhiK0SInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), Form("cPhiK0SInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), 800, 800);
    cPhiK0SInvMass->cd();
    gPad->SetMargin(0.16,0.03,0.13,0.06);
    gStyle->SetOptStat(0);

    RooPlot* frame = x.frame();
    if (indices.size() == 2) {
        frame->SetName(Form("frame_%d_%d", indices[0], indices[1]));
        frame->SetTitle(Form("centFT0M #in [0 - 100] (%%), #it{p}_{T} #in [%1.1f - %1.1f] (GeV/#it{c}); #it{M}(#pi^{+} + #pi^{#minus}) (GeV/#it{c}^{2}); Counts", pTK0S_axis[indices[1]], pTK0S_axis[indices[1]+1]));
    }
    else if (indices.size() == 3) {
        frame->SetName(Form("frame_%d_%d_%d", indices[0], indices[1], indices[2]));
        frame->SetTitle(Form("centFT0M #in [%d - %d] (%%), #it{p}_{T} #in [%1.1f - %1.1f] (GeV/#it{c}); #it{M}(#pi^{+} + #pi^{#minus}) (GeV/#it{c}^{2}); Counts", (int)mult_axis[indices[1]], (int)mult_axis[indices[1]+1], pTK0S_axis[indices[2]], pTK0S_axis[indices[2]+1]));
    }
    data.plotOn(frame);
    model.plotOn(frame);
    model.plotOn(frame, Components(dsCrystalBall), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model.plotOn(frame, Components(bkgDSCB), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();

    /*h1PhiK0SInvMass->Draw();
    h1PhiK0SInvMass->SetTitle("; #it{M}(#pi^{+} + #pi^{#minus}) (GeV/#it{c}^{2}); Counts");
    h1PhiK0SInvMass->GetXaxis()->SetTitleOffset(1.7);
    h1PhiK0SInvMass->GetXaxis()->SetLabelSize(0.03);
    h1PhiK0SInvMass->GetYaxis()->SetTitleOffset(2.3);
    h1PhiK0SInvMass->GetYaxis()->SetLabelSize(0.03);
    h1PhiK0SInvMass->GetZaxis()->SetTitleOffset(1.7);
    h1PhiK0SInvMass->GetZaxis()->SetLabelSize(0.03);

    TF1* fitfunc = model.asTF(RooArgSet(x), RooArgSet(meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB, a1, nsig, nbkg), RooArgSet(x));
    fitfunc->SetLineColor(kRed);
    fitfunc->Draw("SAME");*/

    TLine* line1 = new TLine(lowfitPhiK0S, frame->GetMinimum(), lowfitPhiK0S, frame->GetMaximum());
    line1->SetLineColor(kBlack);
    line1->SetLineStyle(kDashed);
    line1->SetLineWidth(2);
    line1->Draw("same");

    TLine* line2 = new TLine(upfitPhiK0S, frame->GetMinimum(), upfitPhiK0S, frame->GetMaximum());
    line2->SetLineColor(kBlack);
    line2->SetLineStyle(kDashed);
    line2->SetLineWidth(2);
    line2->Draw("same");
    
    file->cd();
    cPhiK0SInvMass->Write();
    delete cPhiK0SInvMass;

    // Calcola l'integrale della funzione prodotto nel range specificato
    x.setRange("signal", lowfitPhiK0S, upfitPhiK0S);
    RooAbsReal* integralsig = dsCrystalBall.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));

    RooProduct sigyield("sigyield", "sigyield", RooArgList(nsig, *integralsig));
    Double_t PhiK0SYieldpTdiff = sigyield.getVal();
    Double_t errPhiK0SYieldpTdiff = sigyield.getPropagatedError(*result, RooArgSet(x));

    return make_pair(PhiK0SYieldpTdiff, errPhiK0SYieldpTdiff);
}

pair<Double_t, Double_t> CountPhiK0S(TH1F* h1PhiK0SInvMass)
{
    Double_t PhiK0SYieldpTdiff = 0, errPhiK0SYieldpTdiff = 0;
    for (Int_t x = 0; x < h1PhiK0SInvMass->GetNbinsX(); x++) {
        PhiK0SYieldpTdiff += h1PhiK0SInvMass->GetBinContent(x);
        errPhiK0SYieldpTdiff += TMath::Power(h1PhiK0SInvMass->GetBinError(x), 2);
    }
    errPhiK0SYieldpTdiff = TMath::Sqrt(errPhiK0SYieldpTdiff);

    return make_pair(PhiK0SYieldpTdiff, errPhiK0SYieldpTdiff);
}

pair<Double_t, Double_t> FitPhiPi(TH1F* h1PhiPiInvMass, vector<Int_t> indices, Int_t isTPCOrTOF, Int_t isDataOrMcReco, TFile* file,
                                  const vector<Double_t>& params = {1., 1., 10., 10., 0., 1., 7., 1.}, 
                                  const vector<Double_t>& lowLimits = {1., 1., 1., 1., -1., 0.001, 3., 0.001}, 
                                  const vector<Double_t>& upLimits = {5., 5., 10., 10., 1.5, 2.5, 10., 5.})
{
    // Definisci le variabili x e y
    RooRealVar x("x", "x", h1PhiPiInvMass->GetXaxis()->GetXmin(), h1PhiPiInvMass->GetXaxis()->GetXmax());
    //RooRealVar x("x", "x", -8., 8.);

    // Converte l'istogramma 2D in un RooDataHist
    RooDataHist data("data", "data", RooArgList(x), h1PhiPiInvMass);

    // Definisci i parametri per la Double Sided Crystal Ball e pol1 per l'asse x
    RooRealVar alpha1CB("alpha1CB", "alpha1CB", params.at(0), lowLimits.at(0), upLimits.at(0));
    RooRealVar alpha2CB("alpha2CB", "alpha2CB", params.at(1), lowLimits.at(1), upLimits.at(1));
    RooRealVar n1CB("n1CB", "n1CB", params.at(2), lowLimits.at(2), upLimits.at(2));
    RooRealVar n2CB("n2CB", "n2CB", params.at(3), lowLimits.at(3), upLimits.at(3));
    RooRealVar meanCB("meanCB", "meanCB", params.at(4), lowLimits.at(4), upLimits.at(4));
    RooRealVar sigmaCB("sigmaCB", "sigmaCB", params.at(5), lowLimits.at(5), upLimits.at(5));
    RooCrystalBall dsCrystalBall("dsCrystalBall", "DoubleSidedCrystalBall1", x, meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB);

    RooRealVar alpha1CB2("alpha1CB2", "alpha1CB2", params.at(0), lowLimits.at(0), upLimits.at(0));
    RooRealVar alpha2CB2("alpha2CB2", "alpha2CB2", params.at(1), lowLimits.at(1), upLimits.at(1));
    RooRealVar n1CB2("n1CB2", "n1CB2", params.at(2), lowLimits.at(2), upLimits.at(2));
    RooRealVar n2CB2("n2CB2", "n2CB2", params.at(3), lowLimits.at(3), upLimits.at(3));
    RooRealVar meanCB2("meanC2B", "meanCB2", params.at(6), lowLimits.at(6), upLimits.at(6));
    RooRealVar sigmaCB2("sigmaCB2", "sigmaCB2", params.at(7), lowLimits.at(7), upLimits.at(7));
    RooCrystalBall dsCrystalBall2("dsCrystalBall2", "DoubleSidedCrystalBall2", x, meanCB2, sigmaCB2, alpha1CB2, n1CB2, alpha2CB2, n2CB2);

    RooRealVar meanG1("meanG1", "meanG1", -7., -9., -5.);
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

    RooRealVar meanG5("meanG5", "meanG5", -3., -4., -1.5);
    RooRealVar sigmaG5("sigmaG5", "sigmaG5", 0.2, 0.1, 2.);
    RooGaussian gauss5("gauss5", "gauss5", x, meanG5, sigmaG5);

    RooRealVar meanG6("meanG6", "meanG6", 9.5, 9.5, 11.);
    RooRealVar sigmaG6("sigmaG6", "sigmaG6", 1., 0.8, 3.);
    RooGaussian gauss6("gauss6", "gauss6", x, meanG6, sigmaG6);

    RooRealVar a1("a1", "a1", 1.0, -1.0, 5.0);
    RooPolynomial bkgDSCB("bkgDSCB", "bkgDSCB", x, RooArgList(a1));

    RooRealVar nsig1("nsig1", "nsig1", 100000, 0, 9000000000);
    RooRealVar nsig2("nsig2", "nsig2", 100000, 0, 90000000);
    RooRealVar nsig3("nsig3", "nsig3", 100000, 0, 90000000);

    RooAddPdf* model;
    if (isDataOrMcReco == 0) { // Data
        if (isTPCOrTOF == 0) { // TPC
            if (indices.size() == 2) {
                if (indices[1] < 6) {
                    if (indices[1] == 0) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3), RooArgList(nsig1, nsig2));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) {
                    if (indices[2] == 0) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3), RooArgList(nsig1, nsig2));
            }
        } else if (isTPCOrTOF == 1) { // TOF
            if (indices.size() == 2){
                if (indices[1] < 8) {
                    if (indices[1] < 6) {
                        if (indices[1] == 2) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1, gauss5), RooArgList(nsig1, nsig2, nsig3));
                        else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    }
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                }
                else {
                    //if (indices[1] == 8) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss6), RooArgList(nsig1, nsig2, nsig3));
                    //else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                    if (indices[1] == 10) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, dsCrystalBall2, gauss6), RooArgList(nsig1, nsig2, nsig3));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, dsCrystalBall2), RooArgList(nsig1, nsig2));
                }
            } else if (indices.size() == 3) {
                if (indices[2] < 8) {
                    if (indices[2] < 6) {
                        if (indices[2] == 2) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1, gauss5), RooArgList(nsig1, nsig2, nsig3));
                        else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    }
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                }
                else {
                    //if (indices[2] == 8) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss6), RooArgList(nsig1, nsig2, nsig3));
                    //else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                    if (indices[2] == 10) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, dsCrystalBall2, gauss6), RooArgList(nsig1, nsig2, nsig3));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, dsCrystalBall2), RooArgList(nsig1, nsig2));
                }
            }
        }
    } else if (isDataOrMcReco == 1 || isDataOrMcReco == 2 || isDataOrMcReco == 3 || isDataOrMcReco == 4) { // MC Reco for Closure Test
        if (isTPCOrTOF == 0) { // TPC
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3), RooArgList(nsig1, nsig2));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3), RooArgList(nsig1, nsig2));
            }
        } else if (isTPCOrTOF == 1) { // TOF
            if (indices.size() == 2){
                if (indices[1] < 3) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 2 && indices[1] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
            } else if (indices.size() == 3) {
                if (indices[2] < 3) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 0 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 1 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 2 && (indices[1] == 5 || indices[1] == 6 ||
                    indices[1] == 7 || indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
            }
        }
    }

    /*if (isDataOrMcReco == 1) {
        if (isTPCOrTOF == 0) {
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
            }
        } else if (isTPCOrTOF == 1) {
            if (indices.size() == 2){
                if (indices[1] < 6) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, bkgDSCB), RooArgList(nsig1, nsig2));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, bkgDSCB), RooArgList(nsig1, nsig2));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 2 && indices[1] == 9 && (indices[2] == 4 || indices[2] == 5)) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, bkgDSCB), RooArgList(nsig1, nsig2, nsig3));
            }
        }
    } else if (isDataOrMcReco == 2) {
        if (isTPCOrTOF == 0) {
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3), RooArgList(nsig1, nsig2));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss3), RooArgList(nsig1, nsig2));
            }
        } else if (isTPCOrTOF == 1) {
            if (indices.size() == 2){
                if (indices[1] < 3) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 2 && indices[1] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
            } else if (indices.size() == 3) {
                if (indices[2] < 3) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 0 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 1 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 2 && (indices[1] == 5 || indices[1] == 6 ||
                    indices[1] == 7 || indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(dsCrystalBall, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
            }
        }
    }*/

    // Fitta il modello ai dati
    RooFitResult* result = model->fitTo(data, Optimize(1), Extended(1), Save(1));

    meanCB.Print();
    sigmaCB.Print();
    alpha1CB.Print();
    n1CB.Print();
    alpha2CB.Print();
    n2CB.Print();
    a1.Print();

    nsig1.Print();
    nsig2.Print();
    nsig3.Print();

    Int_t lowedge = h1PhiPiInvMass->GetXaxis()->FindFixBin(meanCB.getVal() - 4. * sigmaCB.getVal());
    Int_t upedge = h1PhiPiInvMass->GetXaxis()->FindFixBin(meanCB.getVal() + 4. * sigmaCB.getVal());

    Double_t lowfitPhiPi = h1PhiPiInvMass->GetXaxis()->GetBinLowEdge(lowedge);
    Double_t upfitPhiPi = h1PhiPiInvMass->GetXaxis()->GetBinLowEdge(upedge +1);

    TCanvas* cPhiPiInvMass;
    if (indices.size() == 2) cPhiPiInvMass = new TCanvas(Form("cPhiPiInvMass_%d_%d", indices[0], indices[1]), Form("cPhiPiInvMass_%d_%d", indices[0], indices[1]), 800, 800);
    else if (indices.size() == 3) cPhiPiInvMass = new TCanvas(Form("cPhiPiInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), Form("cPhiPiInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), 800, 800);
    cPhiPiInvMass->cd();
    gPad->SetMargin(0.16,0.03,0.13,0.06);
    gStyle->SetOptStat(0);

    RooPlot* frame = x.frame();
    if (indices.size() == 2) {
        frame->SetName(Form("frame_%d_%d", indices[0], indices[1]));
        frame->SetTitle(Form("centFT0M #in [0 - 100] (%%), #it{p}_{T} #in [%1.1f - %1.1f] (GeV/#it{c}); n#sigma(#pi^{#pm}); Counts", pTPi_axis[indices[1]], pTPi_axis[indices[1]+1]));
    }
    else if (indices.size() == 3) {
        frame->SetName(Form("frame_%d_%d_%d", indices[0], indices[1], indices[2]));
        frame->SetTitle(Form("centFT0M #in [%d - %d] (%%), #it{p}_{T} #in [%1.1f - %1.1f] (GeV/#it{c}); n#sigma(#pi^{#pm}); Counts", (int)mult_axis[indices[1]], (int)mult_axis[indices[1]+1], pTPi_axis[indices[2]], pTPi_axis[indices[2]+1]));
    }
    data.plotOn(frame);
    model->plotOn(frame);
    model->plotOn(frame, Components(dsCrystalBall), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss1), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss2), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(dsCrystalBall2), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss3), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss4), LineColor(kOrange), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss5), LineColor(kOrange), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss6), LineColor(kOrange), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(bkgDSCB), LineColor(kOrange), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();

    /*h1PhiPiInvMass->Draw();
    h1PhiPiInvMass->SetTitle("; n#sigma(#pi^{#pm}); Counts");
    h1PhiPiInvMass->GetXaxis()->SetTitleOffset(1.7);
    h1PhiPiInvMass->GetXaxis()->SetLabelSize(0.03);
    h1PhiPiInvMass->GetYaxis()->SetTitleOffset(2.3);
    h1PhiPiInvMass->GetYaxis()->SetLabelSize(0.03);
    h1PhiPiInvMass->GetZaxis()->SetTitleOffset(1.7);
    h1PhiPiInvMass->GetZaxis()->SetLabelSize(0.03);

    TF1* fitfunc = model.asTF(RooArgSet(x), RooArgSet(meanCB, sigmaCB, alpha1CB, n1CB, alpha2CB, n2CB, meanG, sigmaG, nsig1, nsig2), RooArgSet(x));
    fitfunc->SetLineColor(kRed);
    fitfunc->Draw("SAME");*/

    TLine* line1 = new TLine(lowfitPhiPi, frame->GetMinimum(), lowfitPhiPi, frame->GetMaximum());
    line1->SetLineColor(kBlack);
    line1->SetLineStyle(kDashed);
    line1->SetLineWidth(2);
    line1->Draw("same");

    TLine* line2 = new TLine(upfitPhiPi, frame->GetMinimum(), upfitPhiPi, frame->GetMaximum());
    line2->SetLineColor(kBlack);
    line2->SetLineStyle(kDashed);
    line2->SetLineWidth(2);
    line2->Draw("same");

    file->cd();
    cPhiPiInvMass->Write();
    delete cPhiPiInvMass;

    // Calcola l'integrale della funzione prodotto nel range specificato
    x.setRange("signal", lowfitPhiPi, upfitPhiPi);
    RooAbsReal* integralsig = dsCrystalBall.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));
    /*if (isTPCOrTOF == 1 && indices.size() == 2){
        if (indices[1] < 6) integralsig = dsCrystalBall.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));
        else integralsig = gauss1.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));
    } else if (isTPCOrTOF == 1 && indices.size() == 3) {
        if (indices[2] < 6) integralsig = dsCrystalBall.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));
        else integralsig = gauss1.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));
    } else if (isTPCOrTOF == 0) integralsig = dsCrystalBall.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));*/

    RooProduct sigyield("sigyield", "sigyield", RooArgList(nsig1, *integralsig));
    Double_t PhiPiYieldpTdiff = sigyield.getVal();
    Double_t errPhiPiYieldpTdiff = sigyield.getPropagatedError(*result, RooArgSet(x));

    return make_pair(PhiPiYieldpTdiff, errPhiPiYieldpTdiff);
}

pair<Double_t, Double_t> CountPhiPi(TH1F* h1PhiPiInvMass)
{
    Double_t PhiPiYieldpTdiff = 0, errPhiPiYieldpTdiff = 0;
    for (Int_t x = 0; x < h1PhiPiInvMass->GetNbinsX(); x++) {
        PhiPiYieldpTdiff += h1PhiPiInvMass->GetBinContent(x);
        errPhiPiYieldpTdiff += TMath::Power(h1PhiPiInvMass->GetBinError(x), 2);
    }
    errPhiPiYieldpTdiff = TMath::Sqrt(errPhiPiYieldpTdiff);

    return make_pair(PhiPiYieldpTdiff, errPhiPiYieldpTdiff);
}

class PiecewiseGaussianExp : public RooAbsPdf {
    public:
        PiecewiseGaussianExp() {}
    
        PiecewiseGaussianExp(const char *name, const char *title, RooAbsReal& _x,  RooAbsReal& _mu, RooAbsReal& _sigma,  RooAbsReal& _tau)
            : RooAbsPdf(name, title),
                x("x", "Observable", this, _x),
                mu("mu", "Mean", this, _mu),
                sigma("sigma", "Sigma", this, _sigma),
                tau("tau", "Tau", this, _tau) {}

        PiecewiseGaussianExp(const PiecewiseGaussianExp& other, const char* name = 0) 
            : RooAbsPdf(other, name),
                x("x", this, other.x),
                mu("mu", this, other.mu),
                sigma("sigma", this, other.sigma),
                tau("tau", this, other.tau) {}
    
        virtual TObject* clone(const char* newname) const override { 
            return new PiecewiseGaussianExp(*this, newname);
        }
    
        inline virtual ~PiecewiseGaussianExp() {}

        Int_t getAnalyticalIntegral(RooArgSet &allVars, RooArgSet &analVars, const char *rangeName = nullptr) const override
        {
            return matchArgs(allVars, analVars, x) ? 1 : 0;
        }

        double analyticalIntegral(Int_t code, const char *rangeName = nullptr) const override
        {
            if (code != 1) return 0;  // Controllo di sicurezza
    
            const double muVal = mu;
            const double sigmaVal = sigma;
            const double tauVal = tau;
            const double threshold = muVal + tauVal;

            // Ottieni i limiti dell'integrazione
            double xMin = x.min(rangeName);
            double xMax = x.max(rangeName);

            // Caso 1: entrambi i limiti sono nella regione gaussiana
            if (xMax <= threshold) {
                double normMin = (xMin - muVal) / (sqrt(2) * sigmaVal);
                double normMax = (xMax - muVal) / (sqrt(2) * sigmaVal);
                return 0.5 * sqrt(M_PI) * sigmaVal * (TMath::Erf(normMax) - TMath::Erf(normMin));
            }

            // Caso 2: entrambi i limiti sono nella regione gaussiana * esponenziale
            if (xMin >= threshold) {
                double constFactor = exp(-0.5 * (tauVal / sigmaVal) * (tauVal / sigmaVal));
                double expMin = exp(-tauVal * (xMin - threshold) / (sigmaVal * sigmaVal));
                double expMax = exp(-tauVal * (xMax - threshold) / (sigmaVal * sigmaVal));
                return (sigmaVal * sigmaVal / tauVal) * constFactor * (expMin - expMax);
            }

            // Caso 3: xMin nella regione gaussiana, xMax nella regione esponenziale
            double normMin = (xMin - muVal) / (sqrt(2) * sigmaVal);
            double normThresh = (threshold - muVal) / (sqrt(2) * sigmaVal);
            double gaussIntegral = 0.5 * sqrt(M_PI) * sigmaVal * (TMath::Erf(normThresh) - TMath::Erf(normMin));

            double constFactor = exp(-0.5 * (tauVal / sigmaVal) * (tauVal / sigmaVal));
            double expMax = exp(-tauVal * (xMax - threshold) / (sigmaVal * sigmaVal));
            double expIntegral = (sigmaVal * sigmaVal / tauVal) * constFactor * (1 - expMax);

            return gaussIntegral + expIntegral;
        }
    
    protected:
        RooRealProxy x;      // Variabile di osservazione
        RooRealProxy mu;     // Media della gaussiana
        RooRealProxy sigma;  // Deviazione standard della gaussiana
        RooRealProxy tau;    // Parametro per la parte esponenziale
    
        double evaluate() const override {
            const double xVal = x;
            const double muVal = mu;
            const double sigmaVal = sigma;
            const double tauVal = tau;
            const double threshold = muVal + tauVal;
            const double newVar1 = (xVal - muVal) / sigmaVal;
            const double constnewVar1 = tauVal / sigmaVal;
            const double newVar2 = (xVal - threshold) / (sigmaVal * sigmaVal);
            
            if (xVal < threshold) {
                return exp(-0.5 * newVar1 * newVar1);  // ritorna solo la Gaussiana
                //return RooFit::Detail::MathFuncs::gaussian(x, mean, sigma);
            } else {
                return exp(-0.5 * constnewVar1 * constnewVar1) * exp(-tauVal * newVar2);  // Gaussiana * Esponenziale
            }
        }
    
    private:
        ClassDefOverride(PiecewiseGaussianExp, 1)
    };

pair<Double_t, Double_t> FitPhiPi2(TH1F* h1PhiPiInvMass, vector<Int_t> indices, Int_t isTPCOrTOF, Int_t isDataOrMcReco, TFile* file,
    const vector<Double_t>& params = {0., 1., 1., 7., 1.}, 
    const vector<Double_t>& lowLimits = {-1., 0.001, 0.01, 3., 0.001}, 
    const vector<Double_t>& upLimits = {1.5, 2.5, 5., 10., 5.})
{
    // Definisci le variabili x e y
    RooRealVar x("x", "x", h1PhiPiInvMass->GetXaxis()->GetXmin(), h1PhiPiInvMass->GetXaxis()->GetXmax());
    //RooRealVar x("x", "x", -8., 8.);

    // Converte l'istogramma 2D in un RooDataHist
    RooDataHist data("data", "data", RooArgList(x), h1PhiPiInvMass);

    // Definisci i parametri per la Double Sided Crystal Ball e pol1 per l'asse x
    RooRealVar mean("mean", "mean", params.at(0), lowLimits.at(0), upLimits.at(0));
    RooRealVar sigma("sigma", "sigma", params.at(1), lowLimits.at(1), upLimits.at(1));
    RooRealVar tau("tau", "tau", params.at(2), lowLimits.at(2), upLimits.at(2));

    // Gaussiana
    RooGaussian gauss("gauss", "Gaussian", x, mean, sigma);

    // Threshold per la separazione delle due regioni
    RooFormulaVar threshold("threshold", "@0 + @1", RooArgList(mean, tau));

    // Termine esponenziale
    RooFormulaVar expoterm("expoterm", "(@0 - @1) / (@2*@2)", RooArgList(x, threshold, sigma));
    RooExponential expo("expo", "Exponential", expoterm, tau);

    //RooFormulaVar expoterm("expoterm", "@3*((@0 - @1) / (@2*@2))", RooArgList(x, threshold, sigma, tau));
    //RooGenericPdf expo("expo", "exp(-@0)", RooArgList(expoterm));

    // Funzione a pezzi con RooGenericPdf
    //RooGenericPdf pcGausExp("pcGausExp", "Piecewise function", "(@0 < @1) * @2 + (@0 >= @1) * @2 * @3", RooArgList(x, threshold, gauss, expo));
    PiecewiseGaussianExp pcGausExp("pcGausExp", "pcGausExp", x, mean, sigma, tau);

    RooRealVar meanG1("meanG1", "meanG1", -7., -8., -6.);
    RooRealVar sigmaG1("sigmaG1", "sigmaG1", 0.2, 0.1, 1.);
    RooGaussian gauss1("gauss1", "gauss1", x, meanG1, sigmaG1);

    RooRealVar meanG2("meanG2", "meanG2", params.at(3), lowLimits.at(3), upLimits.at(3));
    RooRealVar sigmaG2("sigmaG2", "sigmaG2", params.at(4), lowLimits.at(4), upLimits.at(4));
    RooGaussian gauss2("gauss2", "gauss2", x, meanG2, sigmaG2);

    RooRealVar meanG3("meanG3", "meanG3", 2., 0., 7.);
    RooRealVar sigmaG3("sigmaG3", "sigmaG3", 1., 0.001, 5.);
    RooGaussian gauss3("gauss3", "gauss3", x, meanG3, sigmaG3);

    RooRealVar meanG4("meanG4", "meanG4", 4., 2., 6.);
    RooRealVar sigmaG4("sigmaG4", "sigmaG4", 0.2, 0.1, 2.);
    RooGaussian gauss4("gauss4", "gauss4", x, meanG4, sigmaG4);

    //RooProdPdf sig1("sig1", "sig1", RooArgList(dsCrystalBall));
    //RooProdPdf sig2("sig2", "sig2", RooArgList(gauss));

    RooRealVar nsig1("nsig1", "nsig1", 100000, 0, 9000000000);
    RooRealVar nsig2("nsig2", "nsig2", 10000, 0, 900000);
    RooRealVar nsig3("nsig3", "nsig3", 10000, 0, 900000);

    //RooAddPdf model("model", "model", RooArgList(sig1, sig2), RooArgList(nsig1, nsig2));
    RooAddPdf* model;//("model", "model", RooArgList(dsCrystalBall, gauss), RooArgList(nsig1, nsig2));
    if (isDataOrMcReco == 0) {
        if (isTPCOrTOF == 0) {
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss3), RooArgList(nsig1, nsig2));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss3), RooArgList(nsig1, nsig2));
            }
        } else if (isTPCOrTOF == 1) {
            if (indices.size() == 2){
                if (indices[1] < 6) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(pcGausExp), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(pcGausExp), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 2 && indices[1] == 9 && (indices[2] == 4 || indices[2] == 5)) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
            }
        }
    } else if (isDataOrMcReco == 1) {
        if (isTPCOrTOF == 0) {
            if (indices.size() == 2) {
                if (indices[1] < 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss3), RooArgList(nsig1, nsig2));
            } else if (indices.size() == 3) {
                if (indices[2] < 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss3), RooArgList(nsig1, nsig2));
            }
        } else if (isTPCOrTOF == 1) {
            if (indices.size() == 2){
                if (indices[1] < 3) {
                    if (indices[1] == 1) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(pcGausExp), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 2 && indices[1] == 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
            } else if (indices.size() == 3) {
                if (indices[2] < 3) {
                    if (indices[2] == 1) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss1), RooArgList(nsig1, nsig2));
                    else model = new RooAddPdf("model", "model", RooArgList(pcGausExp), RooArgList(nsig1));
                }
                else model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2), RooArgList(nsig1, nsig2));
                if (indices[0] == 0 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 1 && (indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
                if (indices[0] == 2 && (indices[1] == 5 || indices[1] == 6 ||
                    indices[1] == 7 || indices[1] == 8 || indices[1] == 9) && indices[2] == 6) model = new RooAddPdf("model", "model", RooArgList(pcGausExp, gauss2, gauss4), RooArgList(nsig1, nsig2, nsig3));
            }
        }
    }

    // Fitta il modello ai dati
    RooFitResult* result = model->fitTo(data, Optimize(1), Extended(1), Save(1));

    mean.Print();
    sigma.Print();
    tau.Print();

    nsig1.Print();
    nsig2.Print();

    Int_t lowedge = h1PhiPiInvMass->GetXaxis()->FindFixBin(mean.getVal() - 3 * sigma.getVal());
    Int_t upedge = h1PhiPiInvMass->GetXaxis()->FindFixBin(mean.getVal() + 3 * sigma.getVal());

    Double_t lowfitPhiPi = h1PhiPiInvMass->GetXaxis()->GetBinLowEdge(lowedge);
    Double_t upfitPhiPi = h1PhiPiInvMass->GetXaxis()->GetBinLowEdge(upedge +1);

    TCanvas* cPhiPiInvMass;
    if (indices.size() == 2) cPhiPiInvMass = new TCanvas(Form("cPhiPiInvMass_%d_%d", indices[0], indices[1]), Form("cPhiPiInvMass_%d_%d", indices[0], indices[1]), 800, 800);
    else if (indices.size() == 3) cPhiPiInvMass = new TCanvas(Form("cPhiPiInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), Form("cPhiPiInvMass_%d_%d_%d", indices[0], indices[1], indices[2]), 800, 800);
    cPhiPiInvMass->cd();
    gPad->SetMargin(0.16,0.03,0.13,0.06);
    gStyle->SetOptStat(0);

    RooPlot* frame = x.frame();
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
    model->plotOn(frame, Components(pcGausExp), LineColor(kRed), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss1), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss2), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss3), LineColor(kGreen), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    model->plotOn(frame, Components(gauss4), LineColor(kOrange), LineStyle(kSolid), Normalization(1.0, RooAbsReal::RelativeExpected));
    frame->Draw();

    TLine* line1 = new TLine(lowfitPhiPi, frame->GetMinimum(), lowfitPhiPi, frame->GetMaximum());
    line1->SetLineColor(kBlack);
    line1->SetLineStyle(kDashed);
    line1->SetLineWidth(2);
    line1->Draw("same");

    TLine* line2 = new TLine(upfitPhiPi, frame->GetMinimum(), upfitPhiPi, frame->GetMaximum());
    line2->SetLineColor(kBlack);
    line2->SetLineStyle(kDashed);
    line2->SetLineWidth(2);
    line2->Draw("same");

    file->cd();
    cPhiPiInvMass->Write();
    delete cPhiPiInvMass;

    // Calcola l'integrale della funzione prodotto nel range specificato
    x.setRange("signal", lowfitPhiPi, upfitPhiPi);
    RooAbsReal* integralsig = pcGausExp.createIntegral(RooArgSet(x), NormSet(x), Range("signal"));
    RooProduct sigyield("sigyield", "sigyield", RooArgList(nsig1, *integralsig));
    Double_t PhiPiYieldpTdiff = sigyield.getVal();
    Double_t errPhiPiYieldpTdiff = sigyield.getPropagatedError(*result, RooArgSet(x));

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
        leg2[i]->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#it{#Deltay}| < %1.1f", deltay_axis[i]));
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
            //h1Yield[i][j]->GetXaxis()->SetLabelOffset(0.5);
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

void fitDataAndMCClosure(int mode) 
{
    //**************************************************************************************************************
    // Histograms retrieving and projections depending on the mode: data (old or new) or MC closure test
    //**************************************************************************************************************

    TFile* file1;
    TDirectoryFile* phik0shortanalysis;
    TDirectoryFile* phik0shortanalysis2;
    TDirectoryFile* phik0shortanalysis3;

    TDirectoryFile* PhipurHist;
    TDirectoryFile* PhipurHist2;
    TDirectoryFile* eventHist;
    TDirectoryFile* eventHist2;

    string purPhiHistName;
    string purPhiHistName2;
    array<string, nbin_deltay> purPhiK0SInvMassHistName;
    array<string, nbin_deltay> purPhiK0SInvMassHistName2;
    array<string, nbin_deltay> purPhiPiInvMassHistName;
    array<string, nbin_deltay> purPhiPiInvMassHistName2;
    string eventHistName;
    string eventHistName2;
    string binEventHistName;
    string binEventHistName2;
    
    TDirectoryFile* PhiK0SHist;
    TDirectoryFile* PhiK0SHist2;

    array<string, nbin_deltay> PhiK0SInvMassHistName;

    TDirectoryFile* PhiPiHist;
    TDirectoryFile* PhiPiHist2;

    array<string, nbin_deltay> PhiPiInvMassHistName;

    string outPath;
    string outFileName;

    /*if (mode == 0) {
        file1 = TFile::Open("AnalysisResults2024.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("phik0shortanalysis");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("dataPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("dataEventHist");

        purPhiHistName = "h3PhipurInvMass";
        purPhiK0SInvMassHistName = {"h3PhipurK0SInvMassInc", "h3PhipurK0SInvMassFCut", "h3PhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3PhipurPiInvMassInc", "h3PhipurPiInvMassFCut", "h3PhipurPiInvMassSCut"};
        eventHistName = "hEventSelection";
        binEventHistName = "With at least a #phi cand";

        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis1->Get("dataPhiK0SHist");
        PhiK0SInvMassHistName = {"h4PhiK0SSEInc", "h4PhiK0SSEFCut", "h4PhiK0SSESCut"};

        PhiPiHist = (TDirectoryFile*)phik0shortanalysis1->Get("dataPhiPionHist");
        PhiPiInvMassHistName = {"h5PhiPiSEInc", "h5PhiPiSEFCut", "h5PhiPiSESCut"};

        outPath = "ResultsData/";
        outFileName = "ResultsData.root";
        //outPath = "ResultsDataNewClasses/";
        //outFileName = "ResultsDataNewClasses.root";
    } else if (mode == 1) {
        
    } else if (mode == 2) {
        
    } else if (mode == 3) {
        file1 = TFile::Open("AnalysisResultsMCClosure2024.root");
        phik0shortanalysis1 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id20119");
        PhipurHist = (TDirectoryFile*)phik0shortanalysis1->Get("closureMCPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis1->Get("mcEventHist");

        purPhiHistName = "h3MCPhipurInvMass";
        purPhiK0SInvMassHistName = {"h3MCPhipurK0SInvMassInc", "h3MCPhipurK0SInvMassFCut", "h3MCPhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3MCPhipurPiInvMassInc", "h3MCPhipurPiInvMassFCut", "h3MCPhipurPiInvMassSCut"};
        eventHistName = "hRecMCEventSelection";
        binEventHistName = "With at least a #phi cand";

        //file2 = TFile::Open("AnalysisResultsMCClosure.root");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id24153");
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis2->Get("closureMCPhiK0SHist");

        PhiK0SInvMassHistName = {"h4ClosureMCPhiK0SSEInc", "h4ClosureMCPhiK0SSEFCut", "h4ClosureMCPhiK0SSESCut"};

        phik0shortanalysis3 = (TDirectoryFile*)file1->Get("phik0shortanalysis_id24154");
        PhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("closureMCPhiPionHist");

        PhiPiInvMassHistName = {"h5ClosureMCPhiPiSEInc", "h5ClosureMCPhiPiSEFCut", "h5ClosureMCPhiPiSESCut"};

        outPath = "ResultsMCClosurePDGAssocOnly/";
        outFileName = "ResultsMCClosurePDGAssocOnly.root";
    } else return;*/

    if (mode == 0) {
        //file1 = TFile::Open("AnalysisResultsData.root");
        file1 = TFile::Open("AnalysisResultsData2.root");
        phik0shortanalysis = (TDirectoryFile*)file1->Get("phik0shortanalysis_id25399");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_newDeltaYClasses_id25399");
        
        PhipurHist = (TDirectoryFile*)phik0shortanalysis->Get("dataPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis->Get("dataEventHist");

        purPhiHistName = "h3PhipurInvMass";
        purPhiK0SInvMassHistName = {"h3PhipurK0SInvMassInc", "h3PhipurK0SInvMassFCut", "h3PhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3PhipurPiInvMassInc", "h3PhipurPiInvMassFCut", "h3PhipurPiInvMassSCut"};
        eventHistName = "hEventSelection";
        binEventHistName = "With at least a #phi cand";

        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis->Get("dataPhiK0SHist");
        PhiK0SHist2 = (TDirectoryFile*)phik0shortanalysis2->Get("dataPhiK0SHist");
        PhiK0SInvMassHistName = {"h4PhiK0SSEInc", "h4PhiK0SSEFCut", "h4PhiK0SSESCut"};

        PhiPiHist = (TDirectoryFile*)phik0shortanalysis->Get("dataPhiPionHist");
        PhiPiHist2 = (TDirectoryFile*)phik0shortanalysis2->Get("dataPhiPionHist");
        PhiPiInvMassHistName = {"h5PhiPiSEInc", "h5PhiPiSEFCut", "h5PhiPiSESCut"};

        //outPath = "ResultsData/";
        //outFileName = "ResultsData.root";
        outPath = "ResultsData2/";
        outFileName = "ResultsData2.root";
    } else if (mode == 1) {
        outPath = "ResultsMCClosureNoPDG/";
        outFileName = "ResultsMCClosureNoPDG.root";
    } else if (mode == 2) {
        outPath = "ResultsMCClosurePDGK0SNoPi/";
        outFileName = "ResultsMCClosurePDGK0SNoPi.root";
    } else if (mode == 3) {
        outPath = "ResultsMCClosurePDGPiNoK0S/";
        outFileName = "ResultsMCClosurePDGPiNoK0S.root";
    } else if (mode == 4) {
        //file1 = TFile::Open("AnalysisResultsMC.root");
        file1 = TFile::Open("AnalysisResultsMC3.root");
        phik0shortanalysis = (TDirectoryFile*)file1->Get("phik0shortanalysis_id26339");
        phik0shortanalysis2 = (TDirectoryFile*)file1->Get("phik0shortanalysis_newDeltaYClasses_id26339");

        PhipurHist = (TDirectoryFile*)phik0shortanalysis->Get("closureMCPhiHist");
        eventHist = (TDirectoryFile*)phik0shortanalysis->Get("mcEventHist");

        purPhiHistName = "h3MCPhipurInvMass";
        purPhiK0SInvMassHistName = {"h3MCPhipurK0SInvMassInc", "h3MCPhipurK0SInvMassFCut", "h3MCPhipurK0SInvMassSCut"};
        purPhiPiInvMassHistName = {"h3MCPhipurPiInvMassInc", "h3MCPhipurPiInvMassFCut", "h3MCPhipurPiInvMassSCut"};
        eventHistName = "hRecMCEventSelection";
        binEventHistName = "With at least a #phi cand";
        
        PhiK0SHist = (TDirectoryFile*)phik0shortanalysis->Get("closureMCPhiK0SHist");
        PhiK0SHist2 = (TDirectoryFile*)phik0shortanalysis2->Get("closureMCPhiK0SHist");
        PhiK0SInvMassHistName = {"h4ClosureMCPhiK0SSEInc", "h4ClosureMCPhiK0SSEFCut", "h4ClosureMCPhiK0SSESCut"};

        PhiPiHist = (TDirectoryFile*)phik0shortanalysis->Get("closureMCPhiPionHist");
        PhiPiHist2 = (TDirectoryFile*)phik0shortanalysis2->Get("closureMCPhiPionHist");
        PhiPiInvMassHistName = {"h5ClosureMCPhiPiSEInc", "h5ClosureMCPhiPiSEFCut", "h5ClosureMCPhiPiSESCut"};

        //outPath = "ResultsMCClosurePDGK0SPi/";
        //outFileName = "ResultsMCClosurePDGK0SPi.root";
        outPath = "ResultsMCClosurePDGK0SPi2/";
        outFileName = "ResultsMCClosurePDGK0SPi2.root";
    } else return;

    //**************************************************************************************************************

    TH1F* hEventSelection = (TH1F*)eventHist->Get(eventHistName.c_str());
    hEventSelection->SetDirectory(0);
    Int_t binNumber = hEventSelection->GetXaxis()->FindBin(binEventHistName.c_str());
    Double_t nEventsPhi = hEventSelection->GetBinContent(binNumber);

    //**************************************************************************************************************

    TH3F* h3PhipurInvMass = (TH3F*)PhipurHist->Get(purPhiHistName.c_str());
    h3PhipurInvMass->SetDirectory(0);

    TH1F* h1PhipurInvMass[nbin_mult];
    for (int j = 0; j < nbin_mult; j++) {
        h1PhipurInvMass[j] = (TH1F*)h3PhipurInvMass->ProjectionZ(Form("Phipur%i",j), j+1, j+1, 1, nbin_pTPhi);
    }

    TH1F* h1PhipurInvMassMB = (TH1F*)h3PhipurInvMass->ProjectionZ("PhipurMB", 1, nbin_mult, 1, nbin_pTPhi);

    //**************************************************************************************************************

    TH3F* h3PhipurK0SInvMass[nbin_deltay_red];
    for (int i = 0; i < nbin_deltay_red; i++) {
        h3PhipurK0SInvMass[i] = (TH3F*)PhipurHist->Get(purPhiK0SInvMassHistName[i].c_str());
        h3PhipurK0SInvMass[i]->SetDirectory(0);
    }

    TH1F* h1PhipurK0SInvMass[nbin_deltay_red][nbin_mult];
    for (int i = 0; i < nbin_deltay_red; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhipurK0SInvMass[i][j] = (TH1F*)h3PhipurK0SInvMass[i]->ProjectionZ(Form("PhipurK0S%i%i", i, j), j+1, j+1, 1, nbin_pTPhi);
        }
    }

    TH1F* h1PhipurK0SInvMassMB[nbin_deltay_red];
    for (int i = 0; i < nbin_deltay_red; i++) {
        h1PhipurK0SInvMassMB[i] = (TH1F*)h3PhipurK0SInvMass[i]->ProjectionZ(Form("PhipurK0SMB%i", i), 1, nbin_mult, 1, nbin_pTPhi);
    }

    //**************************************************************************************************************

    TH3F* h3PhipurPiInvMass[nbin_deltay_red];
    for (int i = 0; i < nbin_deltay_red; i++) {
        h3PhipurPiInvMass[i] = (TH3F*)PhipurHist->Get(purPhiPiInvMassHistName[i].c_str());
        h3PhipurPiInvMass[i]->SetDirectory(0);
    }

    TH1F* h1PhipurPiInvMass[nbin_deltay_red][nbin_mult];
    for (int i = 0; i < nbin_deltay_red; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhipurPiInvMass[i][j] = (TH1F*)h3PhipurPiInvMass[i]->ProjectionZ(Form("PhipurPi%i%i", i, j), j+1, j+1, 1, nbin_pTPhi);
        }
    }

    TH1F* h1PhipurPiInvMassMB[nbin_deltay_red];
    for (int i = 0; i < nbin_deltay_red; i++) {
        h1PhipurPiInvMassMB[i] = (TH1F*)h3PhipurPiInvMass[i]->ProjectionZ(Form("PhipurPiMB%i", i), 1, nbin_mult, 1, nbin_pTPhi);
    }

    //**************************************************************************************************************

    THnSparseF* h4PhiK0SInvMass[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h4PhiK0SInvMass[i] = (THnSparseF*)PhiK0SHist->Get(PhiK0SInvMassHistName[i].c_str());
    }*/
    h4PhiK0SInvMass[0] = (THnSparseF*)PhiK0SHist->Get(PhiK0SInvMassHistName[0].c_str());
    h4PhiK0SInvMass[1] = (THnSparseF*)PhiK0SHist2->Get(PhiK0SInvMassHistName[1].c_str());
    h4PhiK0SInvMass[2] = (THnSparseF*)PhiK0SHist->Get(PhiK0SInvMassHistName[1].c_str());
    h4PhiK0SInvMass[3] = (THnSparseF*)PhiK0SHist2->Get(PhiK0SInvMassHistName[2].c_str());
    h4PhiK0SInvMass[4] = (THnSparseF*)PhiK0SHist->Get(PhiK0SInvMassHistName[2].c_str());

    TH1F* h1PhiK0SInvMass[nbin_deltay][nbin_mult][nbin_pTK0S];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTK0S; k++) {
                h1PhiK0SInvMass[i][j][k] = Project1D(h4PhiK0SInvMass[i], 0, j+1, j+1, 1, k+1, k+1, 3, 1, nbin_massPhi, 2, "", Form("h1PhiK0SInvMass%i_%i_%i", i, j, k));
            }
        }
    }

    TH1F* h1PhiK0SInvMassMB[nbin_deltay][nbin_pTK0S];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int k = 0; k < nbin_pTK0S; k++) {
            h1PhiK0SInvMassMB[i][k] = Project1D(h4PhiK0SInvMass[i], 0, 1, nbin_mult, 1, k+1, k+1, 3, 1, nbin_massPhi, 2, "", Form("h1PhiK0SInvMassMB%i_%i", i, k));
        }
    }   

    //**************************************************************************************************************

    THnSparseF* h5PhiInvMassPiNSigma[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h5PhiInvMassPiNSigma[i] = (THnSparseF*)PhiPiHist->Get(PhiPiInvMassHistName[i].c_str());
    }*/
    h5PhiInvMassPiNSigma[0] = (THnSparseF*)PhiPiHist->Get(PhiPiInvMassHistName[0].c_str());
    h5PhiInvMassPiNSigma[1] = (THnSparseF*)PhiPiHist2->Get(PhiPiInvMassHistName[1].c_str());
    h5PhiInvMassPiNSigma[2] = (THnSparseF*)PhiPiHist->Get(PhiPiInvMassHistName[1].c_str());
    h5PhiInvMassPiNSigma[3] = (THnSparseF*)PhiPiHist2->Get(PhiPiInvMassHistName[2].c_str());
    h5PhiInvMassPiNSigma[4] = (THnSparseF*)PhiPiHist->Get(PhiPiInvMassHistName[2].c_str());

    TH1F* h1PhiInvMassPiNSigmaTPC[nbin_deltay][nbin_mult][nbin_pTPi];
    TH1F* h1PhiInvMassPiNSigmaTOF[nbin_deltay][nbin_mult][nbin_pTPi];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTPi; k++) {
                h1PhiInvMassPiNSigmaTPC[i][j][k] = Project1D(h5PhiInvMassPiNSigma[i], 0, j+1, j+1, 1, k+1, k+1, 4, 1, nbin_massPhi, 2, "", Form("h1PhiInvMassPiNSigmaTPC%i_%i_%i", i, j, k));
                h1PhiInvMassPiNSigmaTOF[i][j][k] = Project1D(h5PhiInvMassPiNSigma[i], 0, j+1, j+1, 1, k+1, k+1, 4, 1, nbin_massPhi, 3, "", Form("h1PhiInvMassPiNSigmaTOF%i_%i_%i", i, j, k));
            }
        }
    }

    TH1F* h1PhiInvMassPiNSigmaTPCMB[nbin_deltay][nbin_pTPi];
    TH1F* h1PhiInvMassPiNSigmaTOFMB[nbin_deltay][nbin_pTPi];
    for (int i = 0; i < nbin_deltay; i++) {
        for (int k = 0; k < nbin_pTPi; k++) {
            h1PhiInvMassPiNSigmaTPCMB[i][k] = Project1D(h5PhiInvMassPiNSigma[i], 0, 1, nbin_mult, 1, k+1, k+1, 4, 1, nbin_massPhi, 2, "", Form("h1PhiInvMassPiNSigmaTPCMB%i_%i", i, k));
            h1PhiInvMassPiNSigmaTOFMB[i][k] = Project1D(h5PhiInvMassPiNSigma[i], 0, 1, nbin_mult, 1, k+1, k+1, 4, 1, nbin_massPhi, 3, "", Form("h1PhiInvMassPiNSigmaTOFMB%i_%i", i, k));
        }
    }

    //**************************************************************************************************************

    PhipurHist->Close();
    eventHist->Close();
    PhiK0SHist->Close();
    PhiK0SHist2->Close();
    PhiPiHist->Close();
    PhiPiHist2->Close();
    phik0shortanalysis->Close();
    phik0shortanalysis2->Close();
    //phik0shortanalysis3->Close();
    
    //****************************************************************************************************
    // Multiplicity-dependent purities
    //****************************************************************************************************

    Double_t purityVoigt[nbin_mult] = {0}, errpurityVoigt[nbin_mult] = {0};
    Double_t purityVoigtK0S[nbin_deltay_red][nbin_mult] = {0}, errpurityVoigtK0S[nbin_deltay_red][nbin_mult] = {0};
    Double_t purityVoigtPi[nbin_deltay_red][nbin_mult] = {0}, errpurityVoigtPi[nbin_deltay_red][nbin_mult] = {0};

    for (int j = 0; j < nbin_mult; j++) {
        tie(purityVoigt[j], errpurityVoigt[j]) = GetPhiPurityAndError(h1PhipurInvMass[j]);
        for (int i = 0; i < nbin_deltay_red; i++) {
            tie(purityVoigtK0S[i][j], errpurityVoigtK0S[i][j]) = GetPhiPurityAndError(h1PhipurK0SInvMass[i][j]);
            tie(purityVoigtPi[i][j], errpurityVoigtPi[i][j]) = GetPhiPurityAndError(h1PhipurPiInvMass[i][j]);
        }
    }

    //********************************************************************************************

    Double_t purityPhi[nbin_mult] = {0}, errpurityPhi[nbin_mult] = {0};
    Double_t purityPhiK0S[nbin_deltay_red][nbin_mult] = {0}, errpurityPhiK0S[nbin_deltay_red][nbin_mult] = {0};
    Double_t purityPhiPi[nbin_deltay_red][nbin_mult] = {0}, errpurityPhiPi[nbin_deltay_red][nbin_mult] = {0};

    for (int j = 0; j < nbin_mult; j++) {
        purityPhi[j] = purityVoigt[j];
        errpurityPhi[j] = errpurityVoigt[j];
        
        for (int i = 0; i < nbin_deltay_red; i++) {
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

    TGraphAsymmErrors* PurityPhiK0S[nbin_deltay_red];
    TGraphAsymmErrors* PurityPhiPi[nbin_deltay_red];
    for (int i = 0; i < nbin_deltay_red; i++) {
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
    Double_t purityVoigtK0SMB[nbin_deltay_red] = {0}, errpurityVoigtK0SMB[nbin_deltay_red] = {0};
    Double_t purityVoigtPiMB[nbin_deltay_red] = {0}, errpurityVoigtPiMB[nbin_deltay_red] = {0};

    tie(purityVoigtMB, errpurityVoigtMB) = GetPhiPurityAndError(h1PhipurInvMassMB);
    for (int i = 0; i < nbin_deltay_red; i++) {
        tie(purityVoigtK0SMB[i], errpurityVoigtK0SMB[i]) = GetPhiPurityAndError(h1PhipurK0SInvMassMB[i]);
        tie(purityVoigtPiMB[i], errpurityVoigtPiMB[i]) = GetPhiPurityAndError(h1PhipurPiInvMassMB[i]);
    }

    //********************************************************************************************

    Double_t purityPhiMB{0}, errpurityPhiMB{0};
    Double_t purityPhiK0SMB[nbin_deltay_red] = {0}, errpurityPhiK0SMB[nbin_deltay_red] = {0};
    Double_t purityPhiPiMB[nbin_deltay_red] = {0}, errpurityPhiPiMB[nbin_deltay_red] = {0};

    purityPhiMB = purityVoigtMB;
    errpurityPhiMB = errpurityVoigtMB;
    
    for (int i = 0; i < nbin_deltay_red; i++) {
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

    string outNameCanvasK0S = outPath + "fitCanvasK0S.root";
    TFile* fileCanvasK0S = new TFile(outNameCanvasK0S.c_str(), "RECREATE");

    string outNameCanvasPiTPC = outPath + "fitCanvasPiTPC.root";
    TFile* fileCanvasPiTPC = new TFile(outNameCanvasPiTPC.c_str(), "RECREATE");

    string outNameCanvasPiTOF = outPath + "fitCanvasPiTOF.root";
    TFile* fileCanvasPiTOF = new TFile(outNameCanvasPiTOF.c_str(), "RECREATE");

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1PhiK0SYield[i][j] = new TH1D(Form("h1PhiK0SYield%i_%i", i, j), Form("h1PhiK0SYield%i_%i", i, j), nbin_pTK0S, pTK0S_axis);
            h1PhiK0SYield[i][j]->SetTitle("; it{p}_{T} (GeV/#it{c}); 1/N_{ev,#phi} d^{2}N_{K^{0}_{S}}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            h1PhiPiTPCYield[i][j] = new TH1D(Form("h1PhiPiTPCYield%i_%i", i, j), Form("h1PhiPiTPCYield%i_%i", i, j), nbin_pTPi, pTPi_axis);
            h1PhiPiTPCYield[i][j]->SetTitle("; it{p}_{T} (GeV/#it{c}); 1/N_{ev,#phi} d^{2}N_{(#pi^{+}+#pi^{#minus})}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            h1PhiPiTOFYield[i][j] = new TH1D(Form("h1PhiPiTOFYield%i_%i", i, j), Form("h1PhiPiTOFYield%i_%i", i, j), nbin_pTPi, pTPi_axis);
            h1PhiPiTOFYield[i][j]->SetTitle("; it{p}_{T} (GeV/#it{c}); 1/N_{ev,#phi} d^{2}N_{(#pi^{+}+#pi^{#minus})}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            h1PhiPiYield[i][j] = new TH1D(Form("h1PhiPiYield%i_%i", i, j), Form("h1PhiPiYield%i_%i", i, j), nbin_pTPi, pTPi_axis);
            h1PhiPiYield[i][j]->SetTitle("; it{p}_{T} (GeV/#it{c}); 1/N_{ev,#phi} d^{2}N_{(#pi^{+}+#pi^{#minus})}/d#it{y}d#it{p}_{T} [(GeV/#it{c})^{-1}]");

            for (int k = 0; k < nbin_pTK0S; k++) {
                //if (i != 0 || j != 7 || k != 6) continue;
                //if (i != 2 || k != 3) continue;
                //if (i != 0) continue;
                //continue;

                tie(PhiK0SYieldpTdiff[i][j][k], errPhiK0SYieldpTdiff[i][j][k]) = FitPhiK0S(h1PhiK0SInvMass[i][j][k], {i, j, k}, fileCanvasK0S);
                PhiK0SYieldpTdiff[i][j][k] = PhiK0SYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTK0S_axis[k+1] - pTK0S_axis[k]) / (nEventsPhi * purityPhi[j]);
                errPhiK0SYieldpTdiff[i][j][k] = errPhiK0SYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTK0S_axis[k+1] - pTK0S_axis[k]) / (nEventsPhi * purityPhi[j]);

                h1PhiK0SYield[i][j]->SetBinContent(k+1, PhiK0SYieldpTdiff[i][j][k]);
                h1PhiK0SYield[i][j]->SetBinError(k+1, errPhiK0SYieldpTdiff[i][j][k]);
            }

            for (int k = 0; k < nbin_pTPi; k++) {
                //if (i != 0 || j != 0 || k != 0) continue;
                //if (i != 0 || j != 0) continue;
                //if (i != 0) continue;

                tie(PhiPiTPCYieldpTdiff[i][j][k], errPhiPiTPCYieldpTdiff[i][j][k]) = FitPhiPi(h1PhiInvMassPiNSigmaTPC[i][j][k], {i, j, k}, 0, mode, fileCanvasPiTPC);
                PhiPiTPCYieldpTdiff[i][j][k] = PhiPiTPCYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhi[j]);
                errPhiPiTPCYieldpTdiff[i][j][k] = errPhiPiTPCYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhi[j]);
                //if (errPhiPiTPCYieldpTdiff[i][j][k] != errPhiPiTPCYieldpTdiff[i][j][k]) errPhiPiTPCYieldpTdiff[i][j][k] = 0.0;

                h1PhiPiTPCYield[i][j]->SetBinContent(k+1, PhiPiTPCYieldpTdiff[i][j][k]);
                h1PhiPiTPCYield[i][j]->SetBinError(k+1, errPhiPiTPCYieldpTdiff[i][j][k]);
                
                tie(PhiPiTOFYieldpTdiff[i][j][k], errPhiPiTOFYieldpTdiff[i][j][k]) = FitPhiPi(h1PhiInvMassPiNSigmaTOF[i][j][k], {i, j, k}, 1, mode, fileCanvasPiTOF);
                PhiPiTOFYieldpTdiff[i][j][k] = PhiPiTOFYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhi[j]);
                errPhiPiTOFYieldpTdiff[i][j][k] = errPhiPiTOFYieldpTdiff[i][j][k] / deltay_axis[i] / ((mult_axis[j+1] - mult_axis[j]) / 100.0) / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhi[j]);
                //if (errPhiPiTOFYieldpTdiff[i][j][k] != errPhiPiTOFYieldpTdiff[i][j][k]) errPhiPiTOFYieldpTdiff[i][j][k] = 0.0;

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

            tie(PhiK0SYieldpTdiffMB[i][k], errPhiK0SYieldpTdiffMB[i][k]) = FitPhiK0S(h1PhiK0SInvMassMB[i][k], {i, k}, fileCanvasK0S);
            PhiK0SYieldpTdiffMB[i][k] = PhiK0SYieldpTdiffMB[i][k] / deltay_axis[i] / (pTK0S_axis[k+1] - pTK0S_axis[k]) / (nEventsPhi * purityPhiMB);
            errPhiK0SYieldpTdiffMB[i][k] = errPhiK0SYieldpTdiffMB[i][k] / deltay_axis[i] / (pTK0S_axis[k+1] - pTK0S_axis[k]) / (nEventsPhi * purityPhiMB);

            h1PhiK0SYieldMB[i]->SetBinContent(k+1, PhiK0SYieldpTdiffMB[i][k]);
            h1PhiK0SYieldMB[i]->SetBinError(k+1, errPhiK0SYieldpTdiffMB[i][k]);
        }

        for (int k = 0; k < nbin_pTPi; k++) {
            //if (i != 0 || k != 0) continue;
            //if (i != 0) continue;
            //continue;

            tie(PhiPiTPCYieldpTdiffMB[i][k], errPhiPiTPCYieldpTdiffMB[i][k]) = FitPhiPi(h1PhiInvMassPiNSigmaTPCMB[i][k], {i, k}, 0, mode, fileCanvasPiTPC);
            PhiPiTPCYieldpTdiffMB[i][k] = PhiPiTPCYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhiMB);
            errPhiPiTPCYieldpTdiffMB[i][k] = errPhiPiTPCYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhiMB);
            if (errPhiPiTPCYieldpTdiffMB[i][k] != errPhiPiTPCYieldpTdiffMB[i][k]) errPhiPiTPCYieldpTdiffMB[i][k] = 0.0;

            h1PhiPiTPCYieldMB[i]->SetBinContent(k+1, PhiPiTPCYieldpTdiffMB[i][k]);
            h1PhiPiTPCYieldMB[i]->SetBinError(k+1, errPhiPiTPCYieldpTdiffMB[i][k]);

            tie(PhiPiTOFYieldpTdiffMB[i][k], errPhiPiTOFYieldpTdiffMB[i][k]) = FitPhiPi(h1PhiInvMassPiNSigmaTOFMB[i][k], {i, k}, 1, mode, fileCanvasPiTOF);
            PhiPiTOFYieldpTdiffMB[i][k] = PhiPiTOFYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhiMB);
            errPhiPiTOFYieldpTdiffMB[i][k] = errPhiPiTOFYieldpTdiffMB[i][k] / deltay_axis[i] / (pTPi_axis[k+1] - pTPi_axis[k]) / (nEventsPhi * purityPhiMB);
            if (errPhiPiTOFYieldpTdiffMB[i][k] != errPhiPiTOFYieldpTdiffMB[i][k]) errPhiPiTOFYieldpTdiffMB[i][k] = 0.0;

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

    /*string outNameYieldK0S = outPath + "PhiK0SYield.txt";
    ofstream outYieldK0S1(outNameYieldK0S);
    if (!outYieldK0S1.is_open()) {
        cerr << "Errore nell'apertura del file." << endl;
        return;
    }
    outNameYieldK0S = outPath + "PhiK0SMBYield.txt";
    ofstream outYieldK0S2(outNameYieldK0S);
    if (!outYieldK0S2.is_open()) {
        cerr << "Errore nell'apertura del file." << endl;
        return;
    }

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTK0S; k++) {
                outYieldK0S1 << PhiK0SYieldpTdiff[i][j][k] << " " << errPhiK0SYieldpTdiff[i][j][k] << endl;
                if (j == 0) outYieldK0S2 << PhiK0SYieldpTdiffMB[i][k] << " " << errPhiK0SYieldpTdiffMB[i][k] << endl;
            }
        }
    }

    outYieldK0S1.close();
    outYieldK0S2.close();

    //********************************************************************************************

    string outNameYieldPi = outPath + "PhiPiYield.txt";
    ofstream outYieldPi1(outNameYieldPi);
    if (!outYieldPi1.is_open()) {
        cerr << "Errore nell'apertura del file." << endl;
        return;
    }
    outNameYieldPi = outPath + "PhiPiMBYield.txt";
    ofstream outYieldPi2(outNameYieldPi);
    if (!outYieldPi2.is_open()) {
        cerr << "Errore nell'apertura del file." << endl;
        return;
    }

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTPi; k++) {
                outYieldPi1 << PhiPiYieldpTdiff[i][j][k] << " " << errPhiPiYieldpTdiff[i][j][k] << endl;
                if (j == 0) outYieldPi2 << PhiPiYieldpTdiffMB[i][k] << " " << errPhiPiYieldpTdiffMB[i][k] << endl;
            }
        }
    }

    outYieldPi1.close();
    outYieldPi2.close();*/

    //********************************************************************************************

    array<TCanvas*, nbin_deltay> cPhiK0SYield = PlotHistograms(h1PhiK0SYield, h1PhiK0SYieldMB, outPath, "rawSpectrumK0SDY%i");

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
        PlotFeatures(K0S[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgK0S);
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
    for (int i = 0; i < nbin_deltay; i++) {
        legK0S2->AddEntry(K0S[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legK0S2->SetTextSize(0.05);
    legK0S2->SetLineWidth(0);
    legK0S2->Draw("same");

    string outNameRawK0SYield = outPath + "rawK0SYield.root";
    cK0S->SaveAs(outNameRawK0SYield.c_str());
    outNameRawK0SYield = outPath + "rawK0SYield.pdf";
    cK0S->SaveAs(outNameRawK0SYield.c_str());

    //********************************************************************************************

    array<TCanvas*, nbin_deltay> cPhiPiTPCYield = PlotHistograms(h1PhiPiTPCYield, h1PhiPiTPCYieldMB, outPath, "rawSpectrumPiTPCDY%i");
    array<TCanvas*, nbin_deltay> cPhiPiTOFYield = PlotHistograms(h1PhiPiTOFYield, h1PhiPiTOFYieldMB, outPath, "rawSpectrumPiTOFDY%i");

    Double_t PhiPiYieldpTint[nbin_deltay][nbin_mult] = {0}, errPhiPiYieldpTint[nbin_deltay][nbin_mult] = {0};

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            for (int k = 0; k < nbin_pTPi; k++) {
                PhiPiYieldpTint[i][j] += PhiPiTOFYieldpTdiff[i][j][k] * (pTPi_axis[k+1] - pTPi_axis[k]);
                errPhiPiYieldpTint[i][j] += TMath::Power(errPhiPiTOFYieldpTdiff[i][j][k] * (pTPi_axis[k+1] - pTPi_axis[k]), 2);
            }
            errPhiPiYieldpTint[i][j] = TMath::Sqrt(errPhiPiYieldpTint[i][j]);
        }
    }

    TMultiGraph* mgPi = new TMultiGraph();
    TGraphAsymmErrors* Pi[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        Pi[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiPiYieldpTint[i], errmult, errmult, errPhiPiYieldpTint[i], errPhiPiYieldpTint[i]);
        PlotFeatures(Pi[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgPi);
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
    for (int i = 0; i < nbin_deltay; i++) {
        legPi2->AddEntry(Pi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legPi2->SetTextSize(0.05);
    legPi2->SetLineWidth(0);
    legPi2->Draw("same");

    string outNameRawPiYield = outPath + "rawPiYield.root";
    cPi->SaveAs(outNameRawPiYield.c_str());
    outNameRawPiYield = outPath + "rawPiYield.pdf";
    cPi->SaveAs(outNameRawPiYield.c_str());

    //********************************************************************************************

    Double_t ratioK0SPi[nbin_deltay][nbin_mult] = {0}, errratioK0SPi[nbin_deltay][nbin_mult] = {0};
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            ratioK0SPi[i][j] = 2 * PhiK0SYieldpTint[i][j] / PhiPiYieldpTint[i][j];
            errratioK0SPi[i][j] = ratioK0SPi[i][j] * TMath::Sqrt(TMath::Power(errPhiK0SYieldpTint[i][j] / PhiK0SYieldpTint[i][j], 2) + TMath::Power(errPhiPiYieldpTint[i][j] / PhiPiYieldpTint[i][j], 2));
        }
    }

    TMultiGraph* mgRatioK0SPi = new TMultiGraph();
    TGraphAsymmErrors* RatioK0SPi[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        RatioK0SPi[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPi[i], errmult, errmult, errratioK0SPi[i], errratioK0SPi[i]);
        PlotFeatures(RatioK0SPi[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgRatioK0SPi);
    }

    TCanvas* cRatioK0SPi = new TCanvas("cRatioK0SPi", "cRatioK0SPi", 1000, 800);
    cRatioK0SPi->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    gPad->SetMargin(0.14,0.05,0.13,0.06);
    mgRatioK0SPi->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; 2K^{0}_{S} / (#pi^{+}+#pi^{#minus})");
    mgRatioK0SPi->GetXaxis()->SetLabelSize(0.045);
    mgRatioK0SPi->GetXaxis()->SetTitleSize(0.045);
    mgRatioK0SPi->GetXaxis()->SetTitleOffset(1.2);
    mgRatioK0SPi->GetYaxis()->SetLabelSize(0.045);
    mgRatioK0SPi->GetYaxis()->SetTitleSize(0.045);
    mgRatioK0SPi->GetYaxis()->SetTitleOffset(1.55);
    mgRatioK0SPi->Draw("AP");
    
    TLegend* legRatioK0SPi1_ = new TLegend(0.51, 0.85, 0.68, 0.88);
    legRatioK0SPi1_->SetHeader("#bf{This work}");
    legRatioK0SPi1_->SetTextSize(0.045);
    legRatioK0SPi1_->SetLineWidth(0);
    legRatioK0SPi1_->Draw("same");

    TLegend* legRatioK0SPi2_ = new TLegend(0.51, 0.65, 0.68, 0.85);
    legRatioK0SPi2_->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    for (int i = 0; i < nbin_deltay; i++) {
        legRatioK0SPi2_->AddEntry(RatioK0SPi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legRatioK0SPi2_->SetTextSize(0.045);
    legRatioK0SPi2_->SetLineWidth(0);
    legRatioK0SPi2_->Draw("same");

    string outNameRawRatioK0SPi = outPath + "ratioK0SPi.root";
    cRatioK0SPi->SaveAs(outNameRawRatioK0SPi.c_str());
    outNameRawRatioK0SPi = outPath + "ratioK0SPi.pdf";
    cRatioK0SPi->SaveAs(outNameRawRatioK0SPi.c_str());

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

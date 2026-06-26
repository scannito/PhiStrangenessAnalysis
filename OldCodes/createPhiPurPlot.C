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

pair<Double_t, Double_t> GetPhiPurityAndError(TH1F* h1PhiInvMass)
{
    h1PhiInvMass->SetTitle("; #it{m} (GeV/#it{c}^{2}); Normalized counts");
    h1PhiInvMass->Scale(1.0 / h1PhiInvMass->Integral());
    Double_t binsize = h1PhiInvMass->GetXaxis()->GetBinWidth(1);

    TF1* fitVoigtPolyPur = new TF1("fitVoigtPolyPur", VoigtPoly, 0.987, 1.06, 7);
    //fitVoigtPolyPur->SetParameter(0, 2);
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

    TF1* fitPolyPur = new TF1("fitPolyPur", PolySqrt, 0.987, 1.06, 3);
    fitPolyPur->SetParameter(0, paramsVoigtPolyPur[4]);
    fitPolyPur->SetParameter(1, paramsVoigtPolyPur[5]);
    fitPolyPur->SetParameter(2, paramsVoigtPolyPur[6]);
    fitPolyPur->SetNpx(400);
    fitPolyPur->SetLineColor(kBlack);
    fitPolyPur->SetLineStyle(kDashed);

    TCanvas* fitPhiPur = new TCanvas("fitPhiPur", "fitPhiPur", 1300, 1300);
    fitPhiPur->cd();
    fitPhiPur->SetTicks(1, 1);
    gStyle->SetOptStat(0);
    h1PhiInvMass->Draw("x0");
    h1PhiInvMass->GetXaxis()->SetRangeUser(1.0, 1.05);
    h1PhiInvMass->GetXaxis()->SetTitleSize(0.045);
    h1PhiInvMass->GetXaxis()->SetLabelSize(0.045);
    h1PhiInvMass->GetXaxis()->SetTitleOffset(1.2);
    h1PhiInvMass->GetYaxis()->SetRangeUser(0.00001, 0.032);
    h1PhiInvMass->GetYaxis()->SetTitleSize(0.045);
    h1PhiInvMass->GetYaxis()->SetLabelSize(0.045);
    h1PhiInvMass->SetMarkerStyle(20);
    h1PhiInvMass->SetMarkerColor(kBlack);
    h1PhiInvMass->SetLineColor(kBlack);
    fitVoigtPolyPur->Draw("same");
    fitVoigtPolyPur->SetLineWidth(3);
    //fitVoigtPur->Draw("same");
    fitPolyPur->Draw("same");
    fitVoigtPur->SetLineWidth(3);
    
    TLegend* legPhi1Pur = new TLegend(0.55, 0.785, 0.75, 0.815);
    legPhi1Pur->SetHeader("ALICE Performance");
    legPhi1Pur->SetTextSize(0.05);
    legPhi1Pur->SetLineWidth(0);
    legPhi1Pur->Draw("same");

    TLegend* legPhi1Pur2 = new TLegend(0.55, 0.73, 0.75, 0.78);
    legPhi1Pur2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV");  
    legPhi1Pur2->SetTextSize(0.04);
    legPhi1Pur2->SetLineWidth(0);
    legPhi1Pur2->Draw("same");

    TLegend* legPhi1Pur3 = new TLegend(0.55, 0.67, 0.75, 0.73);
    legPhi1Pur3->SetHeader("0.4 < #it{p}_{T} < 10 GeV/#it{c}");  
    legPhi1Pur3->SetTextSize(0.04);
    legPhi1Pur3->SetLineWidth(0);
    legPhi1Pur3->Draw("same");

    TLegend* legPhi1Pur4 = new TLegend(0.55, 0.62, 0.75, 0.67);
    legPhi1Pur4->SetHeader("#left|#it{y}#right| < 0.5");
    legPhi1Pur4->SetTextSize(0.04);
    legPhi1Pur4->SetLineWidth(0);
    legPhi1Pur4->Draw("same");

    TLegend* legPhi1Pur5 = new TLegend(0.55, 0.57, 0.75, 0.62);
    legPhi1Pur5->SetHeader("#phi #rightarrow K^{+}K^{#minus}");
    legPhi1Pur5->SetTextSize(0.04);
    legPhi1Pur5->SetLineWidth(0);
    legPhi1Pur5->Draw("same");

    TLegend* legPhi2Pur = new TLegend(0.55, 0.42, 0.75, 0.52);
    legPhi2Pur->AddEntry(fitVoigtPolyPur, "Voigtian fit + bkg.", "l");
    //legPhi2Pur->AddEntry(fitVoigtPur, "Voigtian fit", "l");
    legPhi2Pur->AddEntry(fitPolyPur, "bkg.", "l");
    legPhi2Pur->SetTextSize(0.04);
    legPhi2Pur->SetLineWidth(0);
    legPhi2Pur->Draw("same");

    fitPhiPur->cd();
    
    /*TLine* line1 = new TLine(lowmPhiPur, h1PhiInvMass->GetMinimum(), lowmPhiPur, h1PhiInvMass->GetMaximum());
    TLine* line2 = new TLine(upmPhiPur, h1PhiInvMass->GetMinimum(), upmPhiPur, h1PhiInvMass->GetMaximum());

    line1->SetLineColor(kBlack);
    line1->SetLineStyle(kDashed);
    line1->SetLineWidth(2);
    line1->Draw("same");

    line2->SetLineColor(kBlack);
    line2->SetLineStyle(kDashed);
    line2->SetLineWidth(2);
    line2->Draw("same");*/

    Double_t integralVoigtPolyPur = fitVoigtPolyPur->Integral(lowmPhiPur, upmPhiPur) / binsize;
    Double_t integralVoigt1Pur = fitVoigtPur->Integral(lowmPhiPur, upmPhiPur) / binsize;

    TMatrixDSym covMatrixVoigtPolyPur = fitResultVoigtPolyPur->GetCovarianceMatrix();
    Double_t errintegralVoigtPolyPur = fitVoigtPolyPur->IntegralError(lowmPhiPur, upmPhiPur, paramsVoigtPolyPur, covMatrixVoigtPolyPur.GetMatrixArray()) / binsize;
    Double_t errintegralVoigt1Pur = fitVoigtPolybisPur->IntegralError(lowmPhiPur, upmPhiPur, paramsVoigtPolyPur, covMatrixVoigtPolyPur.GetMatrixArray()) / binsize;

    Double_t purityVoigt = integralVoigt1Pur / integralVoigtPolyPur;
    Double_t errpurityVoigt = purityVoigt * TMath::Sqrt(TMath::Power(errintegralVoigt1Pur / integralVoigt1Pur, 2) + TMath::Power(errintegralVoigtPolyPur / integralVoigtPolyPur, 2));

    return make_pair(purityVoigt, errpurityVoigt);
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

void createPhiPurPlot(int mode) 
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
    
    //********************************************************************************************
    // Multiplicity-integrated purities
    //********************************************************************************************

    Double_t purityVoigtMB{0}, errpurityVoigtMB{0};

    tie(purityVoigtMB, errpurityVoigtMB) = GetPhiPurityAndError(h1PhipurInvMassMB);

    //********************************************************************************************

    Double_t purityPhiMB{0}, errpurityPhiMB{0};

    purityPhiMB = purityVoigtMB;
    errpurityPhiMB = errpurityVoigtMB;

    //return;

    return;
}

#include "Riostream.h"
#include "TFile.h"
#include "TLegend.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include "TMath.h"
#include "TStyle.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TF2.h"
#include "THnSparse.h"
#include "TGraphErrors.h"
#include "TGraphAsymmErrors.h"
#include "TMultiGraph.h"
#include "TCanvas.h"
#include "TString.h"
#include "TLine.h"

#include "YieldMean.C"
#include "CustomTF1.h"
#include "Plot.h"

using namespace std;

TH1D* Project1D(THnSparseD* hn, Int_t axistoproj, Option_t* option = "", string hname = "") 
{ 
    if (!hn) return 0;
    TH1D* h1 = (TH1D*)hn->Projection(axistoproj, option);
    h1->SetName(hname.c_str());
    h1->SetDirectory(0);
    return h1;
}

void CustomDivide(TH1* h1, const TH1* h2, const TH1* h3, Option_t* option = "B") {
    if (!h1 || !h2 || !h3) {
        cerr << "Errore: Fornire tutti e tre gli istogrammi (h1, h2, h3)." << endl;
        return;
    }

    if (h2->GetNbinsX() != h1->GetNbinsX() || h3->GetNbinsX() != h1->GetNbinsX()) {
        cerr << "Errore: Gli istogrammi devono avere lo stesso numero di bin." << endl;
        return;
    }

    TString opt = option;

    for (Int_t i = 1; i <= h1->GetNbinsX(); ++i) {
        Double_t c1 = h1->GetBinCenter(i);
        Double_t y1 = h1->GetBinContent(i);
        Double_t e1 = h1->GetBinError(i);

        Double_t divisor, eDivisor;
        if (c1 < 0.5) {
            divisor = h2->GetBinContent(i);
            eDivisor = h2->GetBinError(i);
        } else {
            divisor = h3->GetBinContent(i);
            eDivisor = h3->GetBinError(i);
        }

        if (divisor != 0) {
            Double_t newContent = y1 / divisor;
            Double_t newError = 0.0;

            if (opt.Contains("B")) {
                newError = TMath::Sqrt((e1 * e1) / (divisor * divisor) + (y1 * y1 * eDivisor * eDivisor) / (divisor * divisor * divisor * divisor));
            }

            h1->SetBinContent(i, newContent);
            h1->SetBinError(i, newError);
        } else {
            h1->SetBinContent(i, 0.0);
            h1->SetBinError(i, 0.0);
        }
    }
}

void PlotFeatures(TGraph* graph, Style_t markstyle, Color_t markcolor, Size_t marksize, Style_t linestyle, Color_t linecolor, Width_t linewidth, Style_t fillstyle, Color_t fillcolor, Float_t alpha, TMultiGraph* mg) 
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

vector<Int_t> Colors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856, 601, kViolet, kPink + 9, kPink + 1, 1};
const vector<Int_t> ColorsFinal = {kBlack, kBlue, kGreen+3, 797, kRed+1};
vector<Int_t> FullMarkers  = {20, 21, 33, 34, 29, 41, 47, 43};
vector<Int_t> EmptyMarkers = {53, 56, 57, 58, 64, 67, 54, 65};
vector<Int_t> Markers = {20, 21, 33, 34, 29, 41, 47, 43, 53, 56, 57, 58, 64, 67, 54, 65};

constexpr Int_t nbin_deltay = 5, nbin_mult = 10, nbin_pTK0S = 9, nbin_pTPi = /*9*/ 11;

//constexpr Double_t deltay_axis[nbin_deltay] = {1.0, 0.5, 0.1};
constexpr Double_t deltay_axis[nbin_deltay] = {1.0, 0.8, 0.5, 0.3, 0.1};
constexpr Double_t mult_axis[nbin_mult+1] = {0.0, 1.0, 5.0, 10.0, 15.0, 20.0, 30.0, 40.0, 50.0, 70.0, 100.0};
constexpr Double_t pTK0S_axis[nbin_pTK0S+1] = {0.1, 0.5, 0.8, 1.2, 1.6, 2.0, 2.5, 3.0, 4.0, 6.0};
//constexpr Double_t pTPi_axis[nbin_pTPi+1] = {0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0};
constexpr Double_t pTPi_axis[nbin_pTPi+1] = {0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0}; 

// computedNdEtaWPhiWSyst3
/*constexpr Double_t mult[nbin_mult] = {28.51, 24.54, 21.51, 19.57, 18.11, 16.30, 14.30, 12.54, 10.33, 6.90};
constexpr Double_t errstatmult[nbin_mult] = {0.08, 0.06, 0.07, 0.06, 0.09, 0.05, 0.06, 0.05, 0.04, 0.02};
constexpr Double_t errsystmult[nbin_mult] = {0.89, 0.82, 0.79, 1.02, 1.15, 0.65, 0.59, 0.55, 0.48, 0.39};*/

//computedNdEtaWPhiWSyst4
/*constexpr Double_t mult[nbin_mult] = {28.66, 24.66, 21.64, 19.61, 18.12, 16.34, 14.37, 12.68, 10.53, 7.12};
constexpr Double_t errstatmult[nbin_mult] = {0.008, 0.005, 0.006, 0.005, 0.007, 0.004, 0.005, 0.004, 0.003, 0.003};
constexpr Double_t errsystmult[nbin_mult] = {0.32, 0.28, 0.25, 0.25, 0.25, 0.24, 0.21, 0.22, 0.21, 0.18};*/

// computedNdEtaWPhiWSyst5
constexpr Double_t mult[nbin_mult] = {28.83, 24.79, 21.76, 19.72, 18.21, 16.43, 14.44, 12.75, 10.60, 7.19};
constexpr Double_t errstatmult[nbin_mult] = {0.007, 0.004, 0.004, 0.004, 0.005, 0.003, 0.003, 0.003, 0.003, 0.002};
constexpr Double_t errsystmult[nbin_mult] = {0.35, 0.32, 0.30, 0.31, 0.31, 0.32, 0.32, 0.38, 0.45, 0.75};

constexpr Double_t topPadHeight = 0.7; 
constexpr Double_t bottomPadHeight = 1.0 - topPadHeight;
constexpr Double_t scaleFactor = bottomPadHeight / topPadHeight;

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

        /*string outName = outPath + name + ".root";
        cYield[i]->SaveAs(Form(outName.c_str(), i));
        outName = outPath + name + ".pdf";
        cYield[i]->SaveAs(Form(outName.c_str(), i));*/
    }

    return cYield;
}

Double_t BarlowVar2(const pair<Double_t, Double_t>& defY, const pair<Double_t, Double_t>& varY)
{
    return (defY.first - varY.first) / sqrt(abs(pow(defY.second, 2) - pow(varY.second, 2)));
}

void ratioK0SPiCorrectedUpdated(int mode) {

    string path;
    string file;

    if (mode == 0) {
        //path = "ResultsData/";
        //file = "ResultsData.root";
        path = "ResultsData2/";
        file = "ResultsData2.root";
    } else if (mode == 1) {
        path = "ResultsMCClosurePDGK0SPi/";
        file = "ResultsMCClosurePDGK0SPi.root";
    } else return;

    TH1D* h1PhiK0SYield[nbin_deltay][nbin_mult];
    TH1D* h1PhiPiTPCYield[nbin_deltay][nbin_mult];
    TH1D* h1PhiPiTOFYield[nbin_deltay][nbin_mult];
    TH1D* h1PhiPiYield[nbin_deltay][nbin_mult];

    TH1D* h1PhiK0SYieldMB[nbin_deltay];
    TH1D* h1PhiPiTPCYieldMB[nbin_deltay];
    TH1D* h1PhiPiTOFYieldMB[nbin_deltay];
    TH1D* h1PhiPiYieldMB[nbin_deltay];

    //********************************************************************************************

    TH1D* h1effPhiK0S[nbin_deltay][nbin_mult];
    TH1D* h1effTrackPhiPi[nbin_deltay][nbin_mult];
    TH1D* h1effMatchPhiPi[nbin_deltay][nbin_mult];
    TH1D* h1effPhiPi[nbin_deltay][nbin_mult];

    TH1D* h1siglossPhiK0S[nbin_deltay][nbin_mult];
    TH1D* h1siglossPhiPi[nbin_deltay][nbin_mult];

    TH1D* h1effPhiK0SMB[nbin_deltay];
    TH1D* h1effTrackPhiPiMB[nbin_deltay];
    TH1D* h1effMatchPhiPiMB[nbin_deltay];
    TH1D* h1effPhiPiMB[nbin_deltay];

    TH1D* h1siglossPhiK0SMB[nbin_deltay];
    TH1D* h1siglossPhiPiMB[nbin_deltay];

    TFile* fileData = TFile::Open(file.c_str());
    //TFile* fileEff = TFile::Open("ResultsEfficiency.root");
    TFile* fileEff = TFile::Open("ResultsEfficiency2.root");

    for (int i = 0; i < nbin_deltay; i++) {
        h1effPhiK0SMB[i] = (TH1D*)fileEff->Get(Form("h1effPhiK0SMultInt%i", i));
        h1effPhiK0SMB[i]->SetDirectory(0);

        h1effTrackPhiPiMB[i] = (TH1D*)fileEff->Get(Form("h1effTrackPhiPiMultInt%i", i));
        h1effTrackPhiPiMB[i]->SetDirectory(0);

        h1effMatchPhiPiMB[i] = (TH1D*)fileEff->Get(Form("h1effMatchPhiPiMultInt%i", i));
        h1effMatchPhiPiMB[i]->SetDirectory(0);

        h1effPhiPiMB[i] = (TH1D*)fileEff->Get(Form("h1effPhiPiMultInt%i", i));
        h1effPhiPiMB[i]->SetDirectory(0);

        h1siglossPhiK0SMB[i] = (TH1D*)fileEff->Get(Form("h1siglossPhiK0SMultInt%i", i));
        h1siglossPhiK0SMB[i]->SetDirectory(0);

        h1siglossPhiPiMB[i] = (TH1D*)fileEff->Get(Form("h1siglossPhiPiMultInt%i", i));
        h1siglossPhiPiMB[i]->SetDirectory(0);

        h1PhiK0SYieldMB[i] = (TH1D*)fileData->Get(Form("h1PhiK0SYieldMB%i", i));
        h1PhiK0SYieldMB[i]->SetDirectory(0);

        h1PhiPiTPCYieldMB[i] = (TH1D*)fileData->Get(Form("h1PhiPiTPCYieldMB%i", i));
        h1PhiPiTPCYieldMB[i]->SetDirectory(0);

        h1PhiPiTOFYieldMB[i] = (TH1D*)fileData->Get(Form("h1PhiPiTOFYieldMB%i", i));
        h1PhiPiTOFYieldMB[i]->SetDirectory(0);

        //********************************************************************************************

        h1PhiK0SYieldMB[i]->Divide(h1PhiK0SYieldMB[i], h1effPhiK0SMB[i], 1, 1);
        h1PhiK0SYieldMB[i]->Divide(h1PhiK0SYieldMB[i], h1siglossPhiK0SMB[0], 1, 1);

        CustomDivide(h1PhiPiTPCYieldMB[i], h1effTrackPhiPiMB[i], h1effPhiPiMB[i]);
        h1PhiPiTPCYieldMB[i]->Divide(h1PhiPiTPCYieldMB[i], h1siglossPhiPiMB[0], 1, 1);

        h1PhiPiTOFYieldMB[i]->Divide(h1PhiPiTOFYieldMB[i], h1effPhiPiMB[i], 1, 1);
        h1PhiPiTOFYieldMB[i]->Divide(h1PhiPiTOFYieldMB[i], h1siglossPhiPiMB[0], 1, 1);

        h1PhiPiYieldMB[i] = (TH1D*)h1PhiPiTPCYieldMB[i]->Clone(Form("h1PhiPiYieldMB%i", i));
        h1PhiPiYieldMB[i]->SetDirectory(0);
        for (int k = 0; k < nbin_pTPi; k++) {
            if (k < 4) {
                if (k == 0) {
                    h1PhiPiYieldMB[i]->SetBinContent(k+1, 0.0);
                    h1PhiPiYieldMB[i]->SetBinError(k+1, 0.0);
                } else {
                    h1PhiPiYieldMB[i]->SetBinContent(k+1, h1PhiPiTPCYieldMB[i]->GetBinContent(k+1));
                    h1PhiPiYieldMB[i]->SetBinError(k+1, h1PhiPiTPCYieldMB[i]->GetBinError(k+1));
                }
            } else if (k == 4) {
                Double_t wTPC = 1.0/ TMath::Power(h1PhiPiTPCYieldMB[i]->GetBinError(k+1), 2);
                Double_t wTOF = 1.0/ TMath::Power(h1PhiPiTOFYieldMB[i]->GetBinError(k+1), 2);
                h1PhiPiYieldMB[i]->SetBinContent(k+1, (h1PhiPiTPCYieldMB[i]->GetBinContent(k+1) * wTPC + h1PhiPiTOFYieldMB[i]->GetBinContent(k+1) * wTOF) / (wTPC + wTOF));
                h1PhiPiYieldMB[i]->SetBinError(k+1, 1.0 / TMath::Sqrt(wTPC + wTOF));
            } else {
                h1PhiPiYieldMB[i]->SetBinContent(k+1, h1PhiPiTOFYieldMB[i]->GetBinContent(k+1));
                h1PhiPiYieldMB[i]->SetBinError(k+1, h1PhiPiTOFYieldMB[i]->GetBinError(k+1));
            }

            if (k == 0 || k > 4) {
                h1PhiPiTPCYieldMB[i]->SetBinContent(k+1, 0.0);
                h1PhiPiTPCYieldMB[i]->SetBinError(k+1, 0.0);
            }

            if (k < 4) {
                h1PhiPiTOFYieldMB[i]->SetBinContent(k+1, 0.0);
                h1PhiPiTOFYieldMB[i]->SetBinError(k+1, 0.0);
            }
        }

        //********************************************************************************************
        for (int j = 0; j < nbin_mult; j++) {
            h1PhiK0SYield[i][j] = (TH1D*)fileData->Get(Form("h1PhiK0SYield%i_%i", i, j));
            h1PhiK0SYield[i][j]->SetDirectory(0);

            h1PhiPiTPCYield[i][j] = (TH1D*)fileData->Get(Form("h1PhiPiTPCYield%i_%i", i, j));
            h1PhiPiTPCYield[i][j]->SetDirectory(0);

            h1PhiPiTOFYield[i][j] = (TH1D*)fileData->Get(Form("h1PhiPiTOFYield%i_%i", i, j));
            h1PhiPiTOFYield[i][j]->SetDirectory(0);

            h1effPhiK0S[i][j] = (TH1D*)fileEff->Get(Form("h1effPhiK0S%i_%i", i, j));
            h1effPhiK0S[i][j]->SetDirectory(0);

            h1effTrackPhiPi[i][j] = (TH1D*)fileEff->Get(Form("h1effTrackPhiPi%i_%i", i, j));
            h1effTrackPhiPi[i][j]->SetDirectory(0);

            h1effMatchPhiPi[i][j] = (TH1D*)fileEff->Get(Form("h1effMatchPhiPi%i_%i", i, j));
            h1effMatchPhiPi[i][j]->SetDirectory(0);

            h1effPhiPi[i][j] = (TH1D*)fileEff->Get(Form("h1effPhiPi%i_%i", i, j));
            h1effPhiPi[i][j]->SetDirectory(0);

            h1siglossPhiK0S[i][j] = (TH1D*)fileEff->Get(Form("h1siglossPhiK0S%i_%i", i, j));
            h1siglossPhiK0S[i][j]->SetDirectory(0);

            h1siglossPhiPi[i][j] = (TH1D*)fileEff->Get(Form("h1siglossPhiPi%i_%i", i, j));
            h1siglossPhiPi[i][j]->SetDirectory(0);

            //********************************************************************************************

            h1PhiK0SYield[i][j]->Divide(h1PhiK0SYield[i][j], h1effPhiK0SMB[i], 1, 1);
            h1PhiK0SYield[i][j]->Divide(h1PhiK0SYield[i][j], h1siglossPhiK0S[0][j], 1, 1);

            CustomDivide(h1PhiPiTPCYield[i][j], h1effTrackPhiPiMB[i], h1effPhiPiMB[i]);
            h1PhiPiTPCYield[i][j]->Divide(h1PhiPiTPCYield[i][j], h1siglossPhiPi[0][j], 1, 1);

            h1PhiPiTOFYield[i][j]->Divide(h1PhiPiTOFYield[i][j], h1effPhiPiMB[i], 1, 1);
            h1PhiPiTOFYield[i][j]->Divide(h1PhiPiTOFYield[i][j], h1siglossPhiPi[0][j], 1, 1);

            h1PhiPiYield[i][j] = (TH1D*)h1PhiPiTPCYield[i][j]->Clone(Form("h1PhiPiYield%i_%i", i, j));
            h1PhiPiYield[i][j]->SetDirectory(0);
            for (int k = 0; k < nbin_pTPi; k++) {
                if (k < 4) {
                    if (k == 0) {
                        h1PhiPiYield[i][j]->SetBinContent(k+1, 0.0);
                        h1PhiPiYield[i][j]->SetBinError(k+1, 0.0);
                    } else {
                        h1PhiPiYield[i][j]->SetBinContent(k+1, h1PhiPiTPCYield[i][j]->GetBinContent(k+1));
                        h1PhiPiYield[i][j]->SetBinError(k+1, h1PhiPiTPCYield[i][j]->GetBinError(k+1));
                    }
                } else if (k == 4) {
                    Double_t wTPC = 1.0/ TMath::Power(h1PhiPiTPCYield[i][j]->GetBinError(k+1), 2);
                    Double_t wTOF = 1.0/ TMath::Power(h1PhiPiTOFYield[i][j]->GetBinError(k+1), 2);
                    h1PhiPiYield[i][j]->SetBinContent(k+1, (h1PhiPiTPCYield[i][j]->GetBinContent(k+1) * wTPC + h1PhiPiTOFYield[i][j]->GetBinContent(k+1) * wTOF) / (wTPC + wTOF));
                    h1PhiPiYield[i][j]->SetBinError(k+1, 1.0 / TMath::Sqrt(wTPC + wTOF));
                } else {
                    h1PhiPiYield[i][j]->SetBinContent(k+1, h1PhiPiTOFYield[i][j]->GetBinContent(k+1));
                    h1PhiPiYield[i][j]->SetBinError(k+1, h1PhiPiTOFYield[i][j]->GetBinError(k+1));
                }

                if (k == 0 || k > 4) {
                    h1PhiPiTPCYield[i][j]->SetBinContent(k+1, 0.0);
                    h1PhiPiTPCYield[i][j]->SetBinError(k+1, 0.0);
                }

                if (k < 4) {
                    h1PhiPiTOFYield[i][j]->SetBinContent(k+1, 0.0);
                    h1PhiPiTOFYield[i][j]->SetBinError(k+1, 0.0);
                }
            }
        }   
    }
    
    fileEff->Close();
    fileData->Close();

    //********************************************************************************************

    TH1D* h1effPhiK0SMBRatio[nbin_deltay];

    TCanvas* cEffPhiK0S = new TCanvas("cEffPhiK0S", "cEffPhiK0S", 800, 800);
    cEffPhiK0S->cd();
    gStyle->SetOptStat(0);

    for (int i = 0; i < nbin_deltay; i++) {
        h1effPhiK0SMBRatio[i] = (TH1D*)h1effPhiK0SMB[i]->Clone(Form("h1effPhiK0SMBRatio%i", i));
        h1effPhiK0SMBRatio[i]->SetMarkerStyle(20);
        h1effPhiK0SMBRatio[i]->SetMarkerColor(Colors[i]);
        h1effPhiK0SMBRatio[i]->SetMarkerSize(1.5);
        h1effPhiK0SMBRatio[i]->SetLineColor(Colors[i]);
        h1effPhiK0SMBRatio[i]->SetLineWidth(2);
        h1effPhiK0SMBRatio[i]->Divide(h1effPhiK0SMB[i], h1effPhiK0SMB[0], 1, 1, "B");
        if (i == 0) {
            h1effPhiK0SMBRatio[i]->Draw();
        } else {
            h1effPhiK0SMBRatio[i]->Draw("same");
        }
    }

    TLegend* legeffK0S = new TLegend(0.15, 0.63, 0.35, 0.83);
    for (int i = 0; i < nbin_deltay; i++) {
        legeffK0S->AddEntry(h1effPhiK0SMBRatio[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legeffK0S->SetTextSize(0.045);
    legeffK0S->SetLineWidth(0);
    legeffK0S->Draw("same");

    TH1D* h1effPhiPiMBRatio[nbin_deltay];

    TCanvas* cEffPhiPi = new TCanvas("cEffPhiPi", "cEffPhiPi", 800, 800);
    cEffPhiPi->cd();
    gStyle->SetOptStat(0);

    for (int i = 0; i < nbin_deltay; i++) {
        h1effPhiPiMBRatio[i] = (TH1D*)h1effPhiPiMB[i]->Clone(Form("h1effPhiPiMBRatio%i", i));
        h1effPhiPiMBRatio[i]->SetMarkerStyle(20);
        h1effPhiPiMBRatio[i]->SetMarkerColor(Colors[i]);
        h1effPhiPiMBRatio[i]->SetMarkerSize(1.5);
        h1effPhiPiMBRatio[i]->SetLineColor(Colors[i]);
        h1effPhiPiMBRatio[i]->SetLineWidth(2);
        h1effPhiPiMBRatio[i]->Divide(h1effPhiPiMB[i], h1effPhiPiMB[0], 1, 1, "B");
        if (i == 0) {
            h1effPhiPiMBRatio[i]->Draw();
        } else {
            h1effPhiPiMBRatio[i]->Draw("same");
        }
    }

    TLegend* legeffPi = new TLegend(0.15, 0.63, 0.35, 0.83);
    for (int i = 0; i < nbin_deltay; i++) {
        legeffPi->AddEntry(h1effPhiPiMBRatio[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legeffPi->SetTextSize(0.045);
    legeffPi->SetLineWidth(0);
    legeffPi->Draw("same");

    //********************************************************************************************

    array<TCanvas*, nbin_deltay> cPhiK0SYield = PlotHistograms(h1PhiK0SYield, h1PhiK0SYieldMB, path, "correctedSpectrumK0SDY%i");

    CustomTF1 customFitFuncK0S;

    const Int_t nFitModes = 5;

    TF1* fitFuncK0S[nbin_deltay][nbin_mult];

    TH1* h1YieldMeanPhiK0S[nbin_deltay][nbin_mult];
    Double_t PhiK0SYieldpTint[nFitModes][nbin_deltay][nbin_mult]{}, errPhiK0SYieldpTint[nFitModes][nbin_deltay][nbin_mult]{}, systPhiK0SYieldpTint[nbin_deltay][nbin_mult]{};
    Double_t PhiK0SYieldpTint2[nFitModes][nbin_deltay][nbin_mult]{}, errPhiK0SYieldpTint2[nFitModes][nbin_deltay][nbin_mult]{};

    TH1* h1Chi2K0S[nbin_deltay];
    array<string, nFitModes> outNameChi2K0S = {"fitChi2K0SLevyTsallis.root", "fitChi2K0SBoseEinstein.root", "fitChi2K0SBlastWave.root", "fitChi2K0SMtExponential.root", "fitChi2K0SPtExponential.root"};
    array<string, nFitModes> outNameExtraK0S = {"extraK0SLevyTsallis.root", "extraK0SBoseEinstein.root", "extraK0SBlastWave.root", "extraK0SMtExponential.root", "extraK0SPtExponential.root"};
    TFile* fileChi2K0S[nFitModes];

    for (int fitMode = 0; fitMode < nFitModes; fitMode++) {
        if (fitMode != 0) continue;
        fileChi2K0S[fitMode] = TFile::Open(outNameChi2K0S[fitMode].data(), "RECREATE");

        for (int i = 0; i < nbin_deltay; i++) {
            if (i != 0) continue;
            //if (i == 1 || i == 3) continue;
            h1Chi2K0S[i] = new TH1D(Form("h1Chi2K0S%i", i), "; centFT0M (%); #chi^{2}/ndf", nbin_mult, mult_axis);
            for (int j = 0; j < nbin_mult; j++) {
                if (fitMode == 0) {
                    fitFuncK0S[i][j] = customFitFuncK0S.GetLeviTsallis(0.497611, 0.03, 0.7, 0.04, Form("fLevyTsallisK0S%i_%i", i, j));
                    fitFuncK0S[i][j]->SetParLimits(3, 0, h1PhiK0SYield[i][j]->GetBinContent(h1PhiK0SYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 1){ 
                    fitFuncK0S[i][j] = customFitFuncK0S.GetBoseEinstein(0.497611, 0.7, 0.04, Form("fBoseEinsteinK0S%i_%i", i, j));
                    //fitFuncK0S[i][j]->SetParLimits(2, 0, h1PhiK0SYield[i][j]->GetBinContent(h1PhiK0SYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 2) {
                    fitFuncK0S[i][j] = customFitFuncK0S.GetBlastWave(0.497611, 0.7, 0.4, 25., 0.04, Form("fBlastWaveK0S%i_%i", i, j));
                    //fitFuncK0S[i][j]->SetParLimits(4, 0, h1PhiK0SYield[i][j]->GetBinContent(h1PhiK0SYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 3) {
                    fitFuncK0S[i][j] = customFitFuncK0S.GetMtExponential(0.497611, 0.7, 0.04, Form("fMtExponentialK0S%i_%i", i, j));
                    //fitFuncK0S[i][j]->SetParLimits(2, 0, h1PhiK0SYield[i][j]->GetBinContent(h1PhiK0SYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 4) {
                    fitFuncK0S[i][j] = customFitFuncK0S.GetPtExponential(0.7, 0.04, Form("fPtExponentialK0S%i_%i", i, j));
                    //fitFuncK0S[i][j]->SetParLimits(1, 0, h1PhiK0SYield[i][j]->GetBinContent(h1PhiK0SYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                }

                for (int k = 0; k < nbin_pTK0S; k++) {
                    PhiK0SYieldpTint[fitMode][i][j] += h1PhiK0SYield[i][j]->GetBinContent(k+1) * (pTK0S_axis[k+1] - pTK0S_axis[k]);

                    PhiK0SYieldpTint2[fitMode][i][j] += h1PhiK0SYield[i][j]->GetBinContent(k+1) * (pTK0S_axis[k+1] - pTK0S_axis[k]);
                    errPhiK0SYieldpTint2[fitMode][i][j] += TMath::Power(h1PhiK0SYield[i][j]->GetBinError(k+1) * (pTK0S_axis[k+1] - pTK0S_axis[k]), 2);
                }
                errPhiK0SYieldpTint2[fitMode][i][j] = TMath::Sqrt(errPhiK0SYieldpTint2[fitMode][i][j]);

                h1YieldMeanPhiK0S[i][j] = YieldMean(h1PhiK0SYield[i][j], fitFuncK0S[i][j], 0.0, 6.0, 0.01, 0.1, "0QI", outNameExtraK0S[fitMode].data(), 0.1, 6.0, "K0S");
                PhiK0SYieldpTint[fitMode][i][j] += h1YieldMeanPhiK0S[i][j]->GetBinContent(5);
                errPhiK0SYieldpTint[fitMode][i][j] = h1YieldMeanPhiK0S[i][j]->GetBinContent(2);

                h1Chi2K0S[i]->SetBinContent(j+1, fitFuncK0S[i][j]->GetChisquare() / fitFuncK0S[i][j]->GetNDF());

                //systPhiK0SYieldpTint[i][j] = 0.05 * PhiK0SYieldpTint[i][j];
            }

            fileChi2K0S[fitMode]->cd();
            h1Chi2K0S[i]->Write();
            delete h1Chi2K0S[i];
        }
    }
    

    //fileChi2K0S->Close();

    Double_t errmult_down[nbin_mult] = {};
    Double_t errmult_up[nbin_mult] = {};

    for (int i = 0; i < nbin_mult; i++) {
        errmult_down[i] = TMath::Sqrt(TMath::Power(errstatmult[i], 2) + TMath::Power(errsystmult[i], 2));
        errmult_up[i] = TMath::Sqrt(TMath::Power(errstatmult[i], 2) + TMath::Power(errsystmult[i], 2));

        cout << "Err mult" << i << "\t" << errmult_up[i] << endl;
    }

    TMultiGraph* mgK0S = new TMultiGraph();
    TMultiGraph* mgK0SSyst = new TMultiGraph();

    TGraphAsymmErrors* K0S[nbin_deltay];
    TGraphAsymmErrors* K0SSyst[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        K0S[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiK0SYieldpTint[0][i], errmult_down, errmult_up, errPhiK0SYieldpTint[0][i], errPhiK0SYieldpTint[0][i]);
        PlotFeatures(K0S[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgK0S);

        //K0SSyst[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiK0SYieldpTint[i], errmult_down, errmult_up, systPhiK0SYieldpTint[i], systPhiK0SYieldpTint[i]);
        //PlotFeatures(K0SSyst[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 1001, ColorsFinal[i], 0.3, mgK0SSyst);
    }

    TCanvas* cK0S = new TCanvas("cK0S", "cK0S", 800, 800);
    cK0S->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    gPad->SetMargin(0.12,0.05,0.13,0.06);
    mgK0S->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; 1/N_{ev,#phi} dN_{K^{0}_{S}}/d#it{y}");
    mgK0S->GetXaxis()->SetLabelSize(0.045);
    mgK0S->GetXaxis()->SetTitleSize(0.045);
    mgK0S->GetXaxis()->SetTitleOffset(1.2);
    mgK0S->GetYaxis()->SetLabelSize(0.045);
    mgK0S->GetYaxis()->SetTitleSize(0.045);
    mgK0S->GetYaxis()->SetTitleOffset(1.2);
    mgK0S->Draw("AP");
    //mgK0S->Draw("A");
    //mgK0SSyst->Draw("PE3");

    TLegend* legK0S1 = new TLegend(0.15, 0.87, 0.35, 0.9);
    legK0S1->SetHeader("#bf{This work}");
    legK0S1->SetTextSize(0.05);
    legK0S1->SetLineWidth(0);
    legK0S1->Draw("same");

    TLegend* legK0S2 = new TLegend(0.15, 0.83, 0.35, 0.86);
    legK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legK0S2->SetTextSize(0.045);
    legK0S2->SetLineWidth(0);
    legK0S2->Draw("same");

    TLegend* legK0S3 = new TLegend(0.15, 0.63, 0.35, 0.83);
    legK0S3->SetHeader("#it{p}_{T} < 6.0 GeV/#it{c}");
    for (int i = 0; i < nbin_deltay; i++) {
        //if (i != 0) continue;
        if (i == 1 || i == 3) continue;
        legK0S3->AddEntry(K0S[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legK0S3->SetTextSize(0.045);
    legK0S3->SetLineWidth(0);
    legK0S3->Draw("same");

    string outNameK0S = path + "correctedK0SYield.root";
    cK0S->SaveAs(outNameK0S.c_str());
    outNameK0S = path + "correctedK0SYield.pdf";
    cK0S->SaveAs(outNameK0S.c_str());

    filesystem::path outPathSystK0S = filesystem::path("Systematics/K0S/LowPtExtra");
    // Creazione della cartella
    try {
        if (filesystem::exists(outPathSystK0S)) {
            cout << "La cartella esiste già: " << outPathSystK0S << endl;
        } else if (filesystem::create_directories(outPathSystK0S)) {
            cout << "Cartella creata con successo: " << outPathSystK0S << endl;
        } else {
            cerr << "Errore sconosciuto nella creazione della cartella." << endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        cerr << "Errore nella creazione della cartella: " << e.what() << endl;
    }

    filesystem::path outFileNameSystK0S = outPathSystK0S / filesystem::path("lowPtExtrapolationSystematics.root");
    TFile* outFileSystK0S = new TFile(outFileNameSystK0S.string().data(), "RECREATE");

    TH1F* h1K0SLowPtExtraSyst[nbin_deltay];
    TH1F* hPassedBarlowCheckvsMultK0S[nbin_deltay];
    TCanvas* cPassedBarlowCheckvsMultK0S[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        if (i != 0) continue;
        string h1K0SLowPtExtraSystName = "h1K0SLowPtExtraSyst" + to_string(i);
        h1K0SLowPtExtraSyst[i] = new TH1F(h1K0SLowPtExtraSystName.data(), "; CentFT0M(%); Syst", nbin_mult, mult_axis);

        string h1PassBarlowK0S = "h1PassBarlowK0SLowPtExtraSyst" + to_string(i);
        hPassedBarlowCheckvsMultK0S[i] = new TH1F(h1PassBarlowK0S.data(), "; CentFT0M(%); Syst", nbin_mult, mult_axis);

        string cPassedBarlowCheckvsMultNameK0S = "cPassedBarlowCheckvsMultLowPtExtra" + to_string(i);
        cPassedBarlowCheckvsMultK0S[i] = new TCanvas(cPassedBarlowCheckvsMultNameK0S.data(), cPassedBarlowCheckvsMultNameK0S.data(), 1000, 800);

        for (int j = 0; j < nbin_mult; j++) {
            h1K0SLowPtExtraSyst[i]->SetBinContent(j+1, abs(PhiK0SYieldpTint[2][i][j] - PhiK0SYieldpTint[0][i][j]) / PhiK0SYieldpTint[0][i][j]);
            h1K0SLowPtExtraSyst[i]->SetBinError(j+1, 0.01 * abs(PhiK0SYieldpTint[2][i][j] - PhiK0SYieldpTint[0][i][j]) / PhiK0SYieldpTint[0][i][j]);

            hPassedBarlowCheckvsMultK0S[i]->SetBinContent(j+1, BarlowVar2({PhiK0SYieldpTint[0][i][j], errPhiK0SYieldpTint[0][i][j]}, {PhiK0SYieldpTint[2][i][j], errPhiK0SYieldpTint[2][i][j]}));
        }

        outFileSystK0S->cd();
        h1K0SLowPtExtraSyst[i]->Write();
        delete h1K0SLowPtExtraSyst[i];

        cPassedBarlowCheckvsMultK0S[i]->cd();
        hPassedBarlowCheckvsMultK0S[i]->Draw();
        TLine* lineK0S1 = new TLine(0, 2, 100, 2);
        lineK0S1->SetLineColor(kRed);
        lineK0S1->SetLineStyle(kDashed);
        lineK0S1->SetLineWidth(2);
        lineK0S1->Draw("same");
        TLine* lineK0S2 = new TLine(0, -2, 100, -2);
        lineK0S2->SetLineColor(kRed);
        lineK0S2->SetLineStyle(kDashed);
        lineK0S2->SetLineWidth(2);
        lineK0S2->Draw("same");

        cPassedBarlowCheckvsMultK0S[i]->Write(cPassedBarlowCheckvsMultNameK0S.data(), TObject::kSingleKey);
        hPassedBarlowCheckvsMultK0S[i]->Write();
        delete hPassedBarlowCheckvsMultK0S[i];
    }

    outFileSystK0S->Close();

    //********************************************************************************************

    array<TCanvas*, nbin_deltay> cPhiPiTPCYield = PlotHistograms(h1PhiPiTPCYield, h1PhiPiTPCYieldMB, path, "correctedSpectrumPiTPCDY%i");
    array<TCanvas*, nbin_deltay> cPhiPiTOFYield = PlotHistograms(h1PhiPiTOFYield, h1PhiPiTOFYieldMB, path, "correctedSpectrumPiTOFDY%i");
    array<TCanvas*, nbin_deltay> cPhiPiYield = PlotHistograms(h1PhiPiYield, h1PhiPiYieldMB, path, "correctedSpectrumPiDY%i");

    CustomTF1 customFitFuncPi;

    TF1* fitFuncPi[nbin_deltay][nbin_mult];

    TH1* h1YieldMeanPhiPi[nbin_deltay][nbin_mult];
    Double_t PhiPiYieldpTint[nFitModes][nbin_deltay][nbin_mult]{}, errPhiPiYieldpTint[nFitModes][nbin_deltay][nbin_mult]{}, systPhiPiYieldpTint[nbin_deltay][nbin_mult]{};
    Double_t PhiPiYieldpTint2[nFitModes][nbin_deltay][nbin_mult]{}, errPhiPiYieldpTint2[nFitModes][nbin_deltay][nbin_mult]{};

    TH1* h1Chi2Pi[nbin_deltay];
    array<string, nFitModes> outNameChi2Pi = {"fitChi2PiLevyTsallis.root", "fitChi2PiBoseEinstein.root", "fitChi2PiBlastWave.root", "fitChi2PiMtExponential.root", "fitChi2PiPtExponential.root"};
    array<string, nFitModes> outNameExtraPi = {"extraPiLevyTsallis.root", "extraPiBoseEinstein.root", "extraPiBlastWave.root", "extraPiMtExponential.root", "extraPiPtExponential.root"};
    TFile* fileChi2Pi[nFitModes];

    for (int fitMode = 0; fitMode < nFitModes; fitMode++) {
        if (fitMode != 0) continue;
        fileChi2Pi[fitMode] = TFile::Open(outNameChi2Pi[fitMode].data(), "RECREATE");

        for (int i = 0; i < nbin_deltay; i++) {
            if (i != 0) continue;
            //if (i == 1 || i == 3) continue;
            h1Chi2Pi[i] = new TH1D(Form("h1Chi2Pi%i", i), "; centFT0M (%); #chi^{2}/ndf", nbin_mult, mult_axis);
            for (int j = 0; j < nbin_mult; j++) {
                if (fitMode == 0) {
                    fitFuncPi[i][j] = customFitFuncPi.GetLeviTsallis(0.13957061, 0.03, 0.7, 0.04, Form("fLevyTsallisPi%i_%i", i, j));
                    fitFuncPi[i][j]->SetParLimits(3, 0, h1PhiPiYield[i][j]->GetBinContent(h1PhiPiYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 1){ 
                    fitFuncPi[i][j] = customFitFuncPi.GetBoseEinstein(0.13957061, 0.7, 0.04, Form("fBoseEinsteinPi%i_%i", i, j));
                    //fitFuncPi[i][j]->SetParLimits(2, 0, h1PhiPiYield[i][j]->GetBinContent(h1PhiPiYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 2) {
                    fitFuncPi[i][j] = customFitFuncPi.GetBlastWave(0.13957061, 0.7, 0.4, 25., 0.04, Form("fBlastWavePi%i_%i", i, j));
                    //fitFuncPi[i][j]->SetParLimits(4, 0, h1PhiPiYield[i][j]->GetBinContent(h1PhiPiYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 3) {
                    fitFuncPi[i][j] = customFitFuncPi.GetMtExponential(0.13957061, 0.7, 0.04, Form("fMtExponentialPi%i_%i", i, j));
                    //fitFuncPi[i][j]->SetParLimits(2, 0, h1PhiPiYield[i][j]->GetBinContent(h1PhiPiYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                } else if (fitMode == 4) {
                    fitFuncPi[i][j] = customFitFuncPi.GetPtExponential(0.7, 0.04, Form("fPtExponentialPi%i_%i", i, j));
                    //fitFuncPi[i][j]->SetParLimits(1, 0, h1PhiPiYield[i][j]->GetBinContent(h1PhiPiYield[i][j]->GetMaximumBin()) * 0.5 * 10);
                }

                for (int k = 0; k < nbin_pTPi; k++) {
                    if (k == 0) continue;
                    PhiPiYieldpTint[fitMode][i][j] += h1PhiPiYield[i][j]->GetBinContent(k+1) * (pTPi_axis[k+1] - pTPi_axis[k]);

                    PhiPiYieldpTint2[fitMode][i][j] += h1PhiPiYield[i][j]->GetBinContent(k+1) * (pTPi_axis[k+1] - pTPi_axis[k]);
                    errPhiPiYieldpTint2[fitMode][i][j] += TMath::Power(h1PhiPiYield[i][j]->GetBinError(k+1) * (pTPi_axis[k+1] - pTPi_axis[k]), 2);
                }
                errPhiPiYieldpTint2[fitMode][i][j] = TMath::Sqrt(errPhiPiYieldpTint2[fitMode][i][j]);

                h1YieldMeanPhiPi[i][j] = YieldMean(h1PhiPiYield[i][j], fitFuncPi[i][j], 0.0, 1.0, 0.01, 0.1, "0QI", outNameExtraPi[fitMode].data(), 0.2, 1.0, "Pi");
                PhiPiYieldpTint[fitMode][i][j] += h1YieldMeanPhiPi[i][j]->GetBinContent(5);
                errPhiPiYieldpTint[fitMode][i][j] = h1YieldMeanPhiPi[i][j]->GetBinContent(2);

                h1Chi2Pi[i]->SetBinContent(j+1, fitFuncPi[i][j]->GetChisquare() / fitFuncPi[i][j]->GetNDF());

                //systPhiPiYieldpTint[i][j] = 0.05 * PhiPiYieldpTint[i][j];
            }

            fileChi2Pi[fitMode]->cd();
            h1Chi2Pi[i]->Write();
            delete h1Chi2Pi[i];
        }
    }

    //fileChi2Pi->Close();

    TMultiGraph* mgPi = new TMultiGraph();
    TMultiGraph* mgPiSyst = new TMultiGraph();

    TGraphAsymmErrors* Pi[nbin_deltay];
    TGraphAsymmErrors* PiSyst[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        //Pi[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiPiTOFYieldpTint[i], errmult_down, errmult_up, errPhiPiTOFYieldpTint[i], errPhiPiTOFYieldpTint[i]);
        Pi[i] = new TGraphAsymmErrors(nbin_mult, mult, PhiPiYieldpTint[0][i], errmult_down, errmult_up, errPhiPiYieldpTint[0][i], errPhiPiYieldpTint[0][i]);
        PlotFeatures(Pi[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgPi);
    }

    TCanvas* cPi = new TCanvas("cPi", "cPi", 800, 800);
    cPi->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    gPad->SetMargin(0.12,0.05,0.13,0.06);
    mgPi->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; 1/N_{ev,#phi} dN_{(#pi^{+}+#pi^{#minus})}/d#it{y}");
    mgPi->GetXaxis()->SetLabelSize(0.045);
    mgPi->GetXaxis()->SetTitleSize(0.045);
    mgPi->GetXaxis()->SetTitleOffset(1.2);
    mgPi->GetYaxis()->SetLabelSize(0.045);
    mgPi->GetYaxis()->SetTitleSize(0.045);
    mgPi->GetYaxis()->SetTitleOffset(1.0);
    mgPi->Draw("AP");
    //mgPi->Draw("A");
    //mgPiSyst->Draw("PE2");

    TLegend* legPi1 = new TLegend(0.15, 0.87, 0.35, 0.9);
    legPi1->SetHeader("#bf{This work}");
    legPi1->SetTextSize(0.045);
    legPi1->SetLineWidth(0);
    legPi1->Draw("same");

    TLegend* legPi2 = new TLegend(0.15, 0.83, 0.35, 0.86);
    legPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legPi2->SetTextSize(0.045);
    legPi2->SetLineWidth(0);
    legPi2->Draw("same");

    TLegend* legPi3 = new TLegend(0.15, 0.63, 0.35, 0.83);
    legPi3->SetHeader("#it{p}_{T} < 3.0 GeV/#it{c}");
    for (int i = 0; i < nbin_deltay; i++) {
        //if (i != 0) continue;
        if (i == 1 || i == 3) continue;
        legPi3->AddEntry(Pi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legPi3->SetTextSize(0.045);
    legPi3->SetLineWidth(0);
    legPi3->Draw("same");

    string outNamePi = path + "correctedPiYield.root";
    cPi->SaveAs(outNamePi.c_str());
    outNamePi = path + "correctedPiYield.pdf";
    cPi->SaveAs(outNamePi.c_str());

    /*filesystem::path outPathSystPi = filesystem::path("Systematics/Pions/LowPtExtra");
    // Creazione della cartella
    try {
        if (filesystem::exists(outPathSystPi)) {
            cout << "La cartella esiste già: " << outPathSystPi << endl;
        } else if (filesystem::create_directories(outPathSystPi)) {
            cout << "Cartella creata con successo: " << outPathSystPi << endl;
        } else {
            cerr << "Errore sconosciuto nella creazione della cartella." << endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        cerr << "Errore nella creazione della cartella: " << e.what() << endl;
    }

    filesystem::path outFileNameSystPi = outPathSystPi / filesystem::path("lowPtExtrapolationSystematics.root");
    TFile* outFileSystPi = new TFile(outFileNameSystPi.string().data(), "RECREATE");

    TH1F* h1PiLowPtExtraSyst[nbin_deltay];
    TH1F* hPassedBarlowCheckvsMultPi[nbin_deltay];
    TCanvas* cPassedBarlowCheckvsMultPi[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        if (i != 0) continue;
        string h1PiLowPtExtraSystName = "h1PiLowPtExtraSyst" + to_string(i);
        h1PiLowPtExtraSyst[i] = new TH1F(h1PiLowPtExtraSystName.data(), "; CentFT0M(%); Syst", nbin_mult, mult_axis);

        string h1PassBarlowPi = "h1PassBarlowPiLowPtExtraSyst" + to_string(i);
        hPassedBarlowCheckvsMultPi[i] = new TH1F(h1PassBarlowPi.data(), "; CentFT0M(%); Syst", nbin_mult, mult_axis);

        string cPassedBarlowCheckvsMultNamePi = "cPassedBarlowCheckvsMultLowPtExtra" + to_string(i);
        cPassedBarlowCheckvsMultPi[i] = new TCanvas(cPassedBarlowCheckvsMultNamePi.data(), cPassedBarlowCheckvsMultNamePi.data(), 1000, 800);

        for (int j = 0; j < nbin_mult; j++) {
            h1PiLowPtExtraSyst[i]->SetBinContent(j+1, abs(PhiPiYieldpTint[2][i][j] - PhiPiYieldpTint[0][i][j]) / PhiPiYieldpTint[0][i][j]);
            h1PiLowPtExtraSyst[i]->SetBinError(j+1, 0.01 * abs(PhiPiYieldpTint[2][i][j] - PhiPiYieldpTint[0][i][j]) / PhiPiYieldpTint[0][i][j]);

            hPassedBarlowCheckvsMultPi[i]->SetBinContent(j+1, BarlowVar2({PhiPiYieldpTint[0][i][j], errPhiPiYieldpTint[0][i][j]}, {PhiPiYieldpTint[2][i][j], errPhiPiYieldpTint[2][i][j]}));
        }

        outFileSystPi->cd();
        h1PiLowPtExtraSyst[i]->Write();
        delete h1PiLowPtExtraSyst[i];

        cPassedBarlowCheckvsMultPi[i]->cd();
        hPassedBarlowCheckvsMultPi[i]->Draw();
        TLine* linePi1 = new TLine(0, 2, 100, 2);
        linePi1->SetLineColor(kRed);
        linePi1->SetLineStyle(kDashed);
        linePi1->SetLineWidth(2);
        linePi1->Draw("same");
        TLine* linePi2 = new TLine(0, -2, 100, -2);
        linePi2->SetLineColor(kRed);
        linePi2->SetLineStyle(kDashed);
        linePi2->SetLineWidth(2);
        linePi2->Draw("same");

        cPassedBarlowCheckvsMultPi[i]->Write(cPassedBarlowCheckvsMultNamePi.data(), TObject::kSingleKey);
        hPassedBarlowCheckvsMultPi[i]->Write();
        delete hPassedBarlowCheckvsMultPi[i];
    }

    outFileSystPi->Close();*/

    //********************************************************************************************

    Double_t ratioK0SPi[nbin_deltay][nbin_mult]{}, errratioK0SPi[nbin_deltay][nbin_mult]{};
    Double_t ratioK0SPi2[nbin_deltay][nbin_mult]{}, errratioK0SP2[nbin_deltay][nbin_mult]{};
    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            ratioK0SPi[i][j] = 2 * PhiK0SYieldpTint[0][i][j] / PhiPiYieldpTint[0][i][j];
            errratioK0SPi[i][j] = ratioK0SPi[i][j] * TMath::Sqrt(TMath::Power(errPhiK0SYieldpTint[0][i][j] / PhiK0SYieldpTint[0][i][j], 2) + TMath::Power(errPhiPiYieldpTint[0][i][j] / PhiPiYieldpTint[0][i][j], 2));

            ratioK0SPi2[i][j] = 2 * PhiK0SYieldpTint[0][i][j] / PhiPiYieldpTint2[0][i][j];
            errratioK0SP2[i][j] = ratioK0SPi2[i][j] * TMath::Sqrt(TMath::Power(errPhiK0SYieldpTint[0][i][j] / PhiK0SYieldpTint[0][i][j], 2) + TMath::Power(errPhiPiYieldpTint2[0][i][j] / PhiPiYieldpTint2[0][i][j], 2));
        }
    }

    TH1F* h1SystTotK0S = new TH1F("h1SystTotK0S", "; centFT0M (%); #sigma_{syst}", nbin_mult, mult_axis);

    vector<string> inSystNameK0S = {"Systematics/K0S/DCADauToPV/phik0shortanalysis_tightDCADauToPV_id25651Systematics.root",
                                    "Systematics/K0S/NSigmaTPC/phik0shortanalysis_tightNSigmaTPC_id25957Systematics.root",
                                    "Systematics/K0S/NSigmaFit/phik0shortanalysis_id25399tightNSigmaFitSystematics.root"};

    vector<string> namehSystK0S = {"h1PhiK0SSystDCADauToPV0", "h1PhiK0SSystNSigmaTPC0", "h1PhiK0SSystNSigmaFit0"};

    vector<string> nameSystK0S = {"DCADauToPV", "NSigmaTPCDau", "NSigmaFit"};

    vector<TFile*> fileSystK0S(inSystNameK0S.size(), nullptr);
    vector<TH1F*> h1SystK0S(inSystNameK0S.size(), nullptr);
    //vector<TCanvas*> cSystK0S(inSystNameK0S.size(), nullptr);

    TCanvas* cSystTotK0S = new TCanvas("cSystK0S", "cSystK0S", 800, 800);

    TLegend* legSystK0S = new TLegend(0.15, 0.87, 0.35, 0.9);
    legSystK0S->SetHeader("#bf{This work}");
    legSystK0S->SetTextSize(0.045);
    legSystK0S->SetLineWidth(0);

    TLegend* legSystK0S2 = new TLegend(0.15, 0.83, 0.35, 0.86);
    legSystK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#Delta#it{y}| < 1.0");
    legSystK0S2->SetTextSize(0.045);
    legSystK0S2->SetLineWidth(0);

    TLegend* legSystK0S3 = new TLegend(0.15, 0.63, 0.35, 0.83);
    legSystK0S3->SetHeader("K^{0}_{S} systematic sources");
    legSystK0S3->SetTextSize(0.045);
    legSystK0S3->SetLineWidth(0);

    for (int i = 0; i < inSystNameK0S.size(); i++) {
        fileSystK0S[i] = TFile::Open(inSystNameK0S[i].data());

        h1SystK0S[i] = (TH1F*)fileSystK0S[i]->Get(namehSystK0S[i].data());
        h1SystK0S[i]->SetDirectory(0);
        h1SystK0S[i]->Smooth(1);

        /*cSystK0S[i] = new TCanvas(Form("cSystK0S%i", i), Form("cSystK0S%i", i), 800, 800);
        cSystK0S[i]->cd();
        h1SystK0S[i]->Draw("HIST");*/
        cSystTotK0S->cd();
        h1SystK0S[i]->SetTitle("; centFT0M (%); #sigma_{syst} (%)");
        h1SystK0S[i]->SetLineColor(Colors[i]);
        h1SystK0S[i]->SetLineWidth(2);
        h1SystK0S[i]->SetMarkerColor(Colors[i]);
        if (i == 0) h1SystK0S[i]->Draw("HIST");
        else h1SystK0S[i]->Draw("HIST SAME");
        legSystK0S3->AddEntry(h1SystK0S[i], Form("%s", nameSystK0S[i].data()), "l");

        fileSystK0S[i]->Close();

        for (int j = 0; j < nbin_mult; j++) {
            h1SystTotK0S->SetBinContent(j+1, TMath::Sqrt(TMath::Power(h1SystTotK0S->GetBinContent(j+1), 2) + TMath::Power(h1SystK0S[i]->GetBinContent(j+1), 2)));
        }
    }
    cSystTotK0S->cd();
    gPad->SetLogy();
    h1SystTotK0S->SetLineColor(kBlack);
    h1SystTotK0S->SetLineWidth(2);
    h1SystTotK0S->SetMarkerColor(kBlack);
    h1SystTotK0S->Draw("HIST SAME");
    legSystK0S3->AddEntry(h1SystTotK0S, "Total", "l");
    legSystK0S->Draw("same");
    legSystK0S2->Draw("same");
    legSystK0S3->Draw("same");

    TH1F* h1SystTotPi = new TH1F("h1SystTotPi", "; centFT0M (%); #sigma_{syst}", nbin_mult, mult_axis);

    vector<string> inSystNamePi = {"Systematics/Pions/DCAxy/phik0shortanalysis_looseparDCAxy_id25965Systematics.root",
                                   "Systematics/Pions/DCAz/phik0shortanalysis_parDCAz_id25964Systematics.root",
                                   "Systematics/Pions/NSigmaFit/phik0shortanalysis_id25399tightNSigmaFitSystematics.root",
                                   "Systematics/Pions/LowPtExtra/lowPtExtrapolationSystematics.root"};

    vector<string> namehSystPi = {"h1PhiPiTOFSystDCAxy0", "h1PhiPiTOFSystDCAz0", "h1PhiPiTOFSystNSigmaFit0", "h1PiLowPtExtraSyst0"};

    vector<string> nameSystPi = {"DCAxy", "DCAz", "NSigmaFit", "LowPtExtrapolation"};

    vector<TFile*> fileSystPi(inSystNamePi.size(), nullptr);
    vector<TH1F*> h1SystPi(inSystNamePi.size(), nullptr);
    //vector<TCanvas*> cSystPi(inSystNamePi.size(), nullptr);

    TCanvas* cSystTotPi = new TCanvas("cSystPi", "cSystPi", 800, 800);

    TLegend* legSystPi = new TLegend(0.15, 0.87, 0.35, 0.9);
    legSystPi->SetHeader("#bf{This work}");
    legSystPi->SetTextSize(0.045);
    legSystPi->SetLineWidth(0);

    TLegend* legSystPi2 = new TLegend(0.15, 0.83, 0.35, 0.86);
    legSystPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#Delta#it{y}| < 1.0");
    legSystPi2->SetTextSize(0.045);
    legSystPi2->SetLineWidth(0);

    TLegend* legSystPi3 = new TLegend(0.15, 0.63, 0.35, 0.83);
    legSystPi3->SetHeader("#pi^{#pm} systematic sources");
    legSystPi3->SetTextSize(0.045);
    legSystPi3->SetLineWidth(0);

    for (int i = 0; i < inSystNamePi.size(); i++) {
        fileSystPi[i] = TFile::Open(inSystNamePi[i].data());

        h1SystPi[i] = (TH1F*)fileSystPi[i]->Get(namehSystPi[i].data());
        h1SystPi[i]->SetDirectory(0);
        h1SystPi[i]->Smooth(1);

        /*cSystPi[i] = new TCanvas(Form("cSystPi%i", i), Form("cSystPi%i", i), 800, 800);
        cSystPi[i]->cd();
        h1SystPi[i]->Draw("HIST");*/
        cSystTotPi->cd();
        h1SystPi[i]->SetTitle("; centFT0M (%); #sigma_{syst} (%)");
        h1SystPi[i]->SetLineColor(Colors[i]);
        h1SystPi[i]->SetLineWidth(2);
        h1SystPi[i]->SetMarkerColor(Colors[i]);
        if (i == 0) h1SystPi[i]->Draw("HIST");
        else h1SystPi[i]->Draw("HIST SAME");
        legSystPi3->AddEntry(h1SystPi[i], Form("%s", nameSystPi[i].data()), "l");

        fileSystPi[i]->Close();

        for (int j = 0; j < nbin_mult; j++) {
            h1SystTotPi->SetBinContent(j+1, TMath::Sqrt(TMath::Power(h1SystTotPi->GetBinContent(j+1), 2) + TMath::Power(h1SystPi[i]->GetBinContent(j+1), 2)));
        }
    }

    
    cSystTotPi->cd();
    gPad->SetLogy();
    h1SystTotPi->SetLineColor(kBlack);
    h1SystTotPi->SetLineWidth(2);
    h1SystTotPi->SetMarkerColor(kBlack);
    h1SystTotPi->Draw("HIST SAME");
    legSystPi3->AddEntry(h1SystTotPi, "Total", "l");
    legSystPi->Draw("same");
    legSystPi2->Draw("same");
    legSystPi3->Draw("same");

    TH1F* h1SystTotK0SPiCorr = new TH1F("h1SystTotK0SPiCorr", "; centFT0M (%); #sigma_{syst}", nbin_mult, mult_axis);

    vector<string> inSystNameK0SPiCorrelated = {"Systematics/K0SPiCorrelated/NTPCClusters/phik0shortanalysis_tightTPCClusters_id25957Systematics.root"};

    vector<string> namehSystK0SPiCorrelated = {"h1PhiK0SPiCorrelatedSystNTPCClusters0"};

    vector<string> nameSystK0SPiCorrelated = {"NTPCClusters"};

    vector<TFile*> fileSystK0SPiCorrelated(inSystNameK0SPiCorrelated.size(), nullptr);
    vector<TH1F*> h1SystK0SPiCorrelated(inSystNameK0SPiCorrelated.size(), nullptr);
    //vector<TCanvas*> cSystK0SPiCorrelated(inSystNameK0SPiCorrelated.size(), nullptr);

    TCanvas* cSystTotK0SPiCorr = new TCanvas("cSystK0SPiCorr", "cSystK0SPiCorr", 800, 800);

    TLegend* legSystK0SPiCorr = new TLegend(0.15, 0.87, 0.35, 0.9);
    legSystK0SPiCorr->SetHeader("#bf{This work}");
    legSystK0SPiCorr->SetTextSize(0.045);
    legSystK0SPiCorr->SetLineWidth(0);

    TLegend* legSystK0SPiCorr2 = new TLegend(0.15, 0.83, 0.35, 0.86);
    legSystK0SPiCorr2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#Delta#it{y}| < 1.0");
    legSystK0SPiCorr2->SetTextSize(0.045);
    legSystK0SPiCorr2->SetLineWidth(0);

    TLegend* legSystK0SPiCorr3 = new TLegend(0.15, 0.73, 0.35, 0.83);
    legSystK0SPiCorr3->SetHeader("Correlated systematic sources");
    legSystK0SPiCorr3->SetTextSize(0.045);
    legSystK0SPiCorr3->SetLineWidth(0);

    for (int i = 0; i < inSystNameK0SPiCorrelated.size(); i++) {
        fileSystK0SPiCorrelated[i] = TFile::Open(inSystNameK0SPiCorrelated[i].data());

        h1SystK0SPiCorrelated[i] = (TH1F*)fileSystK0SPiCorrelated[i]->Get(namehSystK0SPiCorrelated[i].data());
        h1SystK0SPiCorrelated[i]->SetDirectory(0);
        h1SystK0SPiCorrelated[i]->Smooth(1);

        /*cSystK0SPiCorrelated[i] = new TCanvas(Form("cSystK0SPiCorrelated%i", i), Form("cSystK0SPiCorrelated%i", i), 800, 800);
        cSystK0SPiCorrelated[i]->cd();
        h1SystK0SPiCorrelated[i]->Draw("HIST");*/
        cSystTotK0SPiCorr->cd();
        h1SystK0SPiCorrelated[i]->SetTitle("; centFT0M (%); #sigma_{syst} (%)");
        h1SystK0SPiCorrelated[i]->SetLineColor(Colors[i]);
        h1SystK0SPiCorrelated[i]->SetLineWidth(2);
        h1SystK0SPiCorrelated[i]->SetMarkerColor(Colors[i]);
        if (i == 0) h1SystK0SPiCorrelated[i]->Draw("HIST");
        else h1SystK0SPiCorrelated[i]->Draw("HIST SAME");
        legSystK0SPiCorr3->AddEntry(h1SystK0SPiCorrelated[i], Form("%s", nameSystK0SPiCorrelated[i].data()), "l");

        fileSystK0SPiCorrelated[i]->Close();

        for (int j = 0; j < nbin_mult; j++) {
            h1SystTotK0SPiCorr->SetBinContent(j+1, TMath::Sqrt(TMath::Power(h1SystTotK0SPiCorr->GetBinContent(j+1), 2) + TMath::Power(h1SystK0SPiCorrelated[i]->GetBinContent(j+1), 2)));
        }
    }

    cSystTotK0SPiCorr->cd();
    gPad->SetLogy();
    h1SystTotK0SPiCorr->SetTitle("; centFT0M (%); #sigma_{syst} (%)");
    h1SystTotK0SPiCorr->SetLineColor(kBlack);
    h1SystTotK0SPiCorr->SetLineWidth(2);
    h1SystTotK0SPiCorr->SetMarkerColor(kBlack);
    //h1SystTotK0SPiCorr->Draw("HIST SAME");
    //legSystK0SPiCorr3->AddEntry(h1SystTotK0SPiCorr, "Total", "l");
    legSystK0SPiCorr->Draw("same");
    legSystK0SPiCorr2->Draw("same");
    legSystK0SPiCorr3->Draw("same");
    
    TH1F* h1SystTotK0SPi = new TH1F("h1SystTotK0SPi", "; centFT0M (%); #sigma_{syst}", nbin_mult, mult_axis);
    for (int j = 0; j < nbin_mult; j++) {
        h1SystTotK0SPi->SetBinContent(j+1, TMath::Sqrt(TMath::Power(h1SystTotK0S->GetBinContent(j+1), 2) + TMath::Power(h1SystTotPi->GetBinContent(j+1), 2) + TMath::Power(h1SystTotK0SPiCorr->GetBinContent(j+1), 2) + 0.06 * 0.06));
    }

    TCanvas* cSystTotK0SPi = new TCanvas("cSystK0SPi", "cSystK0SPi", 800, 800);
    cSystTotK0SPi->cd();
    h1SystTotK0SPi->Draw("HIST");

    Double_t systRatioK0SPi[nbin_deltay][nbin_mult]{}, systRatioK0SPi2[nbin_deltay][nbin_mult]{};
    for (int i = 0; i < nbin_deltay; i++) {
        if (i != 0) continue;
        for (int j = 0; j < nbin_mult; j++) {
            systRatioK0SPi[i][j] = ratioK0SPi[i][j] * h1SystTotK0SPi->GetBinContent(j+1);
            systRatioK0SPi2[i][j] = ratioK0SPi2[i][j] * h1SystTotK0SPi->GetBinContent(j+1);
        }
    }

    TMultiGraph* mgRatioK0SPi = new TMultiGraph();
    TMultiGraph* mgRatioK0SPiSyst = new TMultiGraph();

    TGraphAsymmErrors* RatioK0SPi[nbin_deltay];
    TGraphAsymmErrors* RatioK0SPi2[nbin_deltay];
    TGraphAsymmErrors* RatioK0SPiSyst[nbin_deltay];
    TGraphAsymmErrors* RatioK0SPiSyst2[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        RatioK0SPi[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPi[i], errmult_down, errmult_up, errratioK0SPi[i], errratioK0SPi[i]);
        PlotFeatures(RatioK0SPi[i], 33, ColorsFinal[i+1], 2, 1, ColorsFinal[i+1], 2, 3001, ColorsFinal[i+1], 0.0, mgRatioK0SPi);

        RatioK0SPiSyst[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPi[i], errmult_down, errmult_up, systRatioK0SPi[i], systRatioK0SPi[i]);
        PlotFeatures(RatioK0SPiSyst[i], 33, ColorsFinal[i+1], 2, 1, ColorsFinal[i+1], 2, 1001, ColorsFinal[i+1], 0.3, mgRatioK0SPiSyst);

        RatioK0SPi2[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPi2[i], errmult_down, errmult_up, errratioK0SP2[i], errratioK0SP2[i]);
        //PlotFeatures(RatioK0SPi2[i], 33, ColorsFinal[i+1], 2, 1, ColorsFinal[i+1], 2, 3001, ColorsFinal[i+1], 0.4, mgRatioK0SPi);

        RatioK0SPiSyst2[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPi2[i], errmult_down, errmult_up, systRatioK0SPi2[i], systRatioK0SPi2[i]);
        //PlotFeatures(RatioK0SPiSyst2[i], 33, ColorsFinal[i+1], 2, 1, ColorsFinal[i+1], 2, 1001, ColorsFinal[i+1], 0.4, mgRatioK0SPiSyst);
    }

    TFile* inFileK0SPiRatioPythiaRopes = new TFile("MCModels/analysis_phi_pythiaRopes.root", "READ");
    TFile* inFileK0SPiRatioEpos4 = new TFile("MCModels/analysis_phi_epos4.root", "READ");

    Double_t xPythiaRopes[nbin_mult], errxPythiaRopes[nbin_mult];
    Double_t ratioK0SPythiaRopes[nbin_mult], errratioK0SPythiaRopes[nbin_mult];

    Double_t xEpos4[nbin_mult], errxEpos4[nbin_mult];
    Double_t ratioK0SEpos4[nbin_mult], errratioK0SEpos4[nbin_mult];

    for (int j = 0; j < nbin_mult; j++) {
        xPythiaRopes[j] = GetMeanContentHisto("hNCh", inFileK0SPiRatioPythiaRopes, j+1);
        errxPythiaRopes[j] = GetMeanErrorHisto("hNCh", inFileK0SPiRatioPythiaRopes, j+1);
              
        ratioK0SPythiaRopes[j] = GetRatioPlusHisto1Input("hNK0s", "hNPion", inFileK0SPiRatioPythiaRopes, j+1);
        errratioK0SPythiaRopes[j] = GetRatioErrorPlusHisto1Input("hNK0s", "hNPion", inFileK0SPiRatioPythiaRopes, j+1);
        
        xEpos4[j] = GetMeanContentHisto("hNCh", inFileK0SPiRatioEpos4, j+1);
        errxEpos4[j] = GetMeanErrorHisto("hNCh", inFileK0SPiRatioEpos4, j+1);

        ratioK0SEpos4[j] = GetRatioPlusHisto1Input("hNK0s", "hNPion", inFileK0SPiRatioEpos4, j+1);
        errratioK0SEpos4[j] = GetRatioErrorPlusHisto1Input("hNK0s", "hNPion", inFileK0SPiRatioEpos4, j+1);
    }

    TMultiGraph* mgRatioK0SPiMC = new TMultiGraph();

    TGraphAsymmErrors* RatioK0SPythiaRopes = new TGraphAsymmErrors(nbin_mult, xPythiaRopes, ratioK0SPythiaRopes, errxPythiaRopes, errxPythiaRopes, errratioK0SPythiaRopes, errratioK0SPythiaRopes);
    RatioK0SPythiaRopes->Scale(2.0);
    PlotFeatures(RatioK0SPythiaRopes, 33, kOrange-3, 0, 2, kOrange-3, 2, 3001, kOrange-3, 0.3, mgRatioK0SPiMC);

    TGraphAsymmErrors* RatioK0SEpos4 = new TGraphAsymmErrors(nbin_mult, xEpos4, ratioK0SEpos4, errxEpos4, errxEpos4, errratioK0SEpos4, errratioK0SEpos4);
    RatioK0SEpos4->Scale(2.0);
    PlotFeatures(RatioK0SEpos4, 33, kMagenta+3, 0, 3, kMagenta+3, 2, 3001, kMagenta+3, 0.3, mgRatioK0SPiMC);

    TMultiGraph* mgRatioK0SPiRun2 = new TMultiGraph();
    TMultiGraph* mgRatioK0SPiRun2Syst = new TMultiGraph();

    TFile* inFileK0SPiRun2 = new TFile("HEPData-ins1784041-v1-Table_18.root", "READ");
    TH1D* h1RatioK0SRun2Value = (TH1D*)inFileK0SPiRun2->Get("Table 18/Hist1D_y1");
    TH1D* h1RatioK0SRun2StatError = (TH1D*)inFileK0SPiRun2->Get("Table 18/Hist1D_y1_e1");
    TH1D* h1RatioK0SRun2SystError = (TH1D*)inFileK0SPiRun2->Get("Table 18/Hist1D_y1_e2");

    int nBins = h1RatioK0SRun2Value->GetNbinsX();

    // Crea due grafici separati
    TGraphErrors* RatioK0SRun2 = new TGraphErrors(nBins);
    TGraphErrors* RatioK0SRun2Syst = new TGraphErrors(nBins);

    // Loop sui bin
    for (int j = 1; j <= nBins; j++) {
        double xCenter = h1RatioK0SRun2Value->GetBinCenter(j);
        double binWidth = h1RatioK0SRun2Value->GetBinWidth(j);
        double yValue = h1RatioK0SRun2Value->GetBinContent(j);

        double statY = h1RatioK0SRun2StatError->GetBinContent(j);
        double systY = h1RatioK0SRun2SystError->GetBinContent(j);

        // Imposta i punti nel grafico con indice j-1
        RatioK0SRun2->SetPoint(j-1, xCenter, yValue);
        RatioK0SRun2->SetPointError(j-1, binWidth/2., statY); // errore in X e Y

        RatioK0SRun2Syst->SetPoint(j-1, xCenter, yValue);
        RatioK0SRun2Syst->SetPointError(j-1, binWidth/2., systY);
    }

    /*TGraphErrors* RatioK0SRun2 = (TGraphErrors*)inFileK0SPiRun2->Get("Table 18/Graph1D_y1");
    TGraphErrors* RatioK0SRun2Syst = new TGraphErrors(*RatioK0SRun2);
    //TGraphErrors* RatioK0SRun2 = new TGraphErrors(h1RatioK0SRun2Value);
    //TGraphErrors* RatioK0SRun2Syst = new TGraphErrors(h1RatioK0SRun2Value);
    for (int j = 1; j <= h1RatioK0SRun2Value->GetNbinsX(); j++) {
        RatioK0SRun2->SetPointError(j-1, RatioK0SRun2->GetErrorX(j-1), h1RatioK0SRun2StatError->GetBinContent(j));
        RatioK0SRun2Syst->SetPointError(j-1, RatioK0SRun2->GetErrorX(j-1), h1RatioK0SRun2SystError->GetBinContent(j));
    }*/
    PlotFeatures(RatioK0SRun2, 33, kBlack, 2, 1, kBlack, 2, 3001, kBlack, 0.0, mgRatioK0SPiRun2);
    PlotFeatures(RatioK0SRun2Syst, 33, kBlack, 2, 1, kBlack, 2, 1001, kBlack, 0.3, mgRatioK0SPiRun2Syst);

    /*vector<Double_t> fracCrossSecTrueRun2 = {0.9, 3.6, 4.4, 4.6, 4.5, 9.0, 9.1, 9.2, 19.2, 35.5};
    
    TFile* inFileK0SMBRun2 = new TFile("HEPData-ins1748157-v1-Table_11a.root", "READ");
    TDirectoryFile* dirK0SMBRun2 = (TDirectoryFile*)inFileK0SMBRun2->Get("Table 11a");
    TH1F* hK0SMBRun2 = (TH1F*)dirK0SMBRun2->Get("Hist1D_y1");
    hK0SMBRun2->SetDirectory(0);
    TH1F* hK0SMBRun2Stat = (TH1F*)dirK0SMBRun2->Get("Hist1D_y1_e1");
    hK0SMBRun2Stat->SetDirectory(0);
    TH1F* hK0SMBRun2Syst = (TH1F*)dirK0SMBRun2->Get("Hist1D_y1_e2");
    hK0SMBRun2Syst->SetDirectory(0);
    TGraphAsymmErrors* gK0SMBRun2 = (TGraphAsymmErrors*)dirK0SMBRun2->Get("Graph1D_y1");
    //dirK0SMBRun2->Close();
    //inFileK0SMBRun2->Close();

    TFile* inFilePiMultDepRun2 = new TFile("OutputYields_pp13mult.root", "READ");
    TGraphAsymmErrors* gPiMultDepWStatRun2 = (TGraphAsymmErrors*)inFilePiMultDepRun2->Get("pi_Stat");
    TGraphAsymmErrors* gPiMultDepWSystRun2 = (TGraphAsymmErrors*)inFilePiMultDepRun2->Get("pi_Syst");
    inFilePiMultDepRun2->Close();

    Double_t K0SMBRun2 = hK0SMBRun2->GetBinContent(1);
    Double_t statErrK0SMBRun2 = hK0SMBRun2Stat->GetBinContent(1);
    Double_t systErrK0SMBRun2 = hK0SMBRun2Syst->GetBinContent(1);

    Double_t* PiMultDepRun2 = gPiMultDepWSystRun2->GetY();
    Double_t* statErrPiMultDepRun2 = gPiMultDepWStatRun2->GetEYhigh();
    Double_t* systHighErrPiMultDepRun2 = gPiMultDepWSystRun2->GetEYhigh();
    Double_t* systLowErrPiMultDepRun2 = gPiMultDepWSystRun2->GetEYlow();

    Double_t PiMBRun2 = 0, statErrPiMBRun2 = 0, systHighErrPiMBRun2 = 0, systLowErrPiMBRun2 = 0;
    for (int j = 0; j < nbin_mult; j++) {
        PiMBRun2 += PiMultDepRun2[j] * fracCrossSecTrueRun2[j];
        statErrPiMBRun2 += TMath::Power(statErrPiMultDepRun2[j] * fracCrossSecTrueRun2[j], 2);
        systHighErrPiMBRun2 += TMath::Power(systHighErrPiMultDepRun2[j] * fracCrossSecTrueRun2[j], 2);
        systLowErrPiMBRun2 += TMath::Power(systLowErrPiMultDepRun2[j] * fracCrossSecTrueRun2[j], 2);
    }
    PiMBRun2 /= 100.0;
    statErrPiMBRun2 = TMath::Sqrt(statErrPiMBRun2) / 100.0;
    systHighErrPiMBRun2 = TMath::Sqrt(systHighErrPiMBRun2) / 100.0;
    systLowErrPiMBRun2 = TMath::Sqrt(systLowErrPiMBRun2) / 100.0;
    
    Double_t ratioK0SMBRun2 = 2.0 * K0SMBRun2 / PiMBRun2;
    Double_t statErrRatioK0SMBRun2 = ratioK0SMBRun2 * TMath::Sqrt(TMath::Power(statErrK0SMBRun2 / K0SMBRun2, 2) + TMath::Power(statErrPiMBRun2 / PiMBRun2, 2));
    Double_t systHighErrRatioK0SMBRun2 = ratioK0SMBRun2 * TMath::Sqrt(TMath::Power(systErrK0SMBRun2 / K0SMBRun2, 2) + TMath::Power(systHighErrPiMBRun2 / PiMBRun2, 2));
    Double_t systLowErrRatioK0SMBRun2 = ratioK0SMBRun2 * TMath::Sqrt(TMath::Power(systErrK0SMBRun2 / K0SMBRun2, 2) + TMath::Power(systLowErrPiMBRun2 / PiMBRun2, 2));

    Double_t* xMBRun2 = gK0SMBRun2->GetX();
    Double_t* errHighXMBRun2 = gK0SMBRun2->GetEXhigh();
    Double_t* errLowXMBRun2 = gK0SMBRun2->GetEXlow();

    TGraphAsymmErrors* RatioK0SMBRun2 = new TGraphAsymmErrors(1, xMBRun2, &ratioK0SMBRun2, errLowXMBRun2, errHighXMBRun2, &statErrRatioK0SMBRun2, &statErrRatioK0SMBRun2);
    PlotFeatures(RatioK0SMBRun2, 33, kBlack, 2, 1, kBlack, 2, 3001, kBlack, 0.4, mgRatioK0SPi);

    TGraphAsymmErrors* RatioK0SMBRun2Syst = new TGraphAsymmErrors(1, xMBRun2, &ratioK0SMBRun2, errLowXMBRun2, errHighXMBRun2, &systLowErrRatioK0SMBRun2, &systHighErrRatioK0SMBRun2);
    PlotFeatures(RatioK0SMBRun2Syst, 33, kBlack, 2, 1, kBlack, 2, 1001, kBlack, 0.3, mgRatioK0SPiSyst);*/

    TFile* inFileK0SPiRatioPythiaRopesRun2 = new TFile("MCModels/analysis_minbias_pythiaRopes.root", "READ");
    TFile* inFileK0SPiRatioEpos4Run2 = new TFile("MCModels/analysis_minbias_epos4.root", "READ");

    Double_t xPythiaRopesRun2[nbin_mult], errxPythiaRopesRun2[nbin_mult];
    Double_t ratioK0SPythiaRopesRun2[nbin_mult], errratioK0SPythiaRopesRun2[nbin_mult];

    Double_t xEpos4Run2[nbin_mult], errxEpos4Run2[nbin_mult];
    Double_t ratioK0SEpos4Run2[nbin_mult], errratioK0SEpos4Run2[nbin_mult];

    for (int j = 0; j < nbin_mult; j++) {
        xPythiaRopesRun2[j] = GetMeanContent("h_dndeta", inFileK0SPiRatioPythiaRopesRun2, j+1);
        errxPythiaRopesRun2[j] = GetMeanContentError("h_dndeta", inFileK0SPiRatioPythiaRopesRun2, j+1);
              
        ratioK0SPythiaRopesRun2[j] = GetRatio("h_dndyK0s", "h_dndyPion", inFileK0SPiRatioPythiaRopesRun2, j+1);
        errratioK0SPythiaRopesRun2[j] = GetRatioError("h_dndyK0s", "h_dndyPion", inFileK0SPiRatioPythiaRopesRun2, j+1);
        
        xEpos4Run2[j] = GetMeanContent("h_dndeta", inFileK0SPiRatioEpos4Run2, j+1);
        errxEpos4Run2[j] = GetMeanContentError("h_dndeta", inFileK0SPiRatioEpos4Run2, j+1);

        ratioK0SEpos4Run2[j] = GetRatio("h_dndyK0s", "h_dndyPion", inFileK0SPiRatioEpos4Run2, j+1);
        errratioK0SEpos4Run2[j] = GetRatioError("h_dndyK0s", "h_dndyPion", inFileK0SPiRatioEpos4Run2, j+1);
    }

    TMultiGraph* mgRatioK0SPiMCRun2 = new TMultiGraph();

    TGraphAsymmErrors* RatioK0SPythiaRopesRun2 = new TGraphAsymmErrors(nbin_mult, xPythiaRopesRun2, ratioK0SPythiaRopesRun2, errxPythiaRopesRun2, errxPythiaRopesRun2, errratioK0SPythiaRopesRun2, errratioK0SPythiaRopesRun2);
    RatioK0SPythiaRopesRun2->Scale(2.0);
    PlotFeatures(RatioK0SPythiaRopesRun2, 33, kOrange-3, 0, 2, kOrange-3, 2, 3001, kOrange-3, 0.3, mgRatioK0SPiMCRun2);

    TGraphAsymmErrors* RatioK0SEpos4Run2 = new TGraphAsymmErrors(nbin_mult, xEpos4Run2, ratioK0SEpos4Run2, errxEpos4Run2, errxEpos4Run2, errratioK0SEpos4Run2, errratioK0SEpos4Run2);
    RatioK0SEpos4Run2->Scale(2.0);
    PlotFeatures(RatioK0SEpos4Run2, 33, kMagenta+3, 0, 3, kMagenta+3, 2, 3001, kMagenta+3, 0.3, mgRatioK0SPiMCRun2);

    TH1D* hDummy = new TH1D("hDummy", "", 2, -0.0001, 31);

    TCanvas* cRatioK0SPiWPhi = new TCanvas("cRatioK0SPiWPhi", "cRatioK0SPiWPhi", 1400, 1000);
    cRatioK0SPiWPhi->cd();
    cRatioK0SPiWPhi->SetTicks(1, 1);
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    gPad->SetMargin(0.14,0.03,0.13,0.06);
    hDummy->SetTitle("; #LTd#it{N}_{ch}/d#it{#eta}#GT_{|#it{#eta}|<0.5} ; 2K^{0}_{S} / (#pi^{+}+#pi^{#minus}) in events with #phi");
    hDummy->GetXaxis()->SetLabelSize(0.045);
    hDummy->GetXaxis()->SetLabelOffset(0.01);
    hDummy->GetXaxis()->SetTitleSize(0.045);
    hDummy->GetXaxis()->SetTitleOffset(1.3);
    hDummy->GetYaxis()->SetLabelSize(0.045);
    hDummy->GetYaxis()->SetTitleSize(0.045);
    hDummy->GetYaxis()->SetTitleOffset(1.2);
    hDummy->GetYaxis()->SetRangeUser(0.07, 0.285);
    hDummy->Draw();

    mgRatioK0SPiMC->Draw("3L");
    mgRatioK0SPi->Draw("P");
    mgRatioK0SPiSyst->Draw("PE2");
    
    TLegend* legRatioK0SPi1WPhi_ = new TLegend(0.15, 0.855, 0.4, 0.91);
    legRatioK0SPi1WPhi_->SetHeader("ALICE Preliminary");
    legRatioK0SPi1WPhi_->SetTextSize(0.045);
    legRatioK0SPi1WPhi_->SetLineWidth(0);
    legRatioK0SPi1WPhi_->Draw("same");

    TLegend* legRatioK0SPi2WPhi_ = new TLegend(0.15, 0.82, 0.4, 0.85);
    legRatioK0SPi2WPhi_->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, #left|#it{y}#right| < 0.5, #left|#Delta#it{y}#right|_{#phi-h} < 1.0");
    legRatioK0SPi2WPhi_->SetTextSize(0.04);
    legRatioK0SPi2WPhi_->SetLineWidth(0);
    legRatioK0SPi2WPhi_->Draw("same");

    TLegend* legRatioK0SPi3WPhi_ = new TLegend(0.15, 0.77, 0.4, 0.81);
    legRatioK0SPi3WPhi_->SetHeader("0.4 < #it{p}_{T} (#phi) < 10 GeV/#it{c}");
    legRatioK0SPi3WPhi_->SetTextSize(0.04);
    legRatioK0SPi3WPhi_->SetLineWidth(0);
    legRatioK0SPi3WPhi_->Draw("same");

    TLegend* legRatioK0SPi6WPhi_ = new TLegend(0.62, 0.63, 0.85, 0.71);
    for (int i = 0; i < nbin_deltay; i++) {
        if (i != 0) continue;
        //if (i == 1 || i == 3) continue;
        //legRatioK0SPi5_->AddEntry(RatioK0SPi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
        legRatioK0SPi6WPhi_->AddEntry(RatioK0SPi[i], "stat", "ep");
        legRatioK0SPi6WPhi_->AddEntry(RatioK0SPiSyst[i], "syst", "f");
    }
    legRatioK0SPi6WPhi_->SetTextSize(0.04);
    legRatioK0SPi6WPhi_->SetLineWidth(0);
    legRatioK0SPi6WPhi_->Draw("same");

    TLegend* legRatioK0SPi7WPhi_ = new TLegend(0.62, 0.74, 0.85, 0.86);
    legRatioK0SPi7WPhi_->AddEntry(RatioK0SPythiaRopes, "PYTHIA8.3 QCD-CR+Ropes", "fl");
    legRatioK0SPi7WPhi_->AddEntry(RatioK0SEpos4, "EPOS4", "fl");
    legRatioK0SPi7WPhi_->SetTextSize(0.04);
    legRatioK0SPi7WPhi_->SetLineWidth(0);
    legRatioK0SPi7WPhi_->Draw("same");

    TCanvas* cRatioK0SPi = new TCanvas("cRatioK0SPi", "cRatioK0SPi", 1400, 1000);

    double leftPadWidth = 0.5;  // 50% a sinistra, 50% a destra

    double lLeft   = 0.15;
    double rLeft   = 0.00;
    double lRight  = 0.00;
    double rRight  = 0.03;
    
    double X = (1 - lRight - rRight) /
           ((1 - lLeft - rLeft) + (1 - lRight - rRight));

    double scaleFactor = (1 - X) / X;

    TPad* leftPad = new TPad("leftPad", "Left Pad", 0, 0, X, 1);
    leftPad->SetRightMargin(rLeft);   // attacca a destra
    leftPad->SetLeftMargin(lLeft);
    leftPad->SetBottomMargin(0.14);
    leftPad->SetTopMargin(0.05);
    leftPad->SetTicks(1,1);
    leftPad->Draw();

    TPad* rightPad = new TPad("rightPad", "Right Pad", X, 0, 1, 1);
    rightPad->SetLeftMargin(lRight);   // attacca a sinistra
    rightPad->SetRightMargin(rRight);
    rightPad->SetBottomMargin(0.14);
    rightPad->SetTopMargin(0.05);
    rightPad->SetTicks(1,1);
    rightPad->Draw();

    /// =======================
    ///   PANNELLO SINISTRO
    /// =======================
    leftPad->cd();

    TH1D* hDummyLeft = new TH1D("hDummyLeft", "", 100, -0.0001, 33);
    hDummyLeft->SetTitle("; #LTd#it{N}_{ch}/d#it{#eta}#GT_{|#it{#eta}| < 0.5, #phi #rightarrow K^{+}K^{#minus} trigger} ; 2K^{0}_{S} / (#pi^{+}+#pi^{#minus})");
    hDummyLeft->GetXaxis()->SetLabelSize(0.045);
    hDummyLeft->GetXaxis()->SetLabelOffset(0.01);
    hDummyLeft->GetXaxis()->SetTitleSize(0.045);
    hDummyLeft->GetXaxis()->SetTitleOffset(1.3);
    hDummyLeft->GetXaxis()->SetRangeUser(0.5, 33);
    hDummyLeft->GetYaxis()->SetLabelSize(0.045);
    hDummyLeft->GetYaxis()->SetTitleSize(0.045);
    hDummyLeft->GetYaxis()->SetTitleOffset(1.6);
    hDummyLeft->GetYaxis()->SetRangeUser(0.07, 0.27);
    hDummyLeft->Draw();

    mgRatioK0SPiMC->Draw("3L");
    mgRatioK0SPi->Draw("P");
    mgRatioK0SPiSyst->Draw("PE2");

    TLegend* legRatioK0SPi1_ = new TLegend(0.19, 0.855, 0.44, 0.91);
    legRatioK0SPi1_->SetHeader("ALICE Preliminary");
    legRatioK0SPi1_->SetTextSize(0.045);
    legRatioK0SPi1_->SetLineWidth(0);
    legRatioK0SPi1_->Draw("same");

    TLegend* legRatioK0SPi2_ = new TLegend(0.19, 0.814, 0.44, 0.844);
    legRatioK0SPi2_->SetHeader("pp #sqrt{#it{s}} = 13.6 TeV #phi #rightarrow K^{+}K^{#minus} candidate trigger");
    legRatioK0SPi2_->SetTextSize(0.04);
    legRatioK0SPi2_->SetLineWidth(0);
    legRatioK0SPi2_->Draw("same");

    TLegend* legRatioK0SPi9_ = new TLegend(0.19, 0.766, 0.44, 0.80);
    legRatioK0SPi9_->SetHeader("|#it{y}| < 0.5, |#Delta#it{y}|_{#phi-h} < 1.0");
    legRatioK0SPi9_->SetTextSize(0.04);
    legRatioK0SPi9_->SetLineWidth(0);
    legRatioK0SPi9_->Draw("same");

    TLegend* legRatioK0SPi3_ = new TLegend(0.19, 0.706, 0.43, 0.75);
    legRatioK0SPi3_->SetHeader("0.4 < #it{p}_{T} (#phi) < 10 GeV/#it{c}");
    legRatioK0SPi3_->SetTextSize(0.04);
    legRatioK0SPi3_->SetLineWidth(0);
    legRatioK0SPi3_->Draw("same");

    TLegend* legRatioK0SPi6_ = new TLegend(0.797, 0.706, 0.95, 0.80);
    for (int i = 0; i < nbin_deltay; i++) {
        if (i != 0) continue;
        //if (i == 1 || i == 3) continue;
        //legRatioK0SPi5_->AddEntry(RatioK0SPi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
        legRatioK0SPi6_->AddEntry(RatioK0SPi[i], "stat", "ep");
        legRatioK0SPi6_->AddEntry(RatioK0SPiSyst[i], "syst", "f");
    }
    legRatioK0SPi6_->SetTextSize(0.04);
    legRatioK0SPi6_->SetLineWidth(0);
    legRatioK0SPi6_->Draw("same");

    /// =======================
    ///   PANNELLO DESTRO
    /// =======================
    rightPad->cd();

    TH1D* hDummyRight = new TH1D("hDummyRight", "", 100, -0.0001, 33);
    hDummyRight->SetTitle("; #LTd#it{N}_{ch}/d#it{#eta}#GT_{|#it{#eta}| < 0.5} ;");
    hDummyRight->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
    hDummyRight->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
    hDummyRight->GetXaxis()->SetTitleOffset(1.3 * scaleFactor);
    hDummyRight->GetXaxis()->SetRangeUser(0.5, 33);
    hDummyRight->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
    hDummyRight->GetYaxis()->SetTitleSize(0.045 / scaleFactor);
    hDummyRight->GetYaxis()->SetTitleOffset(1.2);
    hDummyRight->GetYaxis()->SetRangeUser(0.07, 0.27);
    hDummyRight->Draw();

    mgRatioK0SPiMCRun2->Draw("3L");
    mgRatioK0SPiRun2->Draw("P");
    mgRatioK0SPiRun2Syst->Draw("PE2");

    TLegend* legRatioK0SPi7_ = new TLegend(0.4, 0.766, 0.63, 0.844);
    legRatioK0SPi7_->AddEntry(RatioK0SPythiaRopes, "PYTHIA8.3 QCD-CR+Ropes", "fl");
    legRatioK0SPi7_->AddEntry(RatioK0SEpos4, "EPOS4", "fl");
    legRatioK0SPi7_->SetTextSize(0.035 / scaleFactor);
    legRatioK0SPi7_->SetLineWidth(0);
    legRatioK0SPi7_->Draw("same");

    TLegend* legRatioK0SPi11_ = new TLegend(0.06, 0.855, 0.32, 0.91);
    legRatioK0SPi11_->SetHeader("ALICE, EPJC 80 167 (2020)");
    legRatioK0SPi11_->SetTextSize(0.045 / scaleFactor);
    legRatioK0SPi11_->SetLineWidth(0);
    legRatioK0SPi11_->Draw("same");

    TLegend* legRatioK0SPi8_ = new TLegend(0.06, 0.814, 0.32, 0.844);
    legRatioK0SPi8_->SetHeader("pp #sqrt{#it{s}} = 13 TeV");
    legRatioK0SPi8_->SetTextSize(0.04 / scaleFactor);
    legRatioK0SPi8_->SetLineWidth(0);
    legRatioK0SPi8_->Draw("same");

    TLegend* legRatioK0SPi10_ = new TLegend(0.06, 0.766, 0.32, 0.80);
    legRatioK0SPi10_->SetHeader("|#it{y}| < 0.5");
    legRatioK0SPi10_->SetTextSize(0.04 / scaleFactor);
    legRatioK0SPi10_->SetLineWidth(0);
    legRatioK0SPi10_->Draw("same");

    gStyle->SetCanvasPreferGL(kTRUE);

    cRatioK0SPi->SaveAs("/Users/stefanocannito/Downloads/updateK0SPi2.root");
    cRatioK0SPi->SaveAs("/Users/stefanocannito/Downloads/updateK0SPi2.pdf");
    cRatioK0SPi->SaveAs("/Users/stefanocannito/Downloads/updateK0SPi2.eps");

    //return;









    //********************************************************************************************
    // MC Closure Test
    //********************************************************************************************

    TH2D* h2PhiK0SGenMC[nbin_deltay];
    TH2D* h2PhiPiGenMC[nbin_deltay];

    string nameK0S[nbin_deltay] = {"h2PhiK0SGenMCInc", "h2PhiK0SGenMCFCut", "h2PhiK0SGenMCSCut"};
    string namePi[nbin_deltay] = {"h2PhiPiGenMCInc", "h2PhiPiGenMCFCut", "h2PhiPiGenMCSCut"};

    //TFile* fileMCClosure = TFile::Open("AnalysisResultsMC.root");
    TFile* fileMCClosure = TFile::Open("AnalysisResultsMC3.root");

    TDirectoryFile* phik0shortanalysis = (TDirectoryFile*)fileMCClosure->Get("phik0shortanalysis_id26339");
    TDirectoryFile* mcPhiK0S = (TDirectoryFile*)phik0shortanalysis->Get("mcPhiK0SHist");
    TDirectoryFile* mcPhiPi = (TDirectoryFile*)phik0shortanalysis->Get("mcPhiPionHist");

    TDirectoryFile* phik0shortanalysis2 = (TDirectoryFile*)fileMCClosure->Get("phik0shortanalysis_newDeltaYClasses_id26339");
    TDirectoryFile* mcPhiK0S2 = (TDirectoryFile*)phik0shortanalysis2->Get("mcPhiK0SHist");
    TDirectoryFile* mcPhiPi2 = (TDirectoryFile*)phik0shortanalysis2->Get("mcPhiPionHist");

    /*for (int i = 0; i < nbin_deltay; i++) {
        h2PhiK0SGenMC[i] = (TH2D*)mcPhiK0S->Get(nameK0S[i].c_str());
        h2PhiK0SGenMC[i]->SetDirectory(0);

        h2PhiPiGenMC[i] = (TH2D*)mcPhiPi->Get(namePi[i].c_str());
        h2PhiPiGenMC[i]->SetDirectory(0);
    }*/
    h2PhiK0SGenMC[0] = (TH2D*)mcPhiK0S->Get(nameK0S[0].c_str());
    h2PhiK0SGenMC[0]->SetDirectory(0);
    h2PhiK0SGenMC[1] = (TH2D*)mcPhiK0S2->Get(nameK0S[1].c_str());
    h2PhiK0SGenMC[1]->SetDirectory(0);
    h2PhiK0SGenMC[1]->SetName("h2PhiK0SGenMCFCut1");
    h2PhiK0SGenMC[2] = (TH2D*)mcPhiK0S->Get(nameK0S[1].c_str());
    h2PhiK0SGenMC[2]->SetDirectory(0);
    h2PhiK0SGenMC[2]->SetName("h2PhiK0SGenMCFCut2");
    h2PhiK0SGenMC[3] = (TH2D*)mcPhiK0S2->Get(nameK0S[2].c_str());
    h2PhiK0SGenMC[3]->SetDirectory(0);
    h2PhiK0SGenMC[3]->SetName("h2PhiK0SGenMCSCut1");
    h2PhiK0SGenMC[4] = (TH2D*)mcPhiK0S->Get(nameK0S[2].c_str());
    h2PhiK0SGenMC[4]->SetDirectory(0);
    h2PhiK0SGenMC[4]->SetName("h2PhiK0SGenMCSCut2");

    h2PhiPiGenMC[0] = (TH2D*)mcPhiPi->Get(namePi[0].c_str());
    h2PhiPiGenMC[0]->SetDirectory(0);
    h2PhiPiGenMC[1] = (TH2D*)mcPhiPi2->Get(namePi[1].c_str());
    h2PhiPiGenMC[1]->SetDirectory(0);
    h2PhiPiGenMC[1]->SetName("h2PhiPiGenMCFCut1");
    h2PhiPiGenMC[2] = (TH2D*)mcPhiPi->Get(namePi[1].c_str());
    h2PhiPiGenMC[2]->SetDirectory(0);
    h2PhiPiGenMC[2]->SetName("h2PhiPiGenMCFCut2");
    h2PhiPiGenMC[3] = (TH2D*)mcPhiPi2->Get(namePi[2].c_str());
    h2PhiPiGenMC[3]->SetDirectory(0);
    h2PhiPiGenMC[3]->SetName("h2PhiPiGenMCSCut1");
    h2PhiPiGenMC[4] = (TH2D*)mcPhiPi->Get(namePi[2].c_str());
    h2PhiPiGenMC[4]->SetDirectory(0);
    h2PhiPiGenMC[4]->SetName("h2PhiPiGenMCSCut2");

    mcPhiK0S->Close();
    mcPhiK0S2->Close();
    mcPhiPi->Close();
    mcPhiPi2->Close();
    phik0shortanalysis->Close();
    phik0shortanalysis2->Close();

    Double_t PhiK0SpTIntGenMC[nbin_deltay][nbin_mult] = {0}, errPhiK0SpTIntGenMC[nbin_deltay][nbin_mult] = {0};
    Double_t PhiPipTIntGenMC[nbin_deltay][nbin_mult] = {0}, errPhiPipTIntGenMC[nbin_deltay][nbin_mult] = {0};
    Double_t ratioK0SPiGenMC[nbin_deltay][nbin_mult] = {0}, errratioK0SPiGenMC[nbin_deltay][nbin_mult] = {0};
    Double_t ratioMCClosureK0SPi[nbin_deltay][nbin_mult] = {0}, errratioMCClosureK0SPi[nbin_deltay][nbin_mult] = {0};

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            /*for (int k = 0; k < nbin_pTK0S; k++) {
                PhiK0SpTIntGenMC[i][j] += h2PhiK0SGenMC[i]->GetBinContent(j+1, k+1) / (pTK0S_axis[k+1] - pTK0S_axis[k]);
                errPhiK0SpTIntGenMC[i][j] += TMath::Power(h2PhiK0SGenMC[i]->GetBinError(j+1, k+1) / (pTK0S_axis[k+1] - pTK0S_axis[k]), 2);
            }
            errPhiK0SpTIntGenMC[i][j] = TMath::Sqrt(errPhiK0SpTIntGenMC[i][j]);

            for (int k = 0; k < nbin_pTPi; k++) {
                PhiPipTIntGenMC[i][j] += h2PhiPiGenMC[i]->GetBinContent(j+1, k+1) / (pTPi_axis[k+1] - pTPi_axis[k]);
                errPhiPipTIntGenMC[i][j] += TMath::Power(h2PhiPiGenMC[i]->GetBinError(j+1, k+1) / (pTPi_axis[k+1] - pTPi_axis[k]), 2);
            }
            errPhiPipTIntGenMC[i][j] = TMath::Sqrt(errPhiPipTIntGenMC[i][j]);*/

            PhiK0SpTIntGenMC[i][j] = h2PhiK0SGenMC[i]->ProjectionX(Form("h1PhiK0SpTIntGenMC%i_%i", i, j), 1, nbin_pTK0S+1)->GetBinContent(j+1);
            //PhiK0SpTIntGenMC[i][j] /= (deltay_axis[i] * (mult_axis[j+1] - mult_axis[j]));
            errPhiK0SpTIntGenMC[i][j] = h2PhiK0SGenMC[i]->ProjectionX(Form("h1PhiK0SpTIntGenMC%i_%i", i, j), 1, nbin_pTK0S+1)->GetBinError(j+1);
            
            PhiPipTIntGenMC[i][j] = h2PhiPiGenMC[i]->ProjectionX(Form("h1PhiPipTIntGenMC%i_%i", i, j), 2, nbin_pTPi+1)->GetBinContent(j+1);
            //PhiPipTIntGenMC[i][j] /= (deltay_axis[i] * (mult_axis[j+1] - mult_axis[j]));
            errPhiPipTIntGenMC[i][j] = h2PhiPiGenMC[i]->ProjectionX(Form("h1PhiPipTIntGenMC%i_%i", i, j), 2, nbin_pTPi+1)->GetBinError(j+1);

            ratioK0SPiGenMC[i][j] = 2 * PhiK0SpTIntGenMC[i][j] / PhiPipTIntGenMC[i][j];
            errratioK0SPiGenMC[i][j] = ratioK0SPiGenMC[i][j] * TMath::Sqrt(TMath::Power(errPhiK0SpTIntGenMC[i][j] / PhiK0SpTIntGenMC[i][j], 2) + TMath::Power(errPhiPipTIntGenMC[i][j] / PhiPipTIntGenMC[i][j], 2));

            //********************************************************************************************

            //ratioMCClosureK0SPi[i][j] = PhiK0SYieldpTint[i][j] / PhiK0SpTIntGenMC[i][j];
            //errratioMCClosureK0SPi[i][j] = ratioMCClosureK0SPi[i][j] * TMath::Sqrt(TMath::Power(errPhiK0SYieldpTint[i][j] / PhiK0SYieldpTint[i][j], 2) + TMath::Power(errPhiK0SpTIntGenMC[i][j] / PhiK0SpTIntGenMC[i][j], 2));
            
            //ratioMCClosureK0SPi[i][j] = PhiPiTOFYieldpTint[i][j] / PhiPipTIntGenMC[i][j];
            //errratioMCClosureK0SPi[i][j] = ratioMCClosureK0SPi[i][j] * TMath::Sqrt(TMath::Power(errPhiPiTOFYieldpTint[i][j] / PhiPiTOFYieldpTint[i][j], 2) + TMath::Power(errPhiPipTIntGenMC[i][j] / PhiPipTIntGenMC[i][j], 2));

            //********************************************************************************************

            ratioMCClosureK0SPi[i][j] = ratioK0SPi[i][j] / ratioK0SPiGenMC[i][j];
            errratioMCClosureK0SPi[i][j] = ratioMCClosureK0SPi[i][j] * TMath::Sqrt(TMath::Power(errratioK0SPi[i][j] / ratioK0SPi[i][j], 2) + TMath::Power(errratioK0SPiGenMC[i][j] / ratioK0SPiGenMC[i][j], 2));
            
        }
    }

    TMultiGraph* mgratioK0SPiGenMC = new TMultiGraph();
    TGraphAsymmErrors* ratioK0SPiGenMCGraph[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        ratioK0SPiGenMCGraph[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPiGenMC[i], errmult_down, errmult_up, errratioK0SPiGenMC[i], errratioK0SPiGenMC[i]);
        PlotFeatures(ratioK0SPiGenMCGraph[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgratioK0SPiGenMC);
    }

    TCanvas* cratioK0SPiGenMC = new TCanvas("cratioK0SPiGenMC", "cratioK0SPiGenMC", 1000, 800);
    cratioK0SPiGenMC->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    gPad->SetMargin(0.14,0.05,0.13,0.06);
    mgratioK0SPiGenMC->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; 2K^{0}_{S} / (#pi^{+} + #pi^{#minus})");
    mgratioK0SPiGenMC->GetXaxis()->SetLabelSize(0.045);
    mgratioK0SPiGenMC->GetXaxis()->SetTitleSize(0.045);
    mgratioK0SPiGenMC->GetXaxis()->SetTitleOffset(1.2);
    mgratioK0SPiGenMC->GetYaxis()->SetLabelSize(0.045);
    mgratioK0SPiGenMC->GetYaxis()->SetTitleSize(0.045);
    mgratioK0SPiGenMC->GetYaxis()->SetTitleOffset(1.55);
    mgratioK0SPiGenMC->Draw("AP");

    TLegend* legratioK0SPiGenMC1 = new TLegend(0.51, 0.85, 0.68, 0.88);
    legratioK0SPiGenMC1->SetHeader("#bf{This work}");
    legratioK0SPiGenMC1->SetTextSize(0.045);
    legratioK0SPiGenMC1->SetLineWidth(0);
    legratioK0SPiGenMC1->Draw("same");

    TLegend* legratioK0SPiGenMC2 = new TLegend(0.51, 0.81, 0.68, 0.85);
    legratioK0SPiGenMC2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legratioK0SPiGenMC2->SetTextSize(0.045);
    legratioK0SPiGenMC2->SetLineWidth(0);
    legratioK0SPiGenMC2->Draw("same");

    TLegend* legratioK0SPiGenMC3 = new TLegend(0.51, 0.77, 0.68, 0.81);
    legratioK0SPiGenMC3->SetHeader("0.1 < #it{p}_{T} K^{0}_{S} < 6.0 GeV/#it{c}");
    legratioK0SPiGenMC3->SetTextSize(0.045);
    legratioK0SPiGenMC3->SetLineWidth(0);
    legratioK0SPiGenMC3->Draw("same");

    TLegend* legratioK0SPiGenMC4 = new TLegend(0.51, 0.57, 0.68, 0.77);
    legratioK0SPiGenMC4->SetHeader("0.3 < #it{p}_{T} (#pi^{+}+#pi^{#minus}) < 3.0 GeV/#it{c}");
    for (int i = 0; i < nbin_deltay; i++) {
        legratioK0SPiGenMC4->AddEntry(RatioK0SPi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legratioK0SPiGenMC4->SetTextSize(0.045);
    legratioK0SPiGenMC4->SetLineWidth(0);
    legratioK0SPiGenMC4->Draw("same");

    /*string outNameK0SPiGenMC = path + "ratioK0SPiGenMC.root";
    cratioK0SPiGenMC->SaveAs(outNameK0SPiGenMC.c_str());
    outNameK0SPiGenMC = path + "ratioK0SPiGenMC.pdf";
    cratioK0SPiGenMC->SaveAs(outNameK0SPiGenMC.c_str());*/

    TMultiGraph* mgratioMCClosureK0SPi = new TMultiGraph();
    TGraphAsymmErrors* ratioMCClosureK0SPiGraph[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        ratioMCClosureK0SPiGraph[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioMCClosureK0SPi[i], errmult_down, errmult_up, errratioMCClosureK0SPi[i], errratioMCClosureK0SPi[i]);
        PlotFeatures(ratioMCClosureK0SPiGraph[i], 33, ColorsFinal[i], 2, 1, ColorsFinal[i], 2, 3001, ColorsFinal[i], 0.4, mgratioMCClosureK0SPi);
    }

    TCanvas* cratioMCClosureK0SPi = new TCanvas("cratioMCClosureK0SPi", "cratioMCClosureK0SPi", 1000, 800);
    cratioMCClosureK0SPi->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    gPad->SetMargin(0.14,0.05,0.13,0.06);
    mgratioMCClosureK0SPi->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; (2K^{0}_{S} / (#pi^{+}+#pi^{#minus}))_{reco} / (2K^{0}_{S} / (#pi^{+}+#pi^{#minus}))_{gen}");
    mgratioMCClosureK0SPi->GetXaxis()->SetLabelSize(0.045);
    mgratioMCClosureK0SPi->GetXaxis()->SetTitleSize(0.045);
    mgratioMCClosureK0SPi->GetXaxis()->SetTitleOffset(1.2);
    mgratioMCClosureK0SPi->GetYaxis()->SetLabelSize(0.045);
    mgratioMCClosureK0SPi->GetYaxis()->SetTitleSize(0.045);
    mgratioMCClosureK0SPi->GetYaxis()->SetTitleOffset(1.55);
    mgratioMCClosureK0SPi->Draw("AP");
    
    TLegend* legratioMCClosureK0SPi1 = new TLegend(0.51, 0.85, 0.68, 0.88);
    legratioMCClosureK0SPi1->SetHeader("#bf{This work}");
    legratioMCClosureK0SPi1->SetTextSize(0.045);
    legratioMCClosureK0SPi1->SetLineWidth(0);
    legratioMCClosureK0SPi1->Draw("same");

    TLegend* legratioMCClosureK0SPi2 = new TLegend(0.51, 0.81, 0.68, 0.85);
    legratioMCClosureK0SPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legratioMCClosureK0SPi2->SetTextSize(0.045);
    legratioMCClosureK0SPi2->SetLineWidth(0);
    legratioMCClosureK0SPi2->Draw("same");

    TLegend* legratioMCClosureK0SPi3 = new TLegend(0.51, 0.77, 0.68, 0.81);
    legratioMCClosureK0SPi3->SetHeader("0.4 < #it{p}_{T} #phi < 10.0 GeV/#it{c}");
    legratioMCClosureK0SPi3->SetTextSize(0.045);
    legratioMCClosureK0SPi3->SetLineWidth(0);
    legratioMCClosureK0SPi3->Draw("same");

    TLegend* legratioMCClosureK0SPi4 = new TLegend(0.51, 0.73, 0.68, 0.77);
    legratioMCClosureK0SPi4->SetHeader("0.1 < #it{p}_{T} K^{0}_{S} < 6.0 GeV/#it{c}");
    legratioMCClosureK0SPi4->SetTextSize(0.045);
    legratioMCClosureK0SPi4->SetLineWidth(0);
    legratioMCClosureK0SPi4->Draw("same");

    TLegend* legratioMCClosureK0SPi5 = new TLegend(0.51, 0.53, 0.68, 0.73);
    legratioMCClosureK0SPi5->SetHeader("0.3 < #it{p}_{T} (#pi^{+}+#pi^{#minus}) < 3.0 GeV/#it{c}");
    for (int i = 0; i < nbin_deltay; i++) {
        legratioMCClosureK0SPi5->AddEntry(RatioK0SPi[i], Form("|#it{#Deltay}| < %1.1f", deltay_axis[i]), "p");
    }
    legratioMCClosureK0SPi5->SetTextSize(0.045);
    legratioMCClosureK0SPi5->SetLineWidth(0);
    legratioMCClosureK0SPi5->Draw("same");

    /*string outNameMCClosureK0SPi = path + "ratioMCClosureK0SPi.root";
    cratioMCClosureK0SPi->SaveAs(outNameMCClosureK0SPi.c_str());
    outNameMCClosureK0SPi = path + "ratioMCClosureK0SPi.pdf";
    cratioMCClosureK0SPi->SaveAs(outNameMCClosureK0SPi.c_str());

    TFile* fileTemp = new TFile(Form("TestClosure_%d.root", mode), "RECREATE");
    mgratioMCClosureK0SPi->Write(Form("mgratioMCClosureK0SPi_%d", mode));*/

    //********************************************************************************************
    // Just a test to check the ordering in RecoMC with PDG code
    //********************************************************************************************

    /*TH3D* h3PhiK0SRecMC[nbin_deltay];
    THnSparseD* h4PhiPiRecMCTPCTOF[nbin_deltay]; 

    string h3PhiK0SRecMCName[nbin_deltay] = {"h3RecMCPhiK0SInc", "h3RecMCPhiK0SFCut", "h3RecMCPhiK0SSCut"};
    string h4PhiPiRecMCTPCTOFName[nbin_deltay] = {"h4RecMCPhiPiTPCTOFInc", "h4RecMCPhiPiTPCTOFFCut", "h4RecMCPhiPiTPCTOFSCut"};

    TDirectoryFile* phik0shortanalysis3 = (TDirectoryFile*)fileMCClosure->Get("phik0shortanalysis_id19983");
    TDirectoryFile* mcPhiK0SReco = (TDirectoryFile*)phik0shortanalysis3->Get("mcPhiK0SHist");

    TDirectoryFile* phik0shortanalysis4 = (TDirectoryFile*)fileMCClosure->Get("phik0shortanalysis_id19988");
    TDirectoryFile* mcPhiPiReco = (TDirectoryFile*)phik0shortanalysis4->Get("mcPhiPionHist");

    for (int i = 0; i < nbin_deltay; i++) {
        h3PhiK0SRecMC[i] = (TH3D*)mcPhiK0SReco->Get(h3PhiK0SRecMCName[i].c_str());
        h3PhiK0SRecMC[i]->SetDirectory(0);

        h4PhiPiRecMCTPCTOF[i] = (THnSparseD*)mcPhiPiReco->Get(h4PhiPiRecMCTPCTOFName[i].c_str());
        h4PhiPiRecMCTPCTOF[i]->GetAxis(1)->SetRange(1, h4PhiPiRecMCTPCTOF[i]->GetAxis(1)->GetNbins());
        h4PhiPiRecMCTPCTOF[i]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[i]->GetAxis(2)->GetNbins());
        h4PhiPiRecMCTPCTOF[i]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[i]->GetAxis(3)->GetNbins());
    }

    mcPhiK0SReco->Close();
    mcPhiPiReco->Close();
    phik0shortanalysis3->Close();
    phik0shortanalysis4->Close();

    Double_t PhiK0SpTIntRecMC[nbin_deltay][nbin_mult] = {0}, errPhiK0SpTIntRecMC[nbin_deltay][nbin_mult] = {0};
    Double_t PhiPipTIntRecMC[nbin_deltay][nbin_mult] = {0}, errPhiPipTIntRecMC[nbin_deltay][nbin_mult] = {0};
    Double_t ratioK0SPiRecMC[nbin_deltay][nbin_mult] = {0}, errratioK0SPiRecMC[nbin_deltay][nbin_mult] = {0};

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            PhiK0SpTIntRecMC[i][j] = h3PhiK0SRecMC[i]->ProjectionX(Form("h1PhiK0SpTIntRecMC%i_%i", i, j), 1, nbin_pTK0S, 1, h3PhiK0SRecMC[i]->GetNbinsZ())->GetBinContent(j+1);
            errPhiK0SpTIntRecMC[i][j] = h3PhiK0SRecMC[i]->ProjectionX(Form("h1PhiK0SpTIntRecMC%i_%i", i, j), 1, nbin_pTK0S, 1, h3PhiK0SRecMC[i]->GetNbinsZ())->GetBinError(j+1);
            
            PhiPipTIntRecMC[i][j] = Project1D(h4PhiPiRecMCTPCTOF[i], 0, "", Form("h1PhiPipTIntRecMC%i_%i", i, j))->GetBinContent(j+1);
            errPhiPipTIntRecMC[i][j] = Project1D(h4PhiPiRecMCTPCTOF[i], 0, "", Form("h1PhiPipTIntRecMC%i_%i", i, j))->GetBinError(j+1);

            ratioK0SPiRecMC[i][j] = 2 * PhiK0SpTIntRecMC[i][j] / PhiPipTIntRecMC[i][j];
            errratioK0SPiRecMC[i][j] = ratioK0SPiRecMC[i][j] * TMath::Sqrt(TMath::Power(errPhiK0SpTIntRecMC[i][j] / PhiK0SpTIntRecMC[i][j], 2) + TMath::Power(errPhiPipTIntRecMC[i][j] / PhiPipTIntGenMC[i][j], 2));
        }
    }

    TMultiGraph* mgratioK0SPiRecMC = new TMultiGraph();
    TGraphAsymmErrors* ratioK0SPiRecMCGraph[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        ratioK0SPiRecMCGraph[i] = new TGraphAsymmErrors(nbin_mult, mult, ratioK0SPiRecMC[i], errmult, errmult, errratioK0SPiRecMC[i], errratioK0SPiRecMC[i]);
        if (i == 0) {
            PlotFeatures(ratioK0SPiRecMCGraph[i], 33, kBlack, 2, 1, kBlack, 2, 3001, kBlack, 0.4, mgratioK0SPiRecMC);
        } else if (i == 1) {
            PlotFeatures(ratioK0SPiRecMCGraph[i], 33, kGreen+3, 2, 1, kGreen+3, 2, 3001, kGreen+3, 0.4, mgratioK0SPiRecMC);
        } else if (i == 2) {
            PlotFeatures(ratioK0SPiRecMCGraph[i], 33, kRed+1, 2, 1, kRed+1, 2, 3001, kRed+1, 0.4, mgratioK0SPiRecMC);
        }
    }

    TCanvas* cratioK0SPiRecMC = new TCanvas("cratioK0SPiRecMC", "cratioK0SPiRecMC", 1000, 800);
    cratioK0SPiRecMC->cd();
    //gPad->SetLogy();
    gStyle->SetOptStat(0);
    mgratioK0SPiRecMC->SetTitle("; #LTdN_{ch}/d#eta#GT_{|#eta|<0.5} ; 2K^{0}_{S} / (#pi^{+} + #pi^{#minus})");
    mgratioK0SPiRecMC->Draw("AP");

    TLegend* legratioK0SPiRecMC1 = new TLegend(0.15, 0.82, 0.5, 0.85);
    legratioK0SPiRecMC1->SetHeader("#bf{This work}");
    legratioK0SPiRecMC1->SetTextSize(0.05);
    legratioK0SPiRecMC1->SetLineWidth(0);
    legratioK0SPiRecMC1->Draw("same");

    TLegend* legratioK0SPiRecMC2 = new TLegend(0.15, 0.65, 0.5, 0.82);
    legratioK0SPiRecMC2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y} | < 0.5");
    legratioK0SPiRecMC2->AddEntry(ratioK0SPiRecMCGraph[0], "|#Delta#it{y}| < 1.0", "p");
    legratioK0SPiRecMC2->AddEntry(ratioK0SPiRecMCGraph[1], "|#Delta#it{y}| < 0.5", "p");
    legratioK0SPiRecMC2->AddEntry(ratioK0SPiRecMCGraph[2], "|#Delta#it{y}| < 0.1", "p");
    legratioK0SPiRecMC2->SetTextSize(0.03);
    legratioK0SPiRecMC2->SetLineWidth(0);
    legratioK0SPiRecMC2->Draw("same");

    //cratioK0SPiRecMC->SaveAs("TestnoTLVWPhi/K0S_Pi_MCReco_ratio.pdf");
    //cratioK0SPiRecMC->SaveAs("TestnoTLVWPhi/K0S_Pi_MCReco_ratio.root");*/

    return;
}


// ratioK0SPiCorrectedUpdated_export.C - OldCodes/ratioK0SPiCorrectedUpdated.C, made to export its result.
//
// A copy of the pre-framework macro that produced the K0S/pi ratio, run on its own inputs
// (OldComparison/data, from ~/Downloads/OldAnalysis) so that the result can be compared with the
// framework's (OldComparison/README.md). Lines 1-1136 of the original, the whole computation, are
// unchanged except for the three marked [export]: the include of CustomTF1.h points to OldCodes, the
// include of Plot.h (not available) is dropped, and the function is renamed. After the graphs of the
// ratio are built, an [export] block writes them, and the same numbers as histograms on the
// multiplicity classes, to oldK0SPiRatio.root; the rest of the original (MC models, Run 2 data, final
// canvas, MC closure) is removed, its inputs not being available. Run it from a copy of the data
// directory: it writes next to its inputs, and overwrites Systematics/K0S/LowPtExtra/... (run.sh does).
//
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
#include "../../OldCodes/CustomTF1.h"
// [export] Plot.h is not available; it is used only by the model section, removed below.

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

void ratioK0SPiCorrectedUpdated_export(int mode) {

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

    // [export] Added to this copy on 20 October 2026: what the canvas cRatioK0SPi used to show,
    // as named objects, for |dy| < 1.0 (window 0, the only one this macro computes: the others hold
    // zeros). The graphs as built above (x = <dNch/deta>), and the same numbers on the multiplicity
    // classes, index j being class [mult_axis[j], mult_axis[j+1]] as everywhere in this macro, as
    // histograms comparison_task can divide by. Stat and syst apart.
    {
        TFile fout("oldK0SPiRatio.root", "RECREATE");
        RatioK0SPi[0]->Write("RatioK0SPi_dy1_00_vs_dNdeta_stat");
        RatioK0SPiSyst[0]->Write("RatioK0SPi_dy1_00_vs_dNdeta_syst");
        TH1D hStat("Ratio_K0S_Pi_dy1_00_stat", "old ratioK0SPiCorrectedUpdated.C, stat;Multiplicity percentile (%);2K^{0}_{S}/(#pi^{+}+#pi^{-})", nbin_mult, mult_axis);
        TH1D hSyst("Ratio_K0S_Pi_dy1_00_syst", "old ratioK0SPiCorrectedUpdated.C, syst;Multiplicity percentile (%);2K^{0}_{S}/(#pi^{+}+#pi^{-})", nbin_mult, mult_axis);
        hStat.SetDirectory(0);
        hSyst.SetDirectory(0);
        for (int j = 0; j < nbin_mult; j++) {
            hStat.SetBinContent(j + 1, ratioK0SPi[0][j]);
            hStat.SetBinError(j + 1, errratioK0SPi[0][j]);
            hSyst.SetBinContent(j + 1, ratioK0SPi[0][j]);
            hSyst.SetBinError(j + 1, systRatioK0SPi[0][j]);
        }
        fout.cd();
        hStat.Write();
        hSyst.Write();
        fout.Close();
    }
    // [export] The rest (MC models, Run 2 HEPData, final canvas, MC closure) is removed: its inputs are not here.
}


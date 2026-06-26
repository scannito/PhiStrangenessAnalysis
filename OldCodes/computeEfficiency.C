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
#include "TGraphAsymmErrors.h"
#include "TMultiGraph.h"

using namespace std;

TH2F* Project2D(THnSparseF* hn, Int_t axistoproj1, Int_t axistoproj2, Option_t* option = "", string hname = "") 
{ 
    if (!hn) return 0;
    TH2F* h2 = (TH2F*)hn->Projection(axistoproj1, axistoproj2, option);
    h2->SetName(hname.c_str());
    h2->SetDirectory(0);
    return h2;
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

std::vector<Int_t> Colors = {634, 628, 807, 797, kOrange - 4, 815, 418, 429, 867, 856, 601, kViolet, kPink + 9, kPink + 1, 1};
std::vector<Int_t> FullMarkers  = {20, 21, 33, 34, 29, 41, 47, 43};
std::vector<Int_t> EmptyMarkers = {53, 56, 57, 58, 64, 67, 54, 65};
std::vector<Int_t> Markers = {20, 21, 33, 34, 29, 41, 47, 43, 53, 56, 57, 58, 64, 67, 54, 65};

constexpr Int_t nbin_deltay = 5, nbin_deltay_red = 3, nbin_mult = 10, nbin_pTK0S = 9, nbin_pTPi = /*9*/ 11, nbin_massPhi = 13;

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

void computeEfficiency() 
{
    //TFile* file = TFile::Open("AnalysisResultsEff2024.root");

    /*TFile* file = TFile::Open("AnalysisResultsMC.root");

    TDirectoryFile* phik0shortanalysis = (TDirectoryFile*)file->Get("phik0shortanalysis_id25002");
    TDirectoryFile* mcK0SHist = (TDirectoryFile*)phik0shortanalysis->Get("mcK0SHist");
    TDirectoryFile* mcPhiK0SHist = (TDirectoryFile*)phik0shortanalysis->Get("mcPhiK0SHist");

    TFile* file2 = TFile::Open("AnalysisResultsMC2.root");

    TDirectoryFile* phik0shortanalysis2 = (TDirectoryFile*)file2->Get("phik0shortanalysis_id26355");
    TDirectoryFile* mcPhiK0SHist2 = (TDirectoryFile*)phik0shortanalysis2->Get("mcPhiK0SHist");

    TDirectoryFile* phik0shortanalysis3 = (TDirectoryFile*)file2->Get("phik0shortanalysis_id25003");
    TDirectoryFile* mcPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("mcPionHist");
    TDirectoryFile* mcPhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("mcPhiPionHist");

    TDirectoryFile* phik0shortanalysis4 = (TDirectoryFile*)file2->Get("phik0shortanalysis_id26359");
    TDirectoryFile* mcPhiPiHist2 = (TDirectoryFile*)phik0shortanalysis4->Get("mcPhiPionHist");

    string path = "ResultsEfficiency/";
    string fileOutEffName = "ResultsEfficiency.root";*/

    TFile* file = TFile::Open("AnalysisResultsMC3.root");

    TDirectoryFile* phik0shortanalysis = (TDirectoryFile*)file->Get("phik0shortanalysis_id25002");
    TDirectoryFile* mcK0SHist = (TDirectoryFile*)phik0shortanalysis->Get("mcK0SHist");
    TDirectoryFile* mcPhiK0SHist = (TDirectoryFile*)phik0shortanalysis->Get("mcPhiK0SHist");

    TDirectoryFile* phik0shortanalysis2 = (TDirectoryFile*)file->Get("phik0shortanalysis_id26355");
    TDirectoryFile* mcPhiK0SHist2 = (TDirectoryFile*)phik0shortanalysis2->Get("mcPhiK0SHist");

    TDirectoryFile* phik0shortanalysis3 = (TDirectoryFile*)file->Get("phik0shortanalysis_id25003");
    TDirectoryFile* mcPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("mcPionHist");
    TDirectoryFile* mcPhiPiHist = (TDirectoryFile*)phik0shortanalysis3->Get("mcPhiPionHist");

    TDirectoryFile* phik0shortanalysis4 = (TDirectoryFile*)file->Get("phik0shortanalysis_id26359");
    TDirectoryFile* mcPhiPiHist2 = (TDirectoryFile*)phik0shortanalysis4->Get("mcPhiPionHist");

    string path = "ResultsEfficiency2/";
    string fileOutEffName = "ResultsEfficiency2.root";
        
    //********************************************************************************************

    TH3F* h3K0SRecMC = (TH3F*)mcK0SHist->Get("h3RecMCK0S");
    h3K0SRecMC->SetDirectory(0);
    h3K0SRecMC->GetZaxis()->SetRange(1, h3K0SRecMC->GetNbinsZ());

    TH2F* h2K0SRecMC = (TH2F*)h3K0SRecMC->Project3D("yx");
    h2K0SRecMC->Sumw2();

    TH2F* h2K0SGenMC = (TH2F*)mcK0SHist->Get("h2K0SGenMC");
    h2K0SGenMC->SetDirectory(0);
    h2K0SGenMC->Sumw2();

    TH2F* h2K0SGenMCAssocReco = (TH2F*)mcK0SHist->Get("h2K0SGenMCAssocReco");
    h2K0SGenMCAssocReco->SetDirectory(0);

    //********************************************************************************************

    string h3PhiK0SRecMCName[nbin_deltay_red] = {"h3RecMCPhiK0SInc", "h3RecMCPhiK0SFCut", "h3RecMCPhiK0SSCut"};

    TH3F* h3PhiK0SRecMC[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h3PhiK0SRecMC[i] = (TH3F*)mcPhiK0SHist->Get(h3PhiK0SRecMCName[i].c_str());
        h3PhiK0SRecMC[i]->SetDirectory(0);
        h3PhiK0SRecMC[i]->GetZaxis()->SetRange(1, h3PhiK0SRecMC[i]->GetNbinsZ());
    }*/
    h3PhiK0SRecMC[0] = (TH3F*)mcPhiK0SHist->Get(h3PhiK0SRecMCName[0].c_str());
    h3PhiK0SRecMC[0]->SetDirectory(0);
    h3PhiK0SRecMC[0]->GetZaxis()->SetRange(1, h3PhiK0SRecMC[0]->GetNbinsZ());
    h3PhiK0SRecMC[1] = (TH3F*)mcPhiK0SHist2->Get(h3PhiK0SRecMCName[1].c_str());
    h3PhiK0SRecMC[1]->SetDirectory(0);
    h3PhiK0SRecMC[1]->GetZaxis()->SetRange(1, h3PhiK0SRecMC[1]->GetNbinsZ());
    h3PhiK0SRecMC[1]->SetName("h3RecMCPhiK0SFCut1");
    h3PhiK0SRecMC[2] = (TH3F*)mcPhiK0SHist->Get(h3PhiK0SRecMCName[1].c_str());
    h3PhiK0SRecMC[2]->SetDirectory(0);
    h3PhiK0SRecMC[2]->GetZaxis()->SetRange(1, h3PhiK0SRecMC[2]->GetNbinsZ());
    h3PhiK0SRecMC[2]->SetName("h3RecMCPhiK0SFCut2");
    h3PhiK0SRecMC[3] = (TH3F*)mcPhiK0SHist2->Get(h3PhiK0SRecMCName[2].c_str());
    h3PhiK0SRecMC[3]->SetDirectory(0);
    h3PhiK0SRecMC[3]->GetZaxis()->SetRange(1, h3PhiK0SRecMC[3]->GetNbinsZ());
    h3PhiK0SRecMC[3]->SetName("h3RecMCPhiK0SSCut1");
    h3PhiK0SRecMC[4] = (TH3F*)mcPhiK0SHist->Get(h3PhiK0SRecMCName[2].c_str());
    h3PhiK0SRecMC[4]->SetDirectory(0);
    h3PhiK0SRecMC[4]->GetZaxis()->SetRange(1, h3PhiK0SRecMC[4]->GetNbinsZ());
    h3PhiK0SRecMC[4]->SetName("h3RecMCPhiK0SSCut2");

    TH2F* h2PhiK0SRecMC[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h2PhiK0SRecMC[i] = (TH2F*)h3PhiK0SRecMC[i]->Project3D("yx");
        h2PhiK0SRecMC[i]->Sumw2();
    }

    string h2PhiK0SGenMCName[nbin_deltay_red] = {"h2PhiK0SGenMCInc", "h2PhiK0SGenMCFCut", "h2PhiK0SGenMCSCut"};

    TH2F* h2PhiK0SGenMC[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h2PhiK0SGenMC[i] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCName[i].c_str());
        h2PhiK0SGenMC[i]->SetDirectory(0);
        h2PhiK0SGenMC[i]->Sumw2();
    }*/
    h2PhiK0SGenMC[0] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCName[0].c_str());
    h2PhiK0SGenMC[0]->SetDirectory(0);
    h2PhiK0SGenMC[0]->Sumw2();
    h2PhiK0SGenMC[1] = (TH2F*)mcPhiK0SHist2->Get(h2PhiK0SGenMCName[1].c_str());
    h2PhiK0SGenMC[1]->SetDirectory(0);
    h2PhiK0SGenMC[1]->Sumw2();
    h2PhiK0SGenMC[2] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCName[1].c_str());
    h2PhiK0SGenMC[2]->SetDirectory(0);
    h2PhiK0SGenMC[2]->Sumw2();
    h2PhiK0SGenMC[3] = (TH2F*)mcPhiK0SHist2->Get(h2PhiK0SGenMCName[2].c_str());
    h2PhiK0SGenMC[3]->SetDirectory(0);
    h2PhiK0SGenMC[3]->Sumw2();
    h2PhiK0SGenMC[4] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCName[2].c_str());
    h2PhiK0SGenMC[4]->SetDirectory(0);
    h2PhiK0SGenMC[4]->Sumw2();

    string h2PhiK0SGenMCAssocRecoName[nbin_deltay_red] = {"h2PhiK0SGenMCIncAssocReco", "h2PhiK0SGenMCFCutAssocReco", "h2PhiK0SGenMCSCutAssocReco"};

    TH2F* h2PhiK0SGenMCAssocReco[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h2PhiK0SGenMCAssocReco[i] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCAssocRecoName[i].c_str());
        h2PhiK0SGenMCAssocReco[i]->SetDirectory(0);
        h2PhiK0SGenMCAssocReco[i]->Sumw2();
    }*/
    h2PhiK0SGenMCAssocReco[0] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCAssocRecoName[0].c_str());
    h2PhiK0SGenMCAssocReco[0]->SetDirectory(0);
    h2PhiK0SGenMCAssocReco[0]->Sumw2();
    h2PhiK0SGenMCAssocReco[1] = (TH2F*)mcPhiK0SHist2->Get(h2PhiK0SGenMCAssocRecoName[1].c_str());
    h2PhiK0SGenMCAssocReco[1]->SetDirectory(0);
    h2PhiK0SGenMCAssocReco[1]->Sumw2();
    h2PhiK0SGenMCAssocReco[2] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCAssocRecoName[1].c_str());
    h2PhiK0SGenMCAssocReco[2]->SetDirectory(0);
    h2PhiK0SGenMCAssocReco[2]->Sumw2();
    h2PhiK0SGenMCAssocReco[3] = (TH2F*)mcPhiK0SHist2->Get(h2PhiK0SGenMCAssocRecoName[2].c_str());
    h2PhiK0SGenMCAssocReco[3]->SetDirectory(0);
    h2PhiK0SGenMCAssocReco[3]->Sumw2();
    h2PhiK0SGenMCAssocReco[4] = (TH2F*)mcPhiK0SHist->Get(h2PhiK0SGenMCAssocRecoName[2].c_str());
    h2PhiK0SGenMCAssocReco[4]->SetDirectory(0);
    h2PhiK0SGenMCAssocReco[4]->Sumw2();

    //********************************************************************************************

    TH3F* h3PiRecMCTPC = (TH3F*)mcPiHist->Get("h3RecMCPiTPC");
    h3PiRecMCTPC->SetDirectory(0);
    h3PiRecMCTPC->GetZaxis()->SetRange(1, h3PiRecMCTPC->GetNbinsZ());

    TH2F* h2PiRecMCTPC = (TH2F*)h3PiRecMCTPC->Project3D("yx");
    h2PiRecMCTPC->Sumw2();

    THnSparseF* h4PiRecMCTPCTOF = (THnSparseF*)mcPiHist->Get("h4RecMCPiTPCTOF");
    h4PiRecMCTPCTOF->GetAxis(2)->SetRange(1, h4PiRecMCTPCTOF->GetAxis(2)->GetNbins());
    h4PiRecMCTPCTOF->GetAxis(3)->SetRange(1, h4PiRecMCTPCTOF->GetAxis(3)->GetNbins());

    TH2F* h2PiRecMCTPCTOF = Project2D(h4PiRecMCTPCTOF, 1, 0, "", "h2PiRecMC");
    h2PiRecMCTPCTOF->Sumw2();

    TH2F* h2PiGenMC = (TH2F*)mcPiHist->Get("h2PiGenMC");
    h2PiGenMC->SetDirectory(0);
    h2PiGenMC->Sumw2();

    TH2F* h2PiGenMCAssocReco = (TH2F*)mcPiHist->Get("h2PiGenMCAssocReco");
    h2PiGenMCAssocReco->SetDirectory(0);

    //********************************************************************************************

    string h3PhiPiRecMCTPCName[nbin_deltay_red] = {"h3RecMCPhiPiTPCInc", "h3RecMCPhiPiTPCFCut", "h3RecMCPhiPiTPCSCut"};

    TH3F* h3PhiPiRecMCTPC[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h3PhiPiRecMCTPC[i] = (TH3F*)mcPhiPiHist->Get(h3PhiPiRecMCTPCName[i].c_str());
        h3PhiPiRecMCTPC[i]->SetDirectory(0);
        h3PhiPiRecMCTPC[i]->GetZaxis()->SetRange(1, h3PhiPiRecMCTPC[i]->GetNbinsZ());
    }*/
    h3PhiPiRecMCTPC[0] = (TH3F*)mcPhiPiHist->Get(h3PhiPiRecMCTPCName[0].c_str());
    h3PhiPiRecMCTPC[0]->GetZaxis()->SetRange(1, h3PhiPiRecMCTPC[0]->GetNbinsZ());
    h3PhiPiRecMCTPC[1] = (TH3F*)mcPhiPiHist2->Get(h3PhiPiRecMCTPCName[1].c_str());
    h3PhiPiRecMCTPC[1]->GetZaxis()->SetRange(1, h3PhiPiRecMCTPC[1]->GetNbinsZ());
    h3PhiPiRecMCTPC[1]->SetName("h3RecMCPhiPiTPCFCut1");
    h3PhiPiRecMCTPC[2] = (TH3F*)mcPhiPiHist->Get(h3PhiPiRecMCTPCName[1].c_str());
    h3PhiPiRecMCTPC[2]->GetZaxis()->SetRange(1, h3PhiPiRecMCTPC[2]->GetNbinsZ());
    h3PhiPiRecMCTPC[2]->SetName("h3RecMCPhiPiTPCFCut2");
    h3PhiPiRecMCTPC[3] = (TH3F*)mcPhiPiHist2->Get(h3PhiPiRecMCTPCName[2].c_str());
    h3PhiPiRecMCTPC[3]->GetZaxis()->SetRange(1, h3PhiPiRecMCTPC[3]->GetNbinsZ());
    h3PhiPiRecMCTPC[3]->SetName("h3RecMCPhiPiTPCSCut1");
    h3PhiPiRecMCTPC[4] = (TH3F*)mcPhiPiHist->Get(h3PhiPiRecMCTPCName[2].c_str());
    h3PhiPiRecMCTPC[4]->GetZaxis()->SetRange(1, h3PhiPiRecMCTPC[4]->GetNbinsZ());
    h3PhiPiRecMCTPC[4]->SetName("h3RecMCPhiPiTPCSCut2");

    TH2F* h2PhiPiRecMCTPC[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h2PhiPiRecMCTPC[i] = (TH2F*)h3PhiPiRecMCTPC[i]->Project3D("yx");
        h2PhiPiRecMCTPC[i]->Sumw2();
    }

    string h4PhiPiRecMCTPCTOFName[nbin_deltay_red] = {"h4RecMCPhiPiTPCTOFInc", "h4RecMCPhiPiTPCTOFFCut", "h4RecMCPhiPiTPCTOFSCut"};

    THnSparseF* h4PhiPiRecMCTPCTOF[nbin_deltay]; 
    /*for (int i = 0; i < nbin_deltay; i++) {
        h4PhiPiRecMCTPCTOF[i] = (THnSparseF*)mcPhiPiHist->Get(h4PhiPiRecMCTPCTOFName[i].c_str());
        h4PhiPiRecMCTPCTOF[i]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[i]->GetAxis(2)->GetNbins());
        h4PhiPiRecMCTPCTOF[i]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[i]->GetAxis(3)->GetNbins());
    }*/
    h4PhiPiRecMCTPCTOF[0] = (THnSparseF*)mcPhiPiHist->Get(h4PhiPiRecMCTPCTOFName[0].c_str());
    h4PhiPiRecMCTPCTOF[0]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[0]->GetAxis(2)->GetNbins());
    h4PhiPiRecMCTPCTOF[0]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[0]->GetAxis(3)->GetNbins());
    h4PhiPiRecMCTPCTOF[1] = (THnSparseF*)mcPhiPiHist2->Get(h4PhiPiRecMCTPCTOFName[1].c_str());
    h4PhiPiRecMCTPCTOF[1]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[1]->GetAxis(2)->GetNbins());
    h4PhiPiRecMCTPCTOF[1]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[1]->GetAxis(3)->GetNbins());
    h4PhiPiRecMCTPCTOF[2] = (THnSparseF*)mcPhiPiHist->Get(h4PhiPiRecMCTPCTOFName[1].c_str());
    h4PhiPiRecMCTPCTOF[2]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[2]->GetAxis(2)->GetNbins());
    h4PhiPiRecMCTPCTOF[2]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[2]->GetAxis(3)->GetNbins());
    h4PhiPiRecMCTPCTOF[3] = (THnSparseF*)mcPhiPiHist2->Get(h4PhiPiRecMCTPCTOFName[2].c_str());
    h4PhiPiRecMCTPCTOF[3]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[3]->GetAxis(2)->GetNbins());
    h4PhiPiRecMCTPCTOF[3]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[3]->GetAxis(3)->GetNbins());
    h4PhiPiRecMCTPCTOF[4] = (THnSparseF*)mcPhiPiHist->Get(h4PhiPiRecMCTPCTOFName[2].c_str());
    h4PhiPiRecMCTPCTOF[4]->GetAxis(2)->SetRange(1, h4PhiPiRecMCTPCTOF[4]->GetAxis(2)->GetNbins());
    h4PhiPiRecMCTPCTOF[4]->GetAxis(3)->SetRange(1, h4PhiPiRecMCTPCTOF[4]->GetAxis(3)->GetNbins());

    TH2F* h2PhiPiRecMCTPCTOF[nbin_deltay];
    for (int i = 0; i < nbin_deltay; i++) {
        h2PhiPiRecMCTPCTOF[i] = Project2D(h4PhiPiRecMCTPCTOF[i], 1, 0, "", Form("h2PhiPiRecMCTPCTOF%i", i));
        h2PhiPiRecMCTPCTOF[i]->Sumw2();
    }

    string h2PhiPiGenMCName[nbin_deltay_red] = {"h2PhiPiGenMCInc", "h2PhiPiGenMCFCut", "h2PhiPiGenMCSCut"};

    TH2F* h2PhiPiGenMC[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h2PhiPiGenMC[i] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCName[i].c_str());
        h2PhiPiGenMC[i]->SetDirectory(0);
        h2PhiPiGenMC[i]->Sumw2();
    }*/
    h2PhiPiGenMC[0] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCName[0].c_str());
    h2PhiPiGenMC[0]->Sumw2();
    h2PhiPiGenMC[1] = (TH2F*)mcPhiPiHist2->Get(h2PhiPiGenMCName[1].c_str());
    h2PhiPiGenMC[1]->Sumw2();
    h2PhiPiGenMC[2] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCName[1].c_str());
    h2PhiPiGenMC[2]->Sumw2();
    h2PhiPiGenMC[3] = (TH2F*)mcPhiPiHist2->Get(h2PhiPiGenMCName[2].c_str());
    h2PhiPiGenMC[3]->Sumw2();
    h2PhiPiGenMC[4] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCName[2].c_str());
    h2PhiPiGenMC[4]->Sumw2();

    string h2PhiPiGenMCAssocRecoName[nbin_deltay_red] = {"h2PhiPiGenMCIncAssocReco", "h2PhiPiGenMCFCutAssocReco", "h2PhiPiGenMCSCutAssocReco"};

    TH2F* h2PhiPiGenMCAssocReco[nbin_deltay];
    /*for (int i = 0; i < nbin_deltay; i++) {
        h2PhiPiGenMCAssocReco[i] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCAssocRecoName[i].c_str());
        h2PhiPiGenMCAssocReco[i]->SetDirectory(0);
    }*/
    h2PhiPiGenMCAssocReco[0] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCAssocRecoName[0].c_str());
    h2PhiPiGenMCAssocReco[0]->Sumw2();
    h2PhiPiGenMCAssocReco[1] = (TH2F*)mcPhiPiHist2->Get(h2PhiPiGenMCAssocRecoName[1].c_str());
    h2PhiPiGenMCAssocReco[1]->Sumw2();
    h2PhiPiGenMCAssocReco[2] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCAssocRecoName[1].c_str());
    h2PhiPiGenMCAssocReco[2]->Sumw2();
    h2PhiPiGenMCAssocReco[3] = (TH2F*)mcPhiPiHist2->Get(h2PhiPiGenMCAssocRecoName[2].c_str());
    h2PhiPiGenMCAssocReco[3]->Sumw2();
    h2PhiPiGenMCAssocReco[4] = (TH2F*)mcPhiPiHist->Get(h2PhiPiGenMCAssocRecoName[2].c_str());
    h2PhiPiGenMCAssocReco[4]->Sumw2();

    //********************************************************************************************

    TH2F* h2DCAxyPrimPi = (TH2F*)mcPiHist->Get("h3RecMCDCAxyPrimPi");
    h2DCAxyPrimPi->SetDirectory(0);

    TH1F* h1DCAxyPrimPi[nbin_pTPi];
    for (int k = 0; k < nbin_pTPi; k++) {
        h1DCAxyPrimPi[k] = (TH1F*)h2DCAxyPrimPi->ProjectionY(Form("h1DCAxyPrimPi%i", k), k+1, k+1);
    }

    TH2F* h2DCAxySecPiFromDecays = (TH2F*)mcPiHist->Get("h3RecMCDCAxySecWeakDecayPi");
    h2DCAxySecPiFromDecays->SetDirectory(0);

    TH1F* h1DCAxySecPiFromDecays[nbin_pTPi];
    for (int k = 0; k < nbin_pTPi; k++) {
        h1DCAxySecPiFromDecays[k] = (TH1F*)h2DCAxySecPiFromDecays->ProjectionY(Form("h1DCAxySecPiFromDecays%i", k), k+1, k+1);
    }

    TH2F* h2DCAxySecPiFromMaterial = (TH2F*)mcPiHist->Get("h3RecMCDCAxySecMaterialPi");
    h2DCAxySecPiFromMaterial->SetDirectory(0);

    TH1F* h1DCAxySecPiFromMaterial[nbin_pTPi];
    for (int k = 0; k < nbin_pTPi; k++) {
        h1DCAxySecPiFromMaterial[k] = (TH1F*)h2DCAxySecPiFromMaterial->ProjectionY(Form("h1DCAxySecPiFromMaterial%i", k), k+1, k+1);
    }

    //********************************************************************************************
    
    //mcK0SHist->Close();
    //mcPhiK0SHist->Close();
    //mcPhiK0SHist2->Close();
    //mcPiHist->Close();
    //mcPhiPiHist->Close();
    //mcPhiPiHist2->Close();
    //phik0shortanalysis->Close();
    //phik0shortanalysis2->Close();
    //phik0shortanalysis3->Close();
    //phik0shortanalysis4->Close();

    //*******************************************************************************************

    TH2F* h2effK0S = (TH2F*)h2K0SRecMC->Clone("h2effK0S");
    h2effK0S->Divide(h2K0SRecMC, h2K0SGenMCAssocReco, 1, 1, "B");

    TH2F* h2siglossK0S = (TH2F*)h2K0SGenMCAssocReco->Clone("h2siglossK0S");
    h2siglossK0S->Divide(h2K0SGenMCAssocReco, h2K0SGenMC, 1, 1, "B");

    TH1F* h1effK0S[nbin_mult];
    TH1F* h1siglossK0S[nbin_mult];
    for (int j = 0; j < nbin_mult; j++) {
        h1effK0S[j] = (TH1F*)h2effK0S->ProjectionY(Form("h1effK0S%i", j), j+1, j+1);
        h1siglossK0S[j] = (TH1F*)h2siglossK0S->ProjectionY(Form("h1siglossK0S%i", j), j+1, j+1);
    }

    TH1F* h1K0SRecMCMultInt = (TH1F*)h2K0SRecMC->ProjectionY("h1K0SRecMCMultInt", 1, nbin_mult);
    TH1F* h1K0SGenMCMultInt = (TH1F*)h2K0SGenMC->ProjectionY("h1K0SGenMCMultInt", 1, nbin_mult);
    TH1F* h1K0SGenMCAssocRecoMultInt = (TH1F*)h2K0SGenMCAssocReco->ProjectionY("h1K0SGenMCAssocRecoMultInt", 1, nbin_mult);

    TH1F* h1effK0SMultInt = (TH1F*)h1K0SRecMCMultInt->Clone("h1effK0SMultInt");
    h1effK0SMultInt->Divide(h1K0SRecMCMultInt, h1K0SGenMCAssocRecoMultInt, 1, 1, "B");

    TH1F* h1siglossK0SMultInt = (TH1F*)h1K0SGenMCAssocRecoMultInt->Clone("h1siglossK0SMultInt");
    h1siglossK0SMultInt->Divide(h1K0SGenMCAssocRecoMultInt, h1K0SGenMCMultInt, 1, 1, "B");

    TH1F* h1effK0SRatio[nbin_mult];
    TH1F* h1siglossK0SRatio[nbin_mult];

    TCanvas* ceffK0S = new TCanvas("ceffK0S", "ceffK0S", 800, 800);
    ceffK0S->cd();
    //gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);

    TPad* topPadEffK0S = new TPad("topPadEffK0S", "Top Pad", 0, bottomPadHeight, 1, 1);
    topPadEffK0S->SetBottomMargin(0);
    topPadEffK0S->Draw();
    topPadEffK0S->cd();

    TLegend* legeffK0S1 = new TLegend(0.5, 0.82, 0.8, 0.85);
    legeffK0S1->SetHeader("#bf{This work}");
    legeffK0S1->SetTextSize(0.05);
    legeffK0S1->SetLineWidth(0);

    TLegend* legeffK0S2 = new TLegend(0.5, 0.62, 0.8, 0.82);
    legeffK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, MB");
    legeffK0S2->SetTextSize(0.035);
    legeffK0S2->SetLineWidth(0);
    legeffK0S2->SetNColumns(2);
    
    for (int j = 0; j < nbin_mult; j++) {
        h1effK0S[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); #varepsilon x acc");
        h1effK0S[j]->SetMarkerStyle(20);
        h1effK0S[j]->SetMarkerColor(Colors[j]);
        h1effK0S[j]->SetMarkerSize(1.5);
        h1effK0S[j]->SetLineColor(Colors[j]);
        h1effK0S[j]->SetLineWidth(2);
        h1effK0S[j]->SetFillStyle(3001);
        h1effK0S[j]->SetFillColor(Colors[j]);
        //h1effK0S[j]->GetXaxis()->SetLabelOffset(0.05);
        h1effK0S[j]->GetYaxis()->SetTitleSize(0.045);
        h1effK0S[j]->GetYaxis()->SetTitleOffset(1.0);
        h1effK0S[j]->GetYaxis()->SetLabelSize(0.045);
        h1effK0S[j]->GetYaxis()->SetRangeUser(0.001, 0.6);

        if (j == 0) h1effK0S[j]->Draw();
        else h1effK0S[j]->Draw("same");

        legeffK0S2->AddEntry(h1effK0S[j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
    }

    legeffK0S1->Draw("same");
    legeffK0S2->Draw("same");

    TPad* bottomPadEffK0S = new TPad("bottomPadEffK0S", "Bottom Pad", 0, 0, 1, bottomPadHeight);
    bottomPadEffK0S->SetTopMargin(0);
    bottomPadEffK0S->SetBottomMargin(0.3);
    //bottomPadEffK0S->SetLogy();
    ceffK0S->cd();
    bottomPadEffK0S->Draw();
    bottomPadEffK0S->cd();

    for (int j = 0; j < nbin_mult; j++) {
        h1effK0SRatio[j] = (TH1F*)h1effK0S[j]->Clone(Form("h1effK0SRatio%i", j));
        h1effK0SRatio[j]->Divide(h1effK0SMultInt);
        h1effK0SRatio[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
        h1effK0SRatio[j]->SetMarkerStyle(20);
        h1effK0SRatio[j]->SetMarkerColor(Colors[j]);
        h1effK0SRatio[j]->SetMarkerSize(1.5);
        h1effK0SRatio[j]->SetLineColor(Colors[j]);
        h1effK0SRatio[j]->SetLineWidth(2);
        h1effK0SRatio[j]->SetFillStyle(3001);
        h1effK0SRatio[j]->SetFillColor(Colors[j]);
        h1effK0SRatio[j]->GetXaxis()->SetLabelOffset(0.03);
        h1effK0SRatio[j]->GetXaxis()->SetNdivisions(515);
        h1effK0SRatio[j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
        h1effK0SRatio[j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
        h1effK0SRatio[j]->GetXaxis()->SetTitleOffset(1.2);
        h1effK0SRatio[j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
        h1effK0SRatio[j]->GetYaxis()->SetTitleOffset(0.45);
        h1effK0SRatio[j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
        h1effK0SRatio[j]->GetYaxis()->SetNdivisions(505);
        h1effK0SRatio[j]->GetYaxis()->SetRangeUser(0.81, 1.24);

        if (j == 0) h1effK0SRatio[j]->Draw();
        else h1effK0SRatio[j]->Draw("same");
    }

    string outeffK0S = path + "effK0SMB.root";
    ceffK0S->SaveAs(outeffK0S.c_str());
    outeffK0S = path + "effK0SMB.pdf";
    ceffK0S->SaveAs(outeffK0S.c_str());

    TCanvas* csiglossK0S = new TCanvas("csiglossK0S", "csiglossK0S", 800, 800);
    csiglossK0S->cd();
    //gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);

    TPad* topPadSigLossK0S = new TPad("topPadSigLossK0S", "Top Pad", 0, bottomPadHeight, 1, 1);
    topPadSigLossK0S->SetBottomMargin(0);
    topPadSigLossK0S->Draw();
    topPadSigLossK0S->cd();
    
    TLegend* legsiglossK0S1 = new TLegend(0.5, 0.82, 0.8, 0.85);
    legsiglossK0S1->SetHeader("#bf{This work}");
    legsiglossK0S1->SetTextSize(0.05);
    legsiglossK0S1->SetLineWidth(0);

    TLegend* legsiglossK0S2 = new TLegend(0.5, 0.62, 0.8, 0.82);
    legsiglossK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, MB");
    legsiglossK0S2->SetTextSize(0.035);
    legsiglossK0S2->SetLineWidth(0);
    legsiglossK0S2->SetNColumns(2);
    
    for (int j = 0; j < nbin_mult; j++) {
        h1siglossK0S[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); sig loss");
        h1siglossK0S[j]->SetMarkerStyle(20);
        h1siglossK0S[j]->SetMarkerColor(Colors[j]);
        h1siglossK0S[j]->SetMarkerSize(1.5);
        h1siglossK0S[j]->SetLineColor(Colors[j]);
        h1siglossK0S[j]->SetLineWidth(2);
        h1siglossK0S[j]->SetFillStyle(3001);
        h1siglossK0S[j]->SetFillColor(Colors[j]);
        //h1siglossK0S[j]->GetXaxis()->SetLabelOffset(0.5);
        h1siglossK0S[j]->GetYaxis()->SetTitleSize(0.045);
        h1siglossK0S[j]->GetYaxis()->SetTitleOffset(1.0);
        h1siglossK0S[j]->GetYaxis()->SetLabelSize(0.045);
        h1siglossK0S[j]->GetYaxis()->SetRangeUser(0.4, 1.01);

        if (j == 0) h1siglossK0S[j]->Draw();
        else h1siglossK0S[j]->Draw("same");

        legsiglossK0S2->AddEntry(h1siglossK0S[j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
    }

    legsiglossK0S1->Draw("same");
    legsiglossK0S2->Draw("same");

    TPad* bottomPadSigLossK0S = new TPad("bottomPadSigLossK0S", "Bottom Pad", 0, 0, 1, bottomPadHeight);
    bottomPadSigLossK0S->SetTopMargin(0);
    bottomPadSigLossK0S->SetBottomMargin(0.3);
    //bottomPadSigLossK0S->SetLogy();
    csiglossK0S->cd();
    bottomPadSigLossK0S->Draw();
    bottomPadSigLossK0S->cd();

    for (int j = 0; j < nbin_mult; j++) {
        h1siglossK0SRatio[j] = (TH1F*)h1siglossK0S[j]->Clone(Form("h1siglossK0SRatio%i", j));
        h1siglossK0SRatio[j]->Divide(h1siglossK0SMultInt);
        h1siglossK0SRatio[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
        h1siglossK0SRatio[j]->SetMarkerStyle(20);
        h1siglossK0SRatio[j]->SetMarkerColor(Colors[j]);
        h1siglossK0SRatio[j]->SetMarkerSize(1.5);
        h1siglossK0SRatio[j]->SetLineColor(Colors[j]);
        h1siglossK0SRatio[j]->SetLineWidth(2);
        h1siglossK0SRatio[j]->SetFillStyle(3001);
        h1siglossK0SRatio[j]->SetFillColor(Colors[j]);
        h1siglossK0SRatio[j]->GetXaxis()->SetLabelOffset(0.03);
        h1siglossK0SRatio[j]->GetXaxis()->SetNdivisions(515);
        h1siglossK0SRatio[j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
        h1siglossK0SRatio[j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
        h1siglossK0SRatio[j]->GetXaxis()->SetTitleOffset(1.2);
        h1siglossK0SRatio[j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
        h1siglossK0SRatio[j]->GetYaxis()->SetTitleOffset(0.45);
        h1siglossK0SRatio[j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
        h1siglossK0SRatio[j]->GetYaxis()->SetNdivisions(505);
        h1siglossK0SRatio[j]->GetYaxis()->SetRangeUser(0.61, 1.24);

        if (j == 0) h1siglossK0SRatio[j]->Draw();
        else h1siglossK0SRatio[j]->Draw("same");
    }

    string outsiglossK0S = path + "siglossK0SMB.root";
    csiglossK0S->SaveAs(outsiglossK0S.c_str());
    outsiglossK0S = path + "siglossK0SMB.pdf";
    csiglossK0S->SaveAs(outsiglossK0S.c_str());

    //********************************************************************************************

    TH2F* h2effPhiK0S[nbin_deltay];
    TH2F* h2siglossPhiK0S[nbin_deltay];

    TH1F* h1effPhiK0S[nbin_deltay][nbin_mult];
    TH1F* h1siglossPhiK0S[nbin_deltay][nbin_mult];

    TH1F* h1PhiK0SRecMCMultInt[nbin_deltay];
    TH1F* h1PhiK0SGenMCMultInt[nbin_deltay];
    TH1F* h1PhiK0SGenMCAssocRecoMultInt[nbin_deltay];

    TH1F* h1effPhiK0SMultInt[nbin_deltay];
    TH1F* h1siglossPhiK0SMultInt[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        h2effPhiK0S[i] = (TH2F*)h2PhiK0SRecMC[i]->Clone(Form("h2effeffPhiK0S%i", i));
        h2effPhiK0S[i]->Divide(h2PhiK0SRecMC[i], h2PhiK0SGenMCAssocReco[i], 1, 1, "B");

        h2siglossPhiK0S[i] = (TH2F*)h2PhiK0SGenMCAssocReco[i]->Clone(Form("h2siglossPhiK0S%i", i));
        h2siglossPhiK0S[i]->Divide(h2PhiK0SGenMCAssocReco[i], h2PhiK0SGenMC[i], 1, 1, "B");

        for (int j = 0; j < nbin_mult; j++) {
            h1effPhiK0S[i][j] = (TH1F*)h2effPhiK0S[i]->ProjectionY(Form("h1effPhiK0S%i_%i", i, j), j+1, j+1);
            h1siglossPhiK0S[i][j] = (TH1F*)h2siglossPhiK0S[i]->ProjectionY(Form("h1siglossPhiK0S%i_%i", i, j), j+1, j+1);
        }

        h1PhiK0SRecMCMultInt[i] = (TH1F*)h2PhiK0SRecMC[i]->ProjectionY(Form("h1PhiK0SRecMCMultInt%i", i), 1, nbin_mult);
        h1PhiK0SGenMCMultInt[i] = (TH1F*)h2PhiK0SGenMC[i]->ProjectionY(Form("h1PhiK0SGenMCMultInt%i", i), 1, nbin_mult);
        h1PhiK0SGenMCAssocRecoMultInt[i] = (TH1F*)h2PhiK0SGenMCAssocReco[i]->ProjectionY(Form("h1PhiK0SGenMCAssocRecoMultInt%i", i), 1, nbin_mult);

        h1effPhiK0SMultInt[i] = (TH1F*)h1PhiK0SRecMCMultInt[i]->Clone(Form("h1effPhiK0SMultInt%i", i));
        h1effPhiK0SMultInt[i]->Divide(h1PhiK0SRecMCMultInt[i], h1PhiK0SGenMCAssocRecoMultInt[i], 1, 1, "B");

        h1siglossPhiK0SMultInt[i] = (TH1F*)h1PhiK0SGenMCAssocRecoMultInt[i]->Clone(Form("h1siglossPhiK0SMultInt%i", i));
        h1siglossPhiK0SMultInt[i]->Divide(h1PhiK0SGenMCAssocRecoMultInt[i], h1PhiK0SGenMCMultInt[i], 1, 1, "B");
    }

    TH1F* h1effPhiK0SRatio[nbin_deltay][nbin_mult];
    TH1F* h1siglossPhiK0SRatio[nbin_deltay][nbin_mult];

    TCanvas* ceffPhiK0S[nbin_deltay];
    TPad* topPadEffPhiK0S[nbin_deltay];
    TPad* bottomPadEffPhiK0S[nbin_deltay];

    TLegend* legeffPhiK0S1 [nbin_deltay];
    TLegend* legeffPhiK0S2 [nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        ceffPhiK0S[i] = new TCanvas(Form("ceffPhiK0S%i", i), Form("ceffPhiK0S%i", i), 800, 800);
        ceffPhiK0S[i]->cd();
        //gPad->SetMargin(0.16,0.01,0.13,0.06);
        gStyle->SetOptStat(0);

        topPadEffPhiK0S[i] = new TPad("topPad", "Top Pad", 0, bottomPadHeight, 1, 1);
        topPadEffPhiK0S[i]->SetBottomMargin(0);
        topPadEffPhiK0S[i]->Draw();
        topPadEffPhiK0S[i]->cd();
        
        legeffPhiK0S1[i] = new TLegend(0.5, 0.82, 0.8, 0.85);
        legeffPhiK0S1[i]->SetHeader("#bf{This work}");
        legeffPhiK0S1[i]->SetTextSize(0.05);
        legeffPhiK0S1[i]->SetLineWidth(0);

        legeffPhiK0S2[i] = new TLegend(0.5, 0.62, 0.8, 0.82);
        legeffPhiK0S2[i]->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#it{#Deltay}| < %1.1f", deltay_axis[i]));
        legeffPhiK0S2[i]->SetTextSize(0.035);
        legeffPhiK0S2[i]->SetLineWidth(0);
        legeffPhiK0S2[i]->SetNColumns(2);
        
        for (int j = 0; j < nbin_mult; j++) {
            h1effPhiK0S[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); #varepsilon x acc");
            h1effPhiK0S[i][j]->SetMarkerStyle(20);
            h1effPhiK0S[i][j]->SetMarkerColor(Colors[j]);
            h1effPhiK0S[i][j]->SetMarkerSize(1.5);
            h1effPhiK0S[i][j]->SetLineColor(Colors[j]);
            h1effPhiK0S[i][j]->SetLineWidth(2);
            h1effPhiK0S[i][j]->SetFillStyle(3001);
            h1effPhiK0S[i][j]->SetFillColor(Colors[j]);
            //h1effPhiK0S[i][j]->GetXaxis()->SetLabelOffset(0.05);
            h1effPhiK0S[i][j]->GetYaxis()->SetTitleSize(0.045);
            h1effPhiK0S[i][j]->GetYaxis()->SetTitleOffset(1.0);
            h1effPhiK0S[i][j]->GetYaxis()->SetLabelSize(0.045);
            h1effPhiK0S[i][j]->GetYaxis()->SetRangeUser(0.001, 0.6);

            if (j == 0) h1effPhiK0S[i][j]->Draw();
            else h1effPhiK0S[i][j]->Draw("same");

            legeffPhiK0S2[i]->AddEntry(h1effPhiK0S[i][j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
        }

        legeffPhiK0S1[i]->Draw("same");
        legeffPhiK0S2[i]->Draw("same");

        bottomPadEffPhiK0S[i] = new TPad("bottomPad", "Bottom Pad", 0, 0, 1, bottomPadHeight);
        bottomPadEffPhiK0S[i]->SetTopMargin(0);
        bottomPadEffPhiK0S[i]->SetBottomMargin(0.3);
        //bottomPadEffPhiK0S[i]->SetLogy();
        ceffPhiK0S[i]->cd();
        bottomPadEffPhiK0S[i]->Draw();
        bottomPadEffPhiK0S[i]->cd();

        for (int j = 0; j < nbin_mult; j++) {
            h1effPhiK0SRatio[i][j] = (TH1F*)h1effPhiK0S[i][j]->Clone(Form("h1effPhiK0SRatio%i_%i", i, j));
            h1effPhiK0SRatio[i][j]->Divide(h1effPhiK0SMultInt[i]);
            h1effPhiK0SRatio[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
            h1effPhiK0SRatio[i][j]->SetMarkerStyle(20);
            h1effPhiK0SRatio[i][j]->SetMarkerColor(Colors[j]);
            h1effPhiK0SRatio[i][j]->SetMarkerSize(1.5);
            h1effPhiK0SRatio[i][j]->SetLineColor(Colors[j]);
            h1effPhiK0SRatio[i][j]->SetLineWidth(2);
            h1effPhiK0SRatio[i][j]->SetFillStyle(3001);
            h1effPhiK0SRatio[i][j]->SetFillColor(Colors[j]);
            h1effPhiK0SRatio[i][j]->GetXaxis()->SetLabelOffset(0.03);
            h1effPhiK0SRatio[i][j]->GetXaxis()->SetNdivisions(515);
            h1effPhiK0SRatio[i][j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
            h1effPhiK0SRatio[i][j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
            h1effPhiK0SRatio[i][j]->GetXaxis()->SetTitleOffset(1.2);
            h1effPhiK0SRatio[i][j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
            h1effPhiK0SRatio[i][j]->GetYaxis()->SetTitleOffset(0.45);
            h1effPhiK0SRatio[i][j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
            h1effPhiK0SRatio[i][j]->GetYaxis()->SetNdivisions(505);
            h1effPhiK0SRatio[i][j]->GetYaxis()->SetRangeUser(0.81, 1.24);

            if (j == 0) h1effPhiK0SRatio[i][j]->Draw();
            else h1effPhiK0SRatio[i][j]->Draw("same");
        }

        string outeffPhiK0S = path + "effK0SDY%i.root";
        ceffPhiK0S[i]->SaveAs(Form(outeffPhiK0S.c_str(), i));
        outeffPhiK0S = path + "effK0SDY%i.pdf";
        ceffPhiK0S[i]->SaveAs(Form(outeffPhiK0S.c_str(), i));
    }

    TCanvas* csiglossPhiK0S[nbin_deltay];
    TPad* topPadSigLossPhiK0S[nbin_deltay];
    TPad* bottomPadSigLossPhiK0S[nbin_deltay];

    TLegend* legsiglossPhiK0S1 [nbin_deltay];
    TLegend* legsiglossPhiK0S2 [nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        csiglossPhiK0S[i] = new TCanvas(Form("csiglossPhiK0S%i", i), Form("csiglossPhiK0S%i", i), 800, 800);
        csiglossPhiK0S[i]->cd();
        //gPad->SetMargin(0.16,0.01,0.13,0.06);
        gStyle->SetOptStat(0);

        topPadSigLossPhiK0S[i] = new TPad("topPad", "Top Pad", 0, bottomPadHeight, 1, 1);
        topPadSigLossPhiK0S[i]->SetBottomMargin(0);
        topPadSigLossPhiK0S[i]->Draw();
        topPadSigLossPhiK0S[i]->cd();

        legsiglossPhiK0S1[i] = new TLegend(0.5, 0.82, 0.8, 0.85);
        legsiglossPhiK0S1[i]->SetHeader("#bf{This work}");
        legsiglossPhiK0S1[i]->SetTextSize(0.05);
        legsiglossPhiK0S1[i]->SetLineWidth(0);

        legsiglossPhiK0S2[i] = new TLegend(0.5, 0.62, 0.8, 0.82);
        legsiglossPhiK0S2[i]->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#it{#Deltay}| < %1.1f", deltay_axis[i]));
        legsiglossPhiK0S2[i]->SetTextSize(0.035);
        legsiglossPhiK0S2[i]->SetLineWidth(0);
        legsiglossPhiK0S2[i]->SetNColumns(2);
        
        for (int j = 0; j < nbin_mult; j++) {
            h1siglossPhiK0S[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); sig loss");
            h1siglossPhiK0S[i][j]->SetMarkerStyle(20);
            h1siglossPhiK0S[i][j]->SetMarkerColor(Colors[j]);
            h1siglossPhiK0S[i][j]->SetMarkerSize(1.5);
            h1siglossPhiK0S[i][j]->SetLineColor(Colors[j]);
            h1siglossPhiK0S[i][j]->SetLineWidth(2);
            h1siglossPhiK0S[i][j]->SetFillStyle(3001);
            h1siglossPhiK0S[i][j]->SetFillColor(Colors[j]);
            //h1siglossPhiK0S[i][j]->GetXaxis()->SetLabelOffset(0.5);
            h1siglossPhiK0S[i][j]->GetYaxis()->SetTitleSize(0.045);
            h1siglossPhiK0S[i][j]->GetYaxis()->SetTitleOffset(1.0);
            h1siglossPhiK0S[i][j]->GetYaxis()->SetLabelSize(0.045);
            h1siglossPhiK0S[i][j]->GetYaxis()->SetRangeUser(0.4, 1.01);

            if (j == 0) h1siglossPhiK0S[i][j]->Draw();
            else h1siglossPhiK0S[i][j]->Draw("same");

            legsiglossPhiK0S2[i]->AddEntry(h1siglossPhiK0S[i][j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
        }

        legsiglossPhiK0S1[i]->Draw("same");
        legsiglossPhiK0S2[i]->Draw("same");

        bottomPadSigLossPhiK0S[i] = new TPad("bottomPad", "Bottom Pad", 0, 0, 1, bottomPadHeight);
        bottomPadSigLossPhiK0S[i]->SetTopMargin(0);
        bottomPadSigLossPhiK0S[i]->SetBottomMargin(0.3);
        //bottomPadSigLossPhiK0S[i]->SetLogy();
        csiglossPhiK0S[i]->cd();
        bottomPadSigLossPhiK0S[i]->Draw();
        bottomPadSigLossPhiK0S[i]->cd();

        for (int j = 0; j < nbin_mult; j++) {
            h1siglossPhiK0SRatio[i][j] = (TH1F*)h1siglossPhiK0S[i][j]->Clone(Form("h1effPhiK0SRatio%i_%i", i, j));
            h1siglossPhiK0SRatio[i][j]->Divide(h1siglossPhiK0SMultInt[i]);
            h1siglossPhiK0SRatio[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
            h1siglossPhiK0SRatio[i][j]->SetMarkerStyle(20);
            h1siglossPhiK0SRatio[i][j]->SetMarkerColor(Colors[j]);
            h1siglossPhiK0SRatio[i][j]->SetMarkerSize(1.5);
            h1siglossPhiK0SRatio[i][j]->SetLineColor(Colors[j]);
            h1siglossPhiK0SRatio[i][j]->SetLineWidth(2);
            h1siglossPhiK0SRatio[i][j]->SetFillStyle(3001);
            h1siglossPhiK0SRatio[i][j]->SetFillColor(Colors[j]);
            h1siglossPhiK0SRatio[i][j]->GetXaxis()->SetLabelOffset(0.03);
            h1siglossPhiK0SRatio[i][j]->GetXaxis()->SetNdivisions(515);
            h1siglossPhiK0SRatio[i][j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
            h1siglossPhiK0SRatio[i][j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
            h1siglossPhiK0SRatio[i][j]->GetXaxis()->SetTitleOffset(1.2);
            h1siglossPhiK0SRatio[i][j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
            h1siglossPhiK0SRatio[i][j]->GetYaxis()->SetTitleOffset(0.45);
            h1siglossPhiK0SRatio[i][j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
            h1siglossPhiK0SRatio[i][j]->GetYaxis()->SetNdivisions(505);
            h1siglossPhiK0SRatio[i][j]->GetYaxis()->SetRangeUser(0.61, 1.24);

            if (j == 0) h1siglossPhiK0SRatio[i][j]->Draw();
            else h1siglossPhiK0SRatio[i][j]->Draw("same");
        }

        string outsiglossPhiK0S = path + "siglossK0SDY%i.root";
        csiglossPhiK0S[i]->SaveAs(Form(outsiglossPhiK0S.c_str(), i));
        outsiglossPhiK0S = path + "siglossK0SDY%i.pdf";
        csiglossPhiK0S[i]->SaveAs(Form(outsiglossPhiK0S.c_str(), i));
    }

    //********************************************************************************************

    TH2F* h2effTrackPi = (TH2F*)h2PiRecMCTPC->Clone("h2effTrackPi");
    h2effTrackPi->Divide(h2PiRecMCTPC, h2PiGenMCAssocReco, 1, 1, "B");

    TH2F* h2effMatchPi = (TH2F*)h2PiRecMCTPCTOF->Clone("h2effMatchPi");
    h2effMatchPi->Divide(h2PiRecMCTPCTOF, h2PiRecMCTPC, 1, 1, "B");

    TH2F* h2siglossPi = (TH2F*)h2PiGenMCAssocReco->Clone("h2siglossPi");
    h2siglossPi->Divide(h2PiGenMCAssocReco, h2PiGenMC, 1, 1, "B");

    TH1F* h1effTrackPi[nbin_mult];
    TH1F* h1effMatchPi[nbin_mult];
    TH1F* h1effPi[nbin_mult];
    TH1F* h1siglossPi[nbin_mult];
    for (int j = 0; j < nbin_mult; j++) {
        h1effTrackPi[j] = (TH1F*)h2effTrackPi->ProjectionY(Form("h1effTrackPi%i", j), j+1, j+1);
        h1effTrackPi[j]->SetBinContent(1, 0);
        h1effTrackPi[j]->SetBinError(1, 0);
        h1effMatchPi[j] = (TH1F*)h2effMatchPi->ProjectionY(Form("h1effMatchPi%i", j), j+1, j+1);
        h1effMatchPi[j]->SetBinContent(1, 0);
        h1effMatchPi[j]->SetBinError(1, 0);
        h1effPi[j] = (TH1F*)h1effTrackPi[j]->Clone(Form("h1effPi%i", j));
        h1effPi[j]->Multiply(h1effTrackPi[j], h1effMatchPi[j], 1, 1, "B");
        h1effPi[j]->SetBinContent(1, 0);
        h1effPi[j]->SetBinError(1, 0);
        h1siglossPi[j] = (TH1F*)h2siglossPi->ProjectionY(Form("h1siglossPi%i", j), j+1, j+1);
        h1siglossPi[j]->SetBinContent(1, 0);
        h1siglossPi[j]->SetBinError(1, 0);
    }

    TH1F* h1PiRecMCTPCMultInt = (TH1F*)h2PiRecMCTPC->ProjectionY("h1PiRecMCTPCMultInt", 1, nbin_mult);
    TH1F* h1PiRecMCTPCTOFMultInt = (TH1F*)h2PiRecMCTPCTOF->ProjectionY("h1PiRecMCTPCTOFMultInt", 1, nbin_mult);
    TH1F* h1PiGenMCMultInt = (TH1F*)h2PiGenMC->ProjectionY("h1PiGenMCMultInt", 1, nbin_mult);
    TH1F* h1PiGenMCAssocRecoMultInt = (TH1F*)h2PiGenMCAssocReco->ProjectionY("h1PiGenMCAssocRecoMultInt", 1, nbin_mult);

    TH1F* h1effTrackPiMultInt = (TH1F*)h1PiRecMCTPCMultInt->Clone("h1effTrackPiMultInt");
    h1effTrackPiMultInt->Divide(h1PiRecMCTPCMultInt, h1PiGenMCAssocRecoMultInt, 1, 1, "B");
    h1effTrackPiMultInt->SetBinContent(1, 0);
    h1effTrackPiMultInt->SetBinError(1, 0);

    TH1F* h1effMatchPiMultInt = (TH1F*)h1PiRecMCTPCTOFMultInt->Clone("h1effMatchPiMultInt");
    h1effMatchPiMultInt->Divide(h1PiRecMCTPCTOFMultInt, h1PiRecMCTPCMultInt, 1, 1, "B");
    h1effMatchPiMultInt->SetBinContent(1, 0);
    h1effMatchPiMultInt->SetBinError(1, 0);

    TH1F* h1effPiMultInt = (TH1F*)h1effTrackPiMultInt->Clone("h1effPiMultInt");
    h1effPiMultInt->Multiply(h1effTrackPiMultInt, h1effMatchPiMultInt, 1, 1, "B");
    h1effPiMultInt->SetBinContent(1, 0);
    h1effPiMultInt->SetBinError(1, 0);

    TH1F* h1siglossPiMultInt = (TH1F*)h1PiGenMCAssocRecoMultInt->Clone("h1siglossPiMultInt");
    h1siglossPiMultInt->Divide(h1PiGenMCAssocRecoMultInt, h1PiGenMCMultInt, 1, 1, "B");
    h1siglossPiMultInt->SetBinContent(1, 0);
    h1siglossPiMultInt->SetBinError(1, 0);

    TH1F* h1effPiRatio[nbin_mult];
    TH1F* h1siglossPiRatio[nbin_mult];

    TCanvas* ceffPi = new TCanvas("ceffPi", "ceffPi", 800, 800);
    ceffPi->cd();
    //gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);

    TPad* topPadEffPi = new TPad("topPadEffPi", "Top Pad", 0, bottomPadHeight, 1, 1);
    topPadEffPi->SetBottomMargin(0);
    topPadEffPi->Draw();
    topPadEffPi->cd();

    TLegend* legeffPi1 = new TLegend(0.5, 0.69, 0.8, 0.72);
    legeffPi1->SetHeader("#bf{This work}");
    legeffPi1->SetTextSize(0.05);
    legeffPi1->SetLineWidth(0);

    TLegend* legeffPi2 = new TLegend(0.5, 0.49, 0.8, 0.69);
    legeffPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, MB");
    legeffPi2->SetTextSize(0.035);
    legeffPi2->SetLineWidth(0);
    legeffPi2->SetNColumns(2);

    TLegend* legeffPi3 = new TLegend(0.5, 0.75, 0.8, 0.78);
    legeffPi3->SetTextSize(0.035);
    legeffPi3->SetLineWidth(0);
    legeffPi3->SetHeader("#bf{Tracking efficiency}");

    TLegend* legeffPi4 = new TLegend(0.5, 0.39, 0.8, 0.42);
    legeffPi4->SetTextSize(0.035);
    legeffPi4->SetLineWidth(0);
    legeffPi4->SetHeader("#bf{Matching efficiency}");

    TLegend* legeffPi5 = new TLegend(0.5, 0.27, 0.8, 0.3);
    legeffPi5->SetTextSize(0.035);
    legeffPi5->SetLineWidth(0);
    legeffPi5->SetHeader("#bf{Total efficiency}");
    
    for (int j = 0; j < nbin_mult; j++) {
        h1effTrackPi[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); #varepsilon x acc");
        h1effTrackPi[j]->SetMarkerStyle(22);
        h1effTrackPi[j]->SetMarkerColor(Colors[j]);
        h1effTrackPi[j]->SetMarkerSize(1.5);
        h1effTrackPi[j]->SetLineColor(Colors[j]);
        h1effTrackPi[j]->SetLineWidth(2);
        h1effTrackPi[j]->SetFillStyle(3001);
        h1effTrackPi[j]->SetFillColor(Colors[j]);
        h1effTrackPi[j]->GetYaxis()->SetTitleSize(0.045);
        h1effTrackPi[j]->GetYaxis()->SetTitleOffset(1.0);
        h1effTrackPi[j]->GetYaxis()->SetLabelSize(0.045);
        h1effTrackPi[j]->GetYaxis()->SetRangeUser(0.02, 1.01);

        if (j == 0) h1effTrackPi[j]->Draw();
        else h1effTrackPi[j]->Draw("same");

        h1effMatchPi[j]->SetMarkerStyle(21);
        h1effMatchPi[j]->SetMarkerColor(Colors[j]);
        h1effMatchPi[j]->SetMarkerSize(1.5);
        h1effMatchPi[j]->SetLineColor(Colors[j]);
        h1effMatchPi[j]->SetLineWidth(2);
        h1effMatchPi[j]->SetFillStyle(3001);
        h1effMatchPi[j]->SetFillColor(Colors[j]);
        h1effMatchPi[j]->Draw("same");

        h1effPi[j]->SetMarkerStyle(20);
        h1effPi[j]->SetMarkerColor(Colors[j]);
        h1effPi[j]->SetMarkerSize(1.5);
        h1effPi[j]->SetLineColor(Colors[j]);
        h1effPi[j]->SetLineWidth(2);
        h1effPi[j]->SetFillStyle(3001);
        h1effPi[j]->SetFillColor(Colors[j]);
        h1effPi[j]->Draw("same");

        legeffPi2->AddEntry(h1effPi[j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
    }

    legeffPi1->Draw("same");
    legeffPi2->Draw("same");
    legeffPi3->Draw("same");
    legeffPi4->Draw("same");
    legeffPi5->Draw("same");

    TPad* bottomPadEffPi = new TPad("bottomPadEffPi", "Bottom Pad", 0, 0, 1, bottomPadHeight);
    bottomPadEffPi->SetTopMargin(0);
    bottomPadEffPi->SetBottomMargin(0.3);
    //bottomPadEffPi->SetLogy();
    ceffPi->cd();
    bottomPadEffPi->Draw();
    bottomPadEffPi->cd();

    for (int j = 0; j < nbin_mult; j++) {
        h1effPiRatio[j] = (TH1F*)h1effPi[j]->Clone(Form("h1effPiRatio%i", j));
        h1effPiRatio[j]->Divide(h1effPiMultInt);
        h1effPiRatio[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
        h1effPiRatio[j]->SetMarkerStyle(20);
        h1effPiRatio[j]->SetMarkerColor(Colors[j]);
        h1effPiRatio[j]->SetMarkerSize(1.5);
        h1effPiRatio[j]->SetLineColor(Colors[j]);
        h1effPiRatio[j]->SetLineWidth(2);
        h1effPiRatio[j]->SetFillStyle(3001);
        h1effPiRatio[j]->SetFillColor(Colors[j]);
        h1effPiRatio[j]->GetXaxis()->SetLabelOffset(0.03);
        h1effPiRatio[j]->GetXaxis()->SetNdivisions(515);
        h1effPiRatio[j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
        h1effPiRatio[j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
        h1effPiRatio[j]->GetXaxis()->SetTitleOffset(1.2);
        h1effPiRatio[j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
        h1effPiRatio[j]->GetYaxis()->SetTitleOffset(0.45);
        h1effPiRatio[j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
        h1effPiRatio[j]->GetYaxis()->SetNdivisions(505);
        h1effPiRatio[j]->GetYaxis()->SetRangeUser(0.81, 1.24);

        if (j == 0) h1effPiRatio[j]->Draw();
        else h1effPiRatio[j]->Draw("same");
    }

    string outeffPi = path + "effPiMB.root";
    ceffPi->SaveAs(outeffPi.c_str());
    outeffPi = path + "effPiMB.pdf";
    ceffPi->SaveAs(outeffPi.c_str());

    TCanvas* csiglossPi = new TCanvas("csiglossPi", "csiglossPi", 800, 800);
    csiglossPi->cd();
    //gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);

    TPad* topPadSigLossPi = new TPad("topPadSigLossPi", "Top Pad", 0, bottomPadHeight, 1, 1);
    topPadSigLossPi->SetBottomMargin(0);
    topPadSigLossPi->Draw();
    topPadSigLossPi->cd();

    TLegend* legsiglossPi1 = new TLegend(0.5, 0.82, 0.8, 0.85);
    legsiglossPi1->SetHeader("#bf{This work}");
    legsiglossPi1->SetTextSize(0.05);
    legsiglossPi1->SetLineWidth(0);

    TLegend* legsiglossPi2 = new TLegend(0.5, 0.62, 0.8, 0.82);
    legsiglossPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, MB");
    legsiglossPi2->SetTextSize(0.035);
    legsiglossPi2->SetLineWidth(0);
    legsiglossPi2->SetNColumns(2);
    
    for (int j = 0; j < nbin_mult; j++) {
        h1siglossPi[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); sig loss");
        h1siglossPi[j]->SetMarkerStyle(20);
        h1siglossPi[j]->SetMarkerColor(Colors[j]);
        h1siglossPi[j]->SetMarkerSize(1.5);
        h1siglossPi[j]->SetLineColor(Colors[j]);
        h1siglossPi[j]->SetLineWidth(2);
        h1siglossPi[j]->SetFillStyle(3001);
        h1siglossPi[j]->SetFillColor(Colors[j]);
        //h1siglossPi[j]->GetXaxis()->SetLabelOffset(0.5);
        h1siglossPi[j]->GetYaxis()->SetTitleSize(0.045);
        h1siglossPi[j]->GetYaxis()->SetTitleOffset(1.0);
        h1siglossPi[j]->GetYaxis()->SetLabelSize(0.045);
        h1siglossPi[j]->GetYaxis()->SetRangeUser(0.4, 1.01);

        if (j == 0) h1siglossPi[j]->Draw();
        else h1siglossPi[j]->Draw("same");

        legsiglossPi2->AddEntry(h1siglossPi[j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
    }

    legsiglossPi1->Draw("same");
    legsiglossPi2->Draw("same");

    TPad* bottomPadSigLossPi = new TPad("bottomPadSigLossPi", "Bottom Pad", 0, 0, 1, bottomPadHeight);
    bottomPadSigLossPi->SetTopMargin(0);
    bottomPadSigLossPi->SetBottomMargin(0.3);
    //bottomPadSigLossPi->SetLogy();
    csiglossPi->cd();
    bottomPadSigLossPi->Draw();
    bottomPadSigLossPi->cd();

    for (int j = 0; j < nbin_mult; j++) {
        h1siglossPiRatio[j] = (TH1F*)h1siglossPi[j]->Clone(Form("h1siglossPiRatio%i", j));
        h1siglossPiRatio[j]->Divide(h1siglossPiMultInt);
        h1siglossPiRatio[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
        h1siglossPiRatio[j]->SetMarkerStyle(20);
        h1siglossPiRatio[j]->SetMarkerColor(Colors[j]);
        h1siglossPiRatio[j]->SetMarkerSize(1.5);
        h1siglossPiRatio[j]->SetLineColor(Colors[j]);
        h1siglossPiRatio[j]->SetLineWidth(2);
        h1siglossPiRatio[j]->SetFillStyle(3001);
        h1siglossPiRatio[j]->SetFillColor(Colors[j]);
        h1siglossPiRatio[j]->GetXaxis()->SetLabelOffset(0.03);
        h1siglossPiRatio[j]->GetXaxis()->SetNdivisions(515);
        h1siglossPiRatio[j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
        h1siglossPiRatio[j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
        h1siglossPiRatio[j]->GetXaxis()->SetTitleOffset(1.2);
        h1siglossPiRatio[j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
        h1siglossPiRatio[j]->GetYaxis()->SetTitleOffset(0.45);
        h1siglossPiRatio[j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
        h1siglossPiRatio[j]->GetYaxis()->SetNdivisions(505);
        h1siglossPiRatio[j]->GetYaxis()->SetRangeUser(0.81, 1.24);

        if (j == 0) h1siglossPiRatio[j]->Draw();
        else h1siglossPiRatio[j]->Draw("same");
    }

    string outsiglossPi = path + "siglossPiMB.root";
    csiglossPi->SaveAs(outsiglossPi.c_str());
    outsiglossPi = path + "siglossPiMB.pdf";
    csiglossPi->SaveAs(outsiglossPi.c_str());

    //********************************************************************************************

    TH2F* h2effTrackPhiPi[nbin_deltay];
    TH2F* h2effMatchPhiPi[nbin_deltay];
    TH2F* h2siglossPhiPi[nbin_deltay];

    TH1F* h1effTrackPhiPi[nbin_deltay][nbin_mult];
    TH1F* h1effMatchPhiPi[nbin_deltay][nbin_mult];
    TH1F* h1effPhiPi[nbin_deltay][nbin_mult];
    TH1F* h1siglossPhiPi[nbin_deltay][nbin_mult];

    TH1F* h1PhiPiRecMCTPCMultInt[nbin_deltay];
    TH1F* h1PhiPiRecMCTPCTOFMultInt[nbin_deltay];
    TH1F* h1PhiPiGenMCMultInt[nbin_deltay];
    TH1F* h1PhiPiGenMCAssocRecoMultInt[nbin_deltay];

    TH1F* h1effTrackPhiPiMultInt[nbin_deltay];
    TH1F* h1effMatchPhiPiMultInt[nbin_deltay];
    TH1F* h1effPhiPiMultInt[nbin_deltay];
    TH1F* h1siglossPhiPiMultInt[nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        h2effTrackPhiPi[i] = (TH2F*)h2PhiPiRecMCTPC[i]->Clone(Form("h2effTrackPhiPi%i", i));
        h2effTrackPhiPi[i]->Divide(h2PhiPiRecMCTPC[i], h2PhiPiGenMCAssocReco[i], 1, 1, "B");

        h2effMatchPhiPi[i] = (TH2F*)h2PhiPiRecMCTPCTOF[i]->Clone(Form("h2effMatchPhiPi%i", i));
        h2effMatchPhiPi[i]->Divide(h2PhiPiRecMCTPCTOF[i], h2PhiPiRecMCTPC[i], 1, 1, "B");

        h2siglossPhiPi[i] = (TH2F*)h2PhiPiGenMCAssocReco[i]->Clone(Form("h2siglossPhiPi%i", i));
        h2siglossPhiPi[i]->Divide(h2PhiPiGenMCAssocReco[i], h2PhiPiGenMC[i], 1, 1, "B");

        for (int j = 0; j < nbin_mult; j++) {
            
            h1effTrackPhiPi[i][j] = (TH1F*)h2effTrackPhiPi[i]->ProjectionY(Form("h1effTrackPhiPi%i_%i", i, j), j+1, j+1);
            h1effTrackPhiPi[i][j]->SetBinContent(1, 0);
            h1effTrackPhiPi[i][j]->SetBinError(1, 0);
            h1effMatchPhiPi[i][j] = (TH1F*)h2effMatchPhiPi[i]->ProjectionY(Form("h1effMatchPhiPi%i_%i", i, j), j+1, j+1);
            h1effMatchPhiPi[i][j]->SetBinContent(1, 0);
            h1effMatchPhiPi[i][j]->SetBinError(1, 0);

            h1effPhiPi[i][j] = (TH1F*)h1effTrackPhiPi[i][j]->Clone(Form("h1effPhiPi%i_%i", i, j));
            h1effPhiPi[i][j]->Multiply(h1effTrackPhiPi[i][j], h1effMatchPhiPi[i][j], 1, 1, "B");
            h1effPhiPi[i][j]->SetBinContent(1, 0);
            h1effPhiPi[i][j]->SetBinError(1, 0);

            h1siglossPhiPi[i][j] = (TH1F*)h2siglossPhiPi[i]->ProjectionY(Form("h1siglossPhiPi%i_%i", i, j), j+1, j+1);
            h1siglossPhiPi[i][j]->SetBinContent(1, 0);
            h1siglossPhiPi[i][j]->SetBinError(1, 0);

        }

        h1PhiPiRecMCTPCMultInt[i] = (TH1F*)h2PhiPiRecMCTPC[i]->ProjectionY(Form("h1PhiPiRecTPCMCMultInt%i", i), 1, nbin_mult);
        h1PhiPiRecMCTPCTOFMultInt[i] = (TH1F*)h2PhiPiRecMCTPCTOF[i]->ProjectionY(Form("h1PhiPiRecTPCTOFMCMultInt%i", i), 1, nbin_mult);
        h1PhiPiGenMCMultInt[i] = (TH1F*)h2PhiPiGenMC[i]->ProjectionY(Form("h1PhiPiGenMCMultInt%i", i), 1, nbin_mult);
        h1PhiPiGenMCAssocRecoMultInt[i] = (TH1F*)h2PhiPiGenMCAssocReco[i]->ProjectionY(Form("h1PhiPiGenMCAssocRecoMultInt%i", i), 1, nbin_mult);

        h1effTrackPhiPiMultInt[i] = (TH1F*)h1PhiPiRecMCTPCMultInt[i]->Clone(Form("h1effTrackPhiPiMultInt%i", i));
        h1effTrackPhiPiMultInt[i]->Divide(h1PhiPiRecMCTPCMultInt[i], h1PhiPiGenMCAssocRecoMultInt[i], 1, 1, "B");
        h1effTrackPhiPiMultInt[i]->SetBinContent(1, 0);
        h1effTrackPhiPiMultInt[i]->SetBinError(1, 0);

        h1effMatchPhiPiMultInt[i] = (TH1F*)h1PhiPiRecMCTPCTOFMultInt[i]->Clone(Form("h1effMatchPhiPiMultInt%i", i));
        h1effMatchPhiPiMultInt[i]->Divide(h1PhiPiRecMCTPCTOFMultInt[i], h1PhiPiRecMCTPCMultInt[i], 1, 1, "B");
        h1effMatchPhiPiMultInt[i]->SetBinContent(1, 0);
        h1effMatchPhiPiMultInt[i]->SetBinError(1, 0);

        h1effPhiPiMultInt[i] = (TH1F*)h1effTrackPhiPiMultInt[i]->Clone(Form("h1effPhiPiMultInt%i", i));
        h1effPhiPiMultInt[i]->Multiply(h1effTrackPhiPiMultInt[i], h1effMatchPhiPiMultInt[i], 1, 1, "B");
        h1effPhiPiMultInt[i]->SetBinContent(1, 0);
        h1effPhiPiMultInt[i]->SetBinError(1, 0);

        h1siglossPhiPiMultInt[i] = (TH1F*)h1PhiPiGenMCAssocRecoMultInt[i]->Clone(Form("h1siglossPhiPiMultInt%i", i));
        h1siglossPhiPiMultInt[i]->Divide(h1PhiPiGenMCAssocRecoMultInt[i], h1PhiPiGenMCMultInt[i], 1, 1, "B");
        h1siglossPhiPiMultInt[i]->SetBinContent(1, 0);
        h1siglossPhiPiMultInt[i]->SetBinError(1, 0);
    }

    TH1F* h1effPhiPiRatio[nbin_deltay][nbin_mult];
    TH1F* h1siglossPhiPiRatio[nbin_deltay][nbin_mult];

    TCanvas* ceffPhiPi[nbin_deltay];
    TPad* topPadEffPhiPi[nbin_deltay];
    TPad* bottomPadEffPhiPi[nbin_deltay];

    TLegend* legeffPhiPi1 [nbin_deltay];
    TLegend* legeffPhiPi2 [nbin_deltay];
    TLegend* legeffPhiPi3 [nbin_deltay];
    TLegend* legeffPhiPi4 [nbin_deltay];
    TLegend* legeffPhiPi5 [nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        ceffPhiPi[i] = new TCanvas(Form("ceffPhiPi%i", i), Form("ceffPhiPi%i", i), 800, 800);
        ceffPhiPi[i]->cd();
        //gPad->SetMargin(0.16,0.01,0.13,0.06);
        gStyle->SetOptStat(0);

        topPadEffPhiPi[i] = new TPad("topPad", "Top Pad", 0, bottomPadHeight, 1, 1);
        topPadEffPhiPi[i]->SetBottomMargin(0);
        topPadEffPhiPi[i]->Draw();
        topPadEffPhiPi[i]->cd();

        legeffPhiPi1[i] = new TLegend(0.5, 0.69, 0.8, 0.72);
        legeffPhiPi1[i]->SetHeader("#bf{This work}");
        legeffPhiPi1[i]->SetTextSize(0.05);
        legeffPhiPi1[i]->SetLineWidth(0);

        legeffPhiPi2[i] = new TLegend(0.5, 0.49, 0.8, 0.69);
        legeffPhiPi2[i]->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#it{#Deltay}| < %1.1f", deltay_axis[i]));
        legeffPhiPi2[i]->SetTextSize(0.035);
        legeffPhiPi2[i]->SetLineWidth(0);
        legeffPhiPi2[i]->SetNColumns(2);

        legeffPhiPi3[i] = new TLegend(0.5, 0.75, 0.8, 0.78);
        legeffPhiPi3[i]->SetTextSize(0.035);
        legeffPhiPi3[i]->SetLineWidth(0);
        legeffPhiPi3[i]->SetHeader("#bf{Tracking efficiency}");

        legeffPhiPi4[i] = new TLegend(0.5, 0.39, 0.8, 0.42);
        legeffPhiPi4[i]->SetTextSize(0.035);
        legeffPhiPi4[i]->SetLineWidth(0);
        legeffPhiPi4[i]->SetHeader("#bf{Matching efficiency}");

        legeffPhiPi5[i] = new TLegend(0.5, 0.27, 0.8, 0.3);
        legeffPhiPi5[i]->SetTextSize(0.035);
        legeffPhiPi5[i]->SetLineWidth(0);
        legeffPhiPi5[i]->SetHeader("#bf{Total efficiency}");
        
        for (int j = 0; j < nbin_mult; j++) {
            h1effTrackPhiPi[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); #varepsilon x acc");
            h1effTrackPhiPi[i][j]->SetMarkerStyle(22);
            h1effTrackPhiPi[i][j]->SetMarkerColor(Colors[j]);
            h1effTrackPhiPi[i][j]->SetMarkerSize(1.5);
            h1effTrackPhiPi[i][j]->SetLineColor(Colors[j]);
            h1effTrackPhiPi[i][j]->SetLineWidth(2);
            h1effTrackPhiPi[i][j]->SetFillStyle(3001);
            h1effTrackPhiPi[i][j]->SetFillColor(Colors[j]);
            h1effTrackPhiPi[i][j]->GetYaxis()->SetTitleSize(0.045);
            h1effTrackPhiPi[i][j]->GetYaxis()->SetTitleOffset(1.0);
            h1effTrackPhiPi[i][j]->GetYaxis()->SetLabelSize(0.045);
            h1effTrackPhiPi[i][j]->GetYaxis()->SetRangeUser(0.02, 1.01);

            if (j == 0) h1effTrackPhiPi[i][j]->Draw();
            else h1effTrackPhiPi[i][j]->Draw("same");

            h1effMatchPhiPi[i][j]->SetMarkerStyle(21);
            h1effMatchPhiPi[i][j]->SetMarkerColor(Colors[j]);
            h1effMatchPhiPi[i][j]->SetMarkerSize(1.5);
            h1effMatchPhiPi[i][j]->SetLineColor(Colors[j]);
            h1effMatchPhiPi[i][j]->SetLineWidth(2);
            h1effMatchPhiPi[i][j]->SetFillStyle(3001);
            h1effMatchPhiPi[i][j]->SetFillColor(Colors[j]);
            h1effMatchPhiPi[i][j]->Draw("same");

            h1effPhiPi[i][j]->SetMarkerStyle(20);
            h1effPhiPi[i][j]->SetMarkerColor(Colors[j]);
            h1effPhiPi[i][j]->SetMarkerSize(1.5);
            h1effPhiPi[i][j]->SetLineColor(Colors[j]);
            h1effPhiPi[i][j]->SetLineWidth(2);
            h1effPhiPi[i][j]->SetFillStyle(3001);
            h1effPhiPi[i][j]->SetFillColor(Colors[j]);
            h1effPhiPi[i][j]->Draw("same");

            legeffPhiPi2[i]->AddEntry(h1effPhiPi[i][j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
        }

        legeffPhiPi1[i]->Draw("same");
        legeffPhiPi2[i]->Draw("same");
        legeffPhiPi3[i]->Draw("same");
        legeffPhiPi4[i]->Draw("same");
        legeffPhiPi5[i]->Draw("same");

        bottomPadEffPhiPi[i] = new TPad("bottomPad", "Bottom Pad", 0, 0, 1, bottomPadHeight);
        bottomPadEffPhiPi[i]->SetTopMargin(0);
        bottomPadEffPhiPi[i]->SetBottomMargin(0.3);
        //bottomPadEffPhiPi[i]->SetLogy();
        ceffPhiPi[i]->cd();
        bottomPadEffPhiPi[i]->Draw();
        bottomPadEffPhiPi[i]->cd();

        for (int j = 0; j < nbin_mult; j++) {
            h1effPhiPiRatio[i][j] = (TH1F*)h1effPhiPi[i][j]->Clone(Form("h1effPhiPiRatio%i_%i", i, j));
            h1effPhiPiRatio[i][j]->Divide(h1effPhiPiMultInt[i]);
            h1effPhiPiRatio[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
            h1effPhiPiRatio[i][j]->SetMarkerStyle(20);
            h1effPhiPiRatio[i][j]->SetMarkerColor(Colors[j]);
            h1effPhiPiRatio[i][j]->SetMarkerSize(1.5);
            h1effPhiPiRatio[i][j]->SetLineColor(Colors[j]);
            h1effPhiPiRatio[i][j]->SetLineWidth(2);
            h1effPhiPiRatio[i][j]->SetFillStyle(3001);
            h1effPhiPiRatio[i][j]->SetFillColor(Colors[j]);
            h1effPhiPiRatio[i][j]->GetXaxis()->SetLabelOffset(0.03);
            h1effPhiPiRatio[i][j]->GetXaxis()->SetNdivisions(515);
            h1effPhiPiRatio[i][j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
            h1effPhiPiRatio[i][j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
            h1effPhiPiRatio[i][j]->GetXaxis()->SetTitleOffset(1.2);
            h1effPhiPiRatio[i][j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
            h1effPhiPiRatio[i][j]->GetYaxis()->SetTitleOffset(0.45);
            h1effPhiPiRatio[i][j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
            h1effPhiPiRatio[i][j]->GetYaxis()->SetNdivisions(505);
            h1effPhiPiRatio[i][j]->GetYaxis()->SetRangeUser(0.81, 1.24);

            if (j == 0) h1effPhiPiRatio[i][j]->Draw();
            else h1effPhiPiRatio[i][j]->Draw("same");
        }

        string outeffPhiPi = path + "effPiDY%i.root";
        ceffPhiPi[i]->SaveAs(Form(outeffPhiPi.c_str(), i));
        outeffPhiPi = path + "effPiDY%i.pdf";
        ceffPhiPi[i]->SaveAs(Form(outeffPhiPi.c_str(), i));
    }

    TCanvas* csiglossPhiPi[nbin_deltay];
    TPad* topPadSigLossPhiPi[nbin_deltay];
    TPad* bottomPadSigLossPhiPi[nbin_deltay];

    TLegend* legsiglossPhiPi1 [nbin_deltay];
    TLegend* legsiglossPhiPi2 [nbin_deltay];

    for (int i = 0; i < nbin_deltay; i++) {
        csiglossPhiPi[i] = new TCanvas(Form("csiglossPhiPi%i", i), Form("csiglossPhiPi%i", i), 800, 800);
        csiglossPhiPi[i]->cd();
        //gPad->SetMargin(0.16,0.01,0.13,0.06);
        gStyle->SetOptStat(0);

        topPadSigLossPhiPi[i] = new TPad("topPad", "Top Pad", 0, bottomPadHeight, 1, 1);
        topPadSigLossPhiPi[i]->SetBottomMargin(0);
        topPadSigLossPhiPi[i]->Draw();
        topPadSigLossPhiPi[i]->cd();

        legsiglossPhiPi1[i] = new TLegend(0.5, 0.82, 0.8, 0.85);
        legsiglossPhiPi1[i]->SetHeader("#bf{This work}");
        legsiglossPhiPi1[i]->SetTextSize(0.05);
        legsiglossPhiPi1[i]->SetLineWidth(0);

        legsiglossPhiPi2[i] = new TLegend(0.5, 0.62, 0.8, 0.82);
        legsiglossPhiPi2[i]->SetHeader(Form("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5, |#it{#Deltay}| < %1.1f", deltay_axis[i]));
        legsiglossPhiPi2[i]->SetTextSize(0.035);
        legsiglossPhiPi2[i]->SetLineWidth(0);
        legsiglossPhiPi2[i]->SetNColumns(2);
        
        for (int j = 0; j < nbin_mult; j++) {
            h1siglossPhiPi[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); sig loss");
            h1siglossPhiPi[i][j]->SetMarkerStyle(20);
            h1siglossPhiPi[i][j]->SetMarkerColor(Colors[j]);
            h1siglossPhiPi[i][j]->SetMarkerSize(1.5);
            h1siglossPhiPi[i][j]->SetLineColor(Colors[j]);
            h1siglossPhiPi[i][j]->SetLineWidth(2);
            h1siglossPhiPi[i][j]->SetFillStyle(3001);
            h1siglossPhiPi[i][j]->SetFillColor(Colors[j]);
            //h1siglossPhiPi[i][j]->GetXaxis()->SetLabelOffset(0.5);
            h1siglossPhiPi[i][j]->GetYaxis()->SetTitleSize(0.045);
            h1siglossPhiPi[i][j]->GetYaxis()->SetTitleOffset(1.0);
            h1siglossPhiPi[i][j]->GetYaxis()->SetLabelSize(0.045);
            h1siglossPhiPi[i][j]->GetYaxis()->SetRangeUser(0.4, 1.01);

            if (j == 0) h1siglossPhiPi[i][j]->Draw();
            else h1siglossPhiPi[i][j]->Draw("same");

            legsiglossPhiPi2[i]->AddEntry(h1siglossPhiPi[i][j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
        }

        legsiglossPhiPi1[i]->Draw("same");
        legsiglossPhiPi2[i]->Draw("same");

        bottomPadSigLossPhiPi[i] = new TPad("bottomPad", "Bottom Pad", 0, 0, 1, bottomPadHeight);
        bottomPadSigLossPhiPi[i]->SetTopMargin(0);
        bottomPadSigLossPhiPi[i]->SetBottomMargin(0.3);
        //bottomPadSigLossPhiPi[i]->SetLogy();
        csiglossPhiPi[i]->cd();
        bottomPadSigLossPhiPi[i]->Draw();
        bottomPadSigLossPhiPi[i]->cd();

        for (int j = 0; j < nbin_mult; j++) {
            h1siglossPhiPiRatio[i][j] = (TH1F*)h1siglossPhiPi[i][j]->Clone(Form("h1siglossPhiPiRatio%i_%i", i, j));
            h1siglossPhiPiRatio[i][j]->Divide(h1siglossPhiPiMultInt[i]);
            h1siglossPhiPiRatio[i][j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); Ratio to 0-100 %");
            h1siglossPhiPiRatio[i][j]->SetMarkerStyle(20);
            h1siglossPhiPiRatio[i][j]->SetMarkerColor(Colors[j]);
            h1siglossPhiPiRatio[i][j]->SetMarkerSize(1.5);
            h1siglossPhiPiRatio[i][j]->SetLineColor(Colors[j]);
            h1siglossPhiPiRatio[i][j]->SetLineWidth(2);
            h1siglossPhiPiRatio[i][j]->SetFillStyle(3001);
            h1siglossPhiPiRatio[i][j]->SetFillColor(Colors[j]);
            h1siglossPhiPiRatio[i][j]->GetXaxis()->SetLabelOffset(0.03);
            h1siglossPhiPiRatio[i][j]->GetXaxis()->SetNdivisions(515);
            h1siglossPhiPiRatio[i][j]->GetXaxis()->SetLabelSize(0.045 / scaleFactor);
            h1siglossPhiPiRatio[i][j]->GetXaxis()->SetTitleSize(0.045 / scaleFactor);
            h1siglossPhiPiRatio[i][j]->GetXaxis()->SetTitleOffset(1.2);
            h1siglossPhiPiRatio[i][j]->GetYaxis()->SetTitleSize(0.045 / scaleFactor - 0.01);
            h1siglossPhiPiRatio[i][j]->GetYaxis()->SetTitleOffset(0.45);
            h1siglossPhiPiRatio[i][j]->GetYaxis()->SetLabelSize(0.045 / scaleFactor);
            h1siglossPhiPiRatio[i][j]->GetYaxis()->SetNdivisions(505);
            h1siglossPhiPiRatio[i][j]->GetYaxis()->SetRangeUser(0.81, 1.24);

            if (j == 0) h1siglossPhiPiRatio[i][j]->Draw();
            else h1siglossPhiPiRatio[i][j]->Draw("same");
        }

        string outsiglossPhiPi = path + "siglossPiDY%i.root";
        csiglossPhiPi[i]->SaveAs(Form(outsiglossPhiPi.c_str(), i));
        outsiglossPhiPi = path + "siglossPiDY%i.pdf";
        csiglossPhiPi[i]->SaveAs(Form(outsiglossPhiPi.c_str(), i));
    }

    //********************************************************************************************

    TH1F* h1effPhiK0SRatioToK0S[nbin_mult];
    TH1F* h1effPhiPiRatioToPi[nbin_mult];

    for (int j = 0; j < nbin_mult; j++) {
        h1effPhiK0SRatioToK0S[j] = (TH1F*)h1effPhiK0S[0][j]->Clone(Form("h1effPhiK0SRatioToK0S%i", j));
        h1effPhiK0SRatioToK0S[j]->Divide(h1effK0S[j]);

        h1effPhiPiRatioToPi[j] = (TH1F*)h1effTrackPhiPi[0][j]->Clone(Form("h1effPhiPiRatioToPi%i", j));
        h1effPhiPiRatioToPi[j]->Divide(h1effTrackPi[j]);
    }

    TCanvas* ceffPhiK0SRatioToK0S = new TCanvas("ceffPhiK0SRatioToK0S", "ceffPhiK0SRatioToK0S", 800, 800);
    ceffPhiK0SRatioToK0S->cd();
    //gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);

    TLegend* legeffPhiK0SRatioToK0S1 = new TLegend(0.5, 0.82, 0.8, 0.85);
    legeffPhiK0SRatioToK0S1->SetHeader("#bf{This work}");
    legeffPhiK0SRatioToK0S1->SetTextSize(0.05);
    legeffPhiK0SRatioToK0S1->SetLineWidth(0);

    TLegend* legeffPhiK0SRatioToK0S2 = new TLegend(0.5, 0.62, 0.8, 0.82);
    legeffPhiK0SRatioToK0S2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legeffPhiK0SRatioToK0S2->SetTextSize(0.035);
    legeffPhiK0SRatioToK0S2->SetLineWidth(0);   
    legeffPhiK0SRatioToK0S2->SetNColumns(2);

    for (int j = 0; j < nbin_mult; j++) {
        h1effPhiK0SRatioToK0S[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); With/Without #phi");
        h1effPhiK0SRatioToK0S[j]->SetMarkerStyle(20);
        h1effPhiK0SRatioToK0S[j]->SetMarkerColor(Colors[j]);
        h1effPhiK0SRatioToK0S[j]->SetMarkerSize(1.5);
        h1effPhiK0SRatioToK0S[j]->SetLineColor(Colors[j]);
        h1effPhiK0SRatioToK0S[j]->SetLineWidth(2);
        h1effPhiK0SRatioToK0S[j]->SetFillStyle(3001);
        h1effPhiK0SRatioToK0S[j]->SetFillColor(Colors[j]);
        //h1effPhiK0SRatioToK0S[j]->GetXaxis()->SetLabelOffset(0.5);
        h1effPhiK0SRatioToK0S[j]->GetYaxis()->SetTitleSize(0.045);
        h1effPhiK0SRatioToK0S[j]->GetYaxis()->SetTitleOffset(1.0);
        h1effPhiK0SRatioToK0S[j]->GetYaxis()->SetLabelSize(0.045);
        h1effPhiK0SRatioToK0S[j]->GetYaxis()->SetRangeUser(0.8, 1.2);

        if (j == 0) h1effPhiK0SRatioToK0S[j]->Draw();
        else h1effPhiK0SRatioToK0S[j]->Draw("same");

        legeffPhiK0SRatioToK0S2->AddEntry(h1effPhiK0SRatioToK0S[j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
    }

    legeffPhiK0SRatioToK0S1->Draw("same");
    legeffPhiK0SRatioToK0S2->Draw("same");

    string outeffPhiK0SRatioToK0S = path + "effPhiK0SRatioToK0S.root";
    ceffPhiK0SRatioToK0S->SaveAs(outeffPhiK0SRatioToK0S.c_str());
    outeffPhiK0SRatioToK0S = path + "effPhiK0SRatioToK0S.pdf";
    ceffPhiK0SRatioToK0S->SaveAs(outeffPhiK0SRatioToK0S.c_str());

    TCanvas* ceffPhiPiRatioToPi = new TCanvas("ceffPhiPiRatioToPi", "ceffPhiPiRatioToPi", 800, 800);
    ceffPhiPiRatioToPi->cd();
    //gPad->SetMargin(0.16,0.01,0.13,0.06);
    gStyle->SetOptStat(0);

    TLegend* legeffPhiPiRatioToPi1 = new TLegend(0.5, 0.82, 0.8, 0.85);
    legeffPhiPiRatioToPi1->SetHeader("#bf{This work}");
    legeffPhiPiRatioToPi1->SetTextSize(0.05);
    legeffPhiPiRatioToPi1->SetLineWidth(0);

    TLegend* legeffPhiPiRatioToPi2 = new TLegend(0.5, 0.62, 0.8, 0.82);
    legeffPhiPiRatioToPi2->SetHeader("pp, #sqrt{#it{s}} = 13.6 TeV, |#it{y}| < 0.5");
    legeffPhiPiRatioToPi2->SetTextSize(0.035);
    legeffPhiPiRatioToPi2->SetLineWidth(0);
    legeffPhiPiRatioToPi2->SetNColumns(2);

    for (int j = 0; j < nbin_mult; j++) {
        h1effPhiPiRatioToPi[j]->SetTitle("; #it{p}_{T} (GeV/#it{c}); With/Without #phi");
        h1effPhiPiRatioToPi[j]->SetMarkerStyle(20);
        h1effPhiPiRatioToPi[j]->SetMarkerColor(Colors[j]);
        h1effPhiPiRatioToPi[j]->SetMarkerSize(1.5);
        h1effPhiPiRatioToPi[j]->SetLineColor(Colors[j]);
        h1effPhiPiRatioToPi[j]->SetLineWidth(2);
        h1effPhiPiRatioToPi[j]->SetFillStyle(3001);
        h1effPhiPiRatioToPi[j]->SetFillColor(Colors[j]);
        //h1effPhiPiRatioToPi[j]->GetXaxis()->SetLabelOffset(0.5);
        h1effPhiPiRatioToPi[j]->GetYaxis()->SetTitleSize(0.045);
        h1effPhiPiRatioToPi[j]->GetYaxis()->SetTitleOffset(1.0);
        h1effPhiPiRatioToPi[j]->GetYaxis()->SetLabelSize(0.045);
        h1effPhiPiRatioToPi[j]->GetYaxis()->SetRangeUser(0.8, 1.2);

        if (j == 0) h1effPhiPiRatioToPi[j]->Draw();
        else h1effPhiPiRatioToPi[j]->Draw("same");

        legeffPhiPiRatioToPi2->AddEntry(h1effPhiPiRatioToPi[j], Form("%i-%i %%", (int)mult_axis[j], (int)mult_axis[j+1]), "p");
    }

    legeffPhiPiRatioToPi1->Draw("same");
    legeffPhiPiRatioToPi2->Draw("same");

    string outeffPhiPiRatioToPi = path + "effPhiPiRatioToPi.root";
    ceffPhiPiRatioToPi->SaveAs(outeffPhiPiRatioToPi.c_str());
    outeffPhiPiRatioToPi = path + "effPhiPiRatioToPi.pdf";
    ceffPhiPiRatioToPi->SaveAs(outeffPhiPiRatioToPi.c_str());

    //********************************************************************************************

    //TCanvas* cDCAxyPrimPi[nbin_pTPi];
    //TCanvas* cDCAxySecPiFromDecays[nbin_pTPi];
    //TCanvas* cDCAxySecPiFromMaterial[nbin_pTPi];

    Float_t nPrimPi[nbin_pTPi] = {}, nSecPiFromDecays[nbin_pTPi] = {}, nSecPiFromMaterial[nbin_pTPi] = {}, nTotPi[nbin_pTPi] = {};
    Float_t nPrimPiErr[nbin_pTPi] = {}, nSecPiFromDecaysErr[nbin_pTPi] = {}, nSecPiFromMaterialErr[nbin_pTPi] = {}, nTotPiErr[nbin_pTPi] = {};
    TH1F* primFractionPi = new TH1F("primFractionPi", "; #it{p}_{T} (GeV/#it{c}); #varepsilon_{prim}", nbin_pTPi, pTPi_axis);

    for (int k = 0; k < nbin_pTPi; k++) {
        /*cDCAxyPrimPi[k] = new TCanvas(Form("cDCAxyPrimPi%i", k), Form("cDCAxyPrimPi%i", k), 800, 800);
        cDCAxyPrimPi[k]->cd();
        h1DCAxyPrimPi[k]->Draw();
        h1DCAxyPrimPi[k]->SetMarkerStyle(20);

        cDCAxySecPiFromDecays[k] = new TCanvas(Form("cDCAxySecPiFromDecays%i", k), Form("cDCAxySecPiFromDecays%i", k), 800, 800);
        cDCAxySecPiFromDecays[k]->cd();
        h1DCAxySecPiFromDecays[k]->Draw();
        h1DCAxySecPiFromDecays[k]->SetMarkerStyle(20);

        cDCAxySecPiFromMaterial[k] = new TCanvas(Form("cDCAxySecPiFromMaterial%i", k), Form("cDCAxySecPiFromMaterial%i", k), 800, 800);
        cDCAxySecPiFromMaterial[k]->cd();
        h1DCAxySecPiFromMaterial[k]->Draw();
        h1DCAxySecPiFromMaterial[k]->SetMarkerStyle(20);*/

        for (int bin = 1; bin <= h1DCAxyPrimPi[k]->GetNbinsX(); bin++) {
            nPrimPi[k] += h1DCAxyPrimPi[k]->GetBinContent(bin);
            nSecPiFromDecays[k] += h1DCAxySecPiFromDecays[k]->GetBinContent(bin);
            nSecPiFromMaterial[k] += h1DCAxySecPiFromMaterial[k]->GetBinContent(bin);

            nPrimPiErr[k] += pow(h1DCAxyPrimPi[k]->GetBinError(bin), 2);
            nSecPiFromDecaysErr[k] += pow(h1DCAxySecPiFromDecays[k]->GetBinError(bin), 2);
            nSecPiFromMaterialErr[k] += pow(h1DCAxySecPiFromMaterial[k]->GetBinError(bin), 2);
        }
        nPrimPiErr[k] = sqrt(nPrimPiErr[k]);
        nSecPiFromDecaysErr[k] = sqrt(nSecPiFromDecaysErr[k]);
        nSecPiFromMaterialErr[k] = sqrt(nSecPiFromMaterialErr[k]);

        nTotPi[k] = nPrimPi[k] + nSecPiFromDecays[k] + nSecPiFromMaterial[k];
        nTotPiErr[k] = sqrt(pow(nPrimPiErr[k], 2) + pow(nSecPiFromDecaysErr[k], 2) + pow(nSecPiFromMaterialErr[k], 2));

        primFractionPi->SetBinContent(k+1, nPrimPi[k] / nTotPi[k]);
        primFractionPi->SetBinError(k+1, nPrimPi[k] / nTotPi[k] * sqrt(pow(nPrimPiErr[k] / nPrimPi[k], 2) + pow(nTotPiErr[k] / nTotPi[k], 2)));
    }

    //********************************************************************************************

    TFile* fileOutEff = new TFile(fileOutEffName.c_str(), "RECREATE");
    fileOutEff->cd();

    for (int i = 0; i < nbin_deltay; i++) {
        for (int j = 0; j < nbin_mult; j++) {
            h1effPhiK0S[i][j]->Write();
            h1effTrackPhiPi[i][j]->Write();
            h1effMatchPhiPi[i][j]->Write();
            h1effPhiPi[i][j]->Write();
            h1siglossPhiK0S[i][j]->Write();
            h1siglossPhiPi[i][j]->Write();
        }
        h1effPhiK0SMultInt[i]->Write();
        h1effTrackPhiPiMultInt[i]->Write();
        h1effMatchPhiPiMultInt[i]->Write();
        h1effPhiPiMultInt[i]->Write();
        h1siglossPhiK0SMultInt[i]->Write();
        h1siglossPhiPiMultInt[i]->Write();
    }

    primFractionPi->Write();

    fileOutEff->Close();

    return;
}

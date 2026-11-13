#include <Riostream.h>
#include <TH1.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TFile.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TMath.h>
#include "Plot.h"

void Plot_phi_kappa_paper() {
  
  //TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/analysis_phi_kappa.root");
  TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/analysis_phi_kappa2.root");
  														
  TCanvas* c1 = new TCanvas("c1", "K0s yields", 0, 0, 850, 850);
  TCanvas* c2 = new TCanvas("c2", "Xi yields", 0, 0, 850, 850);
  TCanvas* c3 = new TCanvas("c3", "Omega yields", 0, 0, 850, 850);
  TCanvas* c4 = new TCanvas("c4", "Lambda yields", 0, 0, 850, 850);
  TCanvas* c5 = new TCanvas("c5", "Hadron yields", 0, 0, 850, 850);
  TCanvas* c6 = new TCanvas("c6", "Pion yields", 0, 0, 850, 850);  
  TCanvas* c7 = new TCanvas("c7", "Phi yields", 0, 0, 850, 850);
  TCanvas* c8 = new TCanvas("c8", "ratioK0s yields", 0, 0, 850, 850);
  //TCanvas* c9 = new TCanvas("c9", "ratioXi yields", 0, 0, 850, 850);
  //TCanvas* c10 = new TCanvas("c10", "ratioOmega yields", 0, 0, 850, 850);
  //TCanvas* c11 = new TCanvas("c11", "ratioLambda yields", 0, 0, 850, 850);

  const int nMultClasses = 12;
  const int nRapClasses = 5;
    												               
  double x_MB[nMultClasses], errx_MB[nMultClasses];
  double x_phi[nMultClasses], errx_phi[nMultClasses];
  
  double had_new[nRapClasses][nMultClasses], errhad_new[nRapClasses][nMultClasses];
  double K0s_new[nRapClasses][nMultClasses], errK0s_new[nRapClasses][nMultClasses];
  double Xi_new[nRapClasses][nMultClasses], errXi_new[nRapClasses][nMultClasses];
  double Omega_new[nRapClasses][nMultClasses], errOmega_new[nRapClasses][nMultClasses];
  double Lambda_new[nRapClasses][nMultClasses], errLambda_new[nRapClasses][nMultClasses];
  double Pion_new[nRapClasses][nMultClasses], errPion_new[nRapClasses][nMultClasses];
  double Phi_new[nRapClasses][nMultClasses], errPhi_new[nRapClasses][nMultClasses];

  double ratioK0s_new[nRapClasses][nMultClasses], errratioK0s_new[nRapClasses][nMultClasses];
  double ratioXi_new[nRapClasses][nMultClasses], errratioXi_new[nRapClasses][nMultClasses];
  double ratioOmega_new[nRapClasses][nMultClasses], errratioOmega_new[nRapClasses][nMultClasses];
  double ratioLambda_new[nRapClasses][nMultClasses], errratioLambda_new[nRapClasses][nMultClasses];
							
  for (int j = 0; j < nMultClasses; j++) {
 
    x_MB[j] = GetMeanContentHisto("h_nMBCh", file2, j+1);
    errx_MB[j] = GetMeanContentErrorHisto("h_nMBCh", file2, j+1);
 
    x_phi[j] = GetMeanContentHisto("h_nphiCh", file2, j+1);
    errx_phi[j] = GetMeanContentErrorHisto("h_nphiCh", file2, j+1);

    for (int i = 0; i < nRapClasses; i++) {
           
      had_new[i][j] = GetMeanContentPlusHisto("h_kappa_hadron", file2, i, j+1);
      errhad_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_hadron", file2, i, j+1);	    
      K0s_new[i][j] = GetMeanContentPlusHisto("h_kappa_K0s", file2, i, j+1);
      errK0s_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_K0s", file2, i, j+1);
      Xi_new[i][j] = GetMeanContentPlusHisto("h_kappa_Xi", file2, i, j+1);
      errXi_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_Xi", file2, i, j+1);
      Omega_new[i][j] = GetMeanContentPlusHisto("h_kappa_Omega", file2, i, j+1);
      errOmega_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_Omega", file2, i, j+1);
      Lambda_new[i][j] = GetMeanContentPlusHisto("h_kappa_Lambda", file2, i, j+1);
      errLambda_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_Lambda", file2, i, j+1);
      Pion_new[i][j] = GetMeanContentPlusHisto("h_kappa_Pion", file2, i, j+1);
      errPion_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_Pion", file2, i, j+1);
      Phi_new[i][j] = GetMeanContentPlusHisto("h_kappa_Phi", file2, i, j+1);
      errPhi_new[i][j] = GetMeanContentErrorPlusHisto("h_kappa_Phi", file2, i, j+1);

      ratioK0s_new[i][j] = GetRatioPlusHisto("h_kappa_K0s", "h_kappa_Pion", file2, i, j+1);
      errratioK0s_new[i][j] = GetRatioErrorPlusHisto("h_kappa_K0s", "h_kappa_Pion", file2, i, j+1);
      ratioXi_new[i][j] = GetRatioPlusHisto("h_kappa_Xi", "h_kappa_Pion", file2, i, j+1);
      errratioXi_new[i][j] = GetRatioErrorPlusHisto("h_kappa_Xi", "h_kappa_Pion", file2, i, j+1);
      ratioOmega_new[i][j] = GetRatioPlusHisto("h_kappa_Omega", "h_kappa_Pion", file2, i, j+1);
      errratioOmega_new[i][j] = GetRatioErrorPlusHisto("h_kappa_Omega", "h_kappa_Pion", file2, i, j+1);
      ratioLambda_new[i][j] = GetRatioPlusHisto("h_kappa_Lambda", "h_kappa_Pion", file2, i, j+1);
      errratioLambda_new[i][j] = GetRatioErrorPlusHisto("h_kappa_Lambda", "h_kappa_Pion", file2, i, j+1);
 																						
    }

  }	  

  file2->Close();

  TMultiGraph* mg1 = new TMultiGraph();
  TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();
  TMultiGraph* mg4 = new TMultiGraph();
  TMultiGraph* mg5 = new TMultiGraph();
  TMultiGraph* mg6 = new TMultiGraph();
  TMultiGraph* mg7 = new TMultiGraph();
  TMultiGraph* mg8 = new TMultiGraph();
  TMultiGraph* mg9 = new TMultiGraph();
  TMultiGraph* mg10 = new TMultiGraph();
  TMultiGraph* mg11 = new TMultiGraph();
 
  TGraphErrors* K0splot_new[nRapClasses];
  TGraphErrors* Xiplot_new[nRapClasses];
  TGraphErrors* Omegaplot_new[nRapClasses];
  TGraphErrors* Lambdaplot_new[nRapClasses];
  TGraphErrors* Pionplot_new[nRapClasses];
  TGraphErrors* Phiplot_new[nRapClasses];
  TGraphErrors* hadronplot_new[nRapClasses];

  TGraphErrors* ratioK0splot_new[nRapClasses];
  TGraphErrors* ratioXiplot_new[nRapClasses];
  TGraphErrors* ratioOmegaplot_new[nRapClasses];
  TGraphErrors* ratioLambdaplot_new[nRapClasses];

  for (int i = 0; i < nRapClasses; i++) {
 
    if (i==0) hadronplot_new[i] = new TGraphErrors(nMultClasses, x_MB, had_new[i], errx_MB, errhad_new[i]);
    else hadronplot_new[i] = new TGraphErrors(nMultClasses, x_phi, had_new[i], errx_phi, errhad_new[i]);
    if (i==0) PlotFeaturesPlus(hadronplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg5);
    else if (i==1) PlotFeaturesPlus(hadronplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg5);
    else if (i==2) PlotFeaturesPlus(hadronplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg5);
    else if (i==3) PlotFeaturesPlus(hadronplot_new[i],33,kBlue+2,2,1,kBlue+2,2,3002,kBlue+2,0.40,mg5);
    else if (i==4) PlotFeaturesPlus(hadronplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg5);

    if (i==0) K0splot_new[i] = new TGraphErrors(nMultClasses, x_MB, K0s_new[i], errx_MB, errK0s_new[i]);
    else K0splot_new[i] = new TGraphErrors(nMultClasses, x_phi, K0s_new[i], errx_phi, errK0s_new[i]);
    if (i==0) PlotFeaturesPlus(K0splot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg1);
    else if (i==3) PlotFeaturesPlus(K0splot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg1);
    else if (i==4) PlotFeaturesPlus(K0splot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg1);
 
    if (i==0) Xiplot_new[i] = new TGraphErrors(nMultClasses, x_MB, Xi_new[i], errx_MB, errXi_new[i]);
    else Xiplot_new[i] = new TGraphErrors(nMultClasses, x_phi, Xi_new[i], errx_phi, errXi_new[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg2);
    else if (i==3) PlotFeaturesPlus(Xiplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg2);
    else if (i==4) PlotFeaturesPlus(Xiplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg2);
  
    if (i==0) Omegaplot_new[i] = new TGraphErrors(nMultClasses, x_MB, Omega_new[i], errx_MB, errOmega_new[i]);
    else Omegaplot_new[i] = new TGraphErrors(nMultClasses, x_phi, Omega_new[i], errx_phi, errOmega_new[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg3);
    else if (i==3) PlotFeaturesPlus(Omegaplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg3);
    else if (i==4) PlotFeaturesPlus(Omegaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg3);
 
    if (i==0) Lambdaplot_new[i] = new TGraphErrors(nMultClasses, x_MB, Lambda_new[i], errx_MB, errLambda_new[i]);
    else Lambdaplot_new[i] = new TGraphErrors(nMultClasses, x_phi, Lambda_new[i], errx_phi, errLambda_new[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg4);
    else if (i==3) PlotFeaturesPlus(Lambdaplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg4);
    else if (i==4) PlotFeaturesPlus(Lambdaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg4);

    if (i==0) Pionplot_new[i] = new TGraphErrors(nMultClasses, x_MB, Pion_new[i], errx_MB, errPion_new[i]);
    else Pionplot_new[i] = new TGraphErrors(nMultClasses, x_phi, Pion_new[i], errx_phi, errPion_new[i]);
    if (i==0) PlotFeaturesPlus(Pionplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg6);
    else if (i==1) PlotFeaturesPlus(Pionplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg6);
    else if (i==2) PlotFeaturesPlus(Pionplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg6);
    else if (i==3) PlotFeaturesPlus(Pionplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg6);
    else if (i==4) PlotFeaturesPlus(Pionplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg6);

    if (i==0) Phiplot_new[i] = new TGraphErrors(nMultClasses, x_MB, Phi_new[i], errx_MB, errPhi_new[i]);
    else Phiplot_new[i] = new TGraphErrors(nMultClasses, x_phi, Phi_new[i], errx_phi, errPhi_new[i]);
    if (i==0) PlotFeaturesPlus(Phiplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg7);
    else if (i==1) PlotFeaturesPlus(Phiplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg7);
    else if (i==2) PlotFeaturesPlus(Phiplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg7);
    else if (i==3) PlotFeaturesPlus(Phiplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg7);
    else if (i==4) PlotFeaturesPlus(Phiplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg7);
												
    if (i==0) ratioK0splot_new[i] = new TGraphErrors(nMultClasses, x_MB, ratioK0s_new[i], errx_MB, errratioK0s_new[i]);
    else ratioK0splot_new[i] = new TGraphErrors(nMultClasses, x_phi, ratioK0s_new[i], errx_phi, errratioK0s_new[i]);
    if (i==0) PlotFeaturesPlus(ratioK0splot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg8);
    else if (i==1) PlotFeaturesPlus(ratioK0splot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg8);
    else if (i==2) PlotFeaturesPlus(ratioK0splot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg8);
    else if (i==3) PlotFeaturesPlus(ratioK0splot_new[i],33,kBlue+2,2,1,kBlue+2,2,3002,kBlue+2,0.40,mg8);
    else if (i==4) PlotFeaturesPlus(ratioK0splot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg8);
												
    if (i==0) ratioXiplot_new[i] = new TGraphErrors(nMultClasses, x_MB, ratioXi_new[i], errx_MB, errratioXi_new[i]);
    else ratioXiplot_new[i] = new TGraphErrors(nMultClasses, x_phi, ratioXi_new[i], errx_phi, errratioXi_new[i]);
    if (i==0) PlotFeaturesPlus(ratioXiplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg9);
    else if (i==1) PlotFeaturesPlus(ratioXiplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg9);
    else if (i==2) PlotFeaturesPlus(ratioXiplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg9);
    else if (i==3) PlotFeaturesPlus(ratioXiplot_new[i],33,kBlue+2,2,1,kBlue+2,2,3002,kBlue+2,0.40,mg9);
    else if (i==4) PlotFeaturesPlus(ratioXiplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg9);

    if (i==0) ratioOmegaplot_new[i] = new TGraphErrors(nMultClasses, x_MB, ratioOmega_new[i], errx_MB, errratioOmega_new[i]);
    else ratioOmegaplot_new[i] = new TGraphErrors(nMultClasses, x_phi, ratioOmega_new[i], errx_phi, errratioOmega_new[i]);
    if (i==0) PlotFeaturesPlus(ratioOmegaplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg10);
    else if (i==1) PlotFeaturesPlus(ratioOmegaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg10);
    else if (i==2) PlotFeaturesPlus(ratioOmegaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg10);
    else if (i==3) PlotFeaturesPlus(ratioOmegaplot_new[i],33,kBlue+2,2,1,kBlue+2,2,3002,kBlue+2,0.40,mg10);
    else if (i==4) PlotFeaturesPlus(ratioOmegaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg10);

    if (i==0) ratioLambdaplot_new[i] = new TGraphErrors(nMultClasses, x_MB, ratioLambda_new[i], errx_MB, errratioLambda_new[i]);
    else ratioLambdaplot_new[i] = new TGraphErrors(nMultClasses, x_phi, ratioLambda_new[i], errx_phi, errratioLambda_new[i]);
    if (i==0) PlotFeaturesPlus(ratioLambdaplot_new[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg11);
    else if (i==1) PlotFeaturesPlus(ratioLambdaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg11);
    else if (i==2) PlotFeaturesPlus(ratioLambdaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg11);
    else if (i==3) PlotFeaturesPlus(ratioLambdaplot_new[i],33,kBlue+2,2,1,kBlue+2,2,3002,kBlue+2,0.40,mg11);
    else if (i==4) PlotFeaturesPlus(ratioLambdaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg11);
																									
  }

  c1->cd();
  c1->Divide(1,0);
  c1->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c1->GetPad(1)->SetLogx();
  c1->cd(1);
  //c1->SetLogx(true);

  mg1->GetXaxis()->SetMoreLogLabels();
  mg1->GetXaxis()->SetTitleSize(0.05);
  mg1->GetXaxis()->SetTitleOffset(1.3);
  mg1->GetXaxis()->SetLabelOffset(0.);
  mg1->GetXaxis()->SetLabelSize(0.045);
  mg1->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg1->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{K^{0}_{S}}");
  mg1->GetYaxis()->SetTitleOffset(1.5);
  mg1->GetYaxis()->SetTitleSize(0.05);
  mg1->GetYaxis()->SetLabelSize(0.045);
  //mg1->GetYaxis()->SetMaxDigits(3);
  mg1->Draw("A3L");
								
  /*TLegend* L1 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L1->SetTextSize(0.05);
  L1->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L1->SetLineWidth(0);
  L1->Draw("same");

  TLegend* L1_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L1_1->SetTextSize(0.05);
  L1_1->SetHeader("Pythia 8.3 with ropes");
  L1_1->SetLineWidth(0);
  L1_1->Draw("same");*/

  /*TLegend* L1_1 = new TLegend(0.68, 0.67, 0.8, 0.81);
  L1_1->AddEntry(K0splot_new[0], "|#Deltay|<0.1", "l");
  L1_1->AddEntry(K0splot_new[1], "|#Deltay|<0.5", "l");
  L1_1->AddEntry(K0splot_new[2], "|#Deltay|<1.0", "l");
  L1_1->AddEntry(K0splot_new[3], "Inclusive", "l");
  L1_1->SetTextSize(0.025);
  L1_1->SetLineWidth(0);
  L1_1->Draw("same");*/

  c2->Divide(1,0);
  c2->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c2->GetPad(1)->SetLogx();
  c2->cd(1);

  mg2->GetXaxis()->SetMoreLogLabels();
  mg2->GetXaxis()->SetTitleSize(0.05);
  mg2->GetXaxis()->SetTitleOffset(1.3);
  mg2->GetXaxis()->SetLabelOffset(0.);
  mg2->GetXaxis()->SetLabelSize(0.045);
  mg2->GetXaxis()->SetRangeUser(1.9, 33);
  mg2->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg2->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{#Xi^{#minus}+#bar{#Xi}^{+}}");
  mg2->GetYaxis()->SetTitleOffset(1.5);
  mg2->GetYaxis()->SetLabelSize(0.045);
  mg2->GetYaxis()->SetTitleSize(0.05);
  mg2->GetYaxis()->SetRangeUser(0.9, 1.69);
  mg2->GetYaxis()->SetMaxDigits(4);
  mg2->Draw("A3L");

  TLegend* L2 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L2->SetTextSize(0.05);
  L2->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L2->SetLineWidth(0);
  L2->Draw("same");

  TLegend* L2_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L2_1->SetTextSize(0.05);
  L2_1->SetHeader("Pythia 8.3 with ropes");
  L2_1->SetLineWidth(0);
  L2_1->Draw("same");

  TLegend* L2_2 = new TLegend(0.6, 0.2, 0.75, 0.5);
  L2_2->SetHeader("#phi trigger");
  L2_2->AddEntry(Lambdaplot_new[4], "Inclusive", "l");
  L2_2->AddEntry(Lambdaplot_new[3], "|#Deltay|<1.0", "l");
  L2_2->AddEntry(Lambdaplot_new[2], "|#Deltay|<0.5", "l");
  L2_2->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.1", "l");
  L2_2->SetTextSize(0.055);
  L2_2->SetLineWidth(0);
  L2_2->Draw("same");

  TLegend* L2_3 = new TLegend(0.6, 0.5, 0.75, 0.56);
  L2_3->AddEntry(Lambdaplot_new[0], "Minimum bias", "l");
  L2_3->SetTextSize(0.055);
  L2_3->SetLineWidth(0);
  L2_3->Draw("same");
 											
  /*TLatex* L2_0 = new TLatex();
  L2_0->SetNDC();
  L2_0->SetTextSize(0.025);
  L2_0->DrawLatex(0.145, 0.82, "pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L2 = new TLegend(0.13, 0.73, 0.29, 0.81);
  L2->SetTextSize(0.025);
  L2->AddEntry(Xiplot_new[3], "PYTHIA8, CR+Ropes", "l");
  L2->AddEntry(Xiplot_epos[3], "EPOS4", "l");
  L2->SetLineWidth(0);
  L2->Draw("same");

  TLegend* L2_1 = new TLegend(0.4, 0.67, 0.52, 0.81);
  L2_1->AddEntry(Xiplot_new[0], "|#Deltay|<0.1", "l");
  L2_1->AddEntry(Xiplot_new[1], "|#Deltay|<0.5", "l");
  L2_1->AddEntry(Xiplot_new[2], "|#Deltay|<1.0", "l");
  L2_1->AddEntry(Xiplot_new[3], "Inclusive", "l");
  L2_1->SetTextSize(0.025);
  L2_1->SetLineWidth(0);
  L2_1->Draw("same");*/

  c3->Divide(1,0);
  c3->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c3->GetPad(1)->SetLogx();
  c3->cd(1);

  mg3->GetXaxis()->SetMoreLogLabels();
  mg3->GetXaxis()->SetTitleSize(0.05);
  mg3->GetXaxis()->SetTitleOffset(1.3);
  mg3->GetXaxis()->SetLabelOffset(0.);
  mg3->GetXaxis()->SetLabelSize(0.045);
  mg3->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg3->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{#Omega^{#minus}+#bar{#Omega}^{+}}");
  mg3->GetYaxis()->SetTitleOffset(1.5);								
  mg3->GetYaxis()->SetLabelSize(0.045);
  mg3->GetYaxis()->SetTitleSize(0.05);
  //mg3->GetYaxis()->SetMaxDigits(3);
  mg3->Draw("A3L");
 
  /*TLatex* L3_0 = new TLatex();
  L3_0->SetNDC();
  L3_0->SetTextSize(0.025);
  L3_0->DrawLatex(0.145, 0.82, "pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L3 = new TLegend(0.13, 0.73, 0.29, 0.81);
  L3->SetTextSize(0.025);
  L3->AddEntry(Omegaplot_new[3], "PYTHIA8, CR+Ropes", "l");
  L3->AddEntry(Omegaplot_epos[3], "EPOS4", "l");
  L3->SetLineWidth(0);
  L3->Draw("same");

  TLegend* L3_1 = new TLegend(0.4, 0.67, 0.52, 0.81);
  L3_1->AddEntry(Omegaplot_new[0], "|#Deltay|<0.1", "l");
  L3_1->AddEntry(Omegaplot_new[1], "|#Deltay|<0.5", "l");
  L3_1->AddEntry(Omegaplot_new[2], "|#Deltay|<1.0", "l");
  L3_1->AddEntry(Omegaplot_new[3], "Inclusive", "l");
  L3_1->SetTextSize(0.025);
  L3_1->SetLineWidth(0);
  L3_1->Draw("same");*/


  c4->Divide(1,0);
  c4->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c4->GetPad(1)->SetLogx();
  c4->cd(1);

  mg4->GetXaxis()->SetMoreLogLabels();
  mg4->GetXaxis()->SetTitleSize(0.05);
  mg4->GetXaxis()->SetTitleOffset(1.3);
  mg4->GetXaxis()->SetLabelOffset(0.);
  mg4->GetXaxis()->SetLabelSize(0.045);
  mg4->GetXaxis()->SetRangeUser(1.9, 33);
  mg4->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg4->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{#Lambda+#bar{#Lambda}}");
  mg4->GetYaxis()->SetTitleOffset(1.5);
  mg4->GetYaxis()->SetLabelSize(0.045);
  mg4->GetYaxis()->SetTitleSize(0.05);
  mg4->GetYaxis()->SetRangeUser(0.93, 1.46);
  //mg4->GetYaxis()->SetMaxDigits(3);
  mg4->Draw("A3L");

  TLegend* L4 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L4->SetTextSize(0.05);
  L4->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L4->SetLineWidth(0);
  L4->Draw("same");

  TLegend* L4_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L4_1->SetTextSize(0.05);
  L4_1->SetHeader("Pythia 8.3 with ropes");
  L4_1->SetLineWidth(0);
  L4_1->Draw("same");

  TLegend* L4_2 = new TLegend(0.6, 0.2, 0.75, 0.5);
  L4_2->SetHeader("#phi trigger");
  L4_2->AddEntry(Lambdaplot_new[4], "Inclusive", "l");
  L4_2->AddEntry(Lambdaplot_new[3], "|#Deltay|<1.0", "l");
  L4_2->AddEntry(Lambdaplot_new[2], "|#Deltay|<0.5", "l");
  L4_2->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.1", "l");
  L4_2->SetTextSize(0.055);
  L4_2->SetLineWidth(0);
  L4_2->Draw("same");

  TLegend* L4_3 = new TLegend(0.6, 0.5, 0.75, 0.56);
  L4_3->AddEntry(Lambdaplot_new[0], "Minimum bias", "l");
  L4_3->SetTextSize(0.055);
  L4_3->SetLineWidth(0);
  L4_3->Draw("same");

  /*TLegend* L4_1 = new TLegend(0.2, 0.55, 0.35, 0.85);
  L4_1->SetHeader("#phi trigger");
  L4_1->AddEntry(Lambdaplot_new[4], "Inclusive", "l");
  L4_1->AddEntry(Lambdaplot_new[3], "|#Deltay|<1.0", "l");
  L4_1->AddEntry(Lambdaplot_new[2], "|#Deltay|<0.5", "l");
  L4_1->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.1", "l");
  L4_1->SetTextSize(0.055);
  L4_1->SetLineWidth(0);
  L4_1->Draw("same");

  TLegend* L4_2 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L4_2->AddEntry(Lambdaplot_new[0], "Minimum bias", "l");
  L4_2->SetTextSize(0.055);
  L4_2->SetLineWidth(0);
  L4_2->Draw("same");*/

  c5->cd();
  c5->Divide(1,0);
  c5->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c5->GetPad(1)->SetLogx();
  c5->cd(1);
  //c5->SetLogx(true);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.05);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetLabelSize(0.045);
  mg5->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{h}");
  mg5->GetYaxis()->SetTitleOffset(1.5);
  mg5->GetYaxis()->SetTitleSize(0.05);
  mg5->GetYaxis()->SetLabelSize(0.045);
  //mg5->GetYaxis()->SetMaxDigits(3);
  mg5->Draw("A3L");

  TLegend* L5 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L5->SetTextSize(0.05);
  L5->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L5->SetLineWidth(0);
  L5->Draw("same");

  TLegend* L5_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L5_1->SetTextSize(0.05);
  L5_1->SetHeader("Pythia 8.3 with ropes");
  L5_1->SetLineWidth(0);
  L5_1->Draw("same");

  c6->cd();
  c6->Divide(1,0);
  c6->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c6->GetPad(1)->SetLogx();
  c6->cd(1);
  //c6->SetLogx(true);

  mg6->GetXaxis()->SetMoreLogLabels();
  mg6->GetXaxis()->SetTitleSize(0.05);
  mg6->GetXaxis()->SetTitleOffset(1.3);
  mg6->GetXaxis()->SetLabelOffset(0.);
  mg6->GetXaxis()->SetLabelSize(0.045);
  mg6->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg6->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{#pi^{+}+#pi^{#minus}}");
  mg6->GetYaxis()->SetTitleOffset(1.5);
  mg6->GetYaxis()->SetTitleSize(0.05);
  mg6->GetYaxis()->SetLabelSize(0.045);
  //mg6->GetYaxis()->SetMaxDigits(3);
  mg6->Draw("A3L");

  TLegend* L6 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L6->SetTextSize(0.05);
  L6->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L6->SetLineWidth(0);
  L6->Draw("same");

  TLegend* L6_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L6_1->SetTextSize(0.05);
  L6_1->SetHeader("Pythia 8.3 with ropes");
  L6_1->SetLineWidth(0);
  L6_1->Draw("same");

  c7->cd();
  c7->Divide(1,0);
  c7->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c7->GetPad(1)->SetLogx();
  c7->cd(1);
  //c7->SetLogx(true);

  mg7->GetXaxis()->SetMoreLogLabels();
  mg7->GetXaxis()->SetTitleSize(0.05);
  mg7->GetXaxis()->SetTitleOffset(1.3);
  mg7->GetXaxis()->SetLabelOffset(0.);
  mg7->GetXaxis()->SetLabelSize(0.045);
  mg7->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg7->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{K^{0}_{S}}");
  mg7->GetYaxis()->SetTitleOffset(1.5);
  mg7->GetYaxis()->SetTitleSize(0.05);
  mg7->GetYaxis()->SetLabelSize(0.045);
  //mg7->GetYaxis()->SetMaxDigits(3);
  mg7->Draw("A3L");

  TLegend* L7 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L7->SetTextSize(0.05);
  L7->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L7->SetLineWidth(0);
  L7->Draw("same");

  TLegend* L7_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L7_1->SetTextSize(0.05);
  L7_1->SetHeader("Pythia 8.3 with ropes");
  L7_1->SetLineWidth(0);
  L7_1->Draw("same");

  c8->cd();
  c8->Divide(1,0);
  c8->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c8->GetPad(1)->SetLogx();
  c8->cd(1);
  //c8->SetLogx(true);

  mg8->GetXaxis()->SetMoreLogLabels();
  mg8->GetXaxis()->SetTitleSize(0.05);
  mg8->GetXaxis()->SetTitleOffset(1.3);
  mg8->GetXaxis()->SetLabelOffset(0.);
  mg8->GetXaxis()->SetLabelSize(0.045);
  mg8->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg8->GetYaxis()->SetTitle("#LT#kappa_{eff}/#kappa#GT_{K^{0}_{S}}/#LT#kappa_{eff}/#kappa#GT_{#pi^{+}+#pi^{#minus}}");
  mg8->GetYaxis()->SetTitleOffset(1.5);
  mg8->GetYaxis()->SetTitleSize(0.05);
  mg8->GetYaxis()->SetLabelSize(0.045);
  //mg8->GetYaxis()->SetMaxDigits(3);
  mg8->Draw("A3L");

  TLegend* L8 = new TLegend(0.2, 0.85, 0.40, 0.91);
  L8->SetTextSize(0.05);
  L8->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L8->SetLineWidth(0);
  L8->Draw("same");

  TLegend* L8_1 = new TLegend(0.2, 0.79, 0.40, 0.85);
  L8_1->SetTextSize(0.05);
  L8_1->SetHeader("Pythia 8.3 with ropes");
  L8_1->SetLineWidth(0);
  L8_1->Draw("same");

  /*c5->cd();
  c5->SetLogx(true);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.03);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetLabelSize(0.03);
  mg5->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("(p+#bar{p})/(#pi^{+}+#pi^{#minus})");
  mg5->GetYaxis()->SetTitleOffset(1.3);
  mg5->GetYaxis()->SetLabelSize(0.025);
  //mg5->GetYaxis()->SetMaxDigits(3);
  mg5->Draw("A3L");

  TLatex* L5_0 = new TLatex();
  L5_0->SetNDC();
  L5_0->SetTextSize(0.025);
  L5_0->DrawLatex(0.145, 0.82, "pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L5 = new TLegend(0.13, 0.73, 0.29, 0.81);
  L5->SetTextSize(0.025);
  L5->AddEntry(Protonplot_new[3], "PYTHIA8, CR+Ropes", "l");
  L5->AddEntry(Protonplot_epos[3], "EPOS4", "l");
  L5->SetLineWidth(0);
  L5->Draw("same");

  TLegend* L5_1 = new TLegend(0.4, 0.67, 0.52, 0.81);
  L5_1->AddEntry(Protonplot_new[0], "|#Deltay|<0.1", "l");
  L5_1->AddEntry(Protonplot_new[1], "|#Deltay|<0.5", "l");
  L5_1->AddEntry(Protonplot_new[2], "|#Deltay|<1.0", "l");
  L5_1->AddEntry(Protonplot_new[3], "Inclusive", "l");
  L5_1->SetTextSize(0.025);
  L5_1->SetLineWidth(0);
  L5_1->Draw("same");*/

  /*c6->cd();
  c6->SetLogx(true);

  mg6->GetXaxis()->SetMoreLogLabels();
  mg6->GetXaxis()->SetTitleSize(0.03);
  mg6->GetXaxis()->SetTitleOffset(1.3);
  mg6->GetXaxis()->SetLabelOffset(0.);
  mg6->GetXaxis()->SetLabelSize(0.03);
  mg6->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg6->GetYaxis()->SetTitle("2#LTN(#phi)#GT/#LTN(#pi^{+}+#pi^{-})#GT");
  mg6->GetYaxis()->SetTitleOffset(1.2);
  mg6->GetYaxis()->SetLabelSize(0.025);
  //mg6->GetYaxis()->SetMaxDigits(3);
  mg6->Draw("AP");

  TLatex* L6_0 = new TLatex();
  L6_0->SetNDC();
  L6_0->SetTextSize(0.025);
  L6_0->DrawLatex(0.425, 0.82, "pp, #sqrt{s} = 13 TeV");

  TLegend* L6 = new TLegend(0.41, 0.75, 0.57, 0.81);
  L6->SetTextSize(0.025);
  L6->AddEntry(Phiplot_new[3], "PYTHIA8, CR+Ropes", "p");
  L6->AddEntry(Phiplot_epos[3], "EPOS4", "p");
  L6->SetLineWidth(0);
  L6->Draw("same");

  TLegend* L6_1 = new TLegend(0.68, 0.73, 0.88, 0.82);
  L6_1->AddEntry(Phiplot_new[0], "|#Deltay|<0.1", "l");
  L6_1->AddEntry(Phiplot_new[1], "|#Deltay|<0.5", "l");
  L6_1->AddEntry(Phiplot_new[2], "|#Deltay|<1.0", "l");
  L6_1->AddEntry(Phiplot_new[3], "|y|<2.5", "l");
  L6_1->SetTextSize(0.025);
  L6_1->SetLineWidth(0);
  L6_1->Draw("same");*/

  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/k0s_phi_kappa.root");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/xi_phi_kappa.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/omega_phi_kappa.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/lambda_phi_kappa.root");
  c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/hadron_phi_kappa.root");
  c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/pion_phi_kappa.root");
  c7->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_kappa_results/phi_phi_kappa.root");
													
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/k0s_phi_kappa.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/xi_phi_kappa.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/omega_phi_kappa.pdf");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/lambda_phi_kappa.pdf");
  c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/hadron_phi_kappa.pdf");
  c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/pion_phi_kappa.pdf");
  c7->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/phi_phi_kappa.pdf");
} 														 

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

void Plot_phi_Pion() {
  
  TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_pynew.root");
  TFile* file3 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_epos.root");
																
  TCanvas* c1 = new TCanvas("c1", "K0s yields", 0, 0, 850, 850);
  /*TCanvas* c2 = new TCanvas("c2", "Xi yields", 0, 0, 850, 850);
  TCanvas* c3 = new TCanvas("c3", "Omega yields", 0, 0, 850, 850);
  TCanvas* c4 = new TCanvas("c4", "Lambda yields", 0, 0, 850, 850);
  TCanvas* c5 = new TCanvas("c5", "Proton yields", 0, 0, 850, 850);*/

  const int nMultClasses = 12;
  const int nRapClasses = 4;

  /*double x_def[nMultClasses], errx_def[nMultClasses];

  double ratioK0s_def[nRapClasses][nMultClasses], errratioK0s_def[nRapClasses][nMultClasses];
  double ratioXi_def[nRapClasses][nMultClasses], errratioXi_def[nRapClasses][nMultClasses];
  double ratioOmega_def[nRapClasses][nMultClasses], errratioOmega_def[nRapClasses][nMultClasses];
  double ratioLambda_def[nRapClasses][nMultClasses], errratioLambda_def[nRapClasses][nMultClasses];
  double ratioProton_def[nRapClasses][nMultClasses], errratioProton_def[nRapClasses][nMultClasses];*/
    												               
  double x_new[nMultClasses], errx_new[nMultClasses];
  
  double K0s_new[nRapClasses][nMultClasses], errK0s_new[nRapClasses][nMultClasses];
  /*double ratioXi_new[nRapClasses][nMultClasses], errratioXi_new[nRapClasses][nMultClasses];
  double ratioOmega_new[nRapClasses][nMultClasses], errratioOmega_new[nRapClasses][nMultClasses];
  double ratioLambda_new[nRapClasses][nMultClasses], errratioLambda_new[nRapClasses][nMultClasses];
  double ratioProton_new[nRapClasses][nMultClasses], errratioProton_new[nRapClasses][nMultClasses];*/

  double x_epos[nMultClasses], errx_epos[nMultClasses];
  
  double K0s_epos[nRapClasses][nMultClasses], errK0s_epos[nRapClasses][nMultClasses];
  /*double ratioXi_epos[nRapClasses][nMultClasses], errratioXi_epos[nRapClasses][nMultClasses];
  double ratioOmega_epos[nRapClasses][nMultClasses], errratioOmega_epos[nRapClasses][nMultClasses];
  double ratioLambda_epos[nRapClasses][nMultClasses], errratioLambda_epos[nRapClasses][nMultClasses];
  double ratioProton_epos[nRapClasses][nMultClasses], errratioProton_epos[nRapClasses][nMultClasses];*/

  for (int j = 0; j < nMultClasses; j++) {
 
    x_new[j] = GetMeanContentHisto("h_nCh", file2, j+1);
    errx_new[j] = GetMeanContentErrorHisto("h_nCh", file2, j+1);

    x_epos[j] = GetMeanContentHisto("h_nCh", file3, j+1);
    errx_epos[j] = GetMeanContentErrorHisto("h_nCh", file3, j+1);
 
    for (int i = 0; i < nRapClasses; i++) {

      K0s_new[i][j] = GetMeanContentPlusHisto("h_nPion", file2, i+1, j+1);
      errK0s_new[i][j] = GetMeanContentErrorPlusHisto("h_nPion", file2, i+1, j+1);
      /*ratioXi_new[i][j] = GetRatioPlusHisto("h_nXi", "h_nPion", file2, i+1, j+1);
      errratioXi_new[i][j] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file2, i+1, j+1);
      ratioOmega_new[i][j] = GetRatioPlusHisto("h_nOmega", "h_nPion", file2, i+1, j+1);
      errratioOmega_new[i][j] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file2, i+1, j+1);
      ratioLambda_new[i][j] = GetRatioPlusHisto("h_nLambda", "h_nPion", file2, i+1, j+1);
      errratioLambda_new[i][j] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file2, i+1, j+1);
      ratioProton_new[i][j] = GetRatioPlusHisto("h_nProton", "h_nPion", file2, i+1, j+1);
      errratioProton_new[i][j] = GetRatioErrorPlusHisto("h_nProton", "h_nPion", file2, i+1, j+1);*/

      K0s_epos[i][j] = GetMeanContentPlusHisto("h_nPion", file3, i+1, j+1);
      errK0s_epos[i][j] = GetMeanContentErrorPlusHisto("h_nPion", file3, i+1, j+1);
      /*ratioXi_epos[i][j] = GetRatioPlusHisto("h_nXi", "h_nPion", file3, i+1, j+1);
      errratioXi_epos[i][j] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file3, i+1, j+1);
      ratioOmega_epos[i][j] = GetRatioPlusHisto("h_nOmega", "h_nPion", file3, i+1, j+1);
      errratioOmega_epos[i][j] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file3, i+1, j+1);
      ratioLambda_epos[i][j] = GetRatioPlusHisto("h_nLambda", "h_nPion", file3, i+1, j+1);
      errratioLambda_epos[i][j] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file3, i+1, j+1);
      ratioProton_epos[i][j] = GetRatioPlusHisto("h_nProton", "h_nPion", file3, i+1, j+1);
      errratioProton_epos[i][j] = GetRatioErrorPlusHisto("h_nProton", "h_nPion", file3, i+1, j+1);*/
												
    }

  }	  

  file2->Close();
  file3->Close();

  TMultiGraph* mg1 = new TMultiGraph();
  /*TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();
  TMultiGraph* mg4 = new TMultiGraph();
  TMultiGraph* mg5 = new TMultiGraph();*/
 					
  TGraphErrors* K0splot_new[nRapClasses];
  /*TGraphErrors* Xiplot_new[nRapClasses];
  TGraphErrors* Omegaplot_new[nRapClasses];
  TGraphErrors* Lambdaplot_new[nRapClasses];
  TGraphErrors* Protonplot_new[nRapClasses];
  TGraphErrors* Phiplot_new[nRapClasses];*/

  TGraphErrors* K0splot_epos[nRapClasses];
  /*TGraphErrors* Xiplot_epos[nRapClasses];
  TGraphErrors* Omegaplot_epos[nRapClasses];
  TGraphErrors* Lambdaplot_epos[nRapClasses];
  TGraphErrors* Protonplot_epos[nRapClasses];
  TGraphErrors* Phiplot_epos[nRapClasses];*/

  for (int i = 0; i < nRapClasses; i++) {

    K0splot_new[i] = new TGraphErrors(nMultClasses, x_new, K0s_new[i], errx_new, errK0s_new[i]);
    K0splot_new[i]->Scale(2.);
    if (i==0) PlotFeaturesPlus(K0splot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg1);
    else if (i==3) PlotFeaturesPlus(K0splot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg1);

    /*Xiplot_new[i] = new TGraphErrors(nMultClasses, x_new, ratioXi_new[i], errx_new, errratioXi_new[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg2);
    else if (i==3) PlotFeaturesPlus(Xiplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg2);
 
    Omegaplot_new[i] = new TGraphErrors(nMultClasses, x_new, ratioOmega_new[i], errx_new, errratioOmega_new[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg3);
    else if (i==3) PlotFeaturesPlus(Omegaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg3);

    Lambdaplot_new[i] = new TGraphErrors(nMultClasses, x_new, ratioLambda_new[i], errx_new, errratioLambda_new[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg4);
    else if (i==3) PlotFeaturesPlus(Lambdaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg4);

    Protonplot_new[i] = new TGraphErrors(nMultClasses, x_new, ratioProton_new[i], errx_new, errratioProton_new[i]);
    if (i==0) PlotFeaturesPlus(Protonplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg5);
    else if (i==1) PlotFeaturesPlus(Protonplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg5);
    else if (i==2) PlotFeaturesPlus(Protonplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg5);
    else if (i==3) PlotFeaturesPlus(Protonplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg5);*/

    K0splot_epos[i] = new TGraphErrors(nMultClasses, x_epos, K0s_epos[i], errx_epos, errK0s_epos[i]);
    K0splot_epos[i]->Scale(2.);
    if (i==0) PlotFeaturesPlus(K0splot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg1);
    else if (i==3) PlotFeaturesPlus(K0splot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg1);

    /*Xiplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, ratioXi_epos[i], errx_epos, errratioXi_epos[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg2);
    else if (i==3) PlotFeaturesPlus(Xiplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg2);

    Omegaplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, ratioOmega_epos[i], errx_epos, errratioOmega_epos[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg3);
    else if (i==3) PlotFeaturesPlus(Omegaplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg3);
    
    Lambdaplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, ratioLambda_epos[i], errx_epos, errratioLambda_epos[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg4);
    else if (i==3) PlotFeaturesPlus(Lambdaplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg4);

    Protonplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, ratioProton_epos[i], errx_epos, errratioProton_epos[i]);
    if (i==0) PlotFeaturesPlus(Protonplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg5);
    else if (i==1) PlotFeaturesPlus(Protonplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg5);
    else if (i==2) PlotFeaturesPlus(Protonplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg5);
    else if (i==3) PlotFeaturesPlus(Protonplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg5);*/
  
  }

  c1->cd();
  c1->Divide(1,0);
  c1->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c1->GetPad(1)->SetLogx();
  c1->cd(1);

  mg1->GetXaxis()->SetMoreLogLabels();
  mg1->GetXaxis()->SetTitleSize(0.05);
  mg1->GetXaxis()->SetTitleOffset(1.3);
  mg1->GetXaxis()->SetLabelOffset(0.);
  mg1->GetXaxis()->SetLabelSize(0.045);
  mg1->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg1->GetYaxis()->SetTitle("2K^{0}_{S}");
  mg1->GetYaxis()->SetTitleOffset(1.5);
  mg1->GetYaxis()->SetTitleSize(0.05);
  mg1->GetYaxis()->SetLabelSize(0.045);
  mg1->Draw("A3L");
								
  TLegend* L1 = new TLegend(0.43, 0.71, 0.62, 0.90);
  L1->SetTextSize(0.05);
  L1->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L1->AddEntry(K0splot_new[3], "Pythia 8.3 with ropes", "l");
  L1->AddEntry(K0splot_epos[3], "EPOS4", "l");
  L1->SetLineWidth(0);
  L1->Draw("same");

  /*TLegend* L1_1 = new TLegend(0.68, 0.67, 0.8, 0.81);
  L1_1->AddEntry(K0splot_new[0], "|#Deltay|<0.1", "l");
  L1_1->AddEntry(K0splot_new[1], "|#Deltay|<0.5", "l");
  L1_1->AddEntry(K0splot_new[2], "|#Deltay|<1.0", "l");
  L1_1->AddEntry(K0splot_new[3], "Inclusive", "l");
  L1_1->SetTextSize(0.025);
  L1_1->SetLineWidth(0);
  L1_1->Draw("same");*/

  /*c2->Divide(1,0);
  c2->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c2->GetPad(1)->SetLogx();
  c2->cd(1);

  mg2->GetXaxis()->SetMoreLogLabels();
  mg2->GetXaxis()->SetTitleSize(0.05);
  mg2->GetXaxis()->SetTitleOffset(1.3);
  mg2->GetXaxis()->SetLabelOffset(0.);
  mg2->GetXaxis()->SetLabelSize(0.045);
  mg2->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg2->GetYaxis()->SetTitle("(#Xi^{#minus}+#bar{#Xi}^{+})/(#pi^{+}+#pi^{#minus})");
  mg2->GetYaxis()->SetTitleOffset(1.5);
  mg2->GetYaxis()->SetLabelSize(0.045);
  mg2->GetYaxis()->SetTitleSize(0.05);
  mg2->GetYaxis()->SetMaxDigits(4);
  mg2->Draw("A3L");

  TLegend* L2_1 = new TLegend(0.2, 0.65, 0.35, 0.90);
  L2_1->AddEntry(Lambdaplot_new[3], "Inclusive", "l");
  L2_1->AddEntry(Lambdaplot_new[2], "|#Deltay|<1.0", "l");
  L2_1->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.5", "l");
  L2_1->AddEntry(Lambdaplot_new[0], "|#Deltay|<0.1", "l");
  L2_1->SetTextSize(0.055);
  L2_1->SetLineWidth(0);
  L2_1->Draw("same");*/

  /*TLegend* L2 = new TLegend(0.13, 0.73, 0.29, 0.81);
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

  /*c3->Divide(1,0);
  c3->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c3->GetPad(1)->SetLogx();
  c3->cd(1);

  mg3->GetXaxis()->SetMoreLogLabels();
  mg3->GetXaxis()->SetTitleSize(0.05);
  mg3->GetXaxis()->SetTitleOffset(1.3);
  mg3->GetXaxis()->SetLabelOffset(0.);
  mg3->GetXaxis()->SetLabelSize(0.045);
  mg3->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg3->GetYaxis()->SetTitle("(#Omega^{#minus}+#bar{#Omega}^{+})/(#pi^{+}+#pi^{#minus})");
  mg3->GetYaxis()->SetTitleOffset(1.5);								
  mg3->GetYaxis()->SetLabelSize(0.045);
  mg3->GetYaxis()->SetTitleSize(0.05);
  //mg3->GetYaxis()->SetMaxDigits(3);
  mg3->Draw("A3L");

  TLegend* L3_1 = new TLegend(0.2, 0.65, 0.35, 0.90);
  L3_1->AddEntry(Lambdaplot_new[3], "Inclusive", "l");
  L3_1->AddEntry(Lambdaplot_new[2], "|#Deltay|<1.0", "l");
  L3_1->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.5", "l");
  L3_1->AddEntry(Lambdaplot_new[0], "|#Deltay|<0.1", "l");
  L3_1->SetTextSize(0.055);
  L3_1->SetLineWidth(0);
  L3_1->Draw("same");*/

  /*TLegend* L3 = new TLegend(0.13, 0.73, 0.29, 0.81);
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

  /*c4->Divide(1,0);
  c4->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c4->GetPad(1)->SetLogx();
  c4->cd(1);

  mg4->GetXaxis()->SetMoreLogLabels();
  mg4->GetXaxis()->SetTitleSize(0.05);
  mg4->GetXaxis()->SetTitleOffset(1.3);
  mg4->GetXaxis()->SetLabelOffset(0.);
  mg4->GetXaxis()->SetLabelSize(0.045);
  mg4->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg4->GetYaxis()->SetTitle("(#Lambda+#bar{#Lambda})/(#pi^{+}+#pi^{#minus})");
  mg4->GetYaxis()->SetTitleOffset(1.5);
  mg4->GetYaxis()->SetLabelSize(0.045);
  mg4->GetYaxis()->SetTitleSize(0.05);
  mg4->Draw("A3L");

  TLegend* L4 = new TLegend(0.2, 0.71, 0.4, 0.90);
  L4->SetTextSize(0.05);
  L4->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L4->AddEntry(K0splot_new[3], "Pythia 8.3 with ropes", "l");
  L4->AddEntry(K0splot_epos[3], "EPOS4", "l");
  L4->SetLineWidth(0);
  L4->Draw("same");*/

  /*TLegend* L4_1 = new TLegend(0.15, 0.65, 0.35, 0.90);
  L4_1->AddEntry(Lambdaplot_new[3], "Inclusive", "l");
  L4_1->AddEntry(Lambdaplot_new[2], "|#Deltay|<1.0", "l");
  L4_1->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.5", "l");
  L4_1->AddEntry(Lambdaplot_new[0], "|#Deltay|<0.1", "l");
  L4_1->SetTextSize(0.055);
  L4_1->SetLineWidth(0);
  L4_1->Draw("same");*/

  /*c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/k0s.root");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/xi_phi.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/omega_phi.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/lambda_phi.root");*/
  																									
  /*c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/k0s_phi.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/xi_phi.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/omega_phi.pdf");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/lambda_phi.pdf");*/

} 														 

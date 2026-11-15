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

void Plot_Xcore2_paper() {
  
  //TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/analysis_Xcoredetails_epos.root");
  TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_evenmoreclasses_results2/analysis_Xcoredetails_epos.root"); 
  //TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_evenmoreclasses_results2/analysis_Xcore_had_epos.root");
																
  TCanvas* c1 = new TCanvas("c1", "X_K0s", 0, 0, 850, 850);
  TCanvas* c2 = new TCanvas("c2", "X_Xi", 0, 0, 850, 850);
  TCanvas* c3 = new TCanvas("c3", "X_Omega", 0, 0, 850, 850);
  TCanvas* c4 = new TCanvas("c4", "X_Lambda", 0, 0, 850, 850);
  //TCanvas* c5 = new TCanvas("c5", "X_Hadron", 0, 0, 850, 850);
  //TCanvas* c6 = new TCanvas("c6", "X_Phi", 0, 0, 850, 850); 
  //TCanvas* c7 = new TCanvas("c7", "X_Pion", 0, 0, 850, 850); 

  const int nMultClasses = 12;
  const int nTrigClasses = 10;

  double x_minbias[nMultClasses], errx_minbias[nMultClasses];
  double x_phi[nMultClasses], errx_phi[nMultClasses];

  double K0s[nTrigClasses][nMultClasses], errK0s[nTrigClasses][nMultClasses];
  double Xi[nTrigClasses][nMultClasses], errXi[nTrigClasses][nMultClasses];
  double Omega[nTrigClasses][nMultClasses], errOmega[nTrigClasses][nMultClasses];
  double Lambda[nTrigClasses][nMultClasses], errLambda[nTrigClasses][nMultClasses];
  double Hadron[nTrigClasses][nMultClasses], errHadron[nTrigClasses][nMultClasses];
  //double Phi[nTrigClasses][nMultClasses], errPhi[nTrigClasses][nMultClasses];
  //double Pion[nTrigClasses][nMultClasses], errPion[nTrigClasses][nMultClasses];

  double ratioK0s[nTrigClasses/2][nMultClasses], errratioK0s[nTrigClasses/2][nMultClasses];
  double ratioXi[nTrigClasses/2][nMultClasses], errratioXi[nTrigClasses/2][nMultClasses];
  double ratioOmega[nTrigClasses/2][nMultClasses], errratioOmega[nTrigClasses/2][nMultClasses];
  double ratioLambda[nTrigClasses/2][nMultClasses], errratioLambda[nTrigClasses/2][nMultClasses];
  double ratioHadron[nTrigClasses/2][nMultClasses], errratioHadron[nTrigClasses/2][nMultClasses];
  //double ratioPhi[nTrigClasses/2][nMultClasses], errratioPhi[nTrigClasses/2][nMultClasses];
  //double ratioPion[nTrigClasses/2][nMultClasses], errratioPion[nTrigClasses/2][nMultClasses];
									
  for (int j = 0; j < nMultClasses; j++) {

    x_minbias[j] = GetMeanContentHisto("h_nChminb", file1, j+1);
    errx_minbias[j] = GetMeanContentErrorHisto("h_nChminb", file1, j+1);
    x_phi[j] = GetMeanContentHisto("h_nChphi", file1, j+1);
    errx_phi[j] = GetMeanContentErrorHisto("h_nChphi", file1, j+1);

    for (int i = 0; i < nTrigClasses; i++) {
          
      K0s[i][j] = GetMeanContentPlusHisto("h_nK0s", file1, i+1, j+1);
      errK0s[i][j] = GetMeanContentErrorPlusHisto("h_nK0s", file1, i+1, j+1);
      Xi[i][j] = GetMeanContentPlusHisto("h_nXi", file1, i+1, j+1);
      errXi[i][j] = GetMeanContentErrorPlusHisto("h_nXi", file1, i+1, j+1);
      Omega[i][j] = GetMeanContentPlusHisto("h_nOmega", file1, i+1, j+1);
      errOmega[i][j] = GetMeanContentErrorPlusHisto("h_nOmega", file1, i+1, j+1);
      Lambda[i][j] = GetMeanContentPlusHisto("h_nLambda", file1, i+1, j+1);
      errLambda[i][j] = GetMeanContentErrorPlusHisto("h_nLambda", file1, i+1, j+1);
      //Hadron[i][j] = GetMeanContentPlusHisto("h_nhadron", file1, i+1, j+1);
      //errHadron[i][j] = GetMeanContentErrorPlusHisto("h_nhadron", file1, i+1, j+1);
      /*Phi[i][j] = GetMeanContentPlusHisto("h_nPhi", file1, i+1, j+1);
      errPhi[i][j] = GetMeanContentErrorPlusHisto("h_nPhi", file1, i+1, j+1);
      Pion[i][j] = GetMeanContentPlusHisto("h_nPion", file1, i+1, j+1);
      errPion[i][j] = GetMeanContentErrorPlusHisto("h_nPion", file1, i+1, j+1);*/
																					
    }

  }	  

  file1->Close();

  for (int i = 0; i < nTrigClasses/2; i++) {
    for (int j = 0; j < nMultClasses; j++) {
                      
      ratioK0s[i][j] = K0s[2*i][j]/K0s[2*i+1][j];
      errratioK0s[i][j] = GetDoubleRatioError(K0s[2*i][j], errK0s[2*i][j], K0s[2*i+1][j], errK0s[2*i+1][j]);
      ratioXi[i][j] = Xi[2*i][j]/Xi[2*i+1][j];
      errratioXi[i][j] = GetDoubleRatioError(Xi[2*i][j], errXi[2*i][j], Xi[2*i+1][j], errXi[2*i+1][j]);
      ratioOmega[i][j] = Omega[2*i][j]/Omega[2*i+1][j];
      errratioOmega[i][j] = GetDoubleRatioError(Omega[2*i][j], errOmega[2*i][j], Omega[2*i+1][j], errOmega[2*i+1][j]);
      ratioLambda[i][j] = Lambda[2*i][j]/Lambda[2*i+1][j];
      errratioLambda[i][j]= GetDoubleRatioError(Lambda[2*i][j], errLambda[2*i][j], Lambda[2*i+1][j], errLambda[2*i+1][j]);
      //ratioHadron[i][j] = Hadron[2*i][j]/Hadron[2*i+1][j];
      //errratioHadron[i][j] = GetDoubleRatioError(Hadron[2*i][j], errHadron[2*i][j], Hadron[2*i+1][j], errHadron[2*i+1][j]);
      /*ratioPhi[i][j] = Phi[2*i][j]/Phi[2*i+1][j];
      errratioPhi[i][j] = GetDoubleRatioError(Phi[2*i][j], errPhi[2*i][j], Phi[2*i+1][j], errPhi[2*i+1][j]);
      ratioPion[i][j] = Pion[2*i][j]/Pion[2*i+1][j];
      errratioPion[i][j] = GetDoubleRatioError(Pion[2*i][j], errPion[2*i][j], Pion[2*i+1][j], errPion[2*i+1][j]);*/
														
    }
  }						

  TMultiGraph* mg1 = new TMultiGraph();
  TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();
  TMultiGraph* mg4 = new TMultiGraph();
  TMultiGraph* mg5 = new TMultiGraph();
  //TMultiGraph* mg6 = new TMultiGraph();
  //TMultiGraph* mg7 = new TMultiGraph();
 
  TGraphErrors* ratioK0splot[nTrigClasses/2];
  TGraphErrors* ratioXiplot[nTrigClasses/2];
  TGraphErrors* ratioOmegaplot[nTrigClasses/2];
  TGraphErrors* ratioLambdaplot[nTrigClasses/2];
  //TGraphErrors* ratioHadronplot[nTrigClasses/2];
  //TGraphErrors* ratioPhiplot[nTrigClasses/2];
  //TGraphErrors* ratioPionplot[nTrigClasses/2];

  for (int i = 0; i < nTrigClasses/2; i++) {

    if (i==0) ratioK0splot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioK0s[i], errx_minbias, errratioK0s[i]);
    else ratioK0splot[i] = new TGraphErrors(nMultClasses, x_phi, ratioK0s[i], errx_phi, errratioK0s[i]);
    if (i==0) PlotFeaturesPlus(ratioK0splot[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(ratioK0splot[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(ratioK0splot[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg1);
    else if (i==3) PlotFeaturesPlus(ratioK0splot[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg1);
    else if (i==4) PlotFeaturesPlus(ratioK0splot[i],33, kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg1);

    if (i==0) ratioXiplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioXi[i], errx_minbias, errratioXi[i]);
    else ratioXiplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioXi[i], errx_phi, errratioXi[i]);
    if (i==0) PlotFeaturesPlus(ratioXiplot[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(ratioXiplot[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(ratioXiplot[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg2);
    else if (i==3) PlotFeaturesPlus(ratioXiplot[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg2);
    else if (i==4) PlotFeaturesPlus(ratioXiplot[i],33, kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg2);
								
    if (i==0) ratioOmegaplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioOmega[i], errx_minbias, errratioOmega[i]);
    else ratioOmegaplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioOmega[i], errx_phi, errratioOmega[i]);
    if (i==0) PlotFeaturesPlus(ratioOmegaplot[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(ratioOmegaplot[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(ratioOmegaplot[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg3);
    else if (i==3) PlotFeaturesPlus(ratioOmegaplot[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg3);
    else if (i==4) PlotFeaturesPlus(ratioOmegaplot[i],33, kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg3);

    if (i==0) ratioLambdaplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioLambda[i], errx_minbias, errratioLambda[i]);
    else ratioLambdaplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioLambda[i], errx_phi, errratioLambda[i]);
    if (i==0) PlotFeaturesPlus(ratioLambdaplot[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(ratioLambdaplot[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(ratioLambdaplot[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg4);
    else if (i==3) PlotFeaturesPlus(ratioLambdaplot[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg4);
    else if (i==4) PlotFeaturesPlus(ratioLambdaplot[i],33, kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg4);
    								
    /*if (i==0) ratioHadronplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioHadron[i], errx_minbias, errratioHadron[i]);
    else ratioHadronplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioHadron[i], errx_phi, errratioHadron[i]);
    if (i==0) PlotFeaturesPlus(ratioHadronplot[i],33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg5);
    else if (i==1) PlotFeaturesPlus(ratioHadronplot[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg5);
    else if (i==2) PlotFeaturesPlus(ratioHadronplot[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg5);
    else if (i==3) PlotFeaturesPlus(ratioHadronplot[i],33,kBlue+2,2,1,kBlue+2,2,3002,kBlue+2,0.40,mg5);
    else if (i==4) PlotFeaturesPlus(ratioHadronplot[i],33, kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg5);*/
														
    /*if (i==0) ratioPhiplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioPhi[i], errx_minbias, errratioPhi[i]);
    else ratioPhiplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioPhi[i], errx_phi, errratioPhi[i]);
    if (i==0) PlotFeatures(ratioPhiplot[i], kViolet, 34, 2, mg6);
    else if (i==1) PlotFeatures(ratioPhiplot[i], kBlue, 34, 2, mg6);
    else if (i==2) PlotFeatures(ratioPhiplot[i], kSpring-1, 34, 2, mg6);
    else if (i==3) PlotFeatures(ratioPhiplot[i], kRed, 34, 2, mg6);
    else if (i==4) PlotFeatures(ratioPhiplot[i], kBlack, 34, 2, mg6);

    if (i==0) ratioPionplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioPion[i], errx_minbias, errratioPion[i]);
    else ratioPionplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioPion[i], errx_phi, errratioPion[i]);
    if (i==0) PlotFeatures(ratioPionplot[i], kViolet, 34, 2, mg7);
    else if (i==1) PlotFeatures(ratioPionplot[i], kBlue, 34, 2, mg7);
    else if (i==2) PlotFeatures(ratioPionplot[i], kSpring-1, 34, 2, mg7);
    else if (i==3) PlotFeatures(ratioPionplot[i], kRed, 34, 2, mg7);
    else if (i==4) PlotFeatures(ratioPionplot[i], kBlack, 34, 2, mg7);*/

  }

  //TGraphErrors* tempplot = new TGraphErrors(nMultClasses, x_minbias, ratioK0s[0], errx_minbias, errratioK0s[0]);
  //tempplot->SetLineStyle(1);
  //tempplot->SetLineColor(kBlack);

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
  mg1->GetYaxis()->SetTitle("K^{0}_{S core}/K^{0}_{S event}");
  mg1->GetYaxis()->SetTitleOffset(1.5);
  mg1->GetYaxis()->SetLabelSize(0.045);
  mg1->GetYaxis()->SetTitleSize(0.05);
  //mg1->GetYaxis()->SetMaxDigits(3);
  mg1->Draw("A3L");

  /*TLatex* L1_0 = new TLatex();
  L1_0->SetNDC();
  L1_0->SetTextSize(0.03);
  L1_0->DrawLatex(0.15, 0.79, "#textsc{Epos4}, pp, #sqrt{s} = 13 TeV, |y|<2.5");*/

  /*TLegend* L1 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L1->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L1->SetTextSize(0.05);
  L1->Draw("same");
  L1->SetLineWidth(0);

  TLegend* L1_1 = new TLegend(0.2, 0.79, 0.35, 0.85);
  L1_1->SetHeader("EPOS4");
  L1_1->SetTextSize(0.05);
  L1_1->Draw("same");
  L1_1->SetLineWidth(0);*/
									
  /*TLegend* L1_1 = new TLegend(0.13, 0.80, 0.29, 0.80);
  L1_1->AddEntry(ratioK0splot[0], "Minimum bias", "l");
  L1_1->AddEntry(tempplot, "#phi trigger", "l");
  L1_1->AddEntry(ratioK0splot[1], "|#Deltay|<0.1", "l");
  L1_1->AddEntry(ratioK0splot[2], "|#Deltay|<0.5", "l");
  L1_1->AddEntry(ratioK0splot[3], "|#Deltay|<1.0", "l");
  L1_1->AddEntry(ratioK0splot[4], "Inclusive", "l");
  L1_1->SetTextSize(0.035);
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
  mg2->GetXaxis()->SetRangeUser(1.9, 32);
  mg2->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg2->GetYaxis()->SetTitle("(#Xi^{#minus}+#bar{#Xi}^{+})_{core}/(#Xi^{#minus}+#bar{#Xi}^{+})_{event}");
  mg2->GetYaxis()->SetTitleOffset(1.5);
  mg2->GetYaxis()->SetLabelSize(0.045);
  mg2->GetYaxis()->SetTitleSize(0.05);
  mg2->GetYaxis()->SetMaxDigits(4);
  mg2->Draw("A3L");

  TLegend* L2 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L2->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L2->SetTextSize(0.05);
  L2->Draw("same");
  L2->SetLineWidth(0);

  TLegend* L2_1 = new TLegend(0.2, 0.79, 0.35, 0.85);
  L2_1->SetHeader("EPOS4");
  L2_1->SetTextSize(0.05);
  L2_1->Draw("same");
  L2_1->SetLineWidth(0);

  /*TLegend* L2_1 = new TLegend(0.2, 0.55, 0.35, 0.85);
  L2_1->SetHeader("#phi trigger");
  L2_1->AddEntry(ratioLambdaplot[4], "Inclusive", "l");
  L2_1->AddEntry(ratioLambdaplot[3], "|#Deltay|<1.0", "l");
  L2_1->AddEntry(ratioLambdaplot[2], "|#Deltay|<0.5", "l");
  L2_1->AddEntry(ratioLambdaplot[1], "|#Deltay|<0.1", "l");
  L2_1->SetTextSize(0.055);
  L2_1->SetLineWidth(0);
  L2_1->Draw("same");

  TLegend* L2_2 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L2_2->AddEntry(ratioLambdaplot[0], "Minimum bias", "l");
  L2_2->SetTextSize(0.055);
  L2_2->SetLineWidth(0);
  L2_2->Draw("same");*/

  /*TLatex* L2_0 = new TLatex();
  L2_0->SetNDC();
  L2_0->SetTextSize(0.025);
  L2_0->DrawLatex(0.145, 0.82, "EPOS4, pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L2_1 = new TLegend(0.13, 0.63, 0.29, 0.81);
  L2_1->AddEntry(ratioXiplot[0], "Minimum bias", "l");
  L2_1->AddEntry(tempplot, "#phi trigger", "l");
  L2_1->AddEntry(ratioXiplot[1], "|#Deltay|<0.1", "l");
  L2_1->AddEntry(ratioXiplot[2], "|#Deltay|<0.5", "l");
  L2_1->AddEntry(ratioXiplot[3], "|#Deltay|<1.0", "l");
  L2_1->AddEntry(ratioXiplot[4], "Inclusive", "l");
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
  mg3->GetYaxis()->SetTitle("(#Omega^{#minus}+#bar{#Omega}^{+})_{core}/(#Omega^{#minus}+#bar{#Omega}^{+})_{event}");
  mg3->GetYaxis()->SetTitleOffset(1.5);
  mg3->GetYaxis()->SetLabelSize(0.045);
  mg3->GetYaxis()->SetTitleSize(0.05);
  //mg3->GetYaxis()->SetMaxDigits(3);
  mg3->Draw("A3L");

  /*TLatex* L3_0 = new TLatex();
  L3_0->SetNDC();
  L3_0->SetTextSize(0.025);
  L3_0->DrawLatex(0.145, 0.82, "EPOS4, pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L3_1 = new TLegend(0.13, 0.63, 0.29, 0.81);
  L3_1->AddEntry(ratioOmegaplot[0], "Minimum bias", "l");
  L3_1->AddEntry(tempplot, "#phi trigger", "l");
  L3_1->AddEntry(ratioOmegaplot[1], "|#Deltay|<0.1", "l");
  L3_1->AddEntry(ratioOmegaplot[2], "|#Deltay|<0.5", "l");
  L3_1->AddEntry(ratioOmegaplot[3], "|#Deltay|<1.0", "l");
  L3_1->AddEntry(ratioOmegaplot[4], "Inclusive", "l");
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
  mg4->GetXaxis()->SetRangeUser(1.9, 32);
  mg4->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg4->GetYaxis()->SetTitle("(#Lambda+#bar{#Lambda})_{core}/(#Lambda+#bar{#Lambda})_{event}");
  mg4->GetYaxis()->SetTitleOffset(1.5);
  mg4->GetYaxis()->SetLabelSize(0.045);
  mg4->GetYaxis()->SetTitleSize(0.05);
  //mg4->GetYaxis()->SetMaxDigits(3);
  mg4->Draw("A3L");

  TLegend* L4 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L4->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L4->SetTextSize(0.05);
  L4->Draw("same");
  L4->SetLineWidth(0);

  TLegend* L4_1 = new TLegend(0.2, 0.79, 0.35, 0.85);
  L4_1->SetHeader("EPOS4");
  L4_1->SetTextSize(0.05);
  L4_1->Draw("same");
  L4_1->SetLineWidth(0);

  /*TLatex* L4_0 = new TLatex();
  L4_0->SetNDC();
  L4_0->SetTextSize(0.025);
  L4_0->DrawLatex(0.145, 0.82, "EPOS4, pp, #sqrt{s} = 13 TeV, |y|<2.5");*/
										
  /*TLegend* L4_1 = new TLegend(0.2, 0.55, 0.35, 0.85);
  //L4_1->AddEntry(ratioLambdaplot[0], "Minimum bias", "l");
  L4_1->SetHeader("#phi trigger");
  L4_1->AddEntry(ratioLambdaplot[4], "Inclusive", "l");
  L4_1->AddEntry(ratioLambdaplot[3], "|#Deltay|<1.0", "l");
  L4_1->AddEntry(ratioLambdaplot[2], "|#Deltay|<0.5", "l");
  L4_1->AddEntry(ratioLambdaplot[1], "|#Deltay|<0.1", "l");
  L4_1->SetTextSize(0.055);
  L4_1->SetLineWidth(0);
  L4_1->Draw("same");

  TLegend* L4_2 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L4_2->AddEntry(ratioLambdaplot[0], "Minimum bias", "l");
  L4_2->SetTextSize(0.055);
  L4_2->SetLineWidth(0);
  L4_2->Draw("same");*/

  /*c5->Divide(1,0);
  c5->GetPad(1)->SetMargin(0.16,0.01,0.15,0.06);
  c5->GetPad(1)->SetLogx();
  c5->cd(1);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.05);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetLabelSize(0.045);
  mg5->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("h_{core}/h_{event}");
  mg5->GetYaxis()->SetTitleOffset(1.5);
  mg5->GetYaxis()->SetLabelSize(0.045);
  mg5->GetYaxis()->SetTitleSize(0.05);
  //mg5->GetYaxis()->SetMaxDigits(3);
  mg5->Draw("A3L");

  TLegend* L5 = new TLegend(0.2, 0.85, 0.35, 0.91);
  L5->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<2.5");
  L5->SetTextSize(0.05);
  L5->Draw("same");
  L5->SetLineWidth(0);

  TLegend* L5_1 = new TLegend(0.2, 0.79, 0.35, 0.85);
  L5_1->SetHeader("EPOS4");
  L5_1->SetTextSize(0.05);
  L5_1->Draw("same");
  L5_1->SetLineWidth(0);*/

  /*c5->cd();
  c5->SetLogx(true);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.03);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("X_{p + #bar{p}}");
  mg5->GetYaxis()->SetTitleOffset(1.4);
  mg5->GetYaxis()->SetLabelSize(0.03);
  mg5->Draw("A3L");

  TLegend* L5 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L5->SetTextSize(0.02);
  L5->AddEntry(Protonplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L5->AddEntry(Protonplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L5->AddEntry(Protonplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L5->SetLineWidth(0);
  //L5->Draw("same");*/
  
  /*c6->cd();
  c6->SetLogx(true);

  mg6->GetXaxis()->SetMoreLogLabels();
  mg6->GetXaxis()->SetTitleSize(0.03);
  mg6->GetXaxis()->SetTitleOffset(1.3);
  mg6->GetXaxis()->SetLabelOffset(0.);
  mg6->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg6->GetYaxis()->SetTitle("X|_{#phi}");
  mg6->GetYaxis()->SetTitleOffset(1.4);
  mg6->GetYaxis()->SetLabelSize(0.03);
  mg6->Draw("AP");

  TLegend* L6 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L6->SetTextSize(0.02);
  L6->AddEntry(Phiplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L6->AddEntry(Phiplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L6->AddEntry(Phiplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L6->SetLineWidth(0);
  //L6->Draw("same");
  
  c7->cd();
  c7->SetLogx(true);

  mg7->GetXaxis()->SetMoreLogLabels();
  mg7->GetXaxis()->SetTitleSize(0.03);
  mg7->GetXaxis()->SetTitleOffset(1.3);
  mg7->GetXaxis()->SetLabelOffset(0.);
  mg7->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg7->GetYaxis()->SetTitle("X|_{#pi^{+} + #pi^{-}}");
  mg7->GetYaxis()->SetTitleOffset(1.4);
  mg7->GetYaxis()->SetLabelSize(0.03);
  mg7->Draw("AP");*/ 

  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/k0s_Xcore_epos.root");  
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/xi_Xcore_epos.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/omega_Xcore_epos.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/lambda_Xcore_epos.root");
  //c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/hadron_Xcore_epos.root");
  //c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/phi_Xcore_epos.root");
  //c7->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results2/pion_Xcore_epos.root");
 
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/k0s_Xcore_epos.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/xi_Xcore_epos.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/omega_Xcore_epos.pdf");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/lambda_Xcore_epos.pdf");
  //c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/hadron_Xcore_epos.pdf");
} 																								  																																						

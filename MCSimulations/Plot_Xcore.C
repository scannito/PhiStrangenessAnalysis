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

void Plot_Xcore() {
  
  TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/analysis_Xcoredetails_epos.root");
 																 
  TCanvas* c1 = new TCanvas("c1", "X_K0s", 0, 0, 850, 850);
  TCanvas* c2 = new TCanvas("c2", "X_Xi", 0, 0, 850, 850);
  TCanvas* c3 = new TCanvas("c3", "X_Omega", 0, 0, 850, 850);
  TCanvas* c4 = new TCanvas("c4", "X_Lambda", 0, 0, 850, 850);
  TCanvas* c5 = new TCanvas("c5", "X_Proton", 0, 0, 850, 850);
  TCanvas* c6 = new TCanvas("c6", "X_Phi", 0, 0, 850, 850); 
  TCanvas* c7 = new TCanvas("c7", "X_Pion", 0, 0, 850, 850); 

  TCanvas* c8 = new TCanvas("c8", "Phi_core", 0, 0, 850, 850);
  TCanvas* c9 = new TCanvas("c9", "Phi_softstr", 0, 0, 850, 850);
  TCanvas* c10 = new TCanvas("c10", "Phi_hardstr", 0, 0, 850, 850);
  TCanvas* c11 = new TCanvas("c11", "Phi_remnants", 0, 0, 850, 850);
  TCanvas* c12 = new TCanvas("c12", "Phi_corona", 0, 0, 850, 850);

  const int nMultClasses = 11;
  const int nTrigClasses = 30;

  double x_minbias[nMultClasses], errx_minbias[nMultClasses];
  double x_phi[nMultClasses], errx_phi[nMultClasses];

  double K0s[nTrigClasses][nMultClasses], errK0s[nTrigClasses][nMultClasses];
  double Xi[nTrigClasses][nMultClasses], errXi[nTrigClasses][nMultClasses];
  double Omega[nTrigClasses][nMultClasses], errOmega[nTrigClasses][nMultClasses];
  double Lambda[nTrigClasses][nMultClasses], errLambda[nTrigClasses][nMultClasses];
  double Proton[nTrigClasses][nMultClasses], errProton[nTrigClasses][nMultClasses];
  double Phi[nTrigClasses][nMultClasses], errPhi[nTrigClasses][nMultClasses];
  double Pion[nTrigClasses][nMultClasses], errPion[nTrigClasses][nMultClasses];

  double ratioK0s[nTrigClasses/6][nMultClasses], errratioK0s[nTrigClasses/6][nMultClasses];
  double ratioXi[nTrigClasses/6][nMultClasses], errratioXi[nTrigClasses/6][nMultClasses];
  double ratioOmega[nTrigClasses/6][nMultClasses], errratioOmega[nTrigClasses/6][nMultClasses];
  double ratioLambda[nTrigClasses/6][nMultClasses], errratioLambda[nTrigClasses/6][nMultClasses];
  double ratioProton[nTrigClasses/6][nMultClasses], errratioProton[nTrigClasses/6][nMultClasses];
  double ratioPhi[nTrigClasses/6][nMultClasses], errratioPhi[nTrigClasses/6][nMultClasses];
  double ratioPion[nTrigClasses/6][nMultClasses], errratioPion[nTrigClasses/6][nMultClasses];
									
  for (int j = 0; j < nMultClasses; j++) {

    x_minbias[j] = GetMeanContent("h_dndetaminbias", file1, j+1);
    errx_minbias[j] = GetMeanContentError("h_dndetaminbias", file1, j+1);
    x_phi[j] = GetMeanContent("h_dndetaphi", file1, j+1);
    errx_phi[j] = GetMeanContentError("h_dndetaphi", file1, j+1);

    for (int i = 0; i < nTrigClasses; i++) {
          
      K0s[i][j] = GetMeanContentPlus("h_dndyK0s", file1, i+1, j+1);
      errK0s[i][j] = GetMeanContentErrorPlus("h_dndyK0s", file1, i+1, j+1);
      Xi[i][j] = GetMeanContentPlus("h_dndyXi", file1, i+1, j+1);
      errXi[i][j] = GetMeanContentErrorPlus("h_dndyXi", file1, i+1, j+1);
      Omega[i][j] = GetMeanContentPlus("h_dndyOmega", file1, i+1, j+1);
      errOmega[i][j] = GetMeanContentErrorPlus("h_dndyOmega", file1, i+1, j+1);
      Lambda[i][j] = GetMeanContentPlus("h_dndyLambda", file1, i+1, j+1);
      errLambda[i][j] = GetMeanContentErrorPlus("h_dndyLambda", file1, i+1, j+1);
      Proton[i][j] = GetMeanContentPlus("h_dndyProton", file1, i+1, j+1);
      errProton[i][j] = GetMeanContentErrorPlus("h_dndyProton", file1, i+1, j+1);
      Phi[i][j] = GetMeanContentPlus("h_dndyPhi", file1, i+1, j+1);
      errPhi[i][j] = GetMeanContentErrorPlus("h_dndyPhi", file1, i+1, j+1);
      Pion[i][j] = GetMeanContentPlus("h_dndyPion", file1, i+1, j+1);
      errPion[i][j] = GetMeanContentErrorPlus("h_dndyPion", file1, i+1, j+1);
												
    }

  }	  

  file1->Close();

  for (int i = 0; i < nTrigClasses/6; i++) {
    for (int j = 0; j < nMultClasses; j++) {
                                       
      ratioK0s[i][j] = K0s[6*i][j]/K0s[6*i+5][j];
      errratioK0s[i][j] = GetDoubleRatioError(K0s[6*i][j], errK0s[6*i][j], K0s[6*i+5][j], errK0s[6*i+5][j]);
      ratioXi[i][j] = Xi[6*i][j]/Xi[6*i+5][j];
      errratioXi[i][j] = GetDoubleRatioError(Xi[6*i][j], errXi[6*i][j], Xi[6*i+5][j], errXi[6*i+5][j]);
      ratioOmega[i][j] = Omega[6*i][j]/Omega[6*i+5][j];
      errratioOmega[i][j] = GetDoubleRatioError(Omega[6*i][j], errOmega[6*i][j], Omega[6*i+5][j], errOmega[6*i+5][j]);
      ratioLambda[i][j] = Lambda[6*i][j]/Lambda[6*i+5][j];
      errratioLambda[i][j]= GetDoubleRatioError(Lambda[6*i][j], errLambda[6*i][j], Lambda[6*i+5][j], errLambda[6*i+5][j]);
      ratioProton[i][j] = Proton[6*i][j]/Proton[6*i+5][j];
      errratioProton[i][j] = GetDoubleRatioError(Proton[6*i][j], errProton[6*i][j], Proton[6*i+5][j], errProton[6*i+5][j]);
      ratioPhi[i][j] = Phi[6*i][j]/Phi[6*i+5][j];
      errratioPhi[i][j] = GetDoubleRatioError(Phi[6*i][j], errPhi[6*i][j], Phi[6*i+5][j], errPhi[6*i+5][j]);
      ratioPion[i][j] = Pion[6*i][j]/Pion[6*i+5][j];
      errratioPion[i][j] = GetDoubleRatioError(Pion[6*i][j], errPion[6*i][j], Pion[6*i+5][j], errPion[6*i+5][j]);

    }
  }						

  TMultiGraph* mg1 = new TMultiGraph();
  TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();
  TMultiGraph* mg4 = new TMultiGraph();
  TMultiGraph* mg5 = new TMultiGraph();
  TMultiGraph* mg6 = new TMultiGraph();
  TMultiGraph* mg7 = new TMultiGraph();

  TGraphErrors* ratioK0splot[nTrigClasses/6];
  TGraphErrors* ratioXiplot[nTrigClasses/6];
  TGraphErrors* ratioOmegaplot[nTrigClasses/6];
  TGraphErrors* ratioLambdaplot[nTrigClasses/6];
  TGraphErrors* ratioProtonplot[nTrigClasses/6];
  TGraphErrors* ratioPhiplot[nTrigClasses/6];
  TGraphErrors* ratioPionplot[nTrigClasses/6];

  for (int i = 0; i < nTrigClasses/6; i++) {

    if (i==0) ratioK0splot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioK0s[i], errx_minbias, errratioK0s[i]);
    else ratioK0splot[i] = new TGraphErrors(nMultClasses, x_phi, ratioK0s[i], errx_phi, errratioK0s[i]);
    if (i==0) PlotFeatures(ratioK0splot[i], kViolet, 34, 2, mg1);
    else if (i==1) PlotFeatures(ratioK0splot[i], kBlue, 34, 2, mg1);
    else if (i==2) PlotFeatures(ratioK0splot[i], kSpring-1, 34, 2, mg1);
    else if (i==3) PlotFeatures(ratioK0splot[i], kRed, 34, 2, mg1);
    else if (i==4) PlotFeatures(ratioK0splot[i], kBlack, 34, 2, mg1);

    if (i==0) ratioXiplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioXi[i], errx_minbias, errratioXi[i]);
    else ratioXiplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioXi[i], errx_phi, errratioXi[i]);
    if (i==0) PlotFeatures(ratioXiplot[i], kViolet, 34, 2, mg2);
    else if (i==1) PlotFeatures(ratioXiplot[i], kBlue, 34, 2, mg2);
    else if (i==2) PlotFeatures(ratioXiplot[i], kSpring-1, 34, 2, mg2);
    else if (i==3) PlotFeatures(ratioXiplot[i], kRed, 34, 2, mg2);
    else if (i==4) PlotFeatures(ratioXiplot[i], kBlack, 34, 2, mg2);
								
    if (i==0) ratioOmegaplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioOmega[i], errx_minbias, errratioOmega[i]);
    else ratioOmegaplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioOmega[i], errx_phi, errratioOmega[i]);
    if (i==0) PlotFeatures(ratioOmegaplot[i], kViolet, 34, 2, mg3);
    else if (i==1) PlotFeatures(ratioOmegaplot[i], kBlue, 34, 2, mg3);
    else if (i==2) PlotFeatures(ratioOmegaplot[i], kSpring-1, 34, 2, mg3);
    else if (i==3) PlotFeatures(ratioOmegaplot[i], kRed, 34, 2, mg3);
    else if (i==4) PlotFeatures(ratioOmegaplot[i], kBlack, 34, 2, mg3);

    if (i==0) ratioLambdaplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioLambda[i], errx_minbias, errratioLambda[i]);
    else ratioLambdaplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioLambda[i], errx_phi, errratioLambda[i]);
    if (i==0) PlotFeatures(ratioLambdaplot[i], kViolet, 34, 2, mg4);
    else if (i==1) PlotFeatures(ratioLambdaplot[i], kBlue, 34, 2, mg4);
    else if (i==2) PlotFeatures(ratioLambdaplot[i], kSpring-1, 34, 2, mg4);
    else if (i==3) PlotFeatures(ratioLambdaplot[i], kRed, 34, 2, mg4);
    else if (i==4) PlotFeatures(ratioLambdaplot[i], kBlack, 34, 2, mg4);
    								
    if (i==0) ratioProtonplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioProton[i], errx_minbias, errratioProton[i]);
    else ratioProtonplot[i] = new TGraphErrors(nMultClasses, x_phi, ratioProton[i], errx_phi, errratioProton[i]);
    if (i==0) PlotFeatures(ratioProtonplot[i], kViolet, 34, 2, mg5);
    else if (i==1) PlotFeatures(ratioProtonplot[i], kBlue, 34, 2, mg5);
    else if (i==2) PlotFeatures(ratioProtonplot[i], kSpring-1, 34, 2, mg5);
    else if (i==3) PlotFeatures(ratioProtonplot[i], kRed, 34, 2, mg5);
    else if (i==4) PlotFeatures(ratioProtonplot[i], kBlack, 34, 2, mg5);

    if (i==0) ratioPhiplot[i] = new TGraphErrors(nMultClasses, x_minbias, ratioPhi[i], errx_minbias, errratioPhi[i]);
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
    else if (i==4) PlotFeatures(ratioPionplot[i], kBlack, 34, 2, mg7);

  }

  TMultiGraph* mg8 = new TMultiGraph();
  TMultiGraph* mg9 = new TMultiGraph();
  TMultiGraph* mg10 = new TMultiGraph();
  TMultiGraph* mg11 = new TMultiGraph();
  TMultiGraph* mg12 = new TMultiGraph();

  TGraphErrors* Phi_coreplot[nTrigClasses/6];
  TGraphErrors* Phi_softstrplot[nTrigClasses/6];
  TGraphErrors* Phi_hardstrplot[nTrigClasses/6];
  TGraphErrors* Phi_remnantsplot[nTrigClasses/6];
  TGraphErrors* Phi_coronaplot[nTrigClasses/6];

  for (int i = 0; i < nTrigClasses/6; i++) {

    if (i==0) Phi_coreplot[i] = new TGraphErrors(nMultClasses, x_minbias, Phi[6*i], errx_minbias, errPhi[6*i]);
    else Phi_coreplot[i] = new TGraphErrors(nMultClasses, x_phi, Phi[6*i], errx_phi, errPhi[6*i]);
    Phi_coreplot[i]->Scale(2.);
    if (i==0) PlotFeatures(Phi_coreplot[i], kViolet, 34, 2, mg8);
    else if (i==1) PlotFeatures(Phi_coreplot[i], kBlue, 34, 2, mg8);
    else if (i==2) PlotFeatures(Phi_coreplot[i], kSpring-1, 34, 2, mg8);
    else if (i==3) PlotFeatures(Phi_coreplot[i], kRed, 34, 2, mg8);
    else if (i==4) PlotFeatures(Phi_coreplot[i], kBlack, 34, 2, mg8);

    if (i==0) Phi_softstrplot[i] = new TGraphErrors(nMultClasses, x_minbias, Phi[6*i+1], errx_minbias, errPhi[6*i+1]);
    else Phi_softstrplot[i] = new TGraphErrors(nMultClasses, x_phi, Phi[6*i+1], errx_phi, errPhi[6*i+1]);
    Phi_softstrplot[i]->Scale(2.);
    if (i==0) PlotFeatures(Phi_softstrplot[i], kViolet, 34, 2, mg9);
    else if (i==1) PlotFeatures(Phi_softstrplot[i], kBlue, 34, 2, mg9);
    else if (i==2) PlotFeatures(Phi_softstrplot[i], kSpring-1, 34, 2, mg9);
    else if (i==3) PlotFeatures(Phi_softstrplot[i], kRed, 34, 2, mg9);
    else if (i==4) PlotFeatures(Phi_softstrplot[i], kBlack, 34, 2, mg9);
 
    if (i==0) Phi_hardstrplot[i] = new TGraphErrors(nMultClasses, x_minbias, Phi[6*i+2], errx_minbias, errPhi[6*i+2]);
    else Phi_hardstrplot[i] = new TGraphErrors(nMultClasses, x_phi, Phi[6*i+2], errx_phi, errPhi[6*i+2]);
    Phi_hardstrplot[i]->Scale(2.);
    if (i==0) PlotFeatures(Phi_hardstrplot[i], kViolet, 34, 2, mg10);
    else if (i==1) PlotFeatures(Phi_hardstrplot[i], kBlue, 34, 2, mg10);
    else if (i==2) PlotFeatures(Phi_hardstrplot[i], kSpring-1, 34, 2, mg10);
    else if (i==3) PlotFeatures(Phi_hardstrplot[i], kRed, 34, 2, mg10);
    else if (i==4) PlotFeatures(Phi_hardstrplot[i], kBlack, 34, 2, mg10);
 
    if (i==0) Phi_remnantsplot[i] = new TGraphErrors(nMultClasses, x_minbias, Phi[6*i+3], errx_minbias, errPhi[6*i+3]);
    else Phi_remnantsplot[i] = new TGraphErrors(nMultClasses, x_phi, Phi[6*i+3], errx_phi, errPhi[6*i+3]);
    Phi_remnantsplot[i]->Scale(2.);
    if (i==0) PlotFeatures(Phi_remnantsplot[i], kViolet, 34, 2, mg11);
    else if (i==1) PlotFeatures(Phi_remnantsplot[i], kBlue, 34, 2, mg11);
    else if (i==2) PlotFeatures(Phi_remnantsplot[i], kSpring-1, 34, 2, mg11);
    else if (i==3) PlotFeatures(Phi_remnantsplot[i], kRed, 34, 2, mg11);
    else if (i==4) PlotFeatures(Phi_remnantsplot[i], kBlack, 34, 2, mg11);

    if (i==0) Phi_coronaplot[i] = new TGraphErrors(nMultClasses, x_minbias, Phi[6*i+4], errx_minbias, errPhi[6*i+4]);
    else Phi_coronaplot[i] = new TGraphErrors(nMultClasses, x_phi, Phi[6*i+4], errx_phi, errPhi[6*i+4]);
    Phi_coronaplot[i]->Scale(2.);
    if (i==0) PlotFeatures(Phi_coronaplot[i], kViolet, 34, 2, mg12);
    else if (i==1) PlotFeatures(Phi_coronaplot[i], kBlue, 34, 2, mg12);
    else if (i==2) PlotFeatures(Phi_coronaplot[i], kSpring-1, 34, 2, mg12);
    else if (i==3) PlotFeatures(Phi_coronaplot[i], kRed, 34, 2, mg12);
    else if (i==4) PlotFeatures(Phi_coronaplot[i], kBlack, 34, 2, mg12);
									
  }	   

  c1->cd();
  c1->SetLogx(true);

  mg1->GetXaxis()->SetMoreLogLabels();
  mg1->GetXaxis()->SetTitleSize(0.03);
  mg1->GetXaxis()->SetTitleOffset(1.3);
  mg1->GetXaxis()->SetLabelOffset(0.);
  mg1->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg1->GetYaxis()->SetTitle("X|_{K^{0}_{S}}");
  mg1->GetYaxis()->SetTitleOffset(1.4);
  mg1->GetYaxis()->SetLabelSize(0.03);
  mg1->Draw("AP");
 
  /*TLegend* L1 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L1->SetTextSize(0.02);
  L1->AddEntry(K0splot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L1->AddEntry(K0splot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L1->AddEntry(K0splot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L1->SetLineWidth(0);
  //L1->Draw("same");*/

  c2->cd();
  c2->SetLogx(true);

  mg2->GetXaxis()->SetMoreLogLabels();
  mg2->GetXaxis()->SetTitleSize(0.03);
  mg2->GetXaxis()->SetTitleOffset(1.3);
  mg2->GetXaxis()->SetLabelOffset(0.);
  mg2->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg2->GetYaxis()->SetTitle("X|_{#Xi^{-} + #bar{#Xi}^{+}}");
  mg2->GetYaxis()->SetTitleOffset(1.4);
  mg2->GetYaxis()->SetLabelSize(0.03);
  mg2->Draw("AP");
 											
  /*TLegend* L2 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L2->SetTextSize(0.02);
  L2->AddEntry(Xiplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L2->AddEntry(Xiplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L2->AddEntry(Xiplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L2->SetLineWidth(0);
  //L2->Draw("same");*/
								
  c3->cd();
  c3->SetLogx(true);

  mg3->GetXaxis()->SetMoreLogLabels();
  mg3->GetXaxis()->SetTitleSize(0.03);
  mg3->GetXaxis()->SetTitleOffset(1.3);
  mg3->GetXaxis()->SetLabelOffset(0.);
  mg3->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg3->GetYaxis()->SetTitle("X|_{#Omega^{-} + #bar{#Omega}^{+}}");
  mg3->GetYaxis()->SetTitleOffset(1.4);
  mg3->GetYaxis()->SetLabelSize(0.03);
  mg3->Draw("AP");

  /*TLegend* L3 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L3->SetTextSize(0.02);
  L3->AddEntry(Omegaplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L3->AddEntry(Omegaplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L3->AddEntry(Omegaplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L3->SetLineWidth(0);
  //L3->Draw("same");*/

  c4->cd();
  c4->SetLogx(true);

  mg4->GetXaxis()->SetMoreLogLabels();
  mg4->GetXaxis()->SetTitleSize(0.03);
  mg4->GetXaxis()->SetTitleOffset(1.3);
  mg4->GetXaxis()->SetLabelOffset(0.);
  mg4->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg4->GetYaxis()->SetTitle("X|_{#Lambda + #bar{#Lambda}}");
  mg4->GetYaxis()->SetTitleOffset(1.4);
  mg4->GetYaxis()->SetLabelSize(0.03);
  mg4->Draw("AP");

  /*TLegend* L4 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L4->SetTextSize(0.02);							
  L4->AddEntry(Lambdaplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L4->AddEntry(Lambdaplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L4->AddEntry(Lambdaplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L4->SetLineWidth(0);
  //L4->Draw("same");*/

  c5->cd();
  c5->SetLogx(true);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.03);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("X_{p + #bar{p}}");
  mg5->GetYaxis()->SetTitleOffset(1.4);
  mg5->GetYaxis()->SetLabelSize(0.03);
  mg5->Draw("AP");

  /*TLegend* L5 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L5->SetTextSize(0.02);
  L5->AddEntry(Protonplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L5->AddEntry(Protonplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L5->AddEntry(Protonplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L5->SetLineWidth(0);
  //L5->Draw("same");*/
  
  c6->cd();
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

  /*TLegend* L6 = new TLegend(0.45, 0.12, 0.66, 0.28);
  L6->SetTextSize(0.02);
  L6->AddEntry(Phiplot_epos[0], "EPOS4, |#Deltay<0.1|", "p");
  L6->AddEntry(Phiplot_epos[1], "EPOS4, |#Deltay<0.5|", "p");
  L6->AddEntry(Phiplot_epos[2], "EPOS4, |#Deltay<1.0|", "p");
  L6->SetLineWidth(0);
  //L6->Draw("same");*/
  
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
  mg7->Draw("AP"); 

  c8->cd();
  c8->SetLogx(true);

  mg8->GetXaxis()->SetMoreLogLabels();
  mg8->GetXaxis()->SetTitleSize(0.03);
  mg8->GetXaxis()->SetTitleOffset(1.3);
  mg8->GetXaxis()->SetLabelOffset(0.);
  mg8->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg8->GetYaxis()->SetTitle("2#phi in core");
  mg8->GetYaxis()->SetTitleOffset(1.4);
  mg8->GetYaxis()->SetLabelSize(0.03);
  mg8->Draw("AP");

  c9->cd();
  c9->SetLogx(true);

  mg9->GetXaxis()->SetMoreLogLabels();
  mg9->GetXaxis()->SetTitleSize(0.03);
  mg9->GetXaxis()->SetTitleOffset(1.3);
  mg9->GetXaxis()->SetLabelOffset(0.);
  mg9->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg9->GetYaxis()->SetTitle("2#phi in soft strings");
  mg9->GetYaxis()->SetTitleOffset(1.4);
  mg9->GetYaxis()->SetLabelSize(0.03);
  mg9->Draw("AP");
  
  c10->cd();
  c10->SetLogx(true);

  mg10->GetXaxis()->SetMoreLogLabels();
  mg10->GetXaxis()->SetTitleSize(0.03);
  mg10->GetXaxis()->SetTitleOffset(1.3);
  mg10->GetXaxis()->SetLabelOffset(0.);
  mg10->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg10->GetYaxis()->SetTitle("2#phi in hard strings");
  mg10->GetYaxis()->SetTitleOffset(1.4);
  mg10->GetYaxis()->SetLabelSize(0.03);
  mg10->Draw("AP");
   
  c11->cd();
  c11->SetLogx(true); 

  mg11->GetXaxis()->SetMoreLogLabels();
  mg11->GetXaxis()->SetTitleSize(0.03);
  mg11->GetXaxis()->SetTitleOffset(1.3);
  mg11->GetXaxis()->SetLabelOffset(0.);
  mg11->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg11->GetYaxis()->SetTitle("2#phi in remnants");
  mg11->GetYaxis()->SetTitleOffset(1.4);
  mg11->GetYaxis()->SetLabelSize(0.03);
  mg11->Draw("AP");

  c12->cd();
  c12->SetLogx(true);

  mg12->GetXaxis()->SetMoreLogLabels();
  mg12->GetXaxis()->SetTitleSize(0.03);
  mg12->GetXaxis()->SetTitleOffset(1.3);
  mg12->GetXaxis()->SetLabelOffset(0.);
  mg12->GetXaxis()->SetTitle("<dN_{ch}/d#eta>_{|#eta|<0.5}");
  mg12->GetYaxis()->SetTitle("2#phi in corona");
  mg12->GetYaxis()->SetTitleOffset(1.4);
  mg12->GetYaxis()->SetLabelSize(0.03);
  mg12->Draw("AP");
														
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/k0s_Xcore_epos.root");  
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/xi_Xcore_epos.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/omega_Xcore_epos.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/lambda_Xcore_epos.root");
  c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/proton_Xcore_epos.root");
  c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/phi_Xcore_epos.root");
  c7->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/pion_Xcore_epos.root");
 
  c8->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/phi_core_epos.root");
  c9->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/phi_softstr_epos.root");
  c10->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/phi_hardstr_epos.root");
  c11->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/phi_remnants_epos.root");
  c12->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Xcore_moreclasses_results/phi_corona_epos.root");																								
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/k0s_Xcore_epos.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/xi_Xcore_epos.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/omega_Xcore_epos.pdf");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/lambda_Xcore_epos.pdf");
  c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/phi_Xcore_epos.pdf");
} 											  																																						

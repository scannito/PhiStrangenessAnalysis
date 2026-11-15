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

void Plot_phi_and_minbias() {

  TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_evenmoreclasses_results/analysis_minbias_pydef.root");
  TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_evenmoreclasses_results/analysis_minbias_pynew.root");
  TFile* file3 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Minbias_evenmoreclasses_results/analysis_minbias_epos.root");

  TFile* file4 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/analysis_phi_pynew2.root");
  TFile* file5 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/analysis_phi_epos2.root");

  TFile* file6 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/HEPData/HEPData-ins1784041-v1-Table_18.root");
  TFile* file7 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/HEPData/HEPData-ins1784041-v1-Table_19.root");
  																
  TCanvas* c1 = new TCanvas("c1", "K0s yields", 0, 0, 850, 850);
  TCanvas* c2 = new TCanvas("c2", "Xi yields", 0, 0, 850, 850);
  TCanvas* c3 = new TCanvas("c3", "Omega yields", 0, 0, 850, 850);
  TCanvas* c4 = new TCanvas("c4", "Lambda yields", 0, 0, 850, 850);

  const int nMultClasses = 12;
  const int nRapClasses = 3;

  double x_defMB[nMultClasses], errx_defMB[nMultClasses];

  double ratioK0s_defMB[nMultClasses], errratioK0s_defMB[nMultClasses];
  double ratioXi_defMB[nMultClasses], errratioXi_defMB[nMultClasses];
  double ratioOmega_defMB[nMultClasses], errratioOmega_defMB[nMultClasses];
  double ratioLambda_defMB[nMultClasses], errratioLambda_defMB[nMultClasses];
  double ratioProton_defMB[nMultClasses], errratioProton_defMB[nMultClasses];

  double x_newMB[nMultClasses], errx_newMB[nMultClasses];

  double ratioK0s_newMB[nMultClasses], errratioK0s_newMB[nMultClasses];
  double ratioXi_newMB[nMultClasses], errratioXi_newMB[nMultClasses];
  double ratioOmega_newMB[nMultClasses], errratioOmega_newMB[nMultClasses];
  double ratioLambda_newMB[nMultClasses], errratioLambda_newMB[nMultClasses];
  double ratioProton_newMB[nMultClasses], errratioProton_newMB[nMultClasses];

  double x_eposMB[nMultClasses], errx_eposMB[nMultClasses];

  double ratioK0s_eposMB[nMultClasses], errratioK0s_eposMB[nMultClasses];
  double ratioXi_eposMB[nMultClasses], errratioXi_eposMB[nMultClasses];
  double ratioOmega_eposMB[nMultClasses], errratioOmega_eposMB[nMultClasses];
  double ratioLambda_eposMB[nMultClasses], errratioLambda_eposMB[nMultClasses];
  double ratioProton_eposMB[nMultClasses], errratioProton_eposMB[nMultClasses];

  ////////////////////////////////////////////////////////////////////////////////////////////////

  /*double x_defPhi[nMultClasses], errx_defPhi[nMultClasses];

  double ratioK0s_defPhi[nRapClasses][nMultClasses], errratioK0s_defPhi[nRapClasses][nMultClasses];
  double ratioXi_defPhi[nRapClasses][nMultClasses], errratioXi_defPhi[nRapClasses][nMultClasses];
  double ratioOmega_defPhi[nRapClasses][nMultClasses], errratioOmega_defPhi[nRapClasses][nMultClasses];
  double ratioLambda_defPhi[nRapClasses][nMultClasses], errratioLambda_defPhi[nRapClasses][nMultClasses];
  double ratioProton_defPhi[nRapClasses][nMultClasses], errratioProton_defPhi[nRapClasses][nMultClasses];*/
    												               
  double x_newPhi[nMultClasses], errx_newPhi[nMultClasses];
  
  double ratioK0s_newPhi[nRapClasses][nMultClasses], errratioK0s_newPhi[nRapClasses][nMultClasses];
  double ratioXi_newPhi[nRapClasses][nMultClasses], errratioXi_newPhi[nRapClasses][nMultClasses];
  double ratioOmega_newPhi[nRapClasses][nMultClasses], errratioOmega_newPhi[nRapClasses][nMultClasses];
  double ratioLambda_newPhi[nRapClasses][nMultClasses], errratioLambda_newPhi[nRapClasses][nMultClasses];
  double ratioProton_newPhi[nRapClasses][nMultClasses], errratioProton_newPhi[nRapClasses][nMultClasses];

  double x_eposPhi[nMultClasses], errx_eposPhi[nMultClasses];
  
  double ratioK0s_eposPhi[nRapClasses][nMultClasses], errratioK0s_eposPhi[nRapClasses][nMultClasses];
  double ratioXi_eposPhi[nRapClasses][nMultClasses], errratioXi_eposPhi[nRapClasses][nMultClasses];
  double ratioOmega_eposPhi[nRapClasses][nMultClasses], errratioOmega_eposPhi[nRapClasses][nMultClasses];
  double ratioLambda_eposPhi[nRapClasses][nMultClasses], errratioLambda_eposPhi[nRapClasses][nMultClasses];
  double ratioProton_eposPhi[nRapClasses][nMultClasses], errratioProton_eposPhi[nRapClasses][nMultClasses];

  for (int j = 0; j < nMultClasses; j++) {
 
    x_defMB[j] = GetMeanContent("h_dndeta", file1, j+1);
    
    x_newMB[j] = GetMeanContent("h_dndeta", file2, j+1);
    errx_newMB[j] = GetMeanContentError("h_dndeta", file2, j+1);

    x_eposMB[j] = GetMeanContent("h_dndeta", file3, j+1);
    errx_eposMB[j] = GetMeanContentError("h_dndeta", file3, j+1);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    x_newPhi[j] = GetMeanContentHisto("h_nCh", file4, j+1);
    errx_newPhi[j] = GetMeanContentErrorHisto("h_nCh", file4, j+1);

    x_eposPhi[j] = GetMeanContentHisto("h_nCh", file5, j+1);
    errx_eposPhi[j] = GetMeanContentErrorHisto("h_nCh", file5, j+1);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ratioK0s_defMB[j] = GetRatio("h_dndyK0s", "h_dndyPion", file1, j+1);
    errratioK0s_defMB[j] = GetRatioError("h_dndyK0s", "h_dndyPion", file1, j+1);
    ratioXi_defMB[j] = GetRatio("h_dndyXi", "h_dndyPion", file1, j+1);
    errratioXi_defMB[j] = GetRatioError("h_dndyXi", "h_dndyPion", file1, j+1);
    ratioOmega_defMB[j] = GetRatio("h_dndyOmega", "h_dndyPion", file1, j+1);
    errratioOmega_defMB[j] = GetRatioError("h_dndyOmega", "h_dndyPion", file1, j+1);
    ratioLambda_defMB[j] = GetRatio("h_dndyLambda", "h_dndyPion", file1, j+1);
    errratioLambda_defMB[j] = GetRatioError("h_dndyLambda", "h_dndyPion", file1, j+1);
                      
    ratioK0s_newMB[j] = GetRatio("h_dndyK0s", "h_dndyPion", file2, j+1);
    errratioK0s_newMB[j] = GetRatioError("h_dndyK0s", "h_dndyPion", file2, j+1);
    ratioXi_newMB[j] = GetRatio("h_dndyXi", "h_dndyPion", file2, j+1);
    errratioXi_newMB[j] = GetRatioError("h_dndyXi", "h_dndyPion", file2, j+1);
    ratioOmega_newMB[j] = GetRatio("h_dndyOmega", "h_dndyPion", file2, j+1);
    errratioOmega_newMB[j] = GetRatioError("h_dndyOmega", "h_dndyPion", file2, j+1);
    ratioLambda_newMB[j] = GetRatio("h_dndyLambda", "h_dndyPion", file2, j+1);
    errratioLambda_newMB[j] = GetRatioError("h_dndyLambda", "h_dndyPion", file2, j+1);

    ratioK0s_eposMB[j] = GetRatio("h_dndyK0s", "h_dndyPion", file3, j+1);
    errratioK0s_eposMB[j] = GetRatioError("h_dndyK0s", "h_dndyPion", file3, j+1);
    ratioXi_eposMB[j] = GetRatio("h_dndyXi", "h_dndyPion", file3, j+1);
    errratioXi_eposMB[j] = GetRatioError("h_dndyXi", "h_dndyPion", file3, j+1);
    ratioOmega_eposMB[j] = GetRatio("h_dndyOmega", "h_dndyPion", file3, j+1);
    errratioOmega_eposMB[j] = GetRatioError("h_dndyOmega", "h_dndyPion", file3, j+1);
    ratioLambda_eposMB[j] = GetRatio("h_dndyLambda", "h_dndyPion", file3, j+1);
    errratioLambda_eposMB[j] = GetRatioError("h_dndyLambda", "h_dndyPion", file3, j+1);
 
    for (int i = 0; i < nRapClasses; i++) {

      ratioK0s_newPhi[i][j] = GetRatioPlusHisto("h_nK0s", "h_nPion", file4, i+1, j+1);
      errratioK0s_newPhi[i][j] = GetRatioErrorPlusHisto("h_nK0s", "h_nPion", file4, i+1, j+1);
      ratioXi_newPhi[i][j] = GetRatioPlusHisto("h_nXi", "h_nPion", file4, i+1, j+1);
      errratioXi_newPhi[i][j] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file4, i+1, j+1);
      ratioOmega_newPhi[i][j] = GetRatioPlusHisto("h_nOmega", "h_nPion", file4, i+1, j+1);
      errratioOmega_newPhi[i][j] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file4, i+1, j+1);
      ratioLambda_newPhi[i][j] = GetRatioPlusHisto("h_nLambda", "h_nPion", file4, i+1, j+1);
      errratioLambda_newPhi[i][j] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file4, i+1, j+1);
      
      ratioK0s_eposPhi[i][j] = GetRatioPlusHisto("h_nK0s", "h_nPion", file5, i+1, j+1);
      errratioK0s_eposPhi[i][j] = GetRatioErrorPlusHisto("h_nK0s", "h_nPion", file5, i+1, j+1);
      ratioXi_eposPhi[i][j] = GetRatioPlusHisto("h_nXi", "h_nPion", file5, i+1, j+1);
      errratioXi_eposPhi[i][j] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file5, i+1, j+1);
      ratioOmega_eposPhi[i][j] = GetRatioPlusHisto("h_nOmega", "h_nPion", file5, i+1, j+1);
      errratioOmega_eposPhi[i][j] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file5, i+1, j+1);
      ratioLambda_eposPhi[i][j] = GetRatioPlusHisto("h_nLambda", "h_nPion", file5, i+1, j+1);
      errratioLambda_eposPhi[i][j] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file5, i+1, j+1);
												
    }

  }	  

  TDirectoryFile* dir1 = (TDirectoryFile*)file6->Get("Table 18");
  TGraphErrors* K0splot_data = (TGraphErrors*)dir1->Get("Graph1D_y1");
  TGraphErrors* Lambdaplot_data = (TGraphErrors*)dir1->Get("Graph1D_y2");
  TGraphErrors* Xiplot_data = (TGraphErrors*)dir1->Get("Graph1D_y3");

  TDirectoryFile* dir2 = (TDirectoryFile*)file7->Get("Table 19");
  TGraphErrors* Omegaplot_data = (TGraphErrors*)dir2->Get("Graph1D_y1");

  file1->Close();
  file2->Close();
  file3->Close();

  file4->Close();
  file5->Close();

  file6->Close();
  file7->Close();

  TMultiGraph* mg1 = new TMultiGraph();
  TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();
  TMultiGraph* mg4 = new TMultiGraph();

  TGraphErrors* K0splot_defMB = new TGraphErrors(nMultClasses, x_defMB, ratioK0s_defMB, errx_defMB, errratioK0s_defMB);
  K0splot_defMB->Scale(2.);
  PlotFeaturesPlus(K0splot_defMB,34,kViolet,2,3,kViolet,2,3001,kViolet,0.40,mg1);

  TGraphErrors* Xiplot_defMB = new TGraphErrors(nMultClasses, x_defMB, ratioXi_defMB, errx_defMB, errratioXi_defMB);
  PlotFeaturesPlus(Xiplot_defMB,34,kViolet,2,3,kViolet,2,3001,kViolet,0.40,mg2);

  TGraphErrors* Omegaplot_defMB = new TGraphErrors(nMultClasses, x_defMB, ratioOmega_defMB, errx_defMB, errratioOmega_defMB);
  PlotFeaturesPlus(Omegaplot_defMB,34,kViolet,2,3,kViolet,2,3001,kViolet,0.40,mg3);

  TGraphErrors* Lambdaplot_defMB = new TGraphErrors(nMultClasses, x_defMB, ratioLambda_defMB, errx_defMB, errratioLambda_defMB);
  PlotFeaturesPlus(Lambdaplot_defMB,34,kViolet,2,3,kViolet,2,3001,kViolet,0.40,mg4);

  ////////////////////////////////////////////////////////////////////////////////////////////////
 					
  TGraphErrors* K0splot_newMB = new TGraphErrors(nMultClasses, x_newMB, ratioK0s_newMB, errx_newMB, errratioK0s_newMB);
  K0splot_newMB->Scale(2.);
  PlotFeaturesPlus(K0splot_newMB,33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg1);

  TGraphErrors* Xiplot_newMB = new TGraphErrors(nMultClasses, x_newMB, ratioXi_newMB, errx_newMB, errratioXi_newMB);
  PlotFeaturesPlus(Xiplot_newMB,33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg2);

  TGraphErrors* Omegaplot_newMB = new TGraphErrors(nMultClasses, x_newMB, ratioOmega_newMB, errx_newMB, errratioOmega_newMB);
  PlotFeaturesPlus(Omegaplot_newMB,33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg3);

  TGraphErrors* Lambdaplot_newMB = new TGraphErrors(nMultClasses, x_newMB, ratioLambda_newMB, errx_newMB, errratioLambda_newMB);
  PlotFeaturesPlus(Lambdaplot_newMB,33,kViolet,2,1,kViolet,2,3002,kViolet,0.40,mg4);

  ////////////////////////////////////////////////////////////////////////////////////////////////

  TGraphErrors* K0splot_eposMB = new TGraphErrors(nMultClasses, x_eposMB, ratioK0s_eposMB, errx_eposMB, errratioK0s_eposMB);
  K0splot_eposMB->Scale(2.);
  PlotFeaturesPlus(K0splot_eposMB,29,kViolet,2,7,kViolet,2,3003,kViolet,0.40,mg1);

  TGraphErrors* Xiplot_eposMB = new TGraphErrors(nMultClasses, x_eposMB, ratioXi_eposMB, errx_eposMB, errratioXi_eposMB);
  PlotFeaturesPlus(Xiplot_eposMB,29,kViolet,2,7,kViolet,2,3003,kViolet,0.40,mg2);

  TGraphErrors* Omegaplot_eposMB = new TGraphErrors(nMultClasses, x_eposMB, ratioOmega_eposMB, errx_eposMB, errratioOmega_eposMB);
  PlotFeaturesPlus(Omegaplot_eposMB,29,kViolet,2,7,kViolet,2,3003,kViolet,0.40,mg3);

  TGraphErrors* Lambdaplot_eposMB = new TGraphErrors(nMultClasses, x_eposMB, ratioLambda_eposMB, errx_eposMB, errratioLambda_eposMB);
  PlotFeaturesPlus(Lambdaplot_eposMB,29,kViolet,2,7,kViolet,2,3003,kViolet,0.40,mg4);

  ///////////////////////////////////////////////////////////////////////////////////////////
 
  /*TGraphErrors* K0splot_defPhi[nRapClasses];
  TGraphErrors* Xiplot_defPhi[nRapClasses];
  TGraphErrors* Omegaplot_defPhi[nRapClasses];
  TGraphErrors* Lambdaplot_defPhi[nRapClasses];
  TGraphErrors* Protonplot_defPhi[nRapClasses];
  TGraphErrors* Phiplot_defPhi[nRapClasses];*/
 					
  TGraphErrors* K0splot_newPhi[nRapClasses];
  TGraphErrors* Xiplot_newPhi[nRapClasses];
  TGraphErrors* Omegaplot_newPhi[nRapClasses];
  TGraphErrors* Lambdaplot_newPhi[nRapClasses];
  TGraphErrors* Protonplot_newPhi[nRapClasses];
  TGraphErrors* Phiplot_newPhi[nRapClasses];

  TGraphErrors* K0splot_eposPhi[nRapClasses];
  TGraphErrors* Xiplot_eposPhi[nRapClasses];
  TGraphErrors* Omegaplot_eposPhi[nRapClasses];
  TGraphErrors* Lambdaplot_eposPhi[nRapClasses];
  TGraphErrors* Protonplot_eposPhi[nRapClasses];
  TGraphErrors* Phiplot_eposPhi[nRapClasses];

  for (int i = 0; i < nRapClasses; i++) {

    K0splot_newPhi[i] = new TGraphErrors(nMultClasses, x_newPhi, ratioK0s_newPhi[i], errx_newPhi, errratioK0s_newPhi[i]);
    K0splot_newPhi[i]->Scale(2.);
    if (i==0) PlotFeaturesPlus(K0splot_newPhi[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_newPhi[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_newPhi[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg1);

    Xiplot_newPhi[i] = new TGraphErrors(nMultClasses, x_newPhi, ratioXi_newPhi[i], errx_newPhi, errratioXi_newPhi[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_newPhi[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_newPhi[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_newPhi[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg2);
 
    Omegaplot_newPhi[i] = new TGraphErrors(nMultClasses, x_newPhi, ratioOmega_newPhi[i], errx_newPhi, errratioOmega_newPhi[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_newPhi[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_newPhi[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_newPhi[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg3);

    Lambdaplot_newPhi[i] = new TGraphErrors(nMultClasses, x_newPhi, ratioLambda_newPhi[i], errx_newPhi, errratioLambda_newPhi[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_newPhi[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_newPhi[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_newPhi[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg4);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    K0splot_eposPhi[i] = new TGraphErrors(nMultClasses, x_eposPhi, ratioK0s_eposPhi[i], errx_eposPhi, errratioK0s_eposPhi[i]);
    K0splot_eposPhi[i]->Scale(2.);
    if (i==0) PlotFeaturesPlus(K0splot_eposPhi[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_eposPhi[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_eposPhi[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg1);

    Xiplot_eposPhi[i] = new TGraphErrors(nMultClasses, x_eposPhi, ratioXi_eposPhi[i], errx_eposPhi, errratioXi_eposPhi[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_eposPhi[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_eposPhi[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_eposPhi[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg2);

    Omegaplot_eposPhi[i] = new TGraphErrors(nMultClasses, x_eposPhi, ratioOmega_eposPhi[i], errx_eposPhi, errratioOmega_eposPhi[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_eposPhi[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_eposPhi[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_eposPhi[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg3);
    
    Lambdaplot_eposPhi[i] = new TGraphErrors(nMultClasses, x_eposPhi, ratioLambda_eposPhi[i], errx_eposPhi, errratioLambda_eposPhi[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_eposPhi[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_eposPhi[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_eposPhi[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg4);
  
  }

  PlotFeatures(K0splot_data,kBlack,20,1.5);
  PlotFeatures(Xiplot_data,kBlack,20,1.5);
  PlotFeatures(Omegaplot_data,kBlack,20,1.5);
  PlotFeatures(Lambdaplot_data,kBlack,20,1.5);

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
  mg1->GetYaxis()->SetTitle("2K^{0}_{S}/(#pi^{+}+#pi^{#minus})");
  mg1->GetYaxis()->SetTitleOffset(1.5);
  mg1->GetYaxis()->SetTitleSize(0.05);
  mg1->GetYaxis()->SetLabelSize(0.045);
  //mg1->GetYaxis()->SetMaxDigits(3);
  mg1->Draw("A3L");
								
  /*TLatex* L1_0 = new TLatex();
  L1_0->SetNDC();
  L1_0->SetTextSize(0.045);
  L1_0->DrawLatex(0.465, 0.85, "pp, #sqrt{s} = 13 TeV, |y|<2.5");*/
								
  TLegend* L1 = new TLegend(0.43, 0.7, 0.62, 0.90);
  L1->SetTextSize(0.04);
  L1->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<0.8");
  L1->AddEntry(K0splot_data, "ALICE Run2 Data", "lp");
  L1->AddEntry(K0splot_defMB, "Pythia 8.3 Monash", "l");
  L1->AddEntry(K0splot_newPhi[2], "Pythia 8.3 Ropes", "l");
  L1->AddEntry(K0splot_eposPhi[2], "EPOS4", "l");
  L1->SetLineWidth(0);
  L1->Draw("same");

  /*TLegend* L1_1 = new TLegend(0.68, 0.67, 0.8, 0.81);
  L1_1->AddEntry(K0splot_newPhi[0], "|#Deltay|<0.1", "l");
  L1_1->AddEntry(K0splot_newPhi[1], "|#Deltay|<0.5", "l");
  L1_1->AddEntry(K0splot_newPhi[2], "|#Deltay|<1.0", "l");
  L1_1->SetTextSize(0.025);
  L1_1->SetLineWidth(0);
  L1_1->Draw("same");*/

  K0splot_data->Draw("Psame");

  c2->Divide(1,0);
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

  TLegend* L2_1 = new TLegend(0.2, 0.7, 0.35, 0.90);
  L2_1->AddEntry(Xiplot_newMB, "MB", "l");
  L2_1->AddEntry(Xiplot_newPhi[2], "Inclusive", "l");
  L2_1->AddEntry(Xiplot_newPhi[1], "|#Deltay|<0.5", "l");
  L2_1->AddEntry(Xiplot_newPhi[0], "|#Deltay|<0.2", "l");
  L2_1->SetTextSize(0.045);
  L2_1->SetLineWidth(0);
  L2_1->Draw("same");
 											
  /*TLatex* L2_0 = new TLatex();
  L2_0->SetNDC();
  L2_0->SetTextSize(0.025);
  L2_0->DrawLatex(0.145, 0.82, "pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L2 = new TLegend(0.13, 0.73, 0.29, 0.81);
  L2->SetTextSize(0.025);
  L2->AddEntry(Xiplot_newPhi[2], "PYTHIA8, CR+Ropes", "l");
  L2->AddEntry(Xiplot_eposPhi[2], "EPOS4", "l");
  L2->SetLineWidth(0);
  L2->Draw("same");

  TLegend* L2_1 = new TLegend(0.4, 0.67, 0.52, 0.81);
  L2_1->AddEntry(Xiplot_newPhi[0], "|#Deltay|<0.1", "l");
  L2_1->AddEntry(Xiplot_newPhi[1], "|#Deltay|<0.5", "l");
  L2_1->AddEntry(Xiplot_newPhi[2], "|#Deltay|<1.0", "l");
  L2_1->SetTextSize(0.025);
  L2_1->SetLineWidth(0);
  L2_1->Draw("same");*/

  Xiplot_data->Draw("Psame");

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
  mg3->GetYaxis()->SetTitle("(#Omega^{#minus}+#bar{#Omega}^{+})/(#pi^{+}+#pi^{#minus})");
  mg3->GetYaxis()->SetTitleOffset(1.5);								
  mg3->GetYaxis()->SetLabelSize(0.045);
  mg3->GetYaxis()->SetTitleSize(0.05);
  //mg3->GetYaxis()->SetMaxDigits(3);
  mg3->Draw("A3L");

  TLegend* L3_1 = new TLegend(0.2, 0.7, 0.35, 0.90);
  L3_1->AddEntry(Omegaplot_newMB, "MB", "l");
  L3_1->AddEntry(Omegaplot_newPhi[2], "Inclusive", "l");
  L3_1->AddEntry(Omegaplot_newPhi[1], "|#Deltay|<0.5", "l");
  L3_1->AddEntry(Omegaplot_newPhi[0], "|#Deltay|<0.2", "l");
  L3_1->SetTextSize(0.045);
  L3_1->SetLineWidth(0);
  L3_1->Draw("same");
 
  /*TLatex* L3_0 = new TLatex();
  L3_0->SetNDC();
  L3_0->SetTextSize(0.025);
  L3_0->DrawLatex(0.145, 0.82, "pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L3 = new TLegend(0.13, 0.73, 0.29, 0.81);
  L3->SetTextSize(0.025);
  L3->AddEntry(Omegaplot_newPhi[2], "PYTHIA8, CR+Ropes", "l");
  L3->AddEntry(Omegaplot_eposPhi[2], "EPOS4", "l");
  L3->SetLineWidth(0);
  L3->Draw("same");

  TLegend* L3_1 = new TLegend(0.4, 0.67, 0.52, 0.81);
  L3_1->AddEntry(Omegaplot_newPhi[0], "|#Deltay|<0.1", "l");
  L3_1->AddEntry(Omegaplot_newPhi[1], "|#Deltay|<0.5", "l");
  L3_1->AddEntry(Omegaplot_newPhi[2], "|#Deltay|<1.0", "l");
  L3_1->SetTextSize(0.025);
  L3_1->SetLineWidth(0);
  L3_1->Draw("same");*/

  Omegaplot_data->Draw("Psame");

  c4->Divide(1,0);
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
  //mg4->GetYaxis()->SetMaxDigits(3);
  mg4->Draw("A3L");

  TLegend* L4 = new TLegend(0.2, 0.75, 0.4, 0.90);
  L4->SetTextSize(0.04);
  L4->SetHeader("pp, #sqrt{s} = 13 TeV, |y|<0.8");
  L4->AddEntry(K0splot_newPhi[2], "Pythia 8.3 with ropes", "l");
  L4->AddEntry(K0splot_eposPhi[2], "EPOS4", "l");
  L4->SetLineWidth(0);
  L4->Draw("same");

  /*TLegend* L4_1 = new TLegend(0.15, 0.65, 0.35, 0.90);
  L4_1->AddEntry(Lambdaplot_newPhi[2], "|#Deltay|<1.0", "l");
  L4_1->AddEntry(Lambdaplot_newPhi[1], "|#Deltay|<0.5", "l");
  L4_1->AddEntry(Lambdaplot_newPhi[0], "|#Deltay|<0.1", "l");
  L4_1->SetTextSize(0.055);
  L4_1->SetLineWidth(0);
  L4_1->Draw("same");*/

  Lambdaplot_data->Draw("Psame");

  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/k0s_phi.root");  
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/xi_phi.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/omega_phi.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_newmethod_results/lambda_phi.root");
 													
  /*c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/k0s_phi_newmethod.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/xi_phi_newmethod.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/omega_phi_newmethod.pdf");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/lambda_phi_newmethod.pdf");*/
  //c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/proton_phi_newmethod.pdf");
} 														 

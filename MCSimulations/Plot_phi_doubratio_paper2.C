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

void Plot_phi_doubratio_paper2() {
  
  //TFile* file1 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/analysis_phi_pydef.root");
  //TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/analysis_phi2_pynew.root");
  //TFile* file3 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/analysis_phi2_epos.root");
  TFile* file2 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_pynew.root");
  TFile* file3 = new TFile("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/analysis_phi_epos.root");

  TCanvas* c1 = new TCanvas("c1", "K0s yields", 0, 0, 850, 850);
  TCanvas* c2 = new TCanvas("c2", "Xi yields", 0, 0, 850, 850);
  TCanvas* c3 = new TCanvas("c3", "Omega yields", 0, 0, 850, 850);
  TCanvas* c4 = new TCanvas("c4", "Lambda yields", 0, 0, 850, 850);
  //TCanvas* c5 = new TCanvas("c5", "Proton yields", 0, 0, 850, 850);
  //TCanvas* c6 = new TCanvas("c6", "Phi yields", 0, 0, 850, 850);  

  const int nMultClasses = 12;
  const int nRapClasses = 4;

  /*double x_def[nMultClasses], errx_def[nMultClasses];

  double ratioK0s_def[nRapClasses][nMultClasses], errratioK0s_def[nRapClasses][nMultClasses];
  double ratioXi_def[nRapClasses][nMultClasses], errratioXi_def[nRapClasses][nMultClasses];
  double ratioOmega_def[nRapClasses][nMultClasses], errratioOmega_def[nRapClasses][nMultClasses];
  double ratioLambda_def[nRapClasses][nMultClasses], errratioLambda_def[nRapClasses][nMultClasses];
  double ratioProton_def[nRapClasses][nMultClasses], errratioProton_def[nRapClasses][nMultClasses];
  double ratioPhi_def[nRapClasses][nMultClasses], errratioPhi_def[nRapClasses][nMultClasses];
  
  double doubratioK0s_def[nRapClasses][nMultClasses], errdoubratioK0s_def[nRapClasses][nMultClasses];
  double doubratioXi_def[nRapClasses][nMultClasses], errdoubratioXi_def[nRapClasses][nMultClasses];
  double doubratioOmega_def[nRapClasses][nMultClasses], errdoubratioOmega_def[nRapClasses][nMultClasses];
  double doubratioLambda_def[nRapClasses][nMultClasses], errdoubratioLambda_def[nRapClasses][nMultClasses];
  double doubratioProton_def[nRapClasses][nMultClasses], errdoubratioProton_def[nRapClasses][nMultClasses];
  double doubratioPhi_def[nRapClasses][nMultClasses], errdoubratioPhi_def[nRapClasses][nMultClasses];*/
													
  double x_new[nMultClasses], errx_new[nMultClasses];
  
  double ratioK0s_new[nRapClasses][nMultClasses], errratioK0s_new[nRapClasses][nMultClasses];
  double ratioXi_new[nRapClasses][nMultClasses], errratioXi_new[nRapClasses][nMultClasses];
  double ratioOmega_new[nRapClasses][nMultClasses], errratioOmega_new[nRapClasses][nMultClasses];
  double ratioLambda_new[nRapClasses][nMultClasses], errratioLambda_new[nRapClasses][nMultClasses];
  double ratioProton_new[nRapClasses][nMultClasses], errratioProton_new[nRapClasses][nMultClasses];
  double ratioPhi_new[nRapClasses][nMultClasses], errratioPhi_new[nRapClasses][nMultClasses];

  double ratioK0s_new_norm[nRapClasses], errratioK0s_new_norm[nRapClasses];
  double ratioXi_new_norm[nRapClasses], errratioXi_new_norm[nRapClasses];
  double ratioOmega_new_norm[nRapClasses], errratioOmega_new_norm[nRapClasses];
  double ratioLambda_new_norm[nRapClasses], errratioLambda_new_norm[nRapClasses];
  double ratioProton_new_norm[nRapClasses], errratioProton_new_norm[nRapClasses];
  double ratioPhi_new_norm[nRapClasses], errratioPhi_new_norm[nRapClasses];

  double doubratioK0s_new[nRapClasses][nMultClasses], errdoubratioK0s_new[nRapClasses][nMultClasses];
  double doubratioXi_new[nRapClasses][nMultClasses], errdoubratioXi_new[nRapClasses][nMultClasses];
  double doubratioOmega_new[nRapClasses][nMultClasses], errdoubratioOmega_new[nRapClasses][nMultClasses];
  double doubratioLambda_new[nRapClasses][nMultClasses], errdoubratioLambda_new[nRapClasses][nMultClasses];
  double doubratioProton_new[nRapClasses][nMultClasses], errdoubratioProton_new[nRapClasses][nMultClasses];
  double doubratioPhi_new[nRapClasses][nMultClasses], errdoubratioPhi_new[nRapClasses][nMultClasses];

  double x_epos[nMultClasses], errx_epos[nMultClasses];
  
  double ratioK0s_epos[nRapClasses][nMultClasses], errratioK0s_epos[nRapClasses][nMultClasses];
  double ratioXi_epos[nRapClasses][nMultClasses], errratioXi_epos[nRapClasses][nMultClasses];
  double ratioOmega_epos[nRapClasses][nMultClasses], errratioOmega_epos[nRapClasses][nMultClasses];
  double ratioLambda_epos[nRapClasses][nMultClasses], errratioLambda_epos[nRapClasses][nMultClasses];
  double ratioProton_epos[nRapClasses][nMultClasses], errratioProton_epos[nRapClasses][nMultClasses];
  double ratioPhi_epos[nRapClasses][nMultClasses], errratioPhi_epos[nRapClasses][nMultClasses];

  double ratioK0s_epos_norm[nRapClasses], errratioK0s_epos_norm[nRapClasses];
  double ratioXi_epos_norm[nRapClasses], errratioXi_epos_norm[nRapClasses];
  double ratioOmega_epos_norm[nRapClasses], errratioOmega_epos_norm[nRapClasses];
  double ratioLambda_epos_norm[nRapClasses], errratioLambda_epos_norm[nRapClasses];
  double ratioProton_epos_norm[nRapClasses], errratioProton_epos_norm[nRapClasses];
  double ratioPhi_epos_norm[nRapClasses], errratioPhi_epos_norm[nRapClasses];

  double doubratioK0s_epos[nRapClasses][nMultClasses], errdoubratioK0s_epos[nRapClasses][nMultClasses];
  double doubratioXi_epos[nRapClasses][nMultClasses], errdoubratioXi_epos[nRapClasses][nMultClasses];
  double doubratioOmega_epos[nRapClasses][nMultClasses], errdoubratioOmega_epos[nRapClasses][nMultClasses];
  double doubratioLambda_epos[nRapClasses][nMultClasses], errdoubratioLambda_epos[nRapClasses][nMultClasses];
  double doubratioProton_epos[nRapClasses][nMultClasses], errdoubratioProton_epos[nRapClasses][nMultClasses];
  double doubratioPhi_epos[nRapClasses][nMultClasses], errdoubratioPhi_epos[nRapClasses][nMultClasses];

  for (int i = 0; i < nRapClasses; i++) {
 
    ratioK0s_new_norm[i] = GetRatioPlusHisto("h_nK0s", "h_nPion", file2, i+1, 0);
    errratioK0s_new_norm[i] = GetRatioErrorPlusHisto("h_nK0s", "h_nPion", file2, i+1, 0);
    ratioXi_new_norm[i] = GetRatioPlusHisto("h_nXi", "h_nPion", file2, i+1, 0);
    errratioXi_new_norm[i] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file2, i+1, 0);
    ratioOmega_new_norm[i] = GetRatioPlusHisto("h_nOmega", "h_nPion", file2, i+1, 0);
    errratioOmega_new_norm[i] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file2, i+1, 0);
    ratioLambda_new_norm[i] = GetRatioPlusHisto("h_nLambda", "h_nPion", file2, i+1, 0);
    errratioLambda_new_norm[i] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file2, i+1, 0);
    ratioProton_new_norm[i] = GetRatioPlusHisto("h_nProton", "h_nPion", file2, i+1, 0);
    errratioProton_new_norm[i] = GetRatioErrorPlusHisto("h_nProton", "h_nPion", file2, i+1, 0);

    ratioK0s_epos_norm[i] = GetRatioPlusHisto("h_nK0s", "h_nPion", file3, i+1, 0);
    errratioK0s_epos_norm[i] = GetRatioErrorPlusHisto("h_nK0s", "h_nPion", file3, i+1, 0);
    ratioXi_epos_norm[i] = GetRatioPlusHisto("h_nXi", "h_nPion", file3, i+1, 0);
    errratioXi_epos_norm[i] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file3, i+1, 0);
    ratioOmega_epos_norm[i] = GetRatioPlusHisto("h_nOmega", "h_nPion", file3, i+1, 0);
    errratioOmega_epos_norm[i] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file3, i+1, 0);
    ratioLambda_epos_norm[i] = GetRatioPlusHisto("h_nLambda", "h_nPion", file3, i+1, 0);
    errratioLambda_epos_norm[i] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file3, i+1, 0);
    ratioProton_epos_norm[i] = GetRatioPlusHisto("h_nProton", "h_nPion", file3, i+1, 0);
    errratioProton_epos_norm[i] = GetRatioErrorPlusHisto("h_nProton", "h_nPion", file3, i+1, 0);

  }	    

  for (int j = 0; j < nMultClasses; j++) {
 
    //x_def[j] = GetMeanContent("h_dndeta", file1, j+1);
    //errx_def[j] = GetMeanContentError("h_dndeta", file1, j+1);

    //x_new[j] = GetMeanContent("h_dndeta", file2, j+1);
    //errx_new[j] = GetMeanContentError("h_dndeta", file2, j+1);
 
    x_new[j] = GetMeanContentHisto("h_nCh", file2, j+1);
    errx_new[j] = GetMeanContentErrorHisto("h_nCh", file2, j+1);
    
    //x_epos[j] = GetMeanContent("h_dndeta", file3, j+1);
    //errx_epos[j] = GetMeanContentError("h_dndeta", file3, j+1);

    x_epos[j] = GetMeanContentHisto("h_nCh", file3, j+1);
    errx_epos[j] = GetMeanContentErrorHisto("h_nCh", file3, j+1);

    for (int i = 0; i < nRapClasses; i++) {
          
      /*ratioK0s_def[i][j] = GetRatioPlus("h_dndyK0s", "h_dndyPion", file1, i+1, j+1);
      errratioK0s_def[i][j] = GetRatioErrorPlus("h_dndyK0s", "h_dndyPion", file1, i+1, j+1);
      ratioXi_def[i][j] = GetRatioPlus("h_dndyXi", "h_dndyPion", file1, i+1, j+1);
      errratioXi_def[i][j] = GetRatioErrorPlus("h_dndyXi", "h_dndyPion", file1, i+1, j+1);
      ratioOmega_def[i][j] = GetRatioPlus("h_dndyOmega", "h_dndyPion", file1, i+1, j+1);
      errratioOmega_def[i][j] = GetRatioErrorPlus("h_dndyOmega", "h_dndyPion", file1, i+1, j+1);
      ratioLambda_def[i][j] = GetRatioPlus("h_dndyLambda", "h_dndyPion", file1, i+1, j+1);
      errratioLambda_def[i][j] = GetRatioErrorPlus("h_dndyLambda", "h_dndyPion", file1, i+1, j+1);
      ratioProton_def[i][j] = GetRatioPlus("h_dndyProton", "h_dndyPion", file1, i+1, j+1);
      errratioProton_def[i][j] = GetRatioErrorPlus("h_dndyProton", "h_dndyPion", file1, i+1, j+1);
      ratioPhi_def[i][j] = GetRatioPlus("h_dndyPhi", "h_dndyPion", file1, i+1, j+1);
      errratioPhi_def[i][j] = GetRatioErrorPlus("h_dndyPhi", "h_dndyPion", file1, i+1, j+1);*/
																							
      /*ratioK0s_new[i][j] = GetRatioPlus("h_dndyK0s", "h_dndyPion", file2, i+1, j+1);
      errratioK0s_new[i][j] = GetRatioErrorPlus("h_dndyK0s", "h_dndyPion", file2, i+1, j+1);
      ratioXi_new[i][j] = GetRatioPlus("h_dndyXi", "h_dndyPion", file2, i+1, j+1);
      errratioXi_new[i][j] = GetRatioErrorPlus("h_dndyXi", "h_dndyPion", file2, i+1, j+1);
      ratioOmega_new[i][j] = GetRatioPlus("h_dndyOmega", "h_dndyPion", file2, i+1, j+1);
      errratioOmega_new[i][j] = GetRatioErrorPlus("h_dndyOmega", "h_dndyPion", file2, i+1, j+1);
      ratioLambda_new[i][j] = GetRatioPlus("h_dndyLambda", "h_dndyPion", file2, i+1, j+1);
      errratioLambda_new[i][j] = GetRatioErrorPlus("h_dndyLambda", "h_dndyPion", file2, i+1, j+1);
      ratioProton_new[i][j] = GetRatioPlus("h_dndyProton", "h_dndyPion", file2, i+1, j+1);
      errratioProton_new[i][j] = GetRatioErrorPlus("h_dndyProton", "h_dndyPion", file2, i+1, j+1);
      ratioPhi_new[i][j] = GetRatioPlus("h_dndyPhi", "h_dndyPion", file2, i+1, j+1);
      errratioPhi_new[i][j] = GetRatioErrorPlus("h_dndyPhi", "h_dndyPion", file2, i+1, j+1);*/
											
      ratioK0s_new[i][j] = GetRatioPlusHisto("h_nK0s", "h_nPion", file2, i+1, j+1);
      errratioK0s_new[i][j] = GetRatioErrorPlusHisto("h_nK0s", "h_nPion", file2, i+1, j+1);
      ratioXi_new[i][j] = GetRatioPlusHisto("h_nXi", "h_nPion", file2, i+1, j+1);
      errratioXi_new[i][j] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file2, i+1, j+1);
      ratioOmega_new[i][j] = GetRatioPlusHisto("h_nOmega", "h_nPion", file2, i+1, j+1);
      errratioOmega_new[i][j] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file2, i+1, j+1);
      ratioLambda_new[i][j] = GetRatioPlusHisto("h_nLambda", "h_nPion", file2, i+1, j+1);
      errratioLambda_new[i][j] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file2, i+1, j+1);
      ratioProton_new[i][j] = GetRatioPlusHisto("h_nProton", "h_nPion", file2, i+1, j+1);
      errratioProton_new[i][j] = GetRatioErrorPlusHisto("h_nProton", "h_nPion", file2, i+1, j+1);
      //ratioPhi_new[i][j] = GetRatioPlusHisto("h_nPhi", "h_nPion", file2, i+1, j+1);
      //errratioPhi_new[i][j] = GetRatioErrorPlusHisto("h_nPhi", "h_nPion", file2, i+1, j+1); 

      /*ratioK0s_epos[i][j] = GetRatioPlus("h_dndyK0s", "h_dndyPion", file3, i+1, j+1);
      errratioK0s_epos[i][j] = GetRatioErrorPlus("h_dndyK0s", "h_dndyPion", file3, i+1, j+1);
      ratioXi_epos[i][j] = GetRatioPlus("h_dndyXi", "h_dndyPion", file3, i+1, j+1);
      errratioXi_epos[i][j] = GetRatioErrorPlus("h_dndyXi", "h_dndyPion", file3, i+1, j+1);
      ratioOmega_epos[i][j] = GetRatioPlus("h_dndyOmega", "h_dndyPion", file3, i+1, j+1);
      errratioOmega_epos[i][j] = GetRatioErrorPlus("h_dndyOmega", "h_dndyPion", file3, i+1, j+1);
      ratioLambda_epos[i][j] = GetRatioPlus("h_dndyLambda", "h_dndyPion", file3, i+1, j+1);
      errratioLambda_epos[i][j] = GetRatioErrorPlus("h_dndyLambda", "h_dndyPion", file3, i+1, j+1);
      ratioProton_epos[i][j] = GetRatioPlus("h_dndyProton", "h_dndyPion", file3, i+1, j+1);
      errratioProton_epos[i][j] = GetRatioErrorPlus("h_dndyProton", "h_dndyPion", file3, i+1, j+1);
      ratioPhi_epos[i][j] = GetRatioPlus("h_dndyPhi", "h_dndyPion", file3, i+1, j+1);
      errratioPhi_epos[i][j] = GetRatioErrorPlus("h_dndyPhi", "h_dndyPion", file3, i+1, j+1);*/

      ratioK0s_epos[i][j] = GetRatioPlusHisto("h_nK0s", "h_nPion", file3, i+1, j+1);
      errratioK0s_epos[i][j] = GetRatioErrorPlusHisto("h_nK0s", "h_nPion", file3, i+1, j+1);
      ratioXi_epos[i][j] = GetRatioPlusHisto("h_nXi", "h_nPion", file3, i+1, j+1);
      errratioXi_epos[i][j] = GetRatioErrorPlusHisto("h_nXi", "h_nPion", file3, i+1, j+1);
      ratioOmega_epos[i][j] = GetRatioPlusHisto("h_nOmega", "h_nPion", file3, i+1, j+1);
      errratioOmega_epos[i][j] = GetRatioErrorPlusHisto("h_nOmega", "h_nPion", file3, i+1, j+1);
      ratioLambda_epos[i][j] = GetRatioPlusHisto("h_nLambda", "h_nPion", file3, i+1, j+1);
      errratioLambda_epos[i][j] = GetRatioErrorPlusHisto("h_nLambda", "h_nPion", file3, i+1, j+1);
      ratioProton_epos[i][j] = GetRatioPlusHisto("h_nProton", "h_nPion", file3, i+1, j+1);
      errratioProton_epos[i][j] = GetRatioErrorPlusHisto("h_nProton", "h_nPion", file3, i+1, j+1);
      //ratioPhi_epos[i][j] = GetRatioPlusHisto("h_nPhi", "h_nPion", file3, i+1, j+1);
      //errratioPhi_epos[i][j] = GetRatioErrorPlusHisto("h_nPhi", "h_nPion", file3, i+1, j+1);
												
    }

  }	  

  //file1->Close();
  file2->Close();
  file3->Close();

  //const int refindex = 5;

  for (int i = 0; i < nRapClasses; i++) {
    for (int j = 0; j < nMultClasses; j++) {
                                       
      /*doubratioK0s_def[i][j] = ratioK0s_def[i][j]/ratioK0s_def[i][refindex];
      errdoubratioK0s_def[i][j] = GetDoubleRatioError(ratioK0s_def[i][j], errratioK0s_def[i][j], ratioK0s_def[i][refindex], errratioK0s_def[i][refindex]);
      doubratioXi_def[i][j] = ratioXi_def[i][j]/ratioXi_def[i][refindex];
      errdoubratioXi_def[i][j] = GetDoubleRatioError(ratioXi_def[i][j], errratioXi_def[i][j], ratioXi_def[i][refindex], errratioXi_def[i][refindex]);
      doubratioOmega_def[i][j] = ratioOmega_def[i][j]/ratioOmega_def[i][refindex];
      errdoubratioOmega_def[i][j] = GetDoubleRatioError(ratioOmega_def[i][j], errratioOmega_def[i][j], ratioOmega_def[i][refindex], errratioOmega_def[i][refindex]);
      doubratioLambda_def[i][j] = ratioLambda_def[i][j]/ratioLambda_def[i][refindex];
      errdoubratioLambda_def[i][j]= GetDoubleRatioError(ratioLambda_def[i][j], errratioLambda_def[i][j], ratioLambda_def[i][refindex], errratioLambda_def[i][refindex]);
      doubratioProton_def[i][j] = ratioProton_def[i][j]/ratioProton_def[i][refindex];
      errdoubratioProton_def[i][j] = GetDoubleRatioError(ratioProton_def[i][j], errratioProton_def[i][j], ratioProton_def[i][refindex], errratioProton_def[i][refindex]);
      doubratioPhi_def[i][j] = ratioPhi_def[i][j]/ratioPhi_def[i][refindex];
      errdoubratioPhi_def[i][j] = GetDoubleRatioError(ratioPhi_def[i][j], errratioPhi_def[i][j], ratioPhi_def[i][refindex], errratioPhi_def[i][refindex]);*/
																				
      doubratioK0s_new[i][j] = ratioK0s_new[i][j]/ratioK0s_new_norm[i];
      errdoubratioK0s_new[i][j] = GetDoubleRatioError(ratioK0s_new[i][j], errratioK0s_new[i][j], ratioK0s_new_norm[i], errratioK0s_new_norm[i]);
      doubratioXi_new[i][j] = ratioXi_new[i][j]/ratioXi_new_norm[i];
      errdoubratioXi_new[i][j] = GetDoubleRatioError(ratioXi_new[i][j], errratioXi_new[i][j], ratioXi_new_norm[i], errratioXi_new_norm[i]);
      doubratioOmega_new[i][j] = ratioOmega_new[i][j]/ratioOmega_new_norm[i];
      errdoubratioOmega_new[i][j] = GetDoubleRatioError(ratioOmega_new[i][j], errratioOmega_new[i][j], ratioOmega_new_norm[i], errratioOmega_new_norm[i]);
      doubratioLambda_new[i][j] = ratioLambda_new[i][j]/ratioLambda_new_norm[i];
      errdoubratioLambda_new[i][j] = GetDoubleRatioError(ratioLambda_new[i][j], errratioLambda_new[i][j], ratioLambda_new_norm[i], errratioLambda_new_norm[i]);
      doubratioProton_new[i][j] = ratioProton_new[i][j]/ratioProton_new_norm[i];
      errdoubratioProton_new[i][j] = GetDoubleRatioError(ratioProton_new[i][j], errratioProton_new[i][j], ratioProton_new_norm[i], errratioProton_new_norm[i]);
      //doubratioPhi_new[i][j] = ratioPhi_new[i][j]/ratioPhi_new_norm[i];
      //errdoubratioPhi_new[i][j] = GetDoubleRatioError(ratioPhi_new[i][j], errratioPhi_new[i][j], ratioPhi_new_norm[i], errratioPhi_new_norm[i]);
 
      doubratioK0s_epos[i][j] = ratioK0s_epos[i][j]/ratioK0s_epos_norm[i]; 
      errdoubratioK0s_epos[i][j] = GetDoubleRatioError(ratioK0s_epos[i][j], errratioK0s_epos[i][j], ratioK0s_epos_norm[i], errratioK0s_epos_norm[i]);
      doubratioXi_epos[i][j] = ratioXi_epos[i][j]/ratioXi_epos_norm[i];
      errdoubratioXi_epos[i][j] = GetDoubleRatioError(ratioXi_epos[i][j], errratioXi_epos[i][j], ratioXi_epos_norm[i], errratioXi_epos_norm[i]);
      doubratioOmega_epos[i][j] = ratioOmega_epos[i][j]/ratioOmega_epos_norm[i];
      errdoubratioOmega_epos[i][j] = GetDoubleRatioError(ratioOmega_epos[i][j], errratioOmega_epos[i][j], ratioOmega_epos_norm[i], errratioOmega_epos_norm[i]);
      doubratioLambda_epos[i][j] = ratioLambda_epos[i][j]/ratioLambda_epos_norm[i];
      errdoubratioLambda_epos[i][j] = GetDoubleRatioError(ratioLambda_epos[i][j], errratioLambda_epos[i][j], ratioLambda_epos_norm[i], errratioLambda_epos_norm[i]);
      doubratioProton_epos[i][j] = ratioProton_epos[i][j]/ratioProton_epos_norm[i];
      errdoubratioProton_epos[i][j] = GetDoubleRatioError(ratioProton_epos[i][j], errratioProton_epos[i][j], ratioProton_epos_norm[i], errratioProton_epos_norm[i]);
      //doubratioPhi_epos[i][j] = ratioPhi_epos[i][j]/ratioPhi_epos_norm[i];
      //errdoubratioPhi_epos[i][j] = GetDoubleRatioError(ratioPhi_epos[i][j], errratioPhi_epos[i][j], ratioPhi_epos_norm[i], errratioPhi_epos_norm[i]);
 
    }
  }						

  TMultiGraph* mg1 = new TMultiGraph();
  TMultiGraph* mg2 = new TMultiGraph();
  TMultiGraph* mg3 = new TMultiGraph();
  TMultiGraph* mg4 = new TMultiGraph();
  TMultiGraph* mg5 = new TMultiGraph();
  //TMultiGraph* mg6 = new TMultiGraph();

  /*TGraphErrors* K0splot_def[nRapClasses];
  TGraphErrors* Xiplot_def[nRapClasses];
  TGraphErrors* Omegaplot_def[nRapClasses];
  TGraphErrors* Lambdaplot_def[nRapClasses];
  TGraphErrors* Protonplot_def[nRapClasses];
  TGraphErrors* Phiplot_def[nRapClasses];*/

  TGraphErrors* K0splot_new[nRapClasses];
  TGraphErrors* Xiplot_new[nRapClasses];
  TGraphErrors* Omegaplot_new[nRapClasses];
  TGraphErrors* Lambdaplot_new[nRapClasses];
  TGraphErrors* Protonplot_new[nRapClasses];
  TGraphErrors* Phiplot_new[nRapClasses];

  TGraphErrors* K0splot_epos[nRapClasses];
  TGraphErrors* Xiplot_epos[nRapClasses];
  TGraphErrors* Omegaplot_epos[nRapClasses];
  TGraphErrors* Lambdaplot_epos[nRapClasses];
  TGraphErrors* Protonplot_epos[nRapClasses];
  TGraphErrors* Phiplot_epos[nRapClasses];

  for (int i = 0; i < nRapClasses; i++) {

    /*K0splot_def[i] = new TGraphErrors(nMultClasses, x_def, doubratioK0s_def[i], errx_def, errdoubratioK0s_def[i]);
    K0splot_def[i]->Scale(2.);
    if (i==0) {
      K0splot_def[i]->SetMarkerColor(kBlue);
      K0splot_def[i]->SetLineColor(kBlue);
    } 
    else if (i==1) {
      K0splot_def[i]->SetMarkerColor(kSpring-1);
      K0splot_def[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      K0splot_def[i]->SetMarkerColor(kRed);
      K0splot_def[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      K0splot_def[i]->SetMarkerColor(kBlack);
      K0splot_def[i]->SetLineColor(kBlack);
    }    
    K0splot_def[i]->SetMarkerStyle(34);
    K0splot_def[i]->SetMarkerSize(2);
    //mg1->Add(K0splot_def[i]);

    Xiplot_def[i] = new TGraphErrors(nMultClasses, x_def, doubratioXi_def[i], errx_def, errdoubratioXi_def[i]);
    if (i==0) {
      Xiplot_def[i]->SetMarkerColor(kBlue);
      Xiplot_def[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Xiplot_def[i]->SetMarkerColor(kSpring-1);
      Xiplot_def[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Xiplot_def[i]->SetMarkerColor(kRed);
      Xiplot_def[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Xiplot_def[i]->SetMarkerColor(kBlack);
      Xiplot_def[i]->SetLineColor(kBlack);
    }
    Xiplot_def[i]->SetMarkerStyle(34);
    Xiplot_def[i]->SetMarkerSize(2);
    //mg2->Add(Xiplot_def[i]);

    Omegaplot_def[i] = new TGraphErrors(nMultClasses, x_def, doubratioOmega_def[i], errx_def, errdoubratioOmega_def[i]);
    if (i==0) {
      Omegaplot_def[i]->SetMarkerColor(kBlue);
      Omegaplot_def[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Omegaplot_def[i]->SetMarkerColor(kSpring-1);
      Omegaplot_def[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Omegaplot_def[i]->SetMarkerColor(kRed);
      Omegaplot_def[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Omegaplot_def[i]->SetMarkerColor(kBlack);
      Omegaplot_def[i]->SetLineColor(kBlack);
    }
    Omegaplot_def[i]->SetMarkerStyle(34);
    Omegaplot_def[i]->SetMarkerSize(2);
    //mg3->Add(Omegaplot_def[i]);

    Lambdaplot_def[i] = new TGraphErrors(nMultClasses, x_def, doubratioLambda_def[i], errx_def, errdoubratioLambda_def[i]);
    if (i==0) {
      Lambdaplot_def[i]->SetMarkerColor(kBlue);
      Lambdaplot_def[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Lambdaplot_def[i]->SetMarkerColor(kSpring-1);
      Lambdaplot_def[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Lambdaplot_def[i]->SetMarkerColor(kRed);
      Lambdaplot_def[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Lambdaplot_def[i]->SetMarkerColor(kBlack);
      Lambdaplot_def[i]->SetLineColor(kBlack);
    }
    Lambdaplot_def[i]->SetMarkerStyle(34);
    Lambdaplot_def[i]->SetMarkerSize(2);
    //mg4->Add(Lambdaplot_def[i]);

    Protonplot_def[i] = new TGraphErrors(nMultClasses, x_def, doubratioProton_def[i], errx_def, errdoubratioProton_def[i]);
    if (i==0) {
      Protonplot_def[i]->SetMarkerColor(kBlue);
      Protonplot_def[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Protonplot_def[i]->SetMarkerColor(kSpring-1);
      Protonplot_def[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Protonplot_def[i]->SetMarkerColor(kRed);
      Protonplot_def[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Protonplot_def[i]->SetMarkerColor(kBlack);
      Protonplot_def[i]->SetLineColor(kBlack);
    }
    Protonplot_def[i]->SetMarkerStyle(34);
    Protonplot_def[i]->SetMarkerSize(2);
    //mg5->Add(Protonplot_def[i]);

    Phiplot_def[i] = new TGraphErrors(nMultClasses, x_def, doubratioPhi_def[i], errx_def, errdoubratioPhi_def[i]);
    Phiplot_def[i]->Scale(2.);
    if (i==0) {
      Phiplot_def[i]->SetMarkerColor(kBlue);
      Phiplot_def[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Phiplot_def[i]->SetMarkerColor(kSpring-1);
      Phiplot_def[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Phiplot_def[i]->SetMarkerColor(kRed);
      Phiplot_def[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Phiplot_def[i]->SetMarkerColor(kBlack);
      Phiplot_def[i]->SetLineColor(kBlack);
    }
    Phiplot_def[i]->SetMarkerStyle(34);
    Phiplot_def[i]->SetMarkerSize(2);
    //mg6->Add(Phiplot_def[i]);*/

    K0splot_new[i] = new TGraphErrors(nMultClasses, x_new, doubratioK0s_new[i], errx_new, errdoubratioK0s_new[i]);
    //K0splot_new[i]->Scale(2.);
    if (i==0) PlotFeaturesPlus(K0splot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg1);
    else if (i==3) PlotFeaturesPlus(K0splot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg1);

    Xiplot_new[i] = new TGraphErrors(nMultClasses, x_new, doubratioXi_new[i], errx_new, errdoubratioXi_new[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg2);
    else if (i==3) PlotFeaturesPlus(Xiplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg2);

    Omegaplot_new[i] = new TGraphErrors(nMultClasses, x_new, doubratioOmega_new[i], errx_new, errdoubratioOmega_new[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg3);
    else if (i==3) PlotFeaturesPlus(Omegaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg3);

    Lambdaplot_new[i] = new TGraphErrors(nMultClasses, x_new, doubratioLambda_new[i], errx_new, errdoubratioLambda_new[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg4);
    else if (i==3) PlotFeaturesPlus(Lambdaplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg4);

    Protonplot_new[i] = new TGraphErrors(nMultClasses, x_new, doubratioProton_new[i], errx_new, errdoubratioProton_new[i]);
    if (i==0) PlotFeaturesPlus(Protonplot_new[i],33,kRed+1,2,1,kRed+1,2,3002,kRed+1,0.40,mg5);
    else if (i==1) PlotFeaturesPlus(Protonplot_new[i],33,kGreen+3,2,1,kGreen+3,2,3002,kGreen+3,0.40,mg5);
    else if (i==2) PlotFeaturesPlus(Protonplot_new[i],33,kBlue,2,1,kBlue,2,3002,kBlue,0.40,mg5);
    else if (i==3) PlotFeaturesPlus(Protonplot_new[i],33,kBlack,2,1,kBlack,2,3002,kBlack,0.40,mg5);

    /*Phiplot_new[i] = new TGraphErrors(nMultClasses, x_new, doubratioPhi_new[i], errx_new, errdoubratioPhi_new[i]);
    Phiplot_new[i]->Scale(2.);
    if (i==0) {
      Phiplot_new[i]->SetMarkerColor(kBlue);
      Phiplot_new[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Phiplot_new[i]->SetMarkerColor(kSpring-1);
      Phiplot_new[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Phiplot_new[i]->SetMarkerColor(kRed);
      Phiplot_new[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Phiplot_new[i]->SetMarkerColor(kBlack);
      Phiplot_new[i]->SetLineColor(kBlack);
    }
    Phiplot_new[i]->SetMarkerStyle(33);
    Phiplot_new[i]->SetMarkerSize(2);
    mg6->Add(Phiplot_new[i]);*/

    K0splot_epos[i] = new TGraphErrors(nMultClasses, x_epos, doubratioK0s_epos[i], errx_epos, errdoubratioK0s_epos[i]);
    //K0splot_epos[i]->Scale(2.);
    if (i==0) PlotFeaturesPlus(K0splot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg1);
    else if (i==1) PlotFeaturesPlus(K0splot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg1);
    else if (i==2) PlotFeaturesPlus(K0splot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg1);
    else if (i==3) PlotFeaturesPlus(K0splot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg1);

    Xiplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, doubratioXi_epos[i], errx_epos, errdoubratioXi_epos[i]);
    if (i==0) PlotFeaturesPlus(Xiplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg2);
    else if (i==1) PlotFeaturesPlus(Xiplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg2);
    else if (i==2) PlotFeaturesPlus(Xiplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg2);
    else if (i==3) PlotFeaturesPlus(Xiplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg2);

    Omegaplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, doubratioOmega_epos[i], errx_epos, errdoubratioOmega_epos[i]);
    if (i==0) PlotFeaturesPlus(Omegaplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg3);
    else if (i==1) PlotFeaturesPlus(Omegaplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg3);
    else if (i==2) PlotFeaturesPlus(Omegaplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg3);
    else if (i==3) PlotFeaturesPlus(Omegaplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg3);

    Lambdaplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, doubratioLambda_epos[i], errx_epos, errdoubratioLambda_epos[i]);
    if (i==0) PlotFeaturesPlus(Lambdaplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg4);
    else if (i==1) PlotFeaturesPlus(Lambdaplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg4);
    else if (i==2) PlotFeaturesPlus(Lambdaplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg4);
    else if (i==3) PlotFeaturesPlus(Lambdaplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg4);

    Protonplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, doubratioProton_epos[i], errx_epos, errdoubratioProton_epos[i]);
    if (i==0) PlotFeaturesPlus(Protonplot_epos[i],29,kRed+1,2,7,kRed+1,2,3003,kRed+1,0.40,mg5);
    else if (i==1) PlotFeaturesPlus(Protonplot_epos[i],29,kGreen+3,2,7,kGreen+3,2,3003,kGreen+3,0.40,mg5);
    else if (i==2) PlotFeaturesPlus(Protonplot_epos[i],29,kBlue,2,7,kBlue,2,3003,kBlue,0.40,mg5);
    else if (i==3) PlotFeaturesPlus(Protonplot_epos[i],29,kBlack,2,7,kBlack,2,3003,kBlack,0.40,mg5);

    /*Phiplot_epos[i] = new TGraphErrors(nMultClasses, x_epos, doubratioPhi_epos[i], errx_epos, errdoubratioPhi_epos[i]);
    Phiplot_epos[i]->Scale(2.);
    if (i==0) {
      Phiplot_epos[i]->SetMarkerColor(kBlue);
      Phiplot_epos[i]->SetLineColor(kBlue);
    }
    else if (i==1) {
      Phiplot_epos[i]->SetMarkerColor(kSpring-1);
      Phiplot_epos[i]->SetLineColor(kSpring-1);
    }
    else if (i==2) {
      Phiplot_epos[i]->SetMarkerColor(kRed);
      Phiplot_epos[i]->SetLineColor(kRed);
    }
    else if (i==3) {
      Phiplot_epos[i]->SetMarkerColor(kBlack);
      Phiplot_epos[i]->SetLineColor(kBlack);
    }
    Phiplot_epos[i]->SetMarkerStyle(29);
    Phiplot_epos[i]->SetMarkerSize(2);
    mg6->Add(Phiplot_epos[i]);*/
  
  }


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
  mg1->GetYaxis()->SetTitle("2K^{0}_{S}/(#pi^{+}+#pi^{#minus})_{norm to integrated class}");
  mg1->GetYaxis()->SetTitleOffset(1.5);
  mg1->GetYaxis()->SetTitleSize(0.05);
  mg1->GetYaxis()->SetLabelSize(0.045);
  //mg1->GetYaxis()->SetMaxDigits(3);
  mg1->Draw("A3L");

  /*TLatex* L1_0 = new TLatex();
  L1_0->SetNDC();
  L1_0->SetTextSize(0.045);
  L1_0->DrawLatex(0.465, 0.85, "pp, #sqrt{s} = 13 TeV, |y|<2.5");*/

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
  mg2->GetYaxis()->SetTitle("(#Xi^{#minus}+#bar{#Xi}^{+})/(#pi^{+}+#pi^{#minus})_{norm to integrated class}");
  mg2->GetYaxis()->SetTitleOffset(1.5);
  mg2->GetYaxis()->SetTitleSize(0.05);
  mg2->GetYaxis()->SetLabelSize(0.045);
  mg2->GetYaxis()->SetMaxDigits(4);
  mg2->Draw("A3L");

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
  L2_1->SetTextSize(0.035);
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
  mg3->GetYaxis()->SetTitle("(#Omega^{#minus}+#bar{#Omega}^{+})/(#pi^{+}+#pi^{#minus})_{norm to integrated class}");
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
  mg4->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg4->GetYaxis()->SetTitle("(#Lambda+#bar{#Lambda})/(#pi^{+}+#pi^{#minus})_{norm to integrated class}");
  mg4->GetYaxis()->SetTitleOffset(1.5);
  mg4->GetYaxis()->SetLabelSize(0.045);
  mg4->GetYaxis()->SetTitleSize(0.05);
  //mg4->GetYaxis()->SetMaxDigits(3);
  mg4->Draw("A3L");

  /*TLatex* L4_0 = new TLatex();
  L4_0->SetNDC();
  L4_0->SetTextSize(0.025);
  L4_0->DrawLatex(0.145, 0.82, "pp, #sqrt{s} = 13 TeV, |y|<2.5");

  TLegend* L4 = new TLegend(0.13, 0.73, 0.29, 0.81);
  L4->SetTextSize(0.025);
  L4->AddEntry(Lambdaplot_new[3], "PYTHIA8, CR+Ropes", "l");
  L4->AddEntry(Lambdaplot_epos[3], "EPOS4", "l");
  L4->SetLineWidth(0);
  L4->Draw("same");*/

  TLegend* L4_1 = new TLegend(0.2, 0.65, 0.35, 0.90);
  L4_1->AddEntry(Lambdaplot_new[3], "Inclusive", "l");
  L4_1->AddEntry(Lambdaplot_new[2], "|#Deltay|<1.0", "l");
  L4_1->AddEntry(Lambdaplot_new[1], "|#Deltay|<0.5", "l");
  L4_1->AddEntry(Lambdaplot_new[0], "|#Deltay|<0.1", "l");
  L4_1->SetTextSize(0.055);
  L4_1->SetLineWidth(0);
  L4_1->Draw("same");

  /*c5->cd();
  c5->SetLogx(true);

  mg5->GetXaxis()->SetMoreLogLabels();
  mg5->GetXaxis()->SetTitleSize(0.03);
  mg5->GetXaxis()->SetTitleOffset(1.3);
  mg5->GetXaxis()->SetLabelOffset(0.);
  mg5->GetXaxis()->SetLabelSize(0.03);
  mg5->GetXaxis()->SetTitle("#LTdN_{ch}/d#eta#GT_{|#eta|<0.5}");
  mg5->GetYaxis()->SetTitle("(p+#bar{p})/(#pi^{+}+#pi^{#minus})_{norm to integrated class}");
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
  mg6->GetYaxis()->SetTitle("2#LTN(#phi)#GT/#LTN(#pi^{+}+#pi^{-})#GT_{norm IV class}");
  mg6->GetYaxis()->SetTitleOffset(1.1);
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

  /*c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/k0s_phidoubratio.root");  
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/xi_phidoubratio.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/omega_phidoubratio.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/lambda_phidoubratio.root");
  c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/proton_phidoubratio.root");
  c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_moreclasses_results/phi_phidoubratio.root");*/
 											
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/k0s_phidoubratio.root");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/xi_phidoubratio.root");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/omega_phidoubratio.root");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/lambda_phidoubratio.root");
  //c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/proton_phidoubratio.root");
  //c6->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/Phitrig_evenmoreclasses_results2/phi_phidoubratio.root");
													
  c1->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/k0s_phidoubratio.pdf");
  c2->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/xi_phidoubratio.pdf");
  c3->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/omega_phidoubratio.pdf");
  c4->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/lambda_phidoubratio.pdf");
  //c5->SaveAs("/home/etlar/stefano/pythia8310/examples/rootfiles/PDF_images/proton_phidoubratio.pdf");
}  													
